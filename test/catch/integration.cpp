#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#include "Arduino.h"

WiFiClass WiFi;
ESPClass ESP;

#include "catch_utils.hpp"

#include "painlessMesh.h"
#include "painlessMeshConnection.h"

using namespace painlessmesh;
painlessmesh::logger::LogClass Log;

template <typename T, typename U>
bool contains(T &container, const U &val) {
  return (std::find(container.begin(), container.end(), val) !=
          container.end());
}

struct event_test_t {
  std::string type = "";
  uint32_t nodeId = 0;
  std::string description = "";

  event_test_t() {}
  event_test_t(std::string t, uint32_t id, std::string d)
      : type(t), nodeId(id), description(d){};

  bool operator==(const event_test_t &other) {
    return (type == other.type && nodeId == other.nodeId &&
            description == other.description);
  }
  bool operator!=(const event_test_t &other) {
    return !(this->operator==(other));
  }
};

class MeshTest : public painlessMesh {
 public:
  MeshTest(Scheduler *scheduler, size_t nodeId) {
    this->init(scheduler, nodeId, 0);
    timeOffset = runif(0, 1e09);
    server = AsyncServer();
    tcp::initServer<MeshConnection, painlessMesh>(server, (*this));
    this->onNewConnection([&](uint32_t nodeId) {
      event_test_t e;
      e.nodeId = nodeId;
      e.type = "newConnection";
      events.push_back(e);
    });

    this->onReceive([&](uint32_t nodeId, std::string msg) {
      events.push_back(event_test_t("receive", nodeId, msg));
    });
  }

  void connect(MeshTest &mesh) {
    client = AsyncClient(&server);
    tcp::connect<MeshConnection, painlessMesh>(client, IPAddress(), 0, mesh);
  }

  AsyncClient client;
  AsyncServer server;
  std::list<event_test_t> events;
};

class Nodes {
 public:
  Nodes(Scheduler *scheduler, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      auto m = std::make_shared<MeshTest>(scheduler, i + 1);
      if (i > 0) m->connect((*nodes[runif(0, i - 1)]));
      nodes.push_back(m);
    }
  }
  void update() {
    for (auto &&m : nodes) m->update();
  }

  auto size() { return nodes.size(); }

  std::shared_ptr<MeshTest> get(size_t nodeId) { return nodes[nodeId - 1]; }

  std::vector<std::shared_ptr<MeshTest> > nodes;
};

SCENARIO("I can connect two meshes") {
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  // Create meshes
  // Call init with scheduler
  painlessMesh mesh1;
  mesh1.init(&scheduler, 1, 0);
  painlessMesh mesh2;
  mesh2.init(&scheduler, 2, 0);
  auto server = AsyncServer();
  tcp::initServer<MeshConnection, painlessMesh>(server, mesh1);

  auto client = AsyncClient(&server);
  tcp::connect<MeshConnection, painlessMesh>(client, IPAddress(), 0, mesh2);
  // Now mesh1 and mesh2 should be able to talk to each other
  for (auto i = 0; i < 10; ++i) {
    // scheduler.execute();
    mesh1.update();
    mesh2.update();
    // delay(100);
  }

  REQUIRE((*mesh1.getNodeList().begin()) == 2);
  REQUIRE((*mesh2.getNodeList().begin()) == 1);
  scheduler.disableAll();
  mesh1.stop();
  mesh2.stop();
}

// Make a circular mesh, test that it will be broken due to circular
// detection
SCENARIO("MeshTest works") {
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  MeshTest m1(&scheduler, 1);
  MeshTest m2(&scheduler, 2);
  m1.connect(m2);

  for (auto i = 0; i < 10; ++i) {
    m1.update();
    m2.update();
  }

  REQUIRE((*m1.getNodeList().begin()) == 2);
  REQUIRE((*m2.getNodeList().begin()) == 1);
  REQUIRE(m1.events.begin()->nodeId == 2);
  REQUIRE(m2.events.begin()->nodeId == 1);
  REQUIRE(contains(m1.events, event_test_t("newConnection", 2, "")));
}

