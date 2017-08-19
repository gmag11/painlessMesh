#include "espInterface.h"

#ifdef ESP8266
#include "SimpleList.h"

static SimpleList<wifi_ap_record_t> _ap_records;

static system_event_cb_t system_event_cb;

void ICACHE_FLASH_ATTR wifiEventCb(System_Event_t *event) {
    // Forward events to the esp32 system_event_t
    if (system_event_cb) {
        system_event_t system_event;
        switch (event->event) {
            case EVENT_STAMODE_CONNECTED:
                system_event.event_id = SYSTEM_EVENT_STA_CONNECTED;
                system_event_cb(NULL, &system_event);
                break;
            case EVENT_STAMODE_DISCONNECTED:
                system_event.event_id = SYSTEM_EVENT_STA_DISCONNECTED;
                system_event_cb(NULL, &system_event);
                break;
            case EVENT_STAMODE_AUTHMODE_CHANGE:
                system_event.event_id = SYSTEM_EVENT_STA_AUTHMODE_CHANGE;
                system_event_cb(NULL, &system_event);
                break;
            case EVENT_STAMODE_GOT_IP:
                system_event.event_id = SYSTEM_EVENT_STA_GOT_IP;
                system_event_cb(NULL, &system_event);
                break;

            case EVENT_SOFTAPMODE_STACONNECTED:
                system_event.event_id = SYSTEM_EVENT_AP_STACONNECTED;
                system_event_cb(NULL, &system_event);
                break;

            case EVENT_SOFTAPMODE_STADISCONNECTED:
                system_event.event_id = SYSTEM_EVENT_AP_STADISCONNECTED;
                system_event_cb(NULL, &system_event);
                break;
            
            default:
                //staticThis->debugMsg(ERROR, "Unexpected WiFi event: %d\n", event->event);
                break;
        }
    }
}

esp_err_t esp_event_loop_init(system_event_cb_t cb, void *ctx) {
    wifi_set_event_handler_cb(wifiEventCb);
    system_event_cb = cb;
}

esp_err_t esp_wifi_scan_get_ap_num(uint16_t *number) {
    *number = static_cast<uint16_t>(_ap_records.size());
    return ESP_OK;
}

esp_err_t esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records) {
    if (*number <= _ap_records.size()) {
        *number = _ap_records.size();
    }
    size_t i = 0;
    for (auto &&record : _ap_records) {
        ap_records[i] = record;
        ++i;
        if (i >= *number)
            break;
    }
    return ESP_OK;
}

esp_err_t esp_wifi_scan_start(wifi_scan_config_t *config, bool block) {
    // TODO: throw error if block is used
    if (block)
        return ESP_FAIL;
    
    wifi_station_scan(config, [](void *arg, STATUS) {
        // TODO: fill ap list and
        _ap_records.clear();
        bss_info *bssInfo = (bss_info *) arg;
        while (bssInfo != NULL) {
            _ap_records.push_back(*bssInfo);
#ifdef STAILQ_NEXT
            bssInfo = STAILQ_NEXT(bssInfo, next);
#else
            bssInfo = bssInfo->next;
#endif
        }
    
        // raise event: SYSTEM_EVENT_SCAN_DONE
        if (system_event_cb) {
            system_event_t event;
            event.event_id = SYSTEM_EVENT_SCAN_DONE;
            system_event_cb(NULL, &event);
        }
    });
    return ESP_OK;
}

esp_err_t ICACHE_FLASH_ATTR esp_wifi_disconnect(void) {
    if (wifi_station_disconnect())
        return ESP_OK;
    return ESP_FAIL;
}

esp_err_t ICACHE_FLASH_ATTR esp_wifi_set_auto_connect(bool en) {
    if (wifi_station_set_auto_connect(en))
        return ESP_OK;
    return ESP_FAIL;
}

esp_err_t ICACHE_FLASH_ATTR esp_wifi_init(wifi_init_config_t *config) {
    if (wifi_station_get_connect_status() != STATION_IDLE) { // Check if WiFi is idle
        esp_wifi_disconnect();
    }
    return ESP_OK;
}

#endif

