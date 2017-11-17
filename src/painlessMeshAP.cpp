//
//  painlessMeshAP.cpp
//  
//
//  Created by Bill Gray on 7/26/16.
// 
//

#include <Arduino.h>

#include "painlessMesh.h"

extern painlessMesh* staticThis;

// AP functions
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::apInit(void) {
    //    String password( MESH_PASSWORD );

    ip4_addr_t ip, netmask;
    IP4_ADDR(&ip, 10, (_nodeId & 0xFF00) >> 8, (_nodeId & 0xFF), 1);
    IP4_ADDR(&netmask, 255, 255, 255, 0);

    tcpip_adapter_ip_info_t ipInfo;
    ipInfo.ip = ip;
    ipInfo.gw = ip;
    ipInfo.netmask = netmask;
    if (tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &ipInfo) != ESP_OK) {
        debugMsg(ERROR, "tcpip_adapter_set_ip_info() failed\n");
    }

    /*
    debugMsg(STARTUP, "apInit(): Starting AP with SSID=%s IP=%d.%d.%d.%d GW=%d.%d.%d.%d NM=%d.%d.%d.%d\n",
             _meshSSID.c_str(),
             IP2STR(&ipInfo.ip),
             IP2STR(&ipInfo.gw),
             IP2STR(&ipInfo.netmask));
             */

    wifi_config_t apConfig;
    esp_wifi_get_config(ESP_IF_WIFI_AP, &apConfig);

    memset(apConfig.ap.ssid, 0, 32);
    memset(apConfig.ap.password, 0, 64);
    memcpy(apConfig.ap.ssid, _meshSSID.c_str(), _meshSSID.length());
    memcpy(apConfig.ap.password, _meshPassword.c_str(), _meshPassword.length());

    apConfig.ap.authmode = _meshAuthMode; // AUTH_WPA2_PSK
    apConfig.ap.ssid_len = _meshSSID.length();
    apConfig.ap.ssid_hidden = _meshHidden;
    apConfig.ap.channel = _meshChannel;
    apConfig.ap.beacon_interval = 100;
    apConfig.ap.max_connection = _meshMaxConn; // how many stations can connect to ESP8266 softAP at most, max is 4

    esp_wifi_set_config(ESP_IF_WIFI_AP, &apConfig);// Set ESP8266 softap config .
    if (tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP) != ESP_OK)
        debugMsg(ERROR, "DHCP server failed\n");
    else
        debugMsg(STARTUP, "DHCP server started\n");

    // establish AP tcpServers
    tcpServerInit();
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::tcpServerInit() {
    debugMsg(GENERAL, "tcpServerInit():\n");

    _tcpListener = new TCPServer(_meshPort);
    _tcpListener->setNoDelay(true);

    _tcpListener->onClient([](void * arg, TCPClient *client) {
        staticThis->debugMsg(CONNECTION, "New AP connection incoming\n");
        auto conn = std::make_shared<MeshConnection>(client, staticThis, false);
        staticThis->_connections.push_back(conn);
    }, NULL);

    _tcpListener->begin();

    debugMsg(STARTUP, "AP tcp server established on port %d\n", _meshPort);
    return;
}
