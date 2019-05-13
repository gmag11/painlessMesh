#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#define ARDUINOJSON_USE_LONG_LONG 1
#include "ArduinoJson.h"
#undef ARDUINOJSON_ENABLE_ARDUINO_STRING
typedef std::string TSTRING;

#include "catch_utils.hpp"

#include "fake_serial.h"

#include "painlessmesh/plugin.hpp"

using namespace painlessmesh;

class CustomPackage : public plugin::SinglePackage {
 public:
  double sensor = 1.0;

  CustomPackage() : SinglePackage(20) {}

  CustomPackage(JsonObject jsonObj) : SinglePackage(jsonObj) {
    sensor = jsonObj["sensor"];
  }

  JsonObject addTo(JsonObject&& jsonObj) {
    jsonObj = SinglePackage::addTo(std::move(jsonObj));
    jsonObj["sensor"] = sensor;
    return jsonObj;
  }

  size_t jsonObjectSize() { return JSON_OBJECT_SIZE(noJsonFields + 1); }
};

class BCustomPackage : public plugin::BroadcastPackage {
 public:
  double sensor = 1.0;

  BCustomPackage() : BroadcastPackage(21) {}

  BCustomPackage(JsonObject jsonObj) : BroadcastPackage(jsonObj) {
    sensor = jsonObj["sensor"];
  }

  JsonObject addTo(JsonObject&& jsonObj) {
    jsonObj = BroadcastPackage::addTo(std::move(jsonObj));
    jsonObj["sensor"] = sensor;
    return jsonObj;
  }

  size_t jsonObjectSize() { return JSON_OBJECT_SIZE(noJsonFields + 1); }
};

class MockConnection : public layout::Neighbour {
 public:
  bool addMessage(TSTRING msg) { return true; }
};

SCENARIO("We can send a custom package") {
  GIVEN("A package") {
    auto pkg = CustomPackage();
    pkg.from = 1;
    pkg.dest = 2;
    pkg.sensor = 0.5;
    REQUIRE(pkg.routing == router::SINGLE);
    REQUIRE(pkg.type == 20);
    WHEN("Converting it to and from Variant") {
      auto var = protocol::Variant(&pkg);
      auto pkg2 = var.to<CustomPackage>();
      THEN("Should result in the same values") {
        REQUIRE(pkg2.sensor == pkg.sensor);
        REQUIRE(pkg2.from == pkg.from);
        REQUIRE(pkg2.dest == pkg.dest);
        REQUIRE(pkg2.routing == pkg.routing);
        REQUIRE(pkg2.type == pkg.type);
      }
    }
  }

  GIVEN("A broadcast package") {
    auto pkg = BCustomPackage();
    pkg.from = 1;
    pkg.sensor = 0.5;
    REQUIRE(pkg.routing == router::BROADCAST);
    REQUIRE(pkg.type == 21);
    WHEN("Converting it to and from Variant") {
      auto var = protocol::Variant(&pkg);
      auto pkg2 = var.to<CustomPackage>();
      THEN("Should result in the same values") {
        REQUIRE(pkg2.sensor == pkg.sensor);
        REQUIRE(pkg2.from == pkg.from);
        REQUIRE(pkg2.routing == pkg.routing);
        REQUIRE(pkg2.type == pkg.type);
      }
    }
  }

  GIVEN("A package handler function") {
    auto handler = plugin::PackageHandler<MockConnection>();
    auto func = [](protocol::Variant variant) {
      auto pkg = variant.to<CustomPackage>();
      REQUIRE(pkg.routing == router::BROADCAST);
      return false;
    };
    THEN("We can pass it to handler") { handler.onPackage(20, func); }
  }

  GIVEN("A package") {
    auto handler = plugin::PackageHandler<MockConnection>();
    auto pkg = CustomPackage();
    THEN("We can call sendPackage") {
      auto res = handler.sendPackage(&pkg);
      REQUIRE(!res);
    }
  }
}

SCENARIO("We can add tasks to the taskscheduler") {
  GIVEN("A couple of tasks added") {
    Scheduler mScheduler;
    auto handler = plugin::PackageHandler<MockConnection>();
    int i = 0;
    int j = 0;
    int k = 0;
    auto task1 = handler.addTask(mScheduler, 0, 1, [&i]() { ++i; });
    auto task2 = handler.addTask(mScheduler, 0, 3, [&j]() { ++j; });
    auto task3 = handler.addTask(mScheduler, 0, 3, [&k]() { ++k; });
    REQUIRE(task1.use_count() == 2);
    REQUIRE(task2.use_count() == 2);

    WHEN("Executing the tasks") {
      THEN("They should be called and automatically removed") {
        REQUIRE(i == 0);
        REQUIRE(j == 0);
        mScheduler.execute();
        REQUIRE(i == 1);
        mScheduler.execute();
        REQUIRE(task1.use_count() == 1);
        REQUIRE(i == 1);
        REQUIRE(j == 2);
        // Still kept in handler, because hasn't been executed 3 times yet
        REQUIRE(task2.use_count() == 2);
        REQUIRE(task3.use_count() == 2);
        task3->disable();
        REQUIRE(task3.use_count() == 1);
      }
    }
  }
}
// We will test this by implementing the named mesh as a plugin
SCENARIO("We want to implement a named mesh as a custom type") {
  // Note that every plugin will have a unique id (which is also the package
  // type) that can be used to get access to handling functions, data etc.

  // Every plugin has one (or more) class inheriting from plugin::package

  // PluginHandler will be inherited into the painlessMesh class
  // and is responsible for routing

  // Note data will be included in the handling functions
  // [&data](plugin::Package)

  // We should have a namedMesh, which implements sendSingle("name",msg);
  // Then allow people to do class Mesh : public painlessMesh, public NamedMesh

  // Would be nice to be able to override onReceive callback, so we can do
  // something else
}

// Maybe also add ota implementation

// NOTE routing in painlessMesh handleMessage needs to be improved:
// If variant.routing == NEIGHBOUR -> handle and don't sent on
// If variant.routing == SINGLE -> if variant.dest is this nodeID: handle, else
// sent on to its destination if variant.routing == BROADCAST -> handle and sent
// on We can check if routing key exists, otherwise for old messages we can
// hardcode it, with most being NEIGHBOUR except for the SINGLE message type and
// BROADCAST msg type
// Later we could maybe add a multicast type. This would be relatively easy by
// having destination be an vector of nodeIDs.. although would need to calculate
// the set of nodes contained in each route.

// We may need to implement a general Plugin class, which can be passed to
// the handler and will be hold the different packages and tasks.
// This will come out when I try to add actual plugins such as namedmesh
// and ota
