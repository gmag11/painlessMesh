#ifndef _PAINLESS_MESH_CALLBACK_HPP_
#define _PAINLESS_MESH_CALLBACK_HPP_

#include<map>

#include "painlessmesh/configuration.hpp"

#include "painlessmesh/logger.hpp"

extern painlessmesh::logger::LogClass Log;

namespace painlessmesh {

/**
 * Helper functions to work with multiple callbacks
 */
namespace callback {
template <typename... Args>
class List {
 public:
  int execute(Args... args) {
    for (auto&& f : callbacks) {
      f(args...);
    }
    return callbacks.size();
  }

  /*
   * Needs to be wrapped into semaphore
   *
  int executeWithScheduler(Scheduler& scheduler, Args... args) {
    scheduler.execute();
    for (auto&& f : callbacks) {
      f(args...);
      scheduler.execute();
    }
    return callbacks.size();
  }*/

  /** Add callbacks to the end of the list.
   */
  void push_back(std::function<void(Args...)> func) {
    callbacks.push_back(func);
  }

 protected:
  std::list<std::function<void(Args...)>> callbacks;
};

/**
 * Manage callbacks for receiving packages
 */
template <typename... Args>
class PackageCallbackList {
 public:
  /**
   * Add a callback for specific package id
   */
  void onPackage(int id, std::function<void(Args...)> func) {
    callbackMap[id].push_back(func);
  }

  /**
   * Execute all the callbacks associated with a certain package
   */
  int execute(int id, Args... args) { return callbackMap[id].execute(args...); }

 protected:
  std::map<int, List<Args...>> callbackMap;
};

template <typename T>
using MeshPackageCallbackList =
    PackageCallbackList<protocol::Variant, std::shared_ptr<T>, uint32_t>;
}  // namespace callback
}  // namespace painlessmesh

#endif

