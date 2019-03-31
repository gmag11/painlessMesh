//
//  painlessMeshComm.cpp
//
//
//  Created by Bill Gray on 7/26/16.
//
//
#include "painlessMesh.h"

extern painlessMesh* staticThis;

// communications functions
//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendMessage(std::shared_ptr<MeshConnection> conn, uint32_t destId, uint32_t fromId, meshPackageType type, String &msg, bool priority) {
    debugMsg(COMMUNICATION, "sendMessage(conn): conn-nodeId=%u destId=%u type=%d msg=%s\n", conn->nodeId, destId, (uint8_t)type, msg.c_str());

    String package = buildMeshPackage(destId, fromId, type, msg);

    return conn->addMessage(package, priority);
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendMessage(uint32_t destId, uint32_t fromId, meshPackageType type, String &msg, bool priority) {
    debugMsg(COMMUNICATION, "In sendMessage(destId): destId=%u type=%d, msg=%s\n",
             destId, type, msg.c_str());

    std::shared_ptr<MeshConnection> conn = findConnection(destId);
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
        std::shared_ptr<MeshConnection> exclude) {

    // send a message to every node on the mesh
    bool errCode = false;

    if (exclude != NULL)
        debugMsg(COMMUNICATION, "broadcastMessage(): from=%u type=%d, msg=%s exclude=%u\n", from, type, msg.c_str(), exclude->nodeId);
    else
        debugMsg(COMMUNICATION, "broadcastMessage(): from=%u type=%d, msg=%s exclude=NULL\n", from, type, msg.c_str());

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
String ICACHE_FLASH_ATTR painlessMesh::buildMeshPackage(uint32_t destId, uint32_t fromId, meshPackageType type, String &msg) {
    debugMsg(GENERAL, "In buildMeshPackage(): msg=%s\n", msg.c_str());

#if ARDUINOJSON_VERSION_MAJOR==6
    DynamicJsonDocument jsonBuffer(256);
    JsonObject jsonObj = jsonBuffer.to<JsonObject>();
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& jsonObj = jsonBuffer.createObject();
#endif
    jsonObj["dest"] = destId;
    //jsonObj["from"] = _nodeId;
    jsonObj["from"] = fromId;
    jsonObj["type"] = (uint8_t)type;

    switch (type) {
    case NODE_SYNC_REQUEST:
    case NODE_SYNC_REPLY:
    {
#if ARDUINOJSON_VERSION_MAJOR==6
        jsonObj["subs"] = serialized(msg);
#else
        jsonObj["subs"] = RawJson(msg);
#endif
        if (this->isRoot())
            jsonObj["root"] = true;
        break;
    }
    case TIME_SYNC:
#if ARDUINOJSON_VERSION_MAJOR==6
        jsonObj["msg"] = serialized(msg);
#else
        jsonObj["msg"] = RawJson(msg);
#endif
        break;
    default:
        jsonObj["msg"] = msg;
    }

    String ret;
#if ARDUINOJSON_VERSION_MAJOR==6
    serializeJson(jsonObj, ret);
#else
    jsonObj.printTo(ret);
#endif
    return ret;
}
