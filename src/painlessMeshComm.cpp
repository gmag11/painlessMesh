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
bool ICACHE_FLASH_ATTR painlessMesh::sendMessage(std::shared_ptr<meshConnectionType> conn, uint32_t destId, uint32_t fromId, meshPackageType type, String &msg, bool priority) {
    debugMsg(COMMUNICATION, "sendMessage(conn): conn-nodeId=%u destId=%u type=%d msg=%s\n", conn->nodeId, destId, (uint8_t)type, msg.c_str());

    String package = buildMeshPackage(destId, fromId, type, msg);

    return sendPackage(conn, package, priority);
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendMessage(uint32_t destId, uint32_t fromId, meshPackageType type, String &msg, bool priority) {
    debugMsg(COMMUNICATION, "In sendMessage(destId): destId=%u type=%d, msg=%s\n",
             destId, type, msg.c_str());

    std::shared_ptr<meshConnectionType> conn = findConnection(destId);
    if (conn) {
        return sendMessage(conn, destId, fromId, type, msg, priority);
    } else {
        debugMsg(ERROR, "In sendMessage(destId): findConnection( %u ) failed\n", destId);
        return false;
    }
}


//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::broadcastMessage(
        uint32_t from,
        meshPackageType type,
        String &msg,
        std::shared_ptr<meshConnectionType> exclude) {

    // send a message to every node on the mesh
    bool errCode = false;

    if (exclude != NULL)
        debugMsg(COMMUNICATION, "broadcastMessage(): from=%u type=%d, msg=%s exclude=%u\n", from, type, msg.c_str(), exclude->nodeId);
    else
        debugMsg(COMMUNICATION, "broadcastMessage(): from=%u type=%d, msg=%s exclude=NULL\n", from, type, msg.c_str());

    auto connection = _connections.begin();
    if (_connections.size() > 0)
        errCode = true; // Assume true if at least one connections
    for (auto &&connection : _connections) {
        if (!exclude || connection->nodeId != exclude->nodeId) {
            if (!sendMessage(connection, connection->nodeId, from, type, msg))
                errCode = false; // If any error return 0
        }
    }
    return errCode;
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendPackage(std::shared_ptr<meshConnectionType> connection, String &package, bool priority) {
    debugMsg(COMMUNICATION, "Sending to %u-->%s<--\n", connection->nodeId, package.c_str());

    if (package.length() > 1400) {
        debugMsg(ERROR, "sendPackage(): err package too long length=%d\n", package.length());
        return false;
    }

    connection = findConnection(connection->nodeId);

    if (connection) { // Protect against null pointer
        if (connection->sendReady == true) {
            sint8 errCode = espconn_send(connection->esp_conn, (uint8*)package.c_str(), package.length());
            connection->sendReady = false;

            if (errCode == 0) {
                debugMsg(COMMUNICATION, "sendPackage(): Package sent -> %s\n", package.c_str());
                return true;
            } else {
                debugMsg(ERROR, "sendPackage(): espconn_send Failed node=%u, err=%d\n", connection->nodeId, errCode);
                return false;
            }
        } else {
            if (ESP.getFreeHeap() - package.length() >= MIN_FREE_MEMORY) { // If memory heap is enough, queue the message
                if (priority) {
                    connection->sendQueue.push_front(package);
                    debugMsg(COMMUNICATION, "sendPackage(): Package sent to queue beginning -> %d , FreeMem: %d\n", connection->sendQueue.size(), ESP.getFreeHeap());
                } else {
                    if (connection->sendQueue.size() < MAX_MESSAGE_QUEUE) {
                        connection->sendQueue.push_back(package);
                        debugMsg(COMMUNICATION, "sendPackage(): Package sent to queue end -> %d , FreeMem: %d\n", connection->sendQueue.size(), ESP.getFreeHeap());
                    } else {
                        debugMsg(ERROR, "sendPackage(): Message queue full -> %d , FreeMem: %d\n", connection->sendQueue.size(), ESP.getFreeHeap());
                        return false;
                    }

                }
                return true;
            } else {
                //connection->sendQueue.clear(); // Discard all messages if free memory is low
                debugMsg(DEBUG, "sendPackage(): Memory low, message was discarded\n");
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

    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["dest"] = destId;
    //root["from"] = _nodeId;
    root["from"] = fromId;
    root["type"] = (uint8_t)type;

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
