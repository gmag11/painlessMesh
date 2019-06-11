#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#include "Arduino.h"

WiFiClass WiFi;
ESPClass ESP;

#include "catch_utils.hpp"

#include "painlessMesh.h"

using namespace painlessmesh;

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

  // Make a circular mesh, test that it will be broken due to circular
  // detection
}
