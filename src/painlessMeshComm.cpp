//
//  painlessMeshComm.cpp
//
//
//  Created by Bill Gray on 7/26/16.
//
//

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SimpleList.h>

#include "painlessMesh.h"

extern painlessMesh* staticThis;

// communications functions
//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendMessage(meshConnectionType *conn, uint32_t destId, uint32_t fromId, meshPackageType type, String &msg) {
    debugMsg(COMMUNICATION, "sendMessage(conn): conn-nodeId=%d destId=%d type=%d msg=%s\n",
             conn->nodeId, destId, (uint8_t)type, msg.c_str());

    String package = buildMeshPackage(destId, fromId, type, msg);

    return sendPackage(conn, package);
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendMessage(uint32_t destId, uint32_t fromId, meshPackageType type, String &msg) {
    debugMsg(COMMUNICATION, "In sendMessage(destId): destId=%d type=%d, msg=%s\n",
             destId, type, msg.c_str());

    meshConnectionType *conn = findConnection(destId);
    if (conn) {
        return sendMessage(conn, destId, fromId, type, msg);
    } else {
        debugMsg(ERROR, "In sendMessage(destId): findConnection( %u ) failed\n", destId);
        return false;
    }
}


//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::broadcastMessage(uint32_t from,
                                                      meshPackageType type,
                                                      String &msg,
                                                      meshConnectionType *exclude) {

    // send a message to every node on the mesh
    bool errCode = false;

    if (exclude != NULL)
        debugMsg(COMMUNICATION, "broadcastMessage(): from=%d type=%d, msg=%s exclude=%d\n",
                 from, type, msg.c_str(), exclude->nodeId);
    else
        debugMsg(COMMUNICATION, "broadcastMessage(): from=%d type=%d, msg=%s exclude=NULL\n",
                 from, type, msg.c_str());

    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    if (_connections.size() > 0)
        errCode = true; // Assume true if at least one connections
    while (connection != _connections.end()) {
        if (connection != exclude) {
            if (!sendMessage(connection, connection->nodeId, from, type, msg))
                errCode = false; // If any error return 0
        }
        connection++;
    }
    return errCode;
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendPackage(meshConnectionType *connection, String &package) {
    debugMsg(COMMUNICATION, "Sending to %d-->%s<--\n", connection->nodeId, package.c_str());

    if (package.length() > 1400) {
        debugMsg(ERROR, "sendPackage(): err package too long length=%d\n", package.length());
        return false;
    }

    if (connection) { // Protect against null pointer
        if (connection->sendReady == true) {
            sint8 errCode = espconn_send(connection->esp_conn, (uint8*)package.c_str(), package.length());
            connection->sendReady = false;

            if (errCode == 0) {
                return true;
            } else {
                debugMsg(ERROR, "sendPackage(): espconn_send Failed err=%d\n", errCode);
                return false;
            }
        } else {
            if (ESP.getFreeHeap() >= MIN_FREE_MEMMORY) { // If memory heap is enugh, queue the message
                connection->sendQueue.push_back(package);
                return true;
            } else {
                connection->sendQueue.clear(); // Discard all messages
                return false;
            }
        }
    } else {
        return false;
    }
}

//***********************************************************************
String ICACHE_FLASH_ATTR painlessMesh::buildMeshPackage(uint32_t destId, uint32_t fromId, meshPackageType type, String &msg) {
    debugMsg(GENERAL, "In buildMeshPackage(): msg=%s\n", msg.c_str());

    DynamicJsonBuffer jsonBuffer(JSON_BUFSIZE);
    JsonObject& root = jsonBuffer.createObject();
    root["dest"] = destId;
    //root["from"] = _nodeId;
    root["from"] = fromId;
    root["type"] = (uint8_t)type;
    root["timestamp"] = staticThis->getNodeTime();

    switch (type) {
    case NODE_SYNC_REQUEST:
    case NODE_SYNC_REPLY:
    {
        JsonArray& subs = jsonBuffer.parseArray(msg);
        if (!subs.success()) {
            debugMsg(GENERAL, "buildMeshPackage(): subs = jsonBuffer.parseArray( msg ) failed!");
        }
        root["subs"] = subs;
        break;
    }
    case TIME_SYNC:
        root["msg"] = jsonBuffer.parseObject(msg);
        break;
    default:
        root["msg"] = msg;
    }

    String ret;
    root.printTo(ret);
    return ret;
}
