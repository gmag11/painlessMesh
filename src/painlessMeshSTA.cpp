//
//  painlessMeshSTA.cpp
//
//
//  Created by Bill Gray on 7/26/16.
//
//

#include <Arduino.h>
#include <algorithm>
#include <memory>

#include "painlessMeshSTA.h"
#include "painlessMesh.h"

#include "lwip/ip_addr.h"

extern painlessMesh* staticThis;

void ICACHE_FLASH_ATTR painlessMesh::stationManual(
        String ssid, String password, uint16_t port,
        uint8_t *remote_ip) {
    // Set station config
    if (remote_ip != NULL) memcpy(stationScan.manualIP, remote_ip, 4 * sizeof(uint8_t));

    // Start scan
    stationScan.init(this, ssid, password, port);
    stationScan.manual = true;
}

bool ICACHE_FLASH_ATTR painlessMesh::setHostname(const char * hostname){
  if(strlen(hostname) > 32) {
    return false;
  }
  return (tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname) == ESP_OK);
}

ip4_addr_t ICACHE_FLASH_ATTR painlessMesh::getStationIP(){
    tcpip_adapter_ip_info_t ipconfig;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipconfig);
    return ipconfig.ip;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::tcpConnect(void) {
    // TODO: move to Connection or StationConnection?
    debugMsg(GENERAL, "tcpConnect():\n");
    if (stationScan.manual && stationScan.port == 0) return; // We have been configured not to connect to the mesh 

    // TODO: We could pass this to tcpConnect instead of loading it here
    tcpip_adapter_ip_info_t ipconfig;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipconfig);

    if (_station_got_ip && ipconfig.ip.addr != 0) {
        TCPClient *pConn = new TCPClient();

        pConn->onError([](void *, TCPClient * client, int8_t err) {
            staticThis->debugMsg(CONNECTION, "tcp_err(): tcpStationConnection %d\n", err);
            if (client->connected())
                client->close();
            esp_wifi_disconnect();
        });

        auto ip = ipconfig.gw;
        if (stationScan.manualIP[0] != 0)
            //ip = stationScan.manualIP;
            memcpy(&ip, &stationScan.manualIP, 4);

        pConn->onConnect([](void *, TCPClient *client) {
                    staticThis->debugMsg(CONNECTION, "New STA connection incoming\n");
                    auto conn = std::make_shared<MeshConnection>(client, staticThis, true);
                    staticThis->_connections.push_back(conn);
        }, NULL); 

        pConn->connect(IPAddress(ip.addr), stationScan.port);
     } else {
        debugMsg(ERROR, "tcpConnect(): err Something un expected in tcpConnect()\n");
    }
}

//***********************************************************************
// Calculate NodeID from a hardware MAC address
uint32_t ICACHE_FLASH_ATTR painlessMesh::encodeNodeId(uint8_t *hwaddr) {
    debugMsg(GENERAL, "encodeNodeId():\n");
    uint32_t value = 0;

    value |= hwaddr[2] << 24; //Big endian (aka "network order"):
    value |= hwaddr[3] << 16;
    value |= hwaddr[4] << 8;
    value |= hwaddr[5];
    return value;
}

void ICACHE_FLASH_ATTR StationScan::init(painlessMesh *pMesh, String &pssid, 
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
    wifi_scan_config_t scanConfig;
    memset(&scanConfig, 0, sizeof(scanConfig));
    ssid.toCharArray(tempssid, ssid.length() + 1);

    scanConfig.ssid = (uint8_t *) tempssid; // limit scan to mesh ssid
    scanConfig.bssid = 0;
    scanConfig.channel = mesh->_meshChannel; // also limit scan to mesh channel to speed things up ...
    scanConfig.show_hidden = 1; // add hidden APs ... why not? we might want to hide ...

    task.delay(1000*SCAN_INTERVAL); // Scan should be completed by them and next step called. If not then we restart here.

    if (esp_wifi_scan_start(&scanConfig, false) != ESP_OK)
        staticThis->debugMsg(ERROR, "wifi_station_scan() failed!?\n");
}

