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
bool easyMesh::sendMessage( meshConnectionType *conn, uint32_t destId, meshPackageType type, String &msg ) {
    meshPrintDebug("In sendMessage(conn)\n");
    
    String package = buildMeshPackage( destId, type, msg );
    
    return sendPackage( conn, package );
}

//***********************************************************************
bool easyMesh::sendMessage( uint32_t destId, meshPackageType type, String &msg ) {
    meshPrintDebug("In sendMessage(destId, fromId)\n");
 
    meshConnectionType *conn = findConnection( destId );
    if ( conn != NULL ) {
        return sendMessage( conn, destId, type, msg );
    }
    else {
        meshPrintDebug("In sendMessage(destId, fromId): findConnection( destId ) failed\n");
        return false;
    }
}


//***********************************************************************
bool easyMesh::broadcastMessage(uint32_t from,
                                meshPackageType type,
                                String &msg,
                                meshConnectionType *exclude ) {
    
    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection != exclude ) {
            sendMessage( connection, connection->chipId, type, msg );
        }
        connection++;
    }
    return true;
}

//***********************************************************************
bool easyMesh::sendPackage( meshConnectionType *connection, String &package ) {
    //meshPrintDebug("Sending to %d-->%s<--\n", connection->chipId, package.c_str() );
    
    if ( package.length() > 1400 )
        meshPrintDebug("sendPackage(): err package too long length=%d\n", package.length());
    
    if ( connection->sendReady == true ) {
        sint8 errCode = espconn_send( connection->esp_conn, (uint8*)package.c_str(), package.length() );
        connection->sendReady = false;
        
        if ( errCode == 0 ) {
            //meshPrintDebug("sendPackage(): espconn_send Suceeded\n");
            return true;
        }
        else {
            meshPrintDebug("sendPackage(): espconn_send Failed err=%d\n", errCode );
            return false;
        }
    }
    else {
        connection->sendQueue.push_back( package );
    }
}

//***********************************************************************
String easyMesh::buildMeshPackage( uint32_t destId, meshPackageType type, String &msg ) {
    meshPrintDebug("In buildMeshPackage(): msg=%s\n", msg.c_str() );
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonObject& root = jsonBuffer.createObject();
    root["dest"] = destId;
    root["from"] = _chipId;
    root["type"] = (uint8_t)type;
    
    switch( type ) {
        case NODE_SYNC_REQUEST:
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
            
/*        case CONTROL:
            root["msg"] = jsonBuffer.parseObject( msg );
            break;
  */
        default:
            root["msg"] = msg;
    }
    
    String ret;
    root.printTo( ret );
    return ret;
}
