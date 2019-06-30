#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#include "Arduino.h"

#include "catch_utils.hpp"

WiFiClass WiFi;
ESPClass ESP;

#include "painlessmesh/logger.hpp"

using namespace painlessmesh;

logger::LogClass Log;

SCENARIO("Fake Async classes behave similar to real ones") {
  int i = 0;
  std::string j = "";
  auto server = AsyncServer();
  AsyncClient *conn;
  server.onClient([&conn, &i, &j](void *, AsyncClient *client) {
    conn = client;
    conn->onData([&j](void *, AsyncClient *client, void *data,
                      size_t len) { j = std::string((char *)data, len); },
                 NULL);
    ++i;
  });

  auto client = AsyncClient(&server);

  std::string j2 = "";
  client.onData([&j2](void *arg, AsyncClient *client, void *data,
                      size_t len) { j2 = std::string((char *)data, len); },
                NULL);

  client.connect(IPAddress(), 0);

  THEN("server.onConnect is called") { REQUIRE(i == 1); }
  THEN("I can send data") {
    client.write("Blaat", 5);
    REQUIRE(j == "Blaat");

    conn->write("Blaat terug", 11);
    REQUIRE(j2 == "Blaat terug");
  }
  delete conn;
}
