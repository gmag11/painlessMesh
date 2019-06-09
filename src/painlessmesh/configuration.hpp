/**
 * Configution file. To override settings in this file make sure to set them
 * before including painlessMesh.h
 **/
#ifndef _PAINLESS_MESH_CONFIGURATION_HPP_
#define _PAINLESS_MESH_CONFIGURATION_HPP_

#define _TASK_PRIORITY // Support for layered scheduling priority
#define _TASK_STD_FUNCTION

#include <TaskSchedulerDeclarations.h>

#define ARDUINOJSON_USE_LONG_LONG 1
#undef ARDUINOJSON_ENABLE_STD_STRING
#include <ArduinoJson.h>
#undef ARDUINOJSON_ENABLE_STD_STRING

// Enable (arduino) wifi support
#define PAINLESSMESH_ENABLE_ARDUINO_WIFI

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif // ESP32

typedef String TSTRING;

#ifdef ESP32
#define MAX_CONN 10
#else
#define MAX_CONN 4
#endif // DEBUG

#endif
