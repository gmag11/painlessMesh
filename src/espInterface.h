#ifndef ESP_INTERFACE_H
#define ESP_INTERFACE_H

#ifdef ESP8266

#include <ESP8266WiFi.h>
extern "C" {
    #include "user_interface.h"
}

#define WIFI_AUTH_OPEN         AUTH_OPEN
#define WIFI_AUTH_WEP          AUTH_WEP
#define WIFI_AUTH_WPA_PSK      AUTH_WPA_PSK
#define WIFI_AUTH_WPA2_PSK     AUTH_WPA2_PSK
#define WIFI_AUTH_WPA_WPA2_PSK AUTH_WPA_WPA2_PSK
#define WIFI_AUTH_MAX          AUTH_MAX

#define WIFI_PROTOCOL_11B      PHY_MODE_11B
#define WIFI_PROTOCOL_11G      PHY_MODE_11G
#define WIFI_PROTOCOL_11N      PHY_MODE_11N

typedef AUTH_MODE wifi_auth_mode_t;
typedef struct bss_info wifi_ap_record_t;

#elif defined(ESP32)
#include <WiFi.h>
#else
#error Only ESP8266 or ESP32 platform is allowed
#endif // ESP8266

#endif // ESP_INTERFACE_H