SCENARIO("We can send a message") {
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  MeshTest m1(&scheduler, 1);
  MeshTest m2(&scheduler, 2);
  MeshTest m3(&scheduler, 3);
  MeshTest m4(&scheduler, 4);
  MeshTest m5(&scheduler, 5);
  m1.connect(m2);
  m2.connect(m3);
  m4.connect(m2);
  m5.connect(m4);

  for (auto i = 0; i < 25; ++i) {
    m1.update();
    m2.update();
    m3.update();
    m4.update();
    m5.update();
  }
  m1.sendSingle(3, "myMessage");
  m2.sendSingle(3, "myMessage3");
  m1.sendSingle(2, "myMessage2");
  m2.sendBroadcast("myMessage4", true);
  for (auto i = 0; i < 25; ++i) {
    m1.update();
    m2.update();
    m3.update();
    m4.update();
    m5.update();
  }
  REQUIRE(contains(m3.events, event_test_t("receive", 1, "myMessage")));
  REQUIRE(!contains(m1.events, event_test_t("receive", 1, "myMessage")));
  REQUIRE(!contains(m2.events, event_test_t("receive", 1, "myMessage")));
  REQUIRE(contains(m2.events, event_test_t("receive", 1, "myMessage2")));
  REQUIRE(contains(m3.events, event_test_t("receive", 2, "myMessage3")));

  REQUIRE(contains(m1.events, event_test_t("receive", 2, "myMessage4")));
  REQUIRE(contains(m2.events, event_test_t("receive", 2, "myMessage4")));
  REQUIRE(contains(m3.events, event_test_t("receive", 2, "myMessage4")));
  REQUIRE(contains(m4.events, event_test_t("receive", 2, "myMessage4")));
  REQUIRE(contains(m5.events, event_test_t("receive", 2, "myMessage4")));
}

SCENARIO("We can send a message using our Nodes class") {
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  auto n = Nodes(&scheduler, runif(5, 10));

  for (auto i = 0; i < 100; ++i) {
    n.update();
  }
  n.get(1)->sendSingle(3, "myMessage");
  n.get(2)->sendSingle(3, "myMessage3");
  n.get(1)->sendSingle(2, "myMessage2");
  n.get(2)->sendBroadcast("myMessage4", true);
  for (auto i = 0; i < 25; ++i) {
    n.update();
  }
  REQUIRE(contains(n.get(3)->events, event_test_t("receive", 1, "myMessage")));
  REQUIRE(!contains(n.get(1)->events, event_test_t("receive", 1, "myMessage")));
  REQUIRE(!contains(n.get(2)->events, event_test_t("receive", 1, "myMessage")));
  REQUIRE(contains(n.get(2)->events, event_test_t("receive", 1, "myMessage2")));
  REQUIRE(contains(n.get(3)->events, event_test_t("receive", 2, "myMessage3")));

  REQUIRE(contains(n.get(1)->events, event_test_t("receive", 2, "myMessage4")));
  REQUIRE(contains(n.get(2)->events, event_test_t("receive", 2, "myMessage4")));
  REQUIRE(contains(n.get(3)->events, event_test_t("receive", 2, "myMessage4")));
  REQUIRE(contains(n.get(4)->events, event_test_t("receive", 2, "myMessage4")));
  REQUIRE(contains(n.get(5)->events, event_test_t("receive", 2, "myMessage4")));
}

SCENARIO("Time sync is working") {
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  auto n = Nodes(&scheduler, runif(8, 12));

  uint32_t diff = 0;
  for (size_t i = 1; i < n.size(); ++i) {
    diff += ((double)std::abs((int)n.get(i)->getNodeTime() -
                              (int)n.get(i + 1)->getNodeTime())) /
            n.size();
  }
  REQUIRE(diff > 10000);

  for (auto i = 0; i < 1000; ++i) {
    n.update();
    delay(1000);
  }

  diff = 0;
  for (size_t i = 1; i < n.size(); ++i) {
    diff += ((double)std::abs((int)n.get(i)->getNodeTime() -
                              (int)n.get(i + 1)->getNodeTime())) /
            n.size();
  }
  REQUIRE(diff < 10000);
}
