#ifndef ESP_INTERFACE_H
#define ESP_INTERFACE_H

#ifdef ESP8266
extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

// TODO
// 0 Create system_event_t framework
//   0 system_event_t struct
//   0 function to subscribe to system_event_t
//   0 Rename system_event_t in the current code back to System_Event_t
// 0 Scan for AP
//   0 Create the esp get ap functions (make sure to empty the list after accessing it)
//     0 Check whether: wifi_ap_record_t and bss_info are (mostly) compatible (MOSTLY, but not enough)
//   0 Finish esp_wifi_scan_start 
//     0 call system_event_t 
//     0 fill the ap list (Or can we do this in the esp ap records?
//   - Rewrite painlessMesh to use the new functions
//     0 Subscribe to esp_event_loop_t, listening for SYSTEM_EVENT_SCAN_DONE_
//     - Copy relevant info into the aps simple list when scan done (scanComplete()
//     - call esp_wifi_scan_start instead of wifi_station_scan
// - Connect to AP
// - TCP connections
//   - espconn is a thin wrapper around lwip.h. Fastest would be to just keep using this.
// - Setup AP
// - Remove System_Event_t and add it here. We should probably register it in the esp_event_loop_init
// - Default number of accepted clients of the AP is 10 instead of 4
#define WIFI_AUTH_WPA2_PSK AUTH_WPA2_PSK 
typedef _auth_mode wifi_auth_mode_t;


#define WIFI_PROTOCOL_11B         PHY_MODE_11B
#define WIFI_PROTOCOL_11G         PHY_MODE_11G
#define WIFI_PROTOCOL_11N         PHY_MODE_11N

/*
#define PHY_MODE_11B WIFI_PROTOCOL_11B
#define PHY_MODE_11G WIFI_PROTOCOL_11G
#define PHY_MODE_11N WIFI_PROTOCOL_11B
*/

#define ESP_OK          0
#define ESP_FAIL        -1
typedef int32_t esp_err_t;

typedef enum {
    SYSTEM_EVENT_WIFI_READY = 0,           /**< ESP32 WiFi ready */
    SYSTEM_EVENT_SCAN_DONE,                /**< ESP32 finish scanning AP */
    SYSTEM_EVENT_STA_START,                /**< ESP32 station start */
    SYSTEM_EVENT_STA_STOP,                 /**< ESP32 station stop */
    SYSTEM_EVENT_STA_CONNECTED,            /**< ESP32 station connected to AP */
    SYSTEM_EVENT_STA_DISCONNECTED,         /**< ESP32 station disconnected from AP */
    SYSTEM_EVENT_STA_AUTHMODE_CHANGE,      /**< the auth mode of AP connected by ESP32 station changed */
    SYSTEM_EVENT_STA_GOT_IP,               /**< ESP32 station got IP from connected AP */
    SYSTEM_EVENT_STA_WPS_ER_SUCCESS,       /**< ESP32 station wps succeeds in enrollee mode */
    SYSTEM_EVENT_STA_WPS_ER_FAILED,        /**< ESP32 station wps fails in enrollee mode */
    SYSTEM_EVENT_STA_WPS_ER_TIMEOUT,       /**< ESP32 station wps timeout in enrollee mode */
    SYSTEM_EVENT_STA_WPS_ER_PIN,           /**< ESP32 station wps pin code in enrollee mode */
    SYSTEM_EVENT_AP_START,                 /**< ESP32 soft-AP start */
    SYSTEM_EVENT_AP_STOP,                  /**< ESP32 soft-AP stop */
    SYSTEM_EVENT_AP_STACONNECTED,          /**< a station connected to ESP32 soft-AP */
    SYSTEM_EVENT_AP_STADISCONNECTED,       /**< a station disconnected from ESP32 soft-AP */
    SYSTEM_EVENT_AP_PROBEREQRECVED,        /**< Receive probe request packet in soft-AP interface */
    SYSTEM_EVENT_AP_STA_GOT_IP6,           /**< ESP32 station or ap interface v6IP addr is preferred */
    SYSTEM_EVENT_ETH_START,                /**< ESP32 ethernet start */
    SYSTEM_EVENT_ETH_STOP,                 /**< ESP32 ethernet stop */
    SYSTEM_EVENT_ETH_CONNECTED,            /**< ESP32 ethernet phy link up */
    SYSTEM_EVENT_ETH_DISCONNECTED,         /**< ESP32 ethernet phy link down */
    SYSTEM_EVENT_ETH_GOT_IP,               /**< ESP32 ethernet got IP from connected AP */
    SYSTEM_EVENT_MAX
} system_event_id_t;

typedef struct {
    system_event_id_t     event_id;      /**< event ID */
    //system_event_info_t   event_info;    /**< event information */
} system_event_t;

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

/**
  * @brief  Initialize event loop
  *         Create the event handler and task
  *
  * @param  system_event_cb_t cb : application specified event callback, it can be modified by call esp_event_set_cb
  * @param  void *ctx : reserved for user
  *
  * @return ESP_OK : succeed
  * @return others : fail
  */
esp_err_t esp_event_loop_init(system_event_cb_t cb, void *ctx);

typedef bss_info wifi_ap_record_t;

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


// Fields are similar except esp32
typedef scan_config wifi_scan_config_t;
/**
  * @brief     Scan all available APs.
  *
  * @attention If this API is called, the found APs are stored in WiFi driver dynamic allocated memory and the
  *            will be freed in esp_wifi_get_ap_list, so generally, call esp_wifi_get_ap_list to cause
  *            the memory to be freed once the scan is done
  * @attention The values of maximum active scan time and passive scan time per channel are limited to 1500 milliseconds.
  *            Values above 1500ms may cause station to disconnect from AP and are not recommended.
  *
  * @param     config  configuration of scanning
  * @param     block if block is true, this API will block the caller until the scan is done, otherwise
  *                         it will return immediately
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_ERR_WIFI_NOT_INIT: WiFi is not initialized by eps_wifi_init
  *    - ESP_ERR_WIFI_NOT_STARTED: WiFi was not started by esp_wifi_start
  *    - ESP_ERR_WIFI_TIMEOUT: blocking scan is timeout
  *    - others: refer to error code in esp_err.h
  */
esp_err_t esp_wifi_scan_start(wifi_scan_config_t *config, bool block);

#else
#ifdef ESP32

#include "esp_wifi.h"
#include "esp_event.h"
//#include "esp32wrap.h"
//
struct espconn {};
struct esp_tcp {};

typedef int espconn_connect_callback;

#else
#error Only ESP8266 or ESP32 platform is allowed
#endif
#endif
#endif


