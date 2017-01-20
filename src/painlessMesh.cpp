#include <Arduino.h>
#include <ArduinoJson.h>
#include <SimpleList.h>

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#include "painlessMesh.h"
#include "painlessMeshSync.h"


painlessMesh* staticThis;
uint16_t  count = 0;


// general functions
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::init(String ssid, String password, uint16_t port, bool hybridNode, _auth_mode authmode, uint8_t channel, phy_mode_t phymode, uint8_t maxtpw, uint8_t hidden, uint8_t maxconn) {
    // shut everything down, start with a blank slate.
    debugMsg(STARTUP, "init(): %d\n", wifi_station_set_auto_connect(0)); // Disable autoconnect

    randomSeed(analogRead(A0)); // Init random generator seed to generate delay variance

    if (wifi_station_get_connect_status() != STATION_IDLE) { // Check if WiFi is idle
        debugMsg(ERROR, "Station is doing something... wierd!? status=%d\n", wifi_station_get_connect_status());
        wifi_station_disconnect();
    }
    if (!hybridNode)
        wifi_softap_dhcps_stop(); // Disable ESP8266 Soft-AP DHCP server

    wifi_set_event_handler_cb(wifiEventCb); // Register Wi-Fi event handler

    wifi_set_phy_mode(phymode); // allow setting PHY_MODE_11G / PHY_MODE_11B
    system_phy_set_max_tpw(maxtpw); //maximum value of RF Tx Power, unit : 0.25dBm, range [0,82]

    staticThis = this;  // provides a way for static callback methods to access "this" object;

    // start configuration
    if (hybridNode)
        debugMsg(GENERAL, "wifi_set_opmode(STATION_MODE) succeeded? %d\n", wifi_set_opmode(STATION_MODE));
    else
        debugMsg(GENERAL, "wifi_set_opmode(STATIONAP_MODE) succeeded? %d\n", wifi_set_opmode(STATIONAP_MODE));

    _meshSSID = ssid;
    _meshPassword = password;
    _meshPort = port;
    _meshChannel = channel;
    _meshAuthMode = authmode;
    if (password == "")
        _meshAuthMode = AUTH_OPEN; //if no password ... set auth mode to open
    _meshHidden = hidden;
    _meshMaxConn = maxconn;

    uint8_t MAC[] = { 0,0,0,0,0,0 };
    wifi_get_macaddr(SOFTAP_IF, MAC);
    _nodeId = encodeNodeId(MAC);

    if (!hybridNode)
        apInit();       // setup AP

    stationInit();  // setup station

    debugMsg(STARTUP, "init(): tcp_max_con=%u, nodeId = %u\n", espconn_tcp_get_max_con(), _nodeId);
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::update(void) {
    manageStation();
    manageConnections();
    return;
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendSingle(uint32_t &destId, String &msg) {
    debugMsg(COMMUNICATION, "sendSingle(): dest=%d msg=%s\n", destId, msg.c_str());
    return sendMessage(destId, _nodeId, SINGLE, msg);
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendBroadcast(String &msg) {
    debugMsg(COMMUNICATION, "sendBroadcast(): msg=%s\n", msg.c_str());
    return broadcastMessage(_nodeId, BROADCAST, msg);
}

uint32_t ICACHE_FLASH_ATTR painlessMesh::getDelay(uint32_t nodeId) {
    String timeStamp;

    timeStamp = buildTimeStamp(TIME_REQUEST, getNodeTime());
}