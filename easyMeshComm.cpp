//
//  easyMeshComm.cpp
//  
//
//  Created by Bill Gray on 7/26/16.
//
//

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SimpleList.h>

#include "easyMesh.h"

extern easyMesh* staticThis;

// communications functions
//***********************************************************************
bool easyMesh::sendMessage( uint32_t fromId, uint32_t destId, meshPackageType type, String &msg ) {
    //meshPrintDebug("In sendMessage()\n");
    
    String package = buildMeshPackage( fromId, destId, type, msg );
    
    return sendPackage( findConnection( destId ), package );
}

//***********************************************************************
bool easyMesh::sendMessage( uint32_t destId, meshPackageType type, String &msg ) {
    
    return sendMessage( _chipId, destId, type, msg );
}


//***********************************************************************
/*
 bool easyMesh::sendMessage( uint32_t destId, meshPackageType type, const char *msg ) {
    String strMsg(msg);
    return sendMessage( destId, type, strMsg );
}
*/
//***********************************************************************
bool easyMesh::broadcastMessage(uint32_t from,
                                meshPackageType type,
                                String &msg,
                                meshConnectionType *exclude ) {
    
    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection != exclude ) {
            sendMessage( connection->chipId, type, msg );
        }
        connection++;
    }
    return true;
}

//***********************************************************************
bool easyMesh::sendPackage( meshConnectionType *connection, String &package ) {
    //meshPrintDebug("Sending to %d-->%s<--\n", connection->chipId, package.c_str() );
    
    sint8 errCode = espconn_send( connection->esp_conn, (uint8*)package.c_str(), package.length() );
    
    if ( errCode == 0 ) {
        //meshPrintDebug("espconn_send Suceeded\n");
        return true;
    }
    else {
        meshPrintDebug("espconn_send Failed err=%d\n", errCode );
        return false;
    }
}

//***********************************************************************
String easyMesh::buildMeshPackage( uint32_t fromId, uint32_t destId, meshPackageType type, String &msg ) {
    // meshPrintDebug("In buildMeshPackage(): msg=%s\n", msg.c_str() );
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonObject& root = jsonBuffer.createObject();
    root["from"] = fromId;
    root["dest"] = destId;
    root["type"] = (uint8_t)type;
    
    switch( type ) {
        case NODE_SYNC_REQUEST:
            findConnection( destId )->nodeSyncRequest = getNodeTime();
        case NODE_SYNC_REPLY:
        {
            JsonArray& subs = jsonBuffer.parseArray( msg );
            if ( !subs.success() ) {
                meshPrintDebug("buildMeshPackage(): subs = jsonBuffer.parseArray( msg ) failed!");
            }
            root["subs"] = subs;
            break;
        }
        case TIME_SYNC:
            root["msg"] = jsonBuffer.parseObject( msg );
            break;
            
        case CONTROL:
            root["msg"] = jsonBuffer.parseObject( msg );
            break;
            
        default:
            root["msg"] = msg;
    }
    
    String ret;
    root.printTo( ret );
    return ret;
}
