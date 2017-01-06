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

static void(*receivedCallback)(uint32_t from, String &msg);
static void(*newConnectionCallback)(bool adopt);

extern painlessMesh* staticThis;

// connection managment functions
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::setReceiveCallback(void(*onReceive)(uint32_t from, String &msg)) {
    debugMsg(GENERAL, "setReceiveCallback():\n");
    receivedCallback = onReceive;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::setNewConnectionCallback(void(*onNewConnection)(bool adopt)) {
    debugMsg(GENERAL, "setNewConnectionCallback():\n");
    newConnectionCallback = onNewConnection;
}

//***********************************************************************
meshConnectionType* ICACHE_FLASH_ATTR painlessMesh::closeConnection(meshConnectionType *conn) {
    // It seems that more should be done here... perhas send off a packette to
    // make an attempt to tell the other node that we are closing this conneciton?
    debugMsg(CONNECTION, "closeConnection(): conn-nodeId=%d\n", conn->nodeId);
    espconn_disconnect(conn->esp_conn);
    return _connections.erase(conn);
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::manageConnections(void) {
    debugMsg(GENERAL, "manageConnections():\n");

    uint32_t nowNodeTime;
    uint32_t nodeTimeOut = NODE_TIMEOUT;
    uint32_t connLastRecieved;

    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while (connection != _connections.end()) {
        //nowNodeTime = getNodeTime();
        nowNodeTime = system_get_time();
        connLastRecieved = connection->lastReceived;
        
        if (nowNodeTime - connLastRecieved > nodeTimeOut) {
            debugMsg(CONNECTION, "manageConnections(): dropping %d now= %u - last= %u ( %u ) > timeout= %u \n", connection->nodeId, nowNodeTime, connLastRecieved, nowNodeTime - connLastRecieved, nodeTimeOut);
            connection = closeConnection(connection);
            continue;
        }

        if (connection->esp_conn->state == ESPCONN_CLOSE) {
            debugMsg(CONNECTION, "manageConnections(): dropping %d ESPCONN_CLOSE\n", connection->nodeId);
            connection = closeConnection(connection);
            continue;
        }

        switch (connection->nodeSyncStatus) {
        case NEEDED:           // start a nodeSync
            debugMsg(SYNC, "manageConnections(): start nodeSync with %d\n", connection->nodeId);
            startNodeSync(connection);
            //connection->nodeSyncStatus = IN_PROGRESS; // Not needed, already done in startNodeSync()

        case IN_PROGRESS:
            if (system_get_time() - connection->nodeSyncRequest > TIME_RESPONSE_TIMEOUT) {
                // A time sync response did not arrive within maximum time out.
                connection->nodeSyncStatus = COMPLETE;
                debugMsg(ERROR, "manageConnections(): nodeSync response from %d timed out. Status changed to COMPLETE\n", connection->nodeId);
                //} else {
                    //debugMsg(S_TIME | DEBUG, "manageConnections(): timeSync IN_PROGRESS\n", connection->nodeId);
            }
            connection++;
            continue;
        }

        switch (connection->timeSyncStatus) {
        case NEEDED:
            //debugMsg(S_TIME, "manageConnections(): starting timeSync with %d\n", connection->nodeId);
            startTimeSync(connection);

        case IN_PROGRESS:
            if (system_get_time() - connection->timeSyncLastRequested > TIME_RESPONSE_TIMEOUT) {
                // A time sync response did not arrive within maximum time out.
                connection->timeSyncStatus = COMPLETE;
                debugMsg(ERROR, "manageConnections(): timeSync response from %d timed out. Status changed to COMPLETE\n", connection->nodeId);
            //} else {
                //debugMsg(S_TIME | DEBUG, "manageConnections(): timeSync IN_PROGRESS\n", connection->nodeId);
            }
            connection++;
            continue;
        }

        if (connection->newConnection == true) {  // we should only get here once first nodeSync and timeSync are complete
            newConnectionCallback(adoptionCalc(connection));
            connection->newConnection = false;
            debugMsg(SYNC, "connection->newConnection. True --> false\n");
            connection++;
            continue;
        }

        // check to see if we've recieved something lately.  Else, flag for new sync.
        // Stagger AP and STA so that they don't try to start a sync at the same time.
        uint32_t nodeTime = getNodeTime();
        if (connection->nodeSyncRequest == 0) { // nodeSync not in progress
            if ((connection->esp_conn->proto.tcp->local_port == _meshPort  // we are AP
                &&
                nowNodeTime - connLastRecieved > (nodeTimeOut / 2))
                ||
                (connection->esp_conn->proto.tcp->local_port != _meshPort  // we are the STA
                &&
                nowNodeTime - connLastRecieved > (nodeTimeOut * 3 / 4))
                ) {
                connection->nodeSyncStatus = NEEDED;
                debugMsg(SYNC, "manageConnections(): %u nodeSyncStatus changed to NEEDED\n", staticThis->getNodeTime());
            }
        }

        // Start time sync periodically with a random delay 0,65 to 1.35 times TIME_SYNC_INTERVAL
        if ((int32_t)nodeTime - (int32_t)connection->nextTimeSync > 0) {
            if (connection->nextTimeSync != 0) { // Do not resync first time
                debugMsg(SYNC, "manageConnections(): %u Sync needed due to interval timeout\n", nodeTime);
                connection->timeSyncStatus = NEEDED;
            }

            // Add random delay to avoid collisions
            float randomDelay = (float)(random(650, 1350)) / 1000;
            connection->nextTimeSync = getNodeTime() + (TIME_SYNC_INTERVAL*randomDelay);

            //debugMsg(DEBUG, "manageConnections(): Random delay = %f sec\n", (TIME_SYNC_INTERVAL*randomDelay) / 1000000);

        }
        connection++;
    }
}

//***********************************************************************
// Search for a connection to a given nodeID
meshConnectionType* ICACHE_FLASH_ATTR painlessMesh::findConnection(uint32_t nodeId) {
    debugMsg(GENERAL, "In findConnection(nodeId)\n");

    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while (connection != _connections.end()) {

        if (connection->nodeId == nodeId) {  // check direct connections
            debugMsg(GENERAL, "findConnection(nodeId): Found Direct Connection\n");
            return connection;
        }

        String nodeIdStr(nodeId);
        if (connection->subConnections.indexOf(nodeIdStr) != -1) { // check sub-connections
            debugMsg(GENERAL, "findConnection(nodeId): Found Sub Connection\n");
            return connection;
        }

        connection++;
    }
    debugMsg(CONNECTION, "findConnection(%d): did not find connection\n", nodeId);
    return NULL;
}

//***********************************************************************
meshConnectionType* ICACHE_FLASH_ATTR painlessMesh::findConnection(espconn *conn) {
    debugMsg(GENERAL, "In findConnection(esp_conn) conn=0x%x\n", conn);

    int i = 0;

    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while (connection != _connections.end()) {
        if (connection->esp_conn == conn) {
            return connection;
        }
        connection++;
    }

    debugMsg(CONNECTION, "findConnection(espconn): Did not Find\n");
    return NULL;
}

//***********************************************************************
String ICACHE_FLASH_ATTR painlessMesh::subConnectionJson(meshConnectionType *exclude) {
    if (exclude != NULL)
        debugMsg(GENERAL, "subConnectionJson(), exclude=%d\n", exclude->nodeId);

    DynamicJsonBuffer jsonBuffer(JSON_BUFSIZE);
    JsonArray& subArray = jsonBuffer.createArray();
    if (!subArray.success())
        debugMsg(ERROR, "subConnectionJson(): ran out of memory 1");

    SimpleList<meshConnectionType>::iterator sub = _connections.begin();
    while (sub != _connections.end()) {
        if (sub != exclude && sub->nodeId != 0) {  //exclude connection that we are working with & anything too new.
            JsonObject& subObj = jsonBuffer.createObject();
            if (!subObj.success())
                debugMsg(ERROR, "subConnectionJson(): ran out of memory 2");

            subObj["nodeId"] = sub->nodeId;

            if (sub->subConnections.length() != 0) {
                //debugMsg( GENERAL, "subConnectionJson(): sub->subConnections=%s\n", sub->subConnections.c_str() );

                JsonArray& subs = jsonBuffer.parseArray(sub->subConnections);
                if (!subs.success())
                    debugMsg(ERROR, "subConnectionJson(): ran out of memory 3");

                subObj["subs"] = subs;
            }

            if (!subArray.add(subObj))
                debugMsg(ERROR, "subConnectionJson(): ran out of memory 4");
        }
        sub++;
    }

    String ret;
    subArray.printTo(ret);
    debugMsg(GENERAL, "subConnectionJson(): ret=%s\n", ret.c_str());
    return ret;
}

//***********************************************************************
uint16_t ICACHE_FLASH_ATTR painlessMesh::connectionCount(meshConnectionType *exclude) {
    uint16_t count = 0;

    SimpleList<meshConnectionType>::iterator sub = _connections.begin();
    while (sub != _connections.end()) {
        if (sub != exclude) {  //exclude this connection in the calc.
            count += (1 + jsonSubConnCount(sub->subConnections));
        }
        sub++;
    }

    debugMsg(GENERAL, "connectionCount(): count=%d\n", count);
    return count;
}

//***********************************************************************
uint16_t ICACHE_FLASH_ATTR painlessMesh::jsonSubConnCount(String& subConns) {
    debugMsg(GENERAL, "jsonSubConnCount(): subConns=%s\n", subConns.c_str());

    uint16_t count = 0;

    if (subConns.length() < 3)
        return 0;

    DynamicJsonBuffer jsonBuffer(JSON_BUFSIZE);
    JsonArray& subArray = jsonBuffer.parseArray(subConns);

    if (!subArray.success()) {
        debugMsg(ERROR, "subConnCount(): out of memory1\n");
    }

    String str;
    for (uint8_t i = 0; i < subArray.size(); i++) {
        str = subArray.get<String>(i);
        debugMsg(GENERAL, "jsonSubConnCount(): str=%s\n", str.c_str());
        JsonObject& obj = jsonBuffer.parseObject(str);
        if (!obj.success()) {
            debugMsg(ERROR, "subConnCount(): out of memory2\n");
        }

        str = obj.get<String>("subs");
        count += (1 + jsonSubConnCount(str));
    }

    debugMsg(CONNECTION, "jsonSubConnCount(): leaving count=%d\n", count);

    return count;
}

//***********************************************************************
// callback which will be called on successful TCP connection (server or client)
// If we are the station party a node time sync is started

void ICACHE_FLASH_ATTR painlessMesh::meshConnectedCb(void *arg) {
    staticThis->debugMsg(CONNECTION, "meshConnectedCb(): new meshConnection !!!\n");
    meshConnectionType newConn;
    newConn.esp_conn = (espconn *)arg;
    espconn_set_opt(newConn.esp_conn, ESPCONN_NODELAY);  // removes nagle, low latency, but soaks up bandwidth
    //newConn.lastReceived = staticThis->getNodeTime();
    newConn.lastReceived = system_get_time();

    espconn_regist_recvcb(newConn.esp_conn, meshRecvCb); // Register data receive function which will be called back when data are received
    espconn_regist_sentcb(newConn.esp_conn, meshSentCb); // Register data sent function which will be called back when data are successfully sent
    espconn_regist_reconcb(newConn.esp_conn, meshReconCb); // This callback is entered when an error occurs, TCP connection broken
    espconn_regist_disconcb(newConn.esp_conn, meshDisconCb); // Register disconnection function which will be called back under successful TCP disconnection

    staticThis->_connections.push_back(newConn);

    if (newConn.esp_conn->proto.tcp->local_port != staticThis->_meshPort) { // we are the station, start nodeSync
        staticThis->debugMsg(CONNECTION, "meshConnectedCb(): we are STA, start nodeSync\n");
        staticThis->startNodeSync(staticThis->_connections.end() - 1); // Sync with the last connected node
        newConn.timeSyncStatus = NEEDED;
    } else
        staticThis->debugMsg(CONNECTION, "meshConnectedCb(): we are AP\n");

    staticThis->debugMsg(GENERAL, "meshConnectedCb(): leaving\n");
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::meshRecvCb(void *arg, char *data, unsigned short length) {
    meshConnectionType *receiveConn = staticThis->findConnection((espconn *)arg);

    uint32_t receivedAt = staticThis->getNodeTime();

    staticThis->debugMsg(COMMUNICATION, "meshRecvCb(): data=%s fromId=%d\n", data, receiveConn->nodeId);

    if (receiveConn == NULL) {
        staticThis->debugMsg(ERROR, "meshRecvCb(): recieved from unknown connection 0x%x ->%s<-\n", arg, data);
        staticThis->debugMsg(ERROR, "meshRecvCb(): dropping this msg... see if we recover?\n");
        return;
    }


    String somestring(data);      //copy data before json parsing FIXME: can someone explain why this works?

    DynamicJsonBuffer jsonBuffer(JSON_BUFSIZE);
    JsonObject& root = jsonBuffer.parseObject(data);
    if (!root.success()) {   // Test if parsing succeeded.
        staticThis->debugMsg(ERROR, "meshRecvCb(): parseObject() failed. data=%s<--\n", data);
        return;
    }

    staticThis->debugMsg(GENERAL, "meshRecvCb(): Recvd from %d-->%s<--\n", receiveConn->nodeId, data);

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
        if ((uint32_t)root["dest"] == staticThis->getNodeId()) {  // msg for us!
            receivedCallback((uint32_t)root["from"], msg);
        } else {                                                    // pass it along
         //staticThis->sendMessage( (uint32_t)root["dest"], (uint32_t)root["from"], SINGLE, msg );  //this is ineffiecnt
            String tempStr(data);
            staticThis->sendPackage(staticThis->findConnection((uint32_t)root["dest"]), tempStr);
        }
        break;

    case BROADCAST:
        staticThis->broadcastMessage((uint32_t)root["from"], BROADCAST, msg, receiveConn);
        receivedCallback((uint32_t)root["from"], msg);
        break;

    default:
        staticThis->debugMsg(ERROR, "meshRecvCb(): unexpected json, root[\"type\"]=%d", (int)root["type"]);
        return;
    }

    // record that we've gotten a valid package
    //receiveConn->lastReceived = receivedAt;
    receiveConn->lastReceived = system_get_time();
    staticThis->debugMsg(COMMUNICATION, "meshRecvCb(): lastRecieved=%u fromId=%d type=%d\n", receiveConn->lastReceived, receiveConn->nodeId, t_message);
    return;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::meshSentCb(void *arg) {
    staticThis->debugMsg(GENERAL, "meshSentCb():\n");    //data sent successfully
    espconn *conn = (espconn*)arg;
    meshConnectionType *meshConnection = staticThis->findConnection(conn);

    if (meshConnection == NULL) {
        staticThis->debugMsg(ERROR, "meshSentCb(): err did not find meshConnection? Likely it was dropped for some reason\n");
        return;
    }

    if (!meshConnection->sendQueue.empty()) {
        String package = *meshConnection->sendQueue.begin();
        meshConnection->sendQueue.pop_front();
        sint8 errCode = espconn_send(meshConnection->esp_conn, (uint8*)package.c_str(), package.length());
        //connection->sendReady = false;

        if (errCode != 0) {
            staticThis->debugMsg(ERROR, "meshSentCb(): espconn_send Failed err=%d\n", errCode);
        }
    } else {
        meshConnection->sendReady = true;
    }
}
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::meshDisconCb(void *arg) {
    struct espconn *disConn = (espconn *)arg;

    staticThis->debugMsg(CONNECTION, "meshDisconCb(): ");

    //test to see if this connection was on the STATION interface by checking the local port
    if (disConn->proto.tcp->local_port == staticThis->_meshPort) {
        staticThis->debugMsg(CONNECTION, "AP connection.  No new action needed. local_port=%d\n", disConn->proto.tcp->local_port);
    } else {
        staticThis->debugMsg(CONNECTION, "Station Connection! Find new node. local_port=%d\n", disConn->proto.tcp->local_port);
        // should start up automatically when station_status changes to IDLE
        wifi_station_disconnect();
    }

    return;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::meshReconCb(void *arg, sint8 err) {
    staticThis->debugMsg(ERROR, "In meshReconCb(): err=%d\n", err);
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
        staticThis->connectToBestAP(); // Search for APs and connect to the best one
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
