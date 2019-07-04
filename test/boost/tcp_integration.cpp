#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#include "Arduino.h"

#include "catch_utils.hpp"

#include "boost/asynctcp.hpp"

WiFiClass WiFi;
ESPClass ESP;

#include "painlessMeshConnection.h"

#include "painlessmesh/mesh.hpp"

using PMesh = painlessmesh::Mesh<MeshConnection>;

using namespace painlessmesh;
painlessmesh::logger::LogClass Log;

class MeshTest : public PMesh {
 public:
  MeshTest(Scheduler *scheduler, size_t id, boost::asio::io_service &io)
      : io_service(io) {
    this->nodeId = id;
    this->init(scheduler, this->nodeId);
    timeOffset = runif(0, 1e09);
    pServer = std::make_shared<AsyncServer>(io_service, this->nodeId);
    painlessmesh::tcp::initServer<MeshConnection, PMesh>(*pServer, (*this));
  }

  void connect(MeshTest &mesh) {
    auto pClient = new AsyncClient(io_service);
    painlessmesh::tcp::connect<MeshConnection, PMesh>(
        (*pClient), boost::asio::ip::address::from_string("127.0.0.1"),
        mesh.nodeId, (*this));
  }

  std::shared_ptr<AsyncServer> pServer;
  boost::asio::io_service &io_service;
};

class Nodes {
 public:
  Nodes(Scheduler *scheduler, size_t n, boost::asio::io_service &io)
      : io_service(io) {
    for (size_t i = 0; i < n; ++i) {
      auto m = std::make_shared<MeshTest>(scheduler, i + baseID, io_service);
      if (i > 0) m->connect((*nodes[runif(0, i - 1)]));
      nodes.push_back(m);
    }
  }
  void update() {
    for (auto &&m : nodes) {
      m->update();
      io_service.poll();
    }
  }

  void stop() {
    for (auto &&m : nodes) m->stop();
  }

  auto size() { return nodes.size(); }

  std::shared_ptr<MeshTest> get(size_t nodeId) {
    return nodes[nodeId - baseID];
  }

  size_t baseID = 6481;
  std::vector<std::shared_ptr<MeshTest>> nodes;
  boost::asio::io_service &io_service;
};

SCENARIO("We can setup and connect two meshes over localport") {
  using namespace logger;
  Scheduler scheduler;
  Log.setLogLevel(ERROR);
  boost::asio::io_service io_service;

  PMesh mesh1;
  mesh1.init(&scheduler, 6841);
  std::shared_ptr<AsyncServer> pServer;
  pServer = std::make_shared<AsyncServer>(io_service, 6841);
  painlessmesh::tcp::initServer<MeshConnection, PMesh>(*pServer, mesh1);

  PMesh mesh2;
  mesh2.init(&scheduler, 6842);
  auto pClient = new AsyncClient(io_service);
  painlessmesh::tcp::connect<MeshConnection, PMesh>(
      (*pClient), boost::asio::ip::address::from_string("127.0.0.1"), 6841,
      mesh2);

  for (auto i = 0; i < 100; ++i) {
    mesh1.update();
    mesh2.update();
    io_service.poll();
  }

  REQUIRE(layout::size(mesh1.asNodeTree()) == 2);
  REQUIRE(layout::size(mesh2.asNodeTree()) == 2);
}

SCENARIO("The MeshTest class works correctly") {
  using namespace logger;
  Scheduler scheduler;
  Log.setLogLevel(ERROR);
  boost::asio::io_service io_service;

  MeshTest mesh1(&scheduler, 6841, io_service);
  MeshTest mesh2(&scheduler, 6842, io_service);
  mesh2.connect(mesh1);

  for (auto i = 0; i < 100; ++i) {
    mesh1.update();
    mesh2.update();
    io_service.poll();
  }

  REQUIRE(layout::size(mesh1.asNodeTree()) == 2);
  REQUIRE(layout::size(mesh2.asNodeTree()) == 2);
}

