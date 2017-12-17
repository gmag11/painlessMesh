#include <Arduino.h>
#include <ArduinoJson.h>

#include "painlessMesh.h"
#include "painlessMeshSync.h"

#include "lwip/init.h"

painlessMesh* staticThis;
uint16_t  count = 0;

// general functions
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::init(String ssid, String password, uint16_t port, nodeMode connectMode, wifi_auth_mode_t authmode, uint8_t channel, uint8_t phymode, uint8_t maxtpw, uint8_t hidden, uint8_t maxconn) {
    // shut everything down, start with a blank slate.

    randomSeed(analogRead(A0)); // Init random generator seed to generate delay variance

    tcpip_adapter_init();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    if ((esp_wifi_init(&init_config)) != ESP_OK) {
        //debugMsg(ERROR, "Station is doing something... wierd!? status=%d\n", err);
    }
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK)
        debugMsg(ERROR, "Unable to set storage to RAM only.\n");
        
    debugMsg(STARTUP, "init(): %d\n", esp_wifi_set_auto_connect(false)); // Disable autoconnect
    
    if (connectMode == AP_ONLY || connectMode == STA_AP)
        tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP); // Disable ESP8266 Soft-AP DHCP server

    // Should check whether AP_ONLY etc.
    esp_wifi_set_protocol(ESP_IF_WIFI_STA, phymode);
    esp_wifi_set_protocol(ESP_IF_WIFI_AP, phymode);
#ifdef ESP8266
    system_phy_set_max_tpw(maxtpw); //maximum value of RF Tx Power, unit : 0.25dBm, range [0,82]
#endif
    esp_event_loop_init(espWifiEventCb, NULL);

    staticThis = this;  // provides a way for static callback methods to access "this" object;

    // start configuration
    switch (connectMode) {
    case STA_ONLY:
        debugMsg(GENERAL, "esp_wifi_set_mode(STATION_MODE) succeeded? %d\n", esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK);
        break;
    case AP_ONLY:
        debugMsg(GENERAL, "esp_wifi_set_mode(AP_MODE) succeeded? %d\n", esp_wifi_set_mode(WIFI_MODE_AP) == ESP_OK);
        break;
    default:
        debugMsg(GENERAL, "esp_wifi_set_mode(STATION_AP_MODE) succeeded? %d\n", esp_wifi_set_mode(WIFI_MODE_APSTA) == ESP_OK);
    }

    _meshSSID = ssid;
    _meshPassword = password;
    _meshPort = port;
    _meshChannel = channel;
    _meshAuthMode = authmode;
    if (password == "")
        _meshAuthMode = WIFI_AUTH_OPEN; //if no password ... set auth mode to open
    _meshHidden = hidden;
    _meshMaxConn = maxconn;

    uint8_t MAC[] = { 0,0,0,0,0,0 };
    if (esp_wifi_get_mac(ESP_IF_WIFI_AP, MAC) != ESP_OK) {
        debugMsg(ERROR, "init(): esp_wifi_get_mac failed.\n");
    }
    esp_wifi_start();
    _nodeId = encodeNodeId(MAC);

    if (connectMode == AP_ONLY || connectMode == STA_AP)
        apInit();       // setup AP
    if (connectMode == STA_ONLY || connectMode == STA_AP) {
        stationScan.init(this, ssid, password, port);
        scheduler.addTask(stationScan.task);
    }

    //debugMsg(STARTUP, "init(): tcp_max_con=%u, nodeId = %u\n", espconn_tcp_get_max_con(), _nodeId);


    scheduler.enableAll();
}

void ICACHE_FLASH_ATTR painlessMesh::stop() {
    // Close all connections
    while (_connections.size() > 0) {
        auto connection = _connections.begin();
        (*connection)->close();
    }

    // Stop scanning task
    stationScan.task.setCallback(NULL);
    scheduler.deleteTask(stationScan.task);

    // Note that this results in the droppedConnections not to be signalled
    // We might want to change this later
    newConnectionTask.setCallback(NULL);
    scheduler.deleteTask(newConnectionTask);
    droppedConnectionTask.setCallback(NULL);
    scheduler.deleteTask(droppedConnectionTask);

    // Shutdown wifi hardware
    esp_wifi_disconnect();
    tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP); // Disable ESP8266 Soft-AP DHCP server
    esp_wifi_stop();
    esp_wifi_deinit();
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::update(void) {
    scheduler.execute();
    return;
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendSingle(uint32_t &destId, String &msg) {
    debugMsg(COMMUNICATION, "sendSingle(): dest=%u msg=%s\n", destId, msg.c_str());
    return sendMessage(destId, _nodeId, SINGLE, msg);
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendBroadcast(String &msg, bool includeSelf) {
    debugMsg(COMMUNICATION, "sendBroadcast(): msg=%s\n", msg.c_str());
    bool success = broadcastMessage(_nodeId, BROADCAST, msg);
    if (success && includeSelf && this->receivedCallback)
        this->receivedCallback(this->getNodeId(), msg);
    return success; 
}

bool ICACHE_FLASH_ATTR painlessMesh::startDelayMeas(uint32_t nodeId) {
    String timeStamp;
    debugMsg(S_TIME, "startDelayMeas(): NodeId %u\n", nodeId);

    auto conn = findConnection(nodeId);

    if (conn) {
        timeStamp = conn->time.buildTimeStamp(TIME_REQUEST, getNodeTime());
        //conn->timeDelayStatus = IN_PROGRESS;
    } else {
        return false;
    }
    debugMsg(S_TIME, "startDelayMeas(): Sent delay calc request -> %s\n", timeStamp.c_str());
    sendMessage(conn, nodeId, _nodeId, TIME_DELAY, timeStamp);
    //conn->timeSyncLastRequested = system_get_time();
    return true;
}
