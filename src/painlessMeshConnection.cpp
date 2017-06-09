//
//  painlessMeshConnection.cpp
//
//
//  Created by Bill Gray on 7/26/16.
//
//

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SimpleList.h>

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#include "painlessMesh.h"

extern painlessMesh* staticThis;

meshConnectionType::~meshConnectionType() {
    staticThis->debugMsg(CONNECTION, "~meshConnectionType():\n");
    /*if (esp_conn)
        espconn_disconnect(esp_conn);*/
}

// connection managment functions
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::onReceive(receivedCallback_t  cb) {
    debugMsg(GENERAL, "onReceive():\n");
    receivedCallback = cb;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::onNewConnection(newConnectionCallback_t cb) {
    debugMsg(GENERAL, "onNewConnection():\n");
    newConnectionCallback = cb;
}

void ICACHE_FLASH_ATTR painlessMesh::onDroppedConnection(droppedConnectionCallback_t cb) {
    debugMsg(GENERAL, "onNewConnection():\n");
    droppedConnectionCallback = cb;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::onChangedConnections(changedConnectionsCallback_t cb) {
    debugMsg(GENERAL, "onChangedConnections():\n");
    changedConnectionsCallback = cb;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::onNodeTimeAdjusted(nodeTimeAdjustedCallback_t cb) {
    debugMsg(GENERAL, "onNodeTimeAdjusted():\n");
    nodeTimeAdjustedCallback = cb;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::onNodeDelayReceived(nodeDelayCallback_t cb) {
    debugMsg(GENERAL, "onNodeDelayReceived():\n");
    nodeDelayReceivedCallback = cb;
}

void ICACHE_FLASH_ATTR painlessMesh::closeConnectionIt(ConnectionList &connections,
        ConnectionList::iterator conn) {
    auto connection = (*conn);
    connection->timeSyncTask.setCallback(NULL);
    connection->nodeSyncTask.setCallback(NULL);
    connection->nodeTimeoutTask.setCallback(NULL);
    /*connection->timeSyncTask.disable();
    connection->nodeSyncTask.disable();
    connection->nodeTimeoutTask.disable();*/
    connections.erase(conn);

    auto nodeId = connection->nodeId;

    staticThis->droppedConnectionTask.set(TASK_SECOND, TASK_ONCE, [nodeId]() {
        staticThis->debugMsg(CONNECTION, "closingTask():\n");
        staticThis->debugMsg(CONNECTION, "closingTask(): dropping %u now= %u\n", nodeId, staticThis->getNodeTime());
       if (staticThis->changedConnectionsCallback)
            staticThis->changedConnectionsCallback(); // Connection dropped. Signal user            
       if (staticThis->droppedConnectionCallback)
            staticThis->droppedConnectionCallback(nodeId); // Connection dropped. Signal user            
       for (auto &&connection : staticThis->_connections) {
           if (connection->nodeId != nodeId) { // Exclude current
               connection->nodeSyncTask.forceNextIteration();
           }
       }
       staticThis->stability /= 2;
    });
    staticThis->scheduler.addTask(staticThis->droppedConnectionTask);
    staticThis->droppedConnectionTask.enable();

    if (connection->esp_conn && connection->esp_conn->state != ESPCONN_CLOSE) {
        if (connection->esp_conn->proto.tcp->local_port == staticThis->_meshPort) {
            espconn_regist_disconcb(connection->esp_conn, [](void *arg) {
                staticThis->debugMsg(CONNECTION, "dummy disconcb(): AP connection.  No new action needed.\n");
            });

        } else {
            espconn_regist_disconcb(connection->esp_conn, [](void *arg) {
                staticThis->debugMsg(CONNECTION, "dummy disconcb(): Station Connection! Find new node.\n");
                // should start up automatically when station_status changes to IDLE
                wifi_station_disconnect();
            });
        }
        espconn_disconnect(connection->esp_conn);
    }

    if (connection.use_count()>3) {
        // After closing the use count should be 3 (one copy here, 1 in closeConnection and one in the caller)
        // This should then cause the destructor to be called after those last two users
        // go out of scope
        staticThis->debugMsg(ERROR, "closeConnection(): error, use should be below 3, conn-use=%u\n", connection.use_count());
    }
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::closeConnection(std::shared_ptr<meshConnectionType> conn) {
    // A closed connection should (TODO: double check) result in a call to meshDisconCB, which will send sub sync messages
    debugMsg(CONNECTION, "closeConnection(): conn-nodeId=%u, conn-no=%u, conn-use=%u\n", 
            conn->nodeId, _connections.size(), conn.use_count());

    bool found = false;
    auto connection = _connections.begin();
    while (connection != _connections.end()) {
        if ((*connection) == conn) {
            closeConnectionIt(_connections, connection);
            found = true;
            break;
        }
        ++connection;
    }
    debugMsg(CONNECTION, "closeConnection(): done, conn-no=%u, conn-use=%u\n",
            _connections.size(), conn.use_count());
    return found;
}

bool ICACHE_FLASH_ATTR painlessMesh::closeConnectionSTA()
{
    auto connection = _connections.begin();
    while (connection != _connections.end()) {
        if (connection->get()->esp_conn->proto.tcp->local_port != _meshPort) {
            // We found the STA connection, close it
            closeConnectionIt(_connections, connection);
            return true;
        }
        ++connection;
    }
    return false;
}
 
// Check whether a string contains a numeric substring as a complete number
//
// "a:800" does contain "800", but does not contain "80"
bool ICACHE_FLASH_ATTR  stringContainsNumber(const String &subConnections,
                                             const String & nodeIdStr, int from = 0) {
    auto index = subConnections.indexOf(nodeIdStr, from);
    if (index == -1)
        return false;
    // Check that the preceding and following characters are not a number
    else if (index > 0 &&
             index + nodeIdStr.length() + 1 < subConnections.length() &&
             // Preceding character is not a number
             (subConnections.charAt(index - 1) < '0' ||
             subConnections.charAt(index - 1) > '9') &&
             // Following character is not a number
             (subConnections.charAt(index + nodeIdStr.length() + 1) < '0' ||
             subConnections.charAt(index + nodeIdStr.length() + 1) > '9')
             ) {
        return true;
    } else { // Check whether the nodeid occurs further in the subConnections string
        return stringContainsNumber(subConnections, nodeIdStr,
                                    index + nodeIdStr.length());
    }
    return false;
}

//***********************************************************************
// Search for a connection to a given nodeID
std::shared_ptr<meshConnectionType> ICACHE_FLASH_ATTR painlessMesh::findConnection(uint32_t nodeId) {
    debugMsg(GENERAL, "In findConnection(nodeId)\n");

    for (auto &&connection : _connections) {

        if (connection->nodeId == nodeId) {  // check direct connections
            debugMsg(GENERAL, "findConnection(%u): Found Direct Connection\n", nodeId);
            return connection;
        }

        if (stringContainsNumber(connection->subConnections,
            String(nodeId))) { // check sub-connections
            debugMsg(GENERAL, "findConnection(%u): Found Sub Connection through %u\n", nodeId, connection->nodeId);
            return connection;
        }
    }
    debugMsg(CONNECTION, "findConnection(%u): did not find connection\n", nodeId);
    return NULL;
}

//***********************************************************************
std::shared_ptr<meshConnectionType>  ICACHE_FLASH_ATTR painlessMesh::findConnection(espconn *conn) {
    debugMsg(GENERAL, "In findConnection(esp_conn) conn=0x%x\n", conn);

    for (auto &&connection : _connections) {
        if (connection->esp_conn == conn) {
            return connection;
        }
    }

    debugMsg(CONNECTION, "findConnection(espconn): Did not Find\n");
    return NULL;
}

//***********************************************************************
String ICACHE_FLASH_ATTR painlessMesh::subConnectionJson(std::shared_ptr<meshConnectionType> exclude) {
    if (exclude == NULL)
        return subConnectionJsonHelper(_connections);
    else
        return subConnectionJsonHelper(_connections, exclude->nodeId);
}

//***********************************************************************
String ICACHE_FLASH_ATTR painlessMesh::subConnectionJsonHelper(
        ConnectionList &connections,
        uint32_t exclude) {
    if (exclude != 0)
        debugMsg(GENERAL, "subConnectionJson(), exclude=%u\n", exclude);

    String ret = "[";
    for (auto &&sub : connections) {
        if (sub->esp_conn && sub->esp_conn->state == ESPCONN_CLOSE) {
            debugMsg(ERROR, "subConnectionJsonHelper(): Found closed connection");
            // Close connection and start over
            //closeConnection(sub);
            //return subConnectionJsonHelper(_connections, exclude);
        }
        if (sub->nodeId != exclude && sub->nodeId != 0) {  //exclude connection that we are working with & anything too new.
            if (ret.length() > 1)
                ret += String(",");
            ret += String("{\"nodeId\":") + String(sub->nodeId) +
                String(",\"subs\":") + sub->subConnections + String("}");
        }
    }
    ret += String("]");

    debugMsg(GENERAL, "subConnectionJson(): ret=%s\n", ret.c_str());
    return ret;
}

// Calculating the actual number of connected nodes is fairly expensive,
// this calculates a cheap approximation
size_t ICACHE_FLASH_ATTR painlessMesh::approxNoNodes() {
    auto sc = subConnectionJson();
    return approxNoNodes(sc);
}

size_t ICACHE_FLASH_ATTR painlessMesh::approxNoNodes(String &subConns) {
    return max((long int) 1,round(subConns.length()/30.0));
}

//***********************************************************************
SimpleList<uint32_t> ICACHE_FLASH_ATTR painlessMesh::getNodeList() {

    SimpleList<uint32_t> nodeList;

    String nodeJson = subConnectionJson();

    uint index = 0;

    while (index < nodeJson.length()) {
        uint comma = 0;
        index = nodeJson.indexOf("\"nodeId\":");
        if (index == -1)
            break;
        comma = nodeJson.indexOf(',', index);
        String temp = nodeJson.substring(index + 9, comma);
        uint32_t id = strtoul(temp.c_str(), NULL, 10);
        nodeList.push_back(id);
        index = comma + 1;
        nodeJson = nodeJson.substring(index);
    }

    return nodeList;

}

//***********************************************************************
// callback which will be called on successful TCP connection (server or client)
// If we are the station party a node time sync is started

void ICACHE_FLASH_ATTR painlessMesh::meshConnectedCb(void *arg) {
    staticThis->debugMsg(CONNECTION, "meshConnectedCb(): new meshConnection !!!\n");
    auto conn = std::make_shared<meshConnectionType>();
    staticThis->_connections.push_back(conn);
    //auto conn = staticThis->_connections.end()-1;
    conn->esp_conn = (espconn *)arg;
    espconn_set_opt(conn->esp_conn, ESPCONN_NODELAY | ESPCONN_KEEPALIVE);  // removes nagle, low latency, but soaks up bandwidth
    espconn_regist_recvcb(conn->esp_conn, meshRecvCb); // Register data receive function which will be called back when data are received
    espconn_regist_sentcb(conn->esp_conn, meshSentCb); // Register data sent function which will be called back when data are successfully sent
    espconn_regist_reconcb(conn->esp_conn, meshReconCb); // This callback is entered when an error occurs, TCP connection broken
    espconn_regist_disconcb(conn->esp_conn, meshDisconCb); // Register disconnection function which will be called back under successful TCP disconnection

    auto sta = true;
    if (conn->esp_conn->proto.tcp->local_port != staticThis->_meshPort) { // we are the station, start nodeSync
        staticThis->debugMsg(CONNECTION, "meshConnectedCb(): we are STA\n");
    } else {
        sta = false;
        staticThis->debugMsg(CONNECTION, "meshConnectedCb(): we are AP\n");
    }

    conn->nodeTimeoutTask.set(
            NODE_TIMEOUT/1000, TASK_ONCE, [conn](){
        staticThis->debugMsg(CONNECTION, "nodeTimeoutTask():\n");
        staticThis->debugMsg(CONNECTION, "nodeTimeoutTask(): dropping %u now= %u\n", conn->nodeId, staticThis->getNodeTime());

        staticThis->closeConnection(conn);
            });

    staticThis->scheduler.addTask(conn->nodeTimeoutTask);
    conn->nodeTimeoutTask.enableDelayed();

    auto syncInterval = NODE_TIMEOUT/2000;
    if (!sta)
        syncInterval = NODE_TIMEOUT/500;

    conn->nodeSyncTask.set(
            syncInterval, TASK_FOREVER, [conn](){
        staticThis->debugMsg(SYNC, "nodeSyncTask(): \n");
        staticThis->debugMsg(SYNC, "nodeSyncTask(): request with %u\n", 
                conn->nodeId);
        String subs = staticThis->subConnectionJson(conn);
        staticThis->sendMessage(conn, conn->nodeId, 
                staticThis->_nodeId, NODE_SYNC_REQUEST, subs, true);
    });
    staticThis->scheduler.addTask(conn->nodeSyncTask);
    if (sta)
        conn->nodeSyncTask.enable();
    else
        conn->nodeSyncTask.enableDelayed();
    staticThis->debugMsg(GENERAL, "meshConnectedCb(): leaving\n");
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::meshRecvCb(void *arg, char *data, unsigned short length) {
    auto receiveConn = staticThis->findConnection((espconn *)arg);

    uint32_t receivedAt = staticThis->getNodeTime();

    staticThis->debugMsg(COMMUNICATION, "meshRecvCb(): data=%s fromId=%u\n", data, receiveConn ? receiveConn->nodeId : 0);

    if (!receiveConn) {
        staticThis->debugMsg(ERROR, "meshRecvCb(): recieved from unknown connection 0x%x ->%s<-\n", arg, data);
        staticThis->debugMsg(ERROR, "meshRecvCb(): dropping this msg... see if we recover?\n");
        return;
    }


    String somestring(data);      //copy data before json parsing FIXME: can someone explain why this works?

    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(data);
    if (!root.success()) {   // Test if parsing succeeded.
        staticThis->debugMsg(ERROR, "meshRecvCb(): parseObject() failed. data=%s<--\n", data);
        return;
    }

    staticThis->debugMsg(GENERAL, "meshRecvCb(): Recvd from %u-->%s<--\n", receiveConn->nodeId, data);

    String msg = root["msg"];
    meshPackageType t_message = (meshPackageType)(int)root["type"];

    switch (t_message) {
    case NODE_SYNC_REQUEST:
    case NODE_SYNC_REPLY:
        staticThis->handleNodeSync(receiveConn, root);
        break;

    case TIME_SYNC:
        staticThis->handleTimeSync(receiveConn, root, receivedAt);
        break;

    case SINGLE:
    case TIME_DELAY:
        if ((uint32_t)root["dest"] == staticThis->getNodeId()) {  // msg for us!
            if (t_message == TIME_DELAY) {
                staticThis->handleTimeDelay(receiveConn, root, receivedAt);
            } else {
                if (staticThis->receivedCallback)
                    staticThis->receivedCallback((uint32_t)root["from"], msg);
            }
        } else {                                                    // pass it along
            //staticThis->sendMessage( (uint32_t)root["dest"], (uint32_t)root["from"], SINGLE, msg );  //this is ineffiecnt
            String tempStr;
            root.printTo(tempStr);
            std::shared_ptr<meshConnectionType> conn = staticThis->findConnection((uint32_t)root["dest"]);
            if (conn) {
                staticThis->sendPackage(conn, tempStr);
                staticThis->debugMsg(COMMUNICATION, "meshRecvCb(): Message %s to %u forwarded through %u\n", tempStr.c_str(), (uint32_t)root["dest"], conn->nodeId);
            }
        }
        break;

    case BROADCAST:
        staticThis->broadcastMessage((uint32_t)root["from"], BROADCAST, msg, receiveConn);
        if (staticThis->receivedCallback)
            staticThis->receivedCallback((uint32_t)root["from"], msg);
        break;

    default:
        staticThis->debugMsg(ERROR, "meshRecvCb(): unexpected json, root[\"type\"]=%d", (int)root["type"]);
        return;
    }

    // reset timeout 
    receiveConn->nodeTimeoutTask.delay();
    staticThis->debugMsg(COMMUNICATION, "meshRecvCb(): lastRecieved=%u fromId=%d type=%d\n", system_get_time(), receiveConn->nodeId, t_message);
    return;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::meshSentCb(void *arg) {
    staticThis->debugMsg(GENERAL, "meshSentCb():\n");    //data sent successfully
    espconn *conn = (espconn*)arg;
    auto meshConnection = staticThis->findConnection(conn);

    if (!meshConnection) {
        staticThis->debugMsg(ERROR, "meshSentCb(): err did not find meshConnection? Likely it was dropped for some reason\n");
        return;
    }
    
    if (!meshConnection->sendQueue.empty()) {
        for (int i = 0; i < MAX_CONSECUTIVE_SEND; ++i) {
            String package = *meshConnection->sendQueue.begin();

            sint8 errCode = espconn_send(meshConnection->esp_conn, 
                    (uint8*)package.c_str(), package.length());
            if (errCode != 0) {
                staticThis->debugMsg(CONNECTION, "meshSentCb(): espconn_send failed with err=%d Queue size %d. Will retry later\n", errCode, meshConnection->sendQueue.size());
                break;
            }

            meshConnection->sendQueue.pop_front();

            if (meshConnection->sendQueue.empty())
                break;
        }
    } else {
        meshConnection->sendReady = true;
    }
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::meshDisconCb(void *arg) {
    struct espconn *disConn = (espconn *)arg;

    staticThis->debugMsg(CONNECTION, "meshDisconCb(): ");

    auto connection = staticThis->findConnection(disConn);
    if (connection) {
        // Still connected, need to officially disconnect first
        staticThis->closeConnection(connection);

        //test to see if this connection was on the STATION interface by checking the local port
        if (disConn->proto.tcp->local_port == staticThis->_meshPort) {
            staticThis->debugMsg(CONNECTION, "AP connection.  No new action needed. local_port=%d\n", disConn->proto.tcp->local_port);

        } else {
            staticThis->debugMsg(CONNECTION, "Station Connection! Find new node. local_port=%d\n", disConn->proto.tcp->local_port);
            // should start up automatically when station_status changes to IDLE
            //staticThis->stationScan.connectToAP(); // Search for APs and connect to the best one
            wifi_station_disconnect();
        }
    } else {
        // Sometimes we can't find the matching connection by disConn, but
        // turns out that there is then still a closed connection somewhere that we need to close
        // This seems to be an error in the espconn library
        staticThis->debugMsg(ERROR, "meshDisconCb(): Invalid state\n");
        auto connection = staticThis->_connections.begin();
        while (connection != staticThis->_connections.end()) {
            if ((*connection)->esp_conn->state == ESPCONN_CLOSE) {
                staticThis->closeConnectionIt(staticThis->_connections, connection);
                break;
            }
            ++connection;
        }
    }

    return;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::meshReconCb(void *arg, sint8 err) {
    //staticThis->debugMsg(CONNECTION, "meshReconCb(): err=%d.\n", err);
    staticThis->debugMsg(CONNECTION, "meshReconCb(): err=%d. Forwarding to meshDisconCb\n", err);
    struct espconn *disConn = (espconn *)arg;
    auto conn = staticThis->findConnection(disConn);
    if (conn) {
        staticThis->debugMsg(ERROR, "meshReconCb(): err=%d. Closing failed connection\n", err);
        staticThis->closeConnection(conn);
    } else {
        staticThis->debugMsg(ERROR, "meshReconCb(): err=%d. Forwarding to meshDisconCb\n", err);
        meshDisconCb(arg);
    }
}

//***********************************************************************
// Wifi event handler
void ICACHE_FLASH_ATTR painlessMesh::wifiEventCb(System_Event_t *event) {
    switch (event->event) {
    case EVENT_STAMODE_CONNECTED:
        staticThis->debugMsg(CONNECTION, "wifiEventCb(): EVENT_STAMODE_CONNECTED ssid=%s\n", (char*)event->event_info.connected.ssid);
        break;
    case EVENT_STAMODE_DISCONNECTED:
        staticThis->debugMsg(CONNECTION, "wifiEventCb(): EVENT_STAMODE_DISCONNECTED\n");
        //staticThis->closeConnectionSTA();
        staticThis->stationScan.connectToAP(); // Search for APs and connect to the best one
        //wifi_station_disconnect(); // Make sure we are disconnected
        break;
    case EVENT_STAMODE_AUTHMODE_CHANGE:
        staticThis->debugMsg(CONNECTION, "wifiEventCb(): EVENT_STAMODE_AUTHMODE_CHANGE\n");
        break;
    case EVENT_STAMODE_GOT_IP:
        staticThis->debugMsg(CONNECTION, "wifiEventCb(): EVENT_STAMODE_GOT_IP\n");
        staticThis->tcpConnect(); // Connect to TCP port
        break;

    case EVENT_SOFTAPMODE_STACONNECTED:
        staticThis->debugMsg(CONNECTION, "wifiEventCb(): EVENT_SOFTAPMODE_STACONNECTED\n");
        break;

    case EVENT_SOFTAPMODE_STADISCONNECTED:
        staticThis->debugMsg(CONNECTION, "wifiEventCb(): EVENT_SOFTAPMODE_STADISCONNECTED\n");
        break;
    case EVENT_STAMODE_DHCP_TIMEOUT:
        staticThis->debugMsg(CONNECTION, "wifiEventCb(): EVENT_STAMODE_DHCP_TIMEOUT\n");
        break;
    case EVENT_SOFTAPMODE_PROBEREQRECVED:
        // debugMsg( GENERAL, "Event: EVENT_SOFTAPMODE_PROBEREQRECVED\n");  // dont need to know about every probe request
        break;
    default:
        staticThis->debugMsg(ERROR, "Unexpected WiFi event: %d\n", event->event);
        break;
    }
}