SCENARIO("We can send a message using our Nodes class") {
  delay(1000);
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  boost::asio::io_service io_service;
  Nodes n(&scheduler, 12, io_service);

  for (auto i = 0; i < 1000; ++i) {
    n.update();
    delay(10);
  }

  REQUIRE(layout::size(n.nodes[0]->asNodeTree()) == 12);

  int x = 0;
  int y = 0;
  std::string z;
  n.nodes[0]->onReceive([&x, &y, &z](auto id, auto msg) {
    ++x;
    y = id;
    z = msg;
  });
  n.nodes[10]->sendSingle(n.nodes[2]->getNodeId(), "Blaat");
  for (auto i = 0; i < 1000; ++i) n.update();
  REQUIRE(x == 0);
  REQUIRE(y == 0);
  REQUIRE(z == "");

  n.nodes[10]->sendSingle(n.nodes[0]->getNodeId(), "Blaat");
  for (auto i = 0; i < 1000; ++i) n.update();
  REQUIRE(z == "Blaat");
  REQUIRE(x == 1);
  REQUIRE(y == n.nodes[10]->getNodeId());

  n.nodes[5]->onReceive([&x, &y, &z](auto id, auto msg) {
    ++x;
    y = id;
    z = msg;
  });
  n.nodes[10]->sendBroadcast("Blargh");
  for (auto i = 0; i < 10000; ++i) n.update();
  REQUIRE(z == "Blargh");
  REQUIRE(x == 3);
  REQUIRE(y == n.nodes[10]->getNodeId());
  n.stop();
}

SCENARIO("Time sync works") {
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  boost::asio::io_service io_service;
  auto dim = runif(8, 15);
  Nodes n(&scheduler, dim, io_service);

  int diff = 0;
  for (size_t i = 0; i < n.size() - 1; ++i) {
    diff += std::abs((int)n.nodes[0]->getNodeTime() -
                     (int)n.nodes[i + 1]->getNodeTime());
  }
  REQUIRE(diff / n.size() > 10000);

  for (auto i = 0; i < 10000; ++i) {
    n.update();
    delay(10);
  }
  diff = 0;
  for (size_t i = 0; i < n.size() - 1; ++i) {
    diff += std::abs((int)n.nodes[0]->getNodeTime() -
                     (int)n.nodes[i + 1]->getNodeTime());
  }
  REQUIRE(diff / n.size() < 10000);
  n.stop();
}

SCENARIO("Rooting works") {
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  boost::asio::io_service io_service;
  auto dim = runif(8, 15);
  Nodes n(&scheduler, dim, io_service);

  n.nodes[5]->setRoot(true);
  REQUIRE(n.nodes[5]->isRoot());
  REQUIRE(layout::isRooted(n.nodes[5]->asNodeTree()));

  for (auto i = 0; i < 10000; ++i) {
    n.update();
    delay(10);
  }

  for (auto &&node : n.nodes) {
    REQUIRE(layout::isRooted(node->asNodeTree()));
    if (n.nodes[5]->getNodeId() == node->getNodeId()) {
      REQUIRE(node->isRoot());
    } else {
      REQUIRE(!node->isRoot());
    }
  }

  n.stop();
}

SCENARIO("Network loops are detected") {
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  boost::asio::io_service io_service;
  MeshTest mesh1(&scheduler, 6841, io_service);
  MeshTest mesh2(&scheduler, 6842, io_service);
  MeshTest mesh3(&scheduler, 6843, io_service);
  MeshTest mesh4(&scheduler, 6844, io_service);
  MeshTest mesh5(&scheduler, 6845, io_service);

  mesh1.connect(mesh5);
  mesh2.connect(mesh1);
  mesh3.connect(mesh2);
  mesh4.connect(mesh3);
  mesh5.connect(mesh4);

  for (auto i = 0; i < 10000; ++i) {
    mesh1.update();
    io_service.poll();
    mesh2.update();
    io_service.poll();
    mesh3.update();
    io_service.poll();
    mesh4.update();
    io_service.poll();
    mesh5.update();
    io_service.poll();
    delay(10);
  }

  // Looped network, should break up so it can reform
  REQUIRE(layout::size(mesh1.asNodeTree()) < 5);
  REQUIRE(layout::size(mesh2.asNodeTree()) < 5);
  REQUIRE(layout::size(mesh3.asNodeTree()) < 5);
  REQUIRE(layout::size(mesh4.asNodeTree()) < 5);
  REQUIRE(layout::size(mesh5.asNodeTree()) < 5);

  mesh1.stop();
  mesh2.stop();
  mesh3.stop();
  mesh4.stop();
  mesh5.stop();
}

