#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#define ARDUINOJSON_USE_LONG_LONG 1
#include "ArduinoJson.h"
#undef ARDUINOJSON_ENABLE_ARDUINO_STRING
#undef PAINLESSMESH_ENABLE_ARDUINO_STRING
#define PAINLESSMESH_ENABLE_STD_STRING
typedef std::string TSTRING;

#include "catch_utils.hpp"

#include "fake_asynctcp.hpp"
#include "fake_serial.h"

WiFiClass WiFi;
ESPClass ESP;

#define NODE_TIMEOUT 10 * TASK_SECOND

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
