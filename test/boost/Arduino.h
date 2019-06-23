/**
 * Wrapper file, which is used to test on PC hardware
 */
#ifndef ARDUINO_WRAP_H
#define ARDUINO_WRAP_H

#include <sys/time.h>
#include <unistd.h>

#define F(string_literal) string_literal
#define ARDUINO_ARCH_ESP8266
#define PAINLESSMESH_BOOST

#ifndef NULL
#define NULL 0
#endif

inline unsigned long millis() {
  struct timeval te;
  gettimeofday(&te, NULL);  // get current time
  long long milliseconds =
      te.tv_sec * 1000LL + te.tv_usec / 1000;  // calculate milliseconds
  // printf("milliseconds: %lld\n", milliseconds);
  return milliseconds;
}

inline unsigned long micros() {
  struct timeval te;
  gettimeofday(&te, NULL);  // get current time
  long long milliseconds = te.tv_sec * 1000000LL + te.tv_usec;
  return milliseconds;
}

inline void delay(int i) { usleep(i); }

inline void yield() {}

/**
 * Override the configution file.
 **/

#ifndef _PAINLESS_MESH_CONFIGURATION_HPP_
#define _PAINLESS_MESH_CONFIGURATION_HPP_

#define _TASK_PRIORITY  // Support for layered scheduling priority
#define _TASK_STD_FUNCTION

#include <TaskSchedulerDeclarations.h>

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#undef ARDUINOJSON_ENABLE_ARDUINO_STRING

#define ICACHE_FLASH_ATTR

#define PAINLESSMESH_ENABLE_STD_STRING
#define PAINLESSMESH_ENABLE_OTA
#define NODE_TIMEOUT 5 * TASK_SECOND

typedef std::string TSTRING;

#ifdef ESP32
#define MAX_CONN 10
#else
#define MAX_CONN 4
#endif  // DEBUG

#include "boost/asynctcp.hpp"
#include "fake_serial.hpp"

typedef enum {
  WL_NO_SHIELD = 255,  // for compatibility with WiFi Shield library
  WL_IDLE_STATUS = 0,
  WL_NO_SSID_AVAIL = 1,
  WL_SCAN_COMPLETED = 2,
  WL_CONNECTED = 3,
  WL_CONNECT_FAILED = 4,
  WL_CONNECTION_LOST = 5,
  WL_DISCONNECTED = 6
} wl_status_t;

class WiFiClass {
 public:
  void disconnect() {}
  auto status() { return WL_CONNECTED; }
};

class ESPClass {
 public:
  size_t getFreeHeap() { return 1e6; }
};

extern WiFiClass WiFi;
extern ESPClass ESP;

#endif
#endif
