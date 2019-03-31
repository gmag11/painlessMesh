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

extern painlessMesh* staticThis;

void ICACHE_FLASH_ATTR painlessMesh::stationManual(
        String ssid, String password, uint16_t port,
        IPAddress remote_ip) {
    // Set station config
    stationScan.manualIP = remote_ip;

    // Start scan
    stationScan.init(this, ssid, password, port);
    stationScan.manual = true;
}

bool ICACHE_FLASH_ATTR painlessMesh::setHostname(const char * hostname){
#ifdef ESP8266
  return WiFi.hostname(hostname);
#elif defined(ESP32)
  if(strlen(hostname) > 32) {
    return false;
  }
  return WiFi.setHostname(hostname);
#endif // ESP8266
}

IPAddress ICACHE_FLASH_ATTR painlessMesh::getStationIP(){
    return WiFi.localIP();
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::tcpConnect(void) {
    // TODO: move to Connection or StationConnection?
    debugMsg(GENERAL, "tcpConnect():\n");
    if (stationScan.manual && stationScan.port == 0) return; // We have been configured not to connect to the mesh 

    // TODO: We could pass this to tcpConnect instead of loading it here

    if (_station_got_ip && WiFi.localIP()) {
        AsyncClient *pConn = new AsyncClient();

        pConn->onError([](void *, AsyncClient * client, int8_t err) {
            staticThis->debugMsg(CONNECTION, "tcp_err(): tcpStationConnection %d\n", err);
            if (client->connected())
                client->close();
            WiFi.disconnect();
        });

        IPAddress ip = WiFi.gatewayIP();
        if (stationScan.manualIP) {
            ip = stationScan.manualIP;
        }

        pConn->onConnect([](void *, AsyncClient *client) {
            staticThis->debugMsg(CONNECTION, "New STA connection incoming\n");
            auto conn = std::make_shared<MeshConnection>(client, staticThis, true);
            staticThis->_connections.push_back(conn);
        }, NULL); 

        pConn->connect(ip, stationScan.port);
     } else {
        debugMsg(ERROR, "tcpConnect(): err Something un expected in tcpConnect()\n");
    }
}

//***********************************************************************
// Calculate NodeID from a hardware MAC address
uint32_t ICACHE_FLASH_ATTR painlessMesh::encodeNodeId(const uint8_t *hwaddr) {
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

#ifdef ESP32
    WiFi.scanNetworks(true, true);
#elif defined(ESP8266)
    WiFi.scanNetworksAsync([&](int networks) { this->scanComplete(); }, true);
#endif

    task.delay(1000*SCAN_INTERVAL); // Scan should be completed by then and next step called. If not then we restart here.
    return;
}

void ICACHE_FLASH_ATTR StationScan::scanComplete() {
    staticThis->debugMsg(CONNECTION, "scanComplete():-- > scan finished @ %u < --\n", staticThis->getNodeTime());

    aps.clear();
    staticThis->debugMsg(CONNECTION, "scanComplete():-- > Cleared old APs.\n");

    auto num = WiFi.scanComplete();
    if(num == WIFI_SCAN_RUNNING || num == WIFI_SCAN_FAILED) return;

    staticThis->debugMsg(CONNECTION, "scanComplete(): num = %d\n", num);
    
    for (auto i = 0; i < num; ++i) {
        WiFi_AP_Record_t record;
        String  _ssid     = WiFi.SSID(i);
        if(_ssid != ssid) {
            if (_ssid == "" && mesh->_meshHidden) {
                // Hidden mesh
                _ssid = ssid;
            } else {
                continue;
            }
        }

        record.rssi       = WiFi.RSSI(i);
        if(record.rssi == 0) continue;
        uint8_t* _bssid   = WiFi.BSSID(i);


        _ssid.toCharArray((char*)record.ssid, 32);
        memcpy((void *)&record.bssid, (void *)_bssid, sizeof(record.bssid));

        aps.push_back(record);
        staticThis->debugMsg(CONNECTION, "\tfound : %s, %ddBm\n", (char*) record.ssid, (int16_t) record.rssi);
    }

    staticThis->debugMsg(CONNECTION, "\tFound %d nodes\n", aps.size());

    task.yield([this]() {
        // Task filter all unknown
        filterAPs();

        // Next task is to sort by strength
        task.yield([this] {
            aps.sort([](WiFi_AP_Record_t a, WiFi_AP_Record_t b) {
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

void ICACHE_FLASH_ATTR StationScan::requestIP(WiFi_AP_Record_t &ap) {
    mesh->debugMsg(CONNECTION, "connectToAP(): Best AP is %u<---\n", 
            mesh->encodeNodeId(ap.bssid));
    WiFi.begin((char*)ap.ssid, password.c_str(), mesh->_meshChannel, ap.bssid);
    return;
}

void ICACHE_FLASH_ATTR StationScan::connectToAP() {
    // Next task will be to rescan
    task.setCallback([this]() {
        stationScan();
    });

    if (manual) {

        if((WiFi.SSID() == ssid) && mesh->_station_got_ip) {
            mesh->debugMsg(CONNECTION, "connectToAP(): Already connected using manual connection. Disabling scanning.\n");
            task.disable();
            return;
        } else {
            if (mesh->_station_got_ip) {
                mesh->closeConnectionSTA();
                task.enableDelayed(1000 * SCAN_INTERVAL);
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
        if (mesh->_station_got_ip && !(mesh->shouldContainRoot && !mesh->isRooted())) {
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

            int prob = mesh->stability;
            if (!mesh->shouldContainRoot)
                prob /= 2*(1+mesh->approxNoNodes()); // Slower when part of bigger network
            if (!mesh->isRooted() && random(0, 1000) < prob) {
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
