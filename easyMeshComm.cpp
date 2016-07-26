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

extern "C" {
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"
#include "uart.h"
    
#include "c_types.h"
#include "espconn.h"
#include "mem.h"
}

#include "easyMesh.h"

extern easyMesh* staticThis;

// communications functions
//***********************************************************************
bool easyMesh::sendMessage( uint32_t finalDestId, meshPackageType type, String &msg ) {
    //  Serial.printf("In sendMessage()\n");
    
    String package = buildMeshPackage( finalDestId, finalDestId, type, msg );
    
    return sendPackage( findConnection( finalDestId ), package );
}

//***********************************************************************
bool easyMesh::sendMessage( uint32_t finalDestId, meshPackageType type, const char *msg ) {
    String strMsg(msg);
    return sendMessage( finalDestId, type, strMsg );
}

//***********************************************************************
bool easyMesh::broadcastMessage( meshPackageType type, const char *msg ) {
    String strMsg(msg);
    
    SimpleList<meshConnection_t>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        sendMessage( connection->chipId, type, strMsg );
        connection++;
    }
    return true;
}

//***********************************************************************
bool easyMesh::sendPackage( meshConnection_t *connection, String &package ) {
    //   Serial.printf("Sending package-->%s<--\n", package.c_str() );
    
    sint8 errCode = espconn_send( connection->esp_conn, (uint8*)package.c_str(), package.length() );
    
    if ( errCode == 0 ) {
        // Serial.printf("espconn_send Suceeded\n");
        return true;
    }
    else {
        Serial.printf("espconn_send Failed err=%d\n", errCode );
        return false;
    }
}

//***********************************************************************
String easyMesh::buildMeshPackage( uint32_t localDestId, uint32_t finalDestId, meshPackageType type, String &msg ) {
    //    Serial.printf("In buildMeshPackage()\n");
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonObject& root = jsonBuffer.createObject();
    root["from"] = _chipId;
    root["localDest"] = localDestId;
    root["finalDest"] = finalDestId;
    root["type"] = (uint8_t)type;
    
    switch( type ) {
        case MESH_SYNC_REQUEST:
        case MESH_SYNC_REPLY:
        {
            Serial.printf("In buildMeshPackage(): msg=%s\n", msg.c_str() );
            JsonArray& subs = jsonBuffer.parseArray( msg );
            if ( !subs.success() ) {
                Serial.printf("buildMeshPackage(): subs = jsonBuffer.parseArray( msg ) failed!");
            }
            root["subs"] = subs;
            break;
        }
        case TIME_SYNC:
            root["timeStamp"] = jsonBuffer.parseObject( msg );
            break;
            
        case CONTROL:
            root["control"] = jsonBuffer.parseObject( msg );
            break;
            
        default:
            root["msg"] = msg;
    }
    
    String ret;
    root.printTo( ret );
    return ret;
}
