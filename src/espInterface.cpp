#include "espInterface.h"

#ifdef ESP8266
static system_event_cb_t system_event_cb;

esp_err_t esp_event_loop_init(system_event_cb_t cb, void *ctx) {
    system_event_cb = cb;
}

esp_err_t esp_wifi_scan_start(wifi_scan_config_t *config, bool block) {
    // TODO: throw error if block is used
    if (block)
        return ESP_FAIL;
    
    wifi_station_scan(config, [](void *, STATUS) {
        // TODO: fill ap list and
        // raise event: SYSTEM_EVENT_SCAN_DONE
    });
    return ESP_OK;
}
#endif

