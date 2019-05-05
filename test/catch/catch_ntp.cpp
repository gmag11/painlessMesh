#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#define ARDUINOJSON_USE_LONG_LONG 1
#include "ArduinoJson.h"
#undef ARDUINOJSON_ENABLE_ARDUINO_STRING
#undef PAINLESSMESH_ENABLE_ARDUINO_STRING
#define PAINLESSMESH_ENABLE_STD_STRING
typedef std::string TSTRING;

#include "catch_utils.hpp"

#include "fake_serial.h"

#define TASK_FOREVER 0
#define TASK_ONCE 0
#define TASK_MILLISECOND 1
#define TASK_SECOND 1000 * TASK_MILLISECOND
#define TASK_MINUTE 60 * TASK_SECOND

#include "painlessmesh/ntp.hpp"

using namespace painlessmesh;

logger::LogClass Log;
