#include "espInterface.h"

#ifdef ESP8266
#include<list>

esp_err_t ICACHE_FLASH_ATTR esp_wifi_set_mode(wifi_mode_t mode) {
    if (wifi_set_opmode(mode))
        return ESP_OK;
    return ESP_FAIL;
}

static std::list<wifi_ap_record_t> _ap_records;

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

esp_err_t ICACHE_FLASH_ATTR esp_event_loop_init(system_event_cb_t cb, void *ctx) {
    wifi_set_event_handler_cb(wifiEventCb);
    system_event_cb = cb;
    return ESP_OK;
}

esp_err_t ICACHE_FLASH_ATTR esp_wifi_scan_get_ap_num(uint16_t *number) {
    *number = static_cast<uint16_t>(_ap_records.size());
    return ESP_OK;
}

esp_err_t ICACHE_FLASH_ATTR esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records) {
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

esp_err_t ICACHE_FLASH_ATTR esp_wifi_scan_start(wifi_scan_config_t *config, bool block) {
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

esp_err_t ICACHE_FLASH_ATTR esp_wifi_deinit() {
    return ESP_OK;
}

static wifi_storage_t wifi_storage;

esp_err_t esp_wifi_set_storage(wifi_storage_t storage) {
    wifi_storage = storage;
    return ESP_OK;
}

esp_err_t ICACHE_FLASH_ATTR esp_wifi_start() {
    if (system_event_cb) {
        system_event_t event;
        event.event_id = SYSTEM_EVENT_STA_START;
        system_event_cb(NULL, &event);
        system_event_t event_ap;
        event_ap.event_id = SYSTEM_EVENT_AP_START;
        system_event_cb(NULL, &event_ap);
    }
    return ESP_OK;
}

esp_err_t ICACHE_FLASH_ATTR esp_wifi_stop() {
    return ESP_OK;
}

void ICACHE_FLASH_ATTR tcpip_adapter_init() {}

esp_err_t ICACHE_FLASH_ATTR tcpip_adapter_get_ip_info(tcpip_adapter_if_t tcpip_if, tcpip_adapter_ip_info_t *ip_info) {
    if (wifi_get_ip_info(tcpip_if, ip_info))
        return ESP_OK;
    return ESP_FAIL;
}

esp_err_t ICACHE_FLASH_ATTR tcpip_adapter_set_ip_info(tcpip_adapter_if_t tcpip_if, tcpip_adapter_ip_info_t *ip_info) {
    if (wifi_set_ip_info(tcpip_if, ip_info))
        return ESP_OK;
    return ESP_FAIL;
}

esp_err_t ICACHE_FLASH_ATTR tcpip_adapter_dhcps_start(tcpip_adapter_if_t tcpip_if) {
    if (tcpip_if == TCPIP_ADAPTER_IF_AP) {
        if (wifi_softap_dhcps_start())
            return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t ICACHE_FLASH_ATTR tcpip_adapter_dhcps_stop(tcpip_adapter_if_t tcpip_if) {
    if (tcpip_if == TCPIP_ADAPTER_IF_AP) {
        if (wifi_softap_dhcps_stop())
            return ESP_OK;
    }
    return ESP_FAIL;

}

esp_err_t ICACHE_FLASH_ATTR tcpip_adapter_set_hostname(tcpip_adapter_if_t tcpip_if, 
        const char *hostname) {
    // Currently only STA is supported
    if (tcpip_if == TCPIP_ADAPTER_IF_STA) {
        wifi_station_set_hostname((char*)hostname);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t ICACHE_FLASH_ATTR esp_wifi_set_config(wifi_interface_t ifx, wifi_config_t *conf) {
    if (ifx == ESP_IF_WIFI_STA) {
        if (wifi_storage == WIFI_STORAGE_FLASH) {
            if (wifi_station_set_config(&conf->sta))
                return ESP_OK;
        } else {
            if (wifi_station_set_config_current(&conf->sta))
                return ESP_OK;
        }
    }

    if (ifx == ESP_IF_WIFI_AP) {
        if (wifi_storage == WIFI_STORAGE_FLASH) {
            if (wifi_softap_set_config(&conf->ap))
                return ESP_OK;
        } else {
            if (wifi_softap_set_config_current(&conf->ap))
                return ESP_OK;
        }
    }

    return ESP_FAIL;
}

esp_err_t ICACHE_FLASH_ATTR esp_wifi_get_config(wifi_interface_t ifx, wifi_config_t *conf) {
    if (ifx == ESP_IF_WIFI_STA)
        if (wifi_station_get_config(&conf->sta))
            return ESP_OK;

    if (ifx == ESP_IF_WIFI_AP)
        if (wifi_softap_get_config(&conf->ap))
            return ESP_OK;
    return ESP_FAIL;
}

esp_err_t ICACHE_FLASH_ATTR esp_wifi_connect() {
    if (wifi_station_connect())
        return ESP_OK;
    return ESP_FAIL;
}

esp_err_t ICACHE_FLASH_ATTR esp_wifi_get_mac(wifi_interface_t ifx, uint8_t mac[6]) {
    if (wifi_get_macaddr(ifx, mac))
        return ESP_OK;
    return ESP_FAIL;
}


esp_err_t ICACHE_FLASH_ATTR esp_wifi_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap) {
    // NOTE that esp8266 can't use a different mode for ap and station
    if (ifx == ESP_IF_WIFI_STA ||
        ifx == ESP_IF_WIFI_AP)
        if (wifi_set_phy_mode(static_cast<phy_mode_t>(protocol_bitmap)))
            return ESP_OK;
    return ESP_FAIL;
}
#endif

