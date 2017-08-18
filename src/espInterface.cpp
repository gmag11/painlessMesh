#include "espInterface.h"

#ifdef ESP8266
#include "SimpleList.h"

static SimpleList<wifi_ap_record_t> _ap_records;

static system_event_cb_t system_event_cb;

esp_err_t esp_event_loop_init(system_event_cb_t cb, void *ctx) {
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
#endif

