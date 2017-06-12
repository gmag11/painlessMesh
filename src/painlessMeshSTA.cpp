//
//  painlessMeshSTA.cpp
//  
//
//  Created by Bill Gray on 7/26/16.
//
//

#include <Arduino.h>
#include <SimpleList.h>
#include <algorithm>
#include <memory>

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#include "painlessMeshSTA.h"
#include "painlessMesh.h"

extern painlessMesh* staticThis;

void ICACHE_FLASH_ATTR painlessMesh::stationManual(
        String ssid, String password, uint16_t port,
        uint8_t *remote_ip) {
    // Set station config
    memcpy(stationScan.manualIP, remote_ip, 4 * sizeof(uint8_t));

    // Start scan
    stationScan.init(this, ssid, password, port);
    stationScan.manual = true;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::tcpConnect(void) {
    // TODO: move to Connection or StationConnection? 
    debugMsg(GENERAL, "tcpConnect():\n");

    struct ip_info ipconfig;
    wifi_get_ip_info(STATION_IF, &ipconfig);

    if (wifi_station_get_connect_status() == STATION_GOT_IP && 
            ipconfig.ip.addr != 0) {
        // we have successfully connected to wifi as a station.
        debugMsg(CONNECTION, "tcpConnect(): Got local IP=%d.%d.%d.%d\n", IP2STR(&ipconfig.ip));
        debugMsg(CONNECTION, "tcpConnect(): Dest IP=%d.%d.%d.%d\n", IP2STR(&ipconfig.gw));

        // establish tcp connection
        _stationConn.type = ESPCONN_TCP; // TCP Connection
        _stationConn.state = ESPCONN_NONE;
        _stationConn.proto.tcp = &_stationTcp;
        _stationConn.proto.tcp->local_port = espconn_port(); // Get an available port
        _stationConn.proto.tcp->remote_port = stationScan.port; // Global mesh port
        os_memcpy(_stationConn.proto.tcp->local_ip, &ipconfig.ip, 4);
        if (stationScan.manualIP[0] == 0)
            os_memcpy(_stationConn.proto.tcp->remote_ip, &ipconfig.gw, 4);
        else {
            debugMsg(CONNECTION, "tcpConnect(): using manual IP\n");
            os_memcpy(_stationConn.proto.tcp->remote_ip, &stationScan.manualIP, 4);
        }
        espconn_set_opt(&_stationConn, ESPCONN_NODELAY | ESPCONN_KEEPALIVE); // low latency, but soaks up bandwidth

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

void StationScan::init(painlessMesh *pMesh, String &pssid, 
        String &ppassword, uint16_t pPort) {
    ssid = pssid; 
    password = ppassword; 
    mesh = pMesh;
    port = pPort;

        task.set(SCAN_INTERVAL, TASK_FOREVER, [this](){
                stationScan();
                });
    }

// Starts scan for APs whose name is Mesh SSID 
void ICACHE_FLASH_ATTR StationScan::stationScan() {
    staticThis->debugMsg(CONNECTION, "stationScan(): %s\n", ssid.c_str());

    char tempssid[32];
    struct scan_config scanConfig;
    memset(&scanConfig, 0, sizeof(scanConfig));
    ssid.toCharArray(tempssid, ssid.length() + 1);

    scanConfig.ssid = (uint8_t *) tempssid; // limit scan to mesh ssid
    scanConfig.bssid = 0;
    scanConfig.channel = mesh->_meshChannel; // also limit scan to mesh channel to speed things up ...
    scanConfig.show_hidden = 1; // add hidden APs ... why not? we might want to hide ...

    task.delay(1000*SCAN_INTERVAL); // Scan should be completed by them and next step called. If not then we restart here.

    if (!wifi_station_scan(&scanConfig, 
                [](void *arg, STATUS status){
                    bss_info *bssInfo = (bss_info *) arg;
                    staticThis->stationScan.scanComplete(bssInfo);
                })) {
        staticThis->debugMsg(ERROR, "wifi_station_scan() failed!?\n");
        return;
    }
    return;
}

void ICACHE_FLASH_ATTR StationScan::scanComplete(bss_info *bssInfo) {
    staticThis->debugMsg(CONNECTION, "scanComplete():-- > scan finished @ %u < --\n", staticThis->getNodeTime());
    aps.clear();

    while (bssInfo != NULL) {
        staticThis->debugMsg(CONNECTION, "\tfound : % s, % ddBm", (char*) bssInfo->ssid, (int16_t) bssInfo->rssi);
        staticThis->debugMsg(CONNECTION, " MESH< ---");
        aps.push_back(*bssInfo);
        staticThis->debugMsg(CONNECTION, "\n");
#ifdef STAILQ_NEXT
        bssInfo = STAILQ_NEXT(bssInfo, next);
#else
        bssInfo->next;
#endif
    }
    staticThis->debugMsg(CONNECTION, "\tFound % d nodes\n", aps.size());

    task.yield([this]() { 
        // Task filter all unknown
        filterAPs();

        // Next task is to sort by strength
        task.yield([this] {
            std::sort(aps.begin(), aps.end(),
                    [](bss_info a, bss_info b) {
                    return a.rssi > b.rssi;
            });
            // Next task is to connect to the top ap
            task.yield([this]() { 
                connectToAP();
            });
        });
    });
}

void ICACHE_FLASH_ATTR StationScan::filterAPs() {
    auto ap = aps.begin();
    while (ap != aps.end()) {
        auto apNodeId = staticThis->encodeNodeId(ap->bssid);
        if (staticThis->findConnection(apNodeId) != NULL) {
            ap = aps.erase(ap);
            //                debugMsg( GENERAL, "<--already connected\n");
        } else {
            ap++;
            //              debugMsg( GENERAL, "\n");
        }
    }
}

void ICACHE_FLASH_ATTR StationScan::requestIP(bss_info* ap) {
    mesh->debugMsg(CONNECTION, "connectToAP(): Best AP is %u<---\n", 
            mesh->encodeNodeId(ap->bssid));
    struct station_config stationConf;
    stationConf.bssid_set = 1;
    memcpy(&stationConf.bssid, ap->bssid, 6); // Connect to this specific HW Address
    memcpy(&stationConf.ssid, ap->ssid, 32);
    memcpy(&stationConf.password, password.c_str(), 64);
    wifi_station_set_config(&stationConf);
    wifi_station_connect();
}

void ICACHE_FLASH_ATTR StationScan::connectToAP() {
    mesh->debugMsg(CONNECTION, "connectToAP():");
    // Next task will be to rescan
    task.setCallback([this]() {
        stationScan();
    });

    auto statusCode = wifi_station_get_connect_status();
    if (manual) {
        station_config stationConf;
        if (!wifi_station_get_config(&stationConf)) {
            mesh->debugMsg(CONNECTION, "connectToAP(): failed to get current station config. Retrying later\n");
            task.delay(SCAN_INTERVAL);
            return;
        }
        
        if (ssid.equals((char *) stationConf.ssid) && 
                statusCode == STATION_GOT_IP) {
            mesh->debugMsg(CONNECTION, "connectToAP(): Already connected using manual connection. Disabling scanning.\n");
            task.disable();
            return;
        } else {
            if (statusCode == STATION_GOT_IP) {
                mesh->closeConnectionSTA();
                task.enableDelayed(1000*SCAN_INTERVAL);
                return;
            } else if (aps.size() == 0 || 
                    !ssid.equals((char *)aps.begin()->ssid)) {
                task.enableDelayed(SCAN_INTERVAL);
                return;
            }
        }
    }

    if (aps.size() == 0) {
        // No unknown nodes found
        if (statusCode == STATION_GOT_IP) {
            // if already connected -> scan slow
            mesh->debugMsg(CONNECTION, "connectToAP(): Already connected, and no unknown nodes found: scan rate set to slow\n", statusCode);
            task.delay(random(25,36)*SCAN_INTERVAL);
        } else {
            // else scan fast (SCAN_INTERVAL)
            mesh->debugMsg(CONNECTION, "connectToAP(): No unknown nodes found scan rate set to normal\n", statusCode);
            task.delay(SCAN_INTERVAL); 
        }
        mesh->stability += min(1000-mesh->stability,(size_t)25);
    } else {
        if (statusCode == STATION_GOT_IP) {
            mesh->debugMsg(CONNECTION, "connectToAP(): Unknown nodes found. Current stability: %s\n", String(mesh->stability).c_str());
            auto prob = mesh->stability/mesh->approxNoNodes();
            if (random(0, 1000) < prob) {
                mesh->debugMsg(CONNECTION, "connectToAP(): Reconfigure network: %s\n", String(prob).c_str());
                // close STA connection, this will trigger station disconnect which will trigger 
                // connectToAP()
                mesh->closeConnectionSTA();
                mesh->stability = 0; // Discourage switching again
                // wifiEventCB should be triggered before this delay runs out
                // and reset the connecting
                task.delay(1000*SCAN_INTERVAL); 
            } else {
                task.delay(random(4,7)*SCAN_INTERVAL); 
            }
        } else {
            // Else try to connect to first 
            auto ap = aps.begin();
            aps.pop_front();  // drop bestAP from mesh list, so if doesn't work out, we can try the next one
            requestIP(ap);
            // Trying to connect, if that fails we will reconnect later
            mesh->debugMsg(CONNECTION, "connectToAP(): Trying to connect, scan rate set to 4*normal\n", statusCode);
            task.delay(4*SCAN_INTERVAL); 
        }
    }
}
