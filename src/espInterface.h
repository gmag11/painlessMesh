#ifndef ESP_INTERFACE_H
#define ESP_INTERFACE_H

#ifdef ESP8266
extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#define WIFI_AUTH_WPA2_PSK AUTH_WPA2_PSK 
typedef _auth_mode wifi_auth_mode_t;

/*

#define PHY_MODE_11B WIFI_PROTOCOL_11B
#define PHY_MODE_11G WIFI_PROTOCOL_11G
#define PHY_MODE_11N WIFI_PROTOCOL_11B
*/


#else
#ifdef ESP32

#include "esp_wifi_types.h"
//#include "esp32wrap.h"
//
struct bss_info {};
struct espconn {};
struct esp_tcp {};

#else
#error Only ESP8266 or ESP32 platform is allowed
#endif
#endif
#endif


