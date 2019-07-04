#include "Arduino.h"
#ifndef _PAINLESS_MESH_CONFIGURATION_HPP_
#define _PAINLESS_MESH_CONFIGURATION_HPP_

#include<list>

#define _TASK_PRIORITY // Support for layered scheduling priority
#define _TASK_STD_FUNCTION

#include <TaskSchedulerDeclarations.h>

#define ARDUINOJSON_USE_LONG_LONG 1
#undef ARDUINOJSON_ENABLE_STD_STRING
#include <ArduinoJson.h>
#undef ARDUINOJSON_ENABLE_STD_STRING

// Enable (arduino) wifi support
#define PAINLESSMESH_ENABLE_ARDUINO_WIFI

// Enable OTA support
#define PAINLESSMESH_ENABLE_OTA

#define NODE_TIMEOUT 5 * TASK_SECOND
#define SCAN_INTERVAL 30 * TASK_SECOND  // AP scan period in ms

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif // ESP32

typedef String TSTRING;

// backward compatibility
template <typename T>
using SimpleList = std::list<T>;

namespace painlessmesh {
namespace wifi {
class Mesh;
};
};  // namespace painlessmesh

/** A convenience typedef to access the mesh class*/
#ifdef PAINLESSMESH_ENABLE_ARDUINO_WIFI
using painlessMesh = painlessmesh::wifi::Mesh;
#endif

#ifdef ESP32
#define MAX_CONN 10
#else
#define MAX_CONN 4
#endif // DEBUG

#endif
