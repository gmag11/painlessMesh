#ifndef ESP_INTERFACE_H
#define ESP_INTERFACE_H

#ifdef ESP8266
extern "C" {
#include "user_interface.h"
#include "espconn.h"
}


#else
#ifdef ESP32
#include "esp32wrap.h"


#else
#error Only ESP8266 or ESP32 platform is allowed
#endif
#endif
#endif