void ICACHE_FLASH_ATTR StationScan::scanComplete() {
    staticThis->debugMsg(CONNECTION, "scanComplete():-- > scan finished @ %u < --\n", staticThis->getNodeTime());

    aps.clear();
    staticThis->debugMsg(CONNECTION, "scanComplete():-- > Cleared old aps.\n");

    uint16_t num = 0;
    auto err = esp_wifi_scan_get_ap_num(&num);
    if (err != ESP_OK)
        staticThis->debugMsg(CONNECTION, "scanComplete():-- > Error in scanning.\n");
    //wifi_ap_record_t *records = new wifi_ap_record_t[num];
    wifi_ap_record_t *records = (wifi_ap_record_t*)malloc(num*sizeof(wifi_ap_record_t));
    //records = (wifi_ap_record_t *)malloc(num*sizeof(wifi_ap_record_t));
    staticThis->debugMsg(CONNECTION, "scanComplete(): num=%d, err=%d\n", num, err);
    err = esp_wifi_scan_get_ap_records(&num, records);
    staticThis->debugMsg(CONNECTION, "scanComplete(): After getting records, num=%d, err=%d\n", num, err);
    for (uint16_t i = 0; i < num; ++i) {
        aps.push_back(records[i]);
        staticThis->debugMsg(CONNECTION, "\tfound : % s, % ddBm\n", (char*) records[i].ssid, (int16_t) records[i].rssi);
    }
    //delete[] records;
    free(records);
    staticThis->debugMsg(CONNECTION, "\tFound % d nodes\n", aps.size());

    task.yield([this]() {
        // Task filter all unknown
        filterAPs();

        // Next task is to sort by strength
        task.yield([this] {
            aps.sort([](wifi_ap_record_t a, wifi_ap_record_t b) {
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

void ICACHE_FLASH_ATTR StationScan::requestIP(wifi_ap_record_t &ap) {
    mesh->debugMsg(CONNECTION, "connectToAP(): Best AP is %u<---\n", 
            mesh->encodeNodeId(ap.bssid));
    wifi_sta_config_t stationConf;
    stationConf.bssid_set = 1;
    memcpy(&stationConf.bssid, ap.bssid, 6); // Connect to this specific HW Address
    memcpy(&stationConf.ssid, ap.ssid, 32);
    memset(&stationConf.password, 0, 64);
    memcpy(&stationConf.password, password.c_str(), password.length() + 1); // Connect to this specific HW Address
    /*memcpy(&stationConf.bssid, ap.bssid, 6); // Connect to this specific HW Address
    memcpy(&stationConf.ssid, ap.ssid, 32);
    memcpy(&stationConf.password, password.c_str(), 64);*/
    wifi_config_t cfg;
    cfg.sta = stationConf;
    esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg);
    esp_wifi_connect();
}

void ICACHE_FLASH_ATTR StationScan::connectToAP() {
    mesh->debugMsg(CONNECTION, "connectToAP():");
    // Next task will be to rescan
    task.setCallback([this]() {
        stationScan();
    });

    if (manual) {
        wifi_config_t stationConf;
        if (esp_wifi_get_config(ESP_IF_WIFI_STA, &stationConf) != ESP_OK) {
            mesh->debugMsg(CONNECTION, "connectToAP(): failed to get current station config. Retrying later\n");
            task.delay(SCAN_INTERVAL);
            return;
        }
       
        if (ssid.equals((char *) stationConf.sta.ssid) && 
                mesh->_station_got_ip) {
            mesh->debugMsg(CONNECTION, "connectToAP(): Already connected using manual connection. Disabling scanning.\n");
            task.disable();
            return;
        } else {
            if (mesh->_station_got_ip) {
                mesh->closeConnectionSTA();
                task.enableDelayed(1000*SCAN_INTERVAL);
                return;
            } else if (aps.empty() || 
                    !ssid.equals((char *)aps.begin()->ssid)) {
                task.enableDelayed(SCAN_INTERVAL);
                return;
            }
        }
    }

    if (aps.empty()) {
        // No unknown nodes found
        if (mesh->_station_got_ip) {
            // if already connected -> scan slow
            mesh->debugMsg(CONNECTION, "connectToAP(): Already connected, and no unknown nodes found: scan rate set to slow\n");
            task.delay(random(25,36)*SCAN_INTERVAL);
        } else {
            // else scan fast (SCAN_INTERVAL)
            mesh->debugMsg(CONNECTION, "connectToAP(): No unknown nodes found scan rate set to normal\n");
            task.setInterval(SCAN_INTERVAL); 
        }
        mesh->stability += min(1000-mesh->stability,(size_t)25);
    } else {
        if (mesh->_station_got_ip) {
            mesh->debugMsg(CONNECTION, "connectToAP(): Unknown nodes found. Current stability: %s\n", String(mesh->stability).c_str());
            int prob = mesh->stability/mesh->approxNoNodes();
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
            auto ap = aps.front();
            aps.pop_front();  // drop bestAP from mesh list, so if doesn't work out, we can try the next one
            requestIP(ap);
            // Trying to connect, if that fails we will reconnect later
            mesh->debugMsg(CONNECTION, "connectToAP(): Trying to connect, scan rate set to 4*normal\n");
            task.delay(4*SCAN_INTERVAL); 
        }
    }
}
