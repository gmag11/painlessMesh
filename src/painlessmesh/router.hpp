#ifndef _PAINLESS_MESH_ROUTER_HPP_
#define _PAINLESS_MESH_ROUTER_HPP_

#include <map>
#include <algorithm>

#include "painlessmesh/layout.hpp"
#include "painlessmesh/logger.hpp"
#include "painlessmesh/protocol.hpp"

extern painlessmesh::logger::LogClass Log;

namespace painlessmesh {

/**
 * Helper functions to route messages
 */
namespace router {

/**
 * Manage callbacks for receiving packages
 *
 * TODO: Implement a CallbackQueue, which has a list store and an execute
 * function. The map can then be changed to std::map<int, CallbackQueue> and
 * this queue can be used for mesh events etc
 */
template <typename... Args>
class CallbackList {
 public:
  /**
   * Add a callback for specific package id
   */
  void onPackage(int id, std::function<bool(Args...)> func) {
    callbackMap[id].push_back(func);
  }

  /**
   * Execute all the callbacks associated with a certain package
   */
  int execute(int id, Args... args) {
    auto i = 0;
    for (auto&& f : callbackMap[id]) {
      ++i;
      auto last = f(args...);
      if (last) return i;
    }
    return i;
  }

 protected:
  std::map<int, std::list<std::function<bool(Args...)>>> callbackMap;
};

template <typename T>
using MeshCallbackList =
    CallbackList<protocol::Variant, std::shared_ptr<T>, uint32_t>;

template <class T>
std::shared_ptr<T> findRoute(layout::Layout<T> tree,
                             std::function<bool(std::shared_ptr<T>)> func) {
  auto route = std::find_if(tree.subs.begin(), tree.subs.end(), func);
  if (route == tree.subs.end()) return NULL;
  return (*route);
}

template <class T>
std::shared_ptr<T> findRoute(layout::Layout<T> tree, uint32_t nodeId) {
  return findRoute<T>(tree, [nodeId](std::shared_ptr<T> s) {
    return layout::contains((*s), nodeId);
  });
}

template <class T, class U>
bool send(T package, std::shared_ptr<U> conn, bool priority = false) {
  auto variant = painlessmesh::protocol::Variant(package);
  TSTRING msg;
  variant.printTo(msg);
  return conn->addMessage(msg, priority);
}

template <class U>
bool send(protocol::Variant variant, std::shared_ptr<U> conn,
          bool priority = false) {
  TSTRING msg;
  variant.printTo(msg);
  return conn->addMessage(msg, priority);
}

template <class T, class U>
bool send(T package, layout::Layout<U> layout) {
  auto variant = painlessmesh::protocol::Variant(package);
  TSTRING msg;
  variant.printTo(msg);
  auto conn = findRoute<U>(layout, variant.dest);
  if (conn) return conn->addMessage(msg);
  return false;
}

template <class U>
bool send(protocol::Variant variant, layout::Layout<U> layout) {
  TSTRING msg;
  variant.printTo(msg);
  auto conn = findRoute<U>(layout, variant.dest());
  if (conn) return conn->addMessage(msg);
  return false;
}

template <class T, class U>
size_t broadcast(T package, layout::Layout<U> layout, uint32_t exclude) {
  auto variant = painlessmesh::protocol::Variant(package);
  TSTRING msg;
  variant.printTo(msg);
  size_t i = 0;
  for (auto&& conn : layout.subs) {
    if (conn->nodeId != 0 && conn->nodeId != exclude) {
      auto sent = conn->addMessage(msg);
      if (sent) ++i;
    }
  }
  return i;
}

template <class T>
size_t broadcast(protocol::Variant variant, layout::Layout<T> layout,
                 uint32_t exclude) {
  TSTRING msg;
  variant.printTo(msg);
  size_t i = 0;
  for (auto&& conn : layout.subs) {
    if (conn->nodeId != 0 && conn->nodeId != exclude) {
      auto sent = conn->addMessage(msg);
      if (sent) ++i;
    }
  }
  return i;
}

template <class T>
void routePackage(layout::Layout<T> layout, std::shared_ptr<T> connection,
                  TSTRING pkg, MeshCallbackList<T> cbl, uint32_t receivedAt) {
  using namespace logger;
  Log(COMMUNICATION, "routePackage(): Recvd from %u: %s\n", connection->nodeId,
      pkg.c_str());
  auto variant = protocol::Variant(pkg);
  if (variant.error) {
    Log(ERROR,
        "routePackage(): variant parsing failed. total_length=%d, data=%s<--\n",
        pkg.length(), pkg.c_str());
    return;
  }

  if (variant.routing() == SINGLE && variant.dest() != layout.nodeId) {
    // Send on without further processing
    send<T>(variant, layout);
    return;
  } else if (variant.routing() == BROADCAST) {
    broadcast<T>(variant, layout, connection->nodeId);
  }
  auto calls = cbl.execute(variant.type(), variant, connection, receivedAt);
  if (calls == 0)
    Log(DEBUG, "routePackage(): No callbacks executed; %s\n", pkg.c_str());
}

template <class T, class U>
void handleNodeSync(T& mesh, protocol::NodeTree newTree,
                    std::shared_ptr<U> conn) {
  Log(logger::SYNC, "handleNodeSync(): with %u\n", conn->nodeId);

  if (!conn->validSubs(newTree)) {
    Log(logger::SYNC, "handleNodeSync(): invalid new connection\n");
    conn->close();
    return;
  }

  if (conn->newConnection) {
    auto oldConnection = router::findRoute<U>(mesh, newTree.nodeId);
    if (oldConnection) {
      Log(logger::SYNC,
          "handleNodeSync(): already connected to %u. Closing the new "
          "connection \n",
          conn->nodeId);
      conn->close();
      return;
    }

    // TODO: Move this to its own function
    mesh.newConnectionTask.set(
        TASK_SECOND, TASK_ONCE, [&mesh, remoteNodeId = newTree.nodeId]() {
          Log(logger::CONNECTION, "newConnectionTask():\n");
          Log(logger::CONNECTION, "newConnectionTask(): adding %u now= %u\n",
              remoteNodeId, mesh.getNodeTime());
          if (mesh.newConnectionCallback)
            mesh.newConnectionCallback(
                remoteNodeId);  // Connection dropped. Signal user
        });

    mesh._scheduler.addTask(mesh.newConnectionTask);
    mesh.newConnectionTask.enable();

    // Initially interval is every 10 seconds,
    // this will slow down to TIME_SYNC_INTERVAL
    // after first succesfull sync
    conn->timeSyncTask.set(10 * TASK_SECOND, TASK_FOREVER, [conn, &mesh]() {
      Log(logger::S_TIME, "timeSyncTask(): %u\n", conn->nodeId);
      mesh.startTimeSync(conn);
    });
    mesh._scheduler.addTask(conn->timeSyncTask);
    if (conn->station)
      // We are STA, request time immediately
      conn->timeSyncTask.enable();
    else
      // We are the AP, give STA the change to initiate time sync
      conn->timeSyncTask.enableDelayed();
    conn->newConnection = false;
  }

  if (conn->updateSubs(newTree)) {
    if (mesh.changedConnectionsCallback) mesh.changedConnectionsCallback();
    mesh.syncSubConnections(conn->nodeId);
  } else {
    conn->nodeSyncTask.delay();
    mesh.stability += min(1000 - mesh.stability, (size_t)25);
  }
}

template <class T, typename U>
router::MeshCallbackList<U> addPackageCallback(
    router::MeshCallbackList<U>&& callbackList, T& mesh) {
  // REQUEST type,
  callbackList.onPackage(
      protocol::NODE_SYNC_REQUEST,
      [&mesh](protocol::Variant variant, std::shared_ptr<U> connection,
              uint32_t receivedAt) {
        auto newTree = variant.to<protocol::NodeSyncRequest>();
        handleNodeSync<T, U>(mesh, newTree, connection);
        send<protocol::NodeSyncReply>(
            connection->reply(std::move(mesh.asNodeTree())), connection, true);
        return false;
      });

  // Reply type just handle it
  callbackList.onPackage(
      protocol::NODE_SYNC_REPLY,
      [&mesh](protocol::Variant variant, std::shared_ptr<U> connection,
              uint32_t receivedAt) {
        auto newTree = variant.to<protocol::NodeSyncReply>();
        handleNodeSync<T, U>(mesh, newTree, connection);
        return false;
      });

  return callbackList;
}

}  // namespace router
}  // namespace painlessmesh

#endif

