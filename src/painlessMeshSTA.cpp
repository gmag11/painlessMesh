//
//  painlessMeshSTA.cpp
//  
//
//  Created by Bill Gray on 7/26/16.
//
//

#include <Arduino.h>
#include <SimpleList.h>

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#include "painlessMesh.h"



extern painlessMesh* staticThis;

// Station functions
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::stationInit(void) {
    debugMsg(STARTUP, "stationInit():\n");
    startStationScan();
    return;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::manageStation(void) {
    debugMsg(GENERAL, "manageStation():\n");

    static uint8_t previousStatus;
    uint8_t stationStatus = wifi_station_get_connect_status();

    if (stationStatus != previousStatus) {
        switch (stationStatus) {
        case STATION_IDLE:
            debugMsg(MESH_STATUS, "stationStatus Changed to STATION_IDLE\n");
            break;
        case STATION_CONNECTING:
            debugMsg(MESH_STATUS, "stationStatus Changed to STATION_CONNECTING\n");
            break;

        case STATION_WRONG_PASSWORD:
            debugMsg(MESH_STATUS, "stationStatus Changed to STATION_WRONG_PASSWORD\n");
            break;

        case STATION_NO_AP_FOUND:
            debugMsg(MESH_STATUS, "stationStatus Changed to STATION_NO_AP_FOUND\n");
            break;

        case STATION_CONNECT_FAIL:
            debugMsg(MESH_STATUS, "stationStatus Changed to STATION_CONNECT_FAIL\n");
            break;

        case STATION_GOT_IP:
            debugMsg(MESH_STATUS, "stationStatus Changed to STATION_GOT_IP\n");
            break;

        }
        previousStatus = stationStatus;
    }
}

//***********************************************************************
// Starts scan for APs whose name is Mesh SSID
void ICACHE_FLASH_ATTR painlessMesh::startStationScan(void) {
    debugMsg(GENERAL, "startStationScan():\n");

    if (_scanStatus != IDLE) { // Already scanning
        return;
    }

    char tempssid[32];
    struct scan_config scanConfig;
    memset(&scanConfig, 0, sizeof(scanConfig));
    staticThis->_meshSSID.toCharArray(tempssid, staticThis->_meshSSID.length() + 1);

    scanConfig.ssid = (uint8_t *) tempssid; // limit scan to mesh ssid
    scanConfig.bssid = 0;
    scanConfig.channel = staticThis->_meshChannel; // also limit scan to mesh channel to speed things up ...
    scanConfig.show_hidden = 1; // add hidden APs ... why not? we might want to hide ...

    if (!wifi_station_scan(&scanConfig, stationScanCb)) {
        debugMsg(ERROR, "wifi_station_scan() failed!?\n");
        return;
    }
    _scanStatus = SCANNING;
    debugMsg(CONNECTION, "-->scan started @ %u<--\n", staticThis->getNodeTime());
    return;
}

//***********************************************************************
// Starts AP scan, triggered by a timer every SCAN_INTERVAL ms
void ICACHE_FLASH_ATTR painlessMesh::scanTimerCallback(void *arg) {
    //os_timer_disarm(&staticThis->_scanTimer);
    staticThis->startStationScan();

    // this function can be totally elimiated!
}

//***********************************************************************
// Station scan callback
void ICACHE_FLASH_ATTR painlessMesh::stationScanCb(void *arg, STATUS status) {
    char ssid[32];
    bss_info *bssInfo = (bss_info *) arg;
    staticThis->debugMsg(CONNECTION, "stationScanCb():-- > scan finished @ %u < --\n", staticThis->getNodeTime());
    staticThis->_scanStatus = IDLE;

    staticThis->_meshAPs.clear();
    while (bssInfo != NULL) {
        staticThis->debugMsg(CONNECTION, "\tfound : % s, % ddBm", (char*) bssInfo->ssid, (int16_t) bssInfo->rssi);
        staticThis->debugMsg(CONNECTION, " MESH< ---");
        staticThis->_meshAPs.push_back(*bssInfo);
        staticThis->debugMsg(CONNECTION, "\n");
        bssInfo = STAILQ_NEXT(bssInfo, next);
    }
    staticThis->debugMsg(CONNECTION, "\tFound % d nodes with _meshSSID = \"%s\"\n", staticThis->_meshAPs.size(), staticThis->_meshSSID.c_str());

    staticThis->connectToBestAP();
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::connectToBestAP(void) {
    debugMsg(CONNECTION, "connectToBestAP():");
    uint32_t apNodeId = 0;

    // drop any _meshAP's we are already connected to
    SimpleList<bss_info>::iterator ap = _meshAPs.begin();
    while (ap != _meshAPs.end()) {
        apNodeId = encodeNodeId(ap->bssid);
        // debugMsg( GENERAL, "connectToBestAP: sort - ssid=%s, apNodeId=%d", ap->ssid, apNodeId);

        if (findConnection(apNodeId) != NULL) {
            ap = _meshAPs.erase(ap);
            //                debugMsg( GENERAL, "<--already connected\n");
        } else {
            ap++;
            //              debugMsg( GENERAL, "\n");
        }
    }

    uint8 statusCode = wifi_station_get_connect_status();
    if (statusCode != STATION_IDLE) {
        debugMsg(CONNECTION, "connectToBestAP(): station not idle.  code=%d\n", statusCode);
        return false;
    }

    if (staticThis->_meshAPs.empty()) {  // no meshNodes left in most recent scan
        //      debugMsg( GENERAL, "connectToBestAP(): no nodes left in list\n");
        // wait 5 seconds and rescan;
        debugMsg(CONNECTION, "connectToBestAP(): no nodes left in list, rescanning\n");
        os_timer_setfn(&_scanTimer, scanTimerCallback, NULL);
        os_timer_arm(&_scanTimer, SCAN_INTERVAL, 0);
        return false;
    }

    // if we are here, then we have a list of at least 1 meshAPs.
    // find strongest signal of remaining meshAPs... that is not already connected to our AP.
    _nodeStatus = FOUND_MESH;
    SimpleList<bss_info>::iterator bestAP = staticThis->_meshAPs.begin();
    SimpleList<bss_info>::iterator i = staticThis->_meshAPs.begin();
    while (i != staticThis->_meshAPs.end()) {
        if (i->rssi > bestAP->rssi) {
            bestAP = i;
        }
        ++i;
    }

    // connect to bestAP
    debugMsg(CONNECTION, "connectToBestAP(): Best AP is %d<---\n", encodeNodeId(bestAP->bssid));
    struct station_config stationConf;
    stationConf.bssid_set = 1;
    memcpy(&stationConf.bssid, bestAP->bssid, 6); // Connect to this specific HW Address
    memcpy(&stationConf.ssid, bestAP->ssid, 32);
    memcpy(&stationConf.password, _meshPassword.c_str(), 64);
    wifi_station_set_config(&stationConf);
    wifi_station_connect();

    _meshAPs.erase(bestAP);    // drop bestAP from mesh list, so if doesn't work out, we can try the next one
    return true;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::tcpConnect(void) {
    debugMsg(GENERAL, "tcpConnect():\n");

    struct ip_info ipconfig;
    wifi_get_ip_info(STATION_IF, &ipconfig);

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {
        // we have successfully connected to wifi as a station.
        debugMsg(CONNECTION, "tcpConnect(): Got local IP=%d.%d.%d.%d\n", IP2STR(&ipconfig.ip));
        debugMsg(CONNECTION, "tcpConnect(): Dest IP=%d.%d.%d.%d\n", IP2STR(&ipconfig.gw));

        // establish tcp connection
        _stationConn.type = ESPCONN_TCP; // TCP Connection
        _stationConn.state = ESPCONN_NONE;
        _stationConn.proto.tcp = &_stationTcp;
        _stationConn.proto.tcp->local_port = espconn_port(); // Get an available port
        _stationConn.proto.tcp->remote_port = _meshPort; // Global mesh port
        os_memcpy(_stationConn.proto.tcp->local_ip, &ipconfig.ip, 4);
        os_memcpy(_stationConn.proto.tcp->remote_ip, &ipconfig.gw, 4);
        espconn_set_opt(&_stationConn, ESPCONN_NODELAY); // low latency, but soaks up bandwidth

        debugMsg(CONNECTION, "tcpConnect(): connecting type=%d, state=%d, local_ip=%d.%d.%d.%d, local_port=%d, remote_ip=%d.%d.%d.%d remote_port=%d\n",
                 _stationConn.type,
                 _stationConn.state,
                 IP2STR(_stationConn.proto.tcp->local_ip),
                 _stationConn.proto.tcp->local_port,
                 IP2STR(_stationConn.proto.tcp->remote_ip),
                 _stationConn.proto.tcp->remote_port);

        espconn_regist_connectcb(&_stationConn, meshConnectedCb); // Register a connected callback which will be called on successful TCP connection (server or client)
        espconn_regist_recvcb(&_stationConn, meshRecvCb); // Register data receive function which will be called back when data are received
        espconn_regist_sentcb(&_stationConn, meshSentCb); // Register data sent function which will be called back when data are successfully sent
        espconn_regist_reconcb(&_stationConn, meshReconCb); // This callback is entered when an error occurs, TCP connection broken
        espconn_regist_disconcb(&_stationConn, meshDisconCb); // Register disconnection function which will be called back under successful TCP disconnection

        sint8  errCode = espconn_connect(&_stationConn);
        if (errCode != 0) {
            debugMsg(ERROR, "tcpConnect(): err espconn_connect() falied=%d\n", errCode);
        }
    } else {
        debugMsg(ERROR, "tcpConnect(): err Something un expected in tcpConnect()\n");
    }
}
//***********************************************************************
// Calculate NodeID from a hardware MAC address
uint32_t ICACHE_FLASH_ATTR painlessMesh::encodeNodeId(uint8_t *hwaddr) {
    debugMsg(GENERAL, "encodeNodeId():\n");
    uint32 value = 0;

    value |= hwaddr[2] << 24; //Big endian (aka "network order"):
    value |= hwaddr[3] << 16;
    value |= hwaddr[4] << 8;
    value |= hwaddr[5];
    return value;
}