SCENARIO("Disconnects are detected and forwarded") {
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  boost::asio::io_service io_service;
  auto dim = runif(10, 15);
  Nodes n(&scheduler, dim, io_service);

  // Dummy task. This can catch mistaken use of the scheduler
  Task dummyT;
  int y = 0;
  dummyT.set(TASK_MILLISECOND, TASK_FOREVER, [&y]() { ++y; });
  scheduler.addTask(dummyT);
  dummyT.enable();

  for (auto i = 0; i < 1000; ++i) {
    n.update();
    delay(10);
  }

  for (auto &&node : n.nodes) {
    REQUIRE(layout::size(node->asNodeTree()) == dim);
  }

  int x = 0;
  n.nodes[5]->onChangedConnections([&x]() { ++x; });

  n.nodes[5]->onDroppedConnection([&x](auto nodeId) { ++x; });

  n.nodes[dim - 1]->onChangedConnections([&x]() { ++x; });

  auto no = n.nodes[5]->subs.size();
  REQUIRE(no > 0);

  auto ptr = (*n.nodes[5]->subs.begin());

  (*n.nodes[5]->subs.begin())->close();
  for (auto i = 0; i < 1000; ++i) {
    n.update();
    delay(10);
  }

  REQUIRE(n.nodes[5]->subs.size() == no - 1);
  REQUIRE(ptr.use_count() == 1);
  ptr = NULL;

  REQUIRE(x == 3);

  for (auto &&node : n.nodes) {
    REQUIRE(layout::size(node->asNodeTree()) < dim);
  }

  n.stop();
  REQUIRE(y > 0);
}

SCENARIO("Disconnects don't lead to crashes") {
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  boost::asio::io_service io_service;
  auto dim = runif(10, 15);
  Nodes n(&scheduler, dim, io_service);

  // Dummy task. This can catch mistaken use of the scheduler
  Task dummyT;
  int y = 0;
  dummyT.set(TASK_MILLISECOND, TASK_FOREVER, [&y]() { ++y; });
  scheduler.addTask(dummyT);
  dummyT.enable();

  for (auto i = 0; i < 1000; ++i) {
    n.update();
    delay(10);
  }

  for (auto &&node : n.nodes) {
    REQUIRE(layout::size(node->asNodeTree()) == dim);
  }

  int x = 0;
  n.nodes[5]->onChangedConnections([&x]() { ++x; });

  n.nodes[5]->onDroppedConnection([&x](auto nodeId) { ++x; });

  n.nodes[dim - 1]->onChangedConnections([&x]() { ++x; });

  auto no = n.nodes[5]->subs.size();
  REQUIRE(no > 0);

  auto ptr = (*n.nodes[5]->subs.begin());

  (*n.nodes[5]->subs.begin())->close();
  for (auto i = 0; i < 10; ++i) {
    io_service.poll();
  }
  n.update();
  for (auto i = 0; i < 10; ++i) {
    io_service.poll();
  }
  for (auto i = 0; i < 1000; ++i) {
    n.update();
    delay(10);
  }

  REQUIRE(n.nodes[5]->subs.size() == no - 1);
  REQUIRE(ptr.use_count() == 1);
  ptr = NULL;

  REQUIRE(x == 3);

  for (auto &&node : n.nodes) {
    REQUIRE(layout::size(node->asNodeTree()) < dim);
  }

  n.stop();
  REQUIRE(y > 0);
}
