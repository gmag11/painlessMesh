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

#define WIFI_PROTOCOL_11B       PHY_MODE_11B
#define WIFI_PROTOCOL_11G       PHY_MODE_11G
#define WIFI_PROTOCOL_11N       PHY_MODE_11N

#define ESP_OK          0
#define ESP_FAIL        -1

typedef AUTH_MODE wifi_auth_mode_t;
typedef int32_t esp_err_t;
typedef struct bss_info wifi_ap_record_t;

typedef enum {
    SYSTEM_EVENT_SCAN_DONE,

    SYSTEM_EVENT_STA_START,
    SYSTEM_EVENT_STA_CONNECTED,
    SYSTEM_EVENT_STA_DISCONNECTED,
    SYSTEM_EVENT_STA_AUTHMODE_CHANGE,
    SYSTEM_EVENT_STA_GOT_IP,

    SYSTEM_EVENT_AP_START,
    SYSTEM_EVENT_AP_STACONNECTED,
    SYSTEM_EVENT_AP_STADISCONNECTED,
    SYSTEM_EVENT_AP_PROBEREQRECVED,

    SYSTEM_EVENT_MAX
} system_event_id_t;

typedef struct {
    system_event_id_t       event_id;      /**< event ID */
    //system_event_info_t   event_info;    /**< event information */
} system_event_t;

typedef ip_info tcpip_adapter_ip_info_t;

typedef enum {
    WIFI_STORAGE_FLASH,  /**< all configuration will store in both memory and flash */
    WIFI_STORAGE_RAM    /**< all configuration will only store in the memory */
} wifi_storage_t;

typedef enum {
    TCPIP_ADAPTER_IF_STA = STATION_IF,
    TCPIP_ADAPTER_IF_AP = SOFTAP_IF
} tcpip_adapter_if_t;


typedef enum {
    ESP_IF_WIFI_STA = STATION_IF,
    ESP_IF_WIFI_AP = SOFTAP_IF
} wifi_interface_t;

esp_err_t esp_wifi_set_storage(wifi_storage_t storage);
esp_err_t tcpip_adapter_dhcps_stop(tcpip_adapter_if_t tcpip_if);
esp_err_t esp_wifi_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap);

typedef softap_config wifi_ap_config_t;
typedef station_config wifi_sta_config_t;

typedef union {
    wifi_ap_config_t ap;   /**< configuration of AP */
    wifi_sta_config_t sta; /**< configuration of STA */
} wifi_config_t;

typedef struct scan_config wifi_scan_config_t;

/**
  * @brief  Application specified event callback function
  *
  * @param  void *ctx : reserved for user
  * @param  system_event_t *event : event type defined in this file
  *
  * @return ESP_OK : succeed
  * @return others : fail
  */
typedef esp_err_t (*system_event_cb_t)(void *ctx, system_event_t *event);

esp_err_t esp_event_loop_init(system_event_cb_t cb, void *ctx);

esp_err_t esp_wifi_start();


esp_err_t tcpip_adapter_set_ip_info(tcpip_adapter_if_t tcpip_if, tcpip_adapter_ip_info_t *ip_info);

esp_err_t esp_wifi_get_config(wifi_interface_t ifx, wifi_config_t *conf);

esp_err_t tcpip_adapter_set_hostname(tcpip_adapter_if_t tcpip_if, const char *hostname);


esp_err_t esp_wifi_scan_start(wifi_scan_config_t *config, bool block);

/**
  * @brief     Get number of APs found in last scan
  *
  * @param[out] number  store number of APIs found in last scan
  *
  * @attention This API can only be called when the scan is completed, otherwise it may get wrong value.
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_ERR_WIFI_NOT_INIT: WiFi is not initialized by eps_wifi_init
  *    - ESP_ERR_WIFI_NOT_STARTED: WiFi is not started by esp_wifi_start
  *    - ESP_ERR_WIFI_ARG: invalid argument
  */
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *number);

/**
 * @brief     Get AP list found in last scan
 *
 * @param[inout]  number As input param, it stores max AP number ap_records can hold.
 *                As output param, it receives the actual AP number this API returns.
 * @param         ap_records  wifi_ap_record_t array to hold the found APs
 *
 * @return
 *    - ESP_OK: succeed
 *    - ESP_ERR_WIFI_NOT_INIT: WiFi is not initialized by eps_wifi_init
 *    - ESP_ERR_WIFI_NOT_STARTED: WiFi is not started by esp_wifi_start
 *    - ESP_ERR_WIFI_ARG: invalid argument
 *    - ESP_ERR_WIFI_NO_MEM: out of memory
 */
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records);

esp_err_t esp_wifi_set_config(wifi_interface_t ifx, wifi_config_t *conf);

#elif defined(ESP32)

#define ICACHE_FLASH_ATTR 
extern "C" {
    #include "esp_wifi.h"
    #include "esp_event.h"
    #include "esp_event_loop.h"
    typedef ip4_addr ip_addr;
}
#else
#error Only ESP8266 or ESP32 platform is allowed
#endif // ESP8266




#endif // ESP_INTERFACE_H


