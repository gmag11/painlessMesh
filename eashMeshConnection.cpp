//
//  eashMeshConnection.cpp
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

// connection managment functions

//***********************************************************************
meshConnection_t* easyMesh::findConnection( uint32_t chipId ) {
    //    Serial.printf("In findConnection(chipId)\n");
    
    SimpleList<meshConnection_t>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection->chipId == chipId )
            return connection;
        connection++;
    }
    Serial.printf("findConnection(%d): did not find connection\n", chipId );
    return NULL;
}

//***********************************************************************
meshConnection_t* easyMesh::findConnection( espconn *conn ) {
    //    Serial.printf("In findConnection(esp_conn) conn=0x%x\n", conn );
    
    int i=0;
    
    SimpleList<meshConnection_t>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection->esp_conn == conn ) {
            return connection;
        }
        connection++;
    }
    
    Serial.printf("findConnection(espconn) Failed");
    return NULL;
}

//***********************************************************************
void easyMesh::cleanDeadConnections( void ) {
    //Serial.printf("In cleanDeadConnections() size=%d\n", _connections.size() );
    
    //int i=0;
    
    SimpleList<meshConnection_t>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        /*Serial.printf("i=%d esp_conn=0x%x type=%d state=%d\n",
         i,
         connection->esp_conn,
         connection->esp_conn->type,
         connection->esp_conn->state);
         */
        if ( connection->esp_conn->state == ESPCONN_CLOSE ) {
            connection = _connections.erase( connection );
        } else {
            connection++;
        }
        
        //  i++;
    }
    
    if (_connections.empty())
        _nodeStatus = SEARCHING;
    
    return;
}

//***********************************************************************
void easyMesh::meshConnectedCb(void *arg) {
    Serial.printf("new meshConnection !!!\n");
    meshConnection_t newConn;
    newConn.esp_conn = (espconn *)arg;
    staticThis->_connections.push_back( newConn );
    
    espconn_regist_recvcb(newConn.esp_conn, meshRecvCb);
    espconn_regist_sentcb(newConn.esp_conn, meshSentCb);
    espconn_regist_reconcb(newConn.esp_conn, meshReconCb);
    espconn_regist_disconcb(newConn.esp_conn, meshDisconCb);
    
    if( newConn.esp_conn->proto.tcp->local_port != MESH_PORT ) { // we are the station, send station handshake
        //   String package = staticThis->buildMeshPackage( 0, 0, HANDSHAKE, "Station Handshake Msg" );
        //   staticThis->sendPackage( &newConn, package );
        staticThis->sendMessage( 0, HANDSHAKE, "Station Handshake");
    }
}

//***********************************************************************
void easyMesh::meshRecvCb(void *arg, char *data, unsigned short length) {
    //    Serial.printf("In meshRecvCb recvd*-->%s<--*\n", data);
    meshConnection_t *receiveConn = staticThis->findConnection( (espconn *)arg );
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonObject& root = jsonBuffer.parseObject( data );
    if (!root.success()) {   // Test if parsing succeeded.
        Serial.printf("meshRecvCb: parseObject() failed. data=%s<--\n", data);
        return;
    }
    
    switch( (meshPackageType)(int)root["type"] ) {
        case HANDSHAKE:
            staticThis->handleHandShake( receiveConn, root );
            break;
        case MESH_SYNC_REQUEST:
        case MESH_SYNC_REPLY:
            staticThis->handleMeshSync( receiveConn, root );
            break;
        case TIME_SYNC:
            staticThis->handleTimeSync( receiveConn, root );
            break;
        case CONTROL:
            staticThis->handleControl( receiveConn, root );
            break;
        default:
            Serial.printf("meshRecvCb(): unexpected json root[\"type\"]=%d", (int)root["type"]);
    }
    return;
}

//***********************************************************************
void easyMesh::meshSentCb(void *arg) {
    //    Serial.printf("In meshSentCb\r\n");    //data sent successfully
}

//***********************************************************************
void easyMesh::meshDisconCb(void *arg) {
    struct espconn *disConn = (espconn *)arg;
    
    Serial.printf("meshDisconCb: ");
    
    // remove this connection from _connections
    staticThis->cleanDeadConnections();
    
    //test to see if this connection was on the STATION interface by checking the local port
    if ( disConn->proto.tcp->local_port == MESH_PORT ) {
        Serial.printf("AP connection.  No cleanup needed. local_port=%d\n", disConn->proto.tcp->local_port);
    }
    else {
        Serial.printf("Station Connection! Find new node. local_port=%d\n", disConn->proto.tcp->local_port);
        wifi_station_disconnect();
    }
    
    return;
}

//***********************************************************************
void easyMesh::meshReconCb(void *arg, sint8 err) {
    Serial.printf("In meshReconCb err=%d\n", err );
}

//***********************************************************************
void easyMesh::wifiEventCb(System_Event_t *event) {
    switch (event->event) {
        case EVENT_STAMODE_CONNECTED:
            Serial.printf("Event: EVENT_STAMODE_CONNECTED ssid=%s\n", (char*)event->event_info.connected.ssid );
            break;
        case EVENT_STAMODE_DISCONNECTED:
            Serial.printf("Event: EVENT_STAMODE_DISCONNECTED\n");
            staticThis->connectToBestAP();
            break;
        case EVENT_STAMODE_AUTHMODE_CHANGE:
            Serial.printf("Event: EVENT_STAMODE_AUTHMODE_CHANGE\n");
            break;
        case EVENT_STAMODE_GOT_IP:
            Serial.printf("Event: EVENT_STAMODE_GOT_IP\n");
            staticThis->tcpConnect();
            break;
            
        case EVENT_SOFTAPMODE_STACONNECTED:
            Serial.printf("Event: EVENT_SOFTAPMODE_STACONNECTED\n");
            break;
            
        case EVENT_SOFTAPMODE_STADISCONNECTED:
            Serial.printf("Event: EVENT_SOFTAPMODE_STADISCONNECTED\n");
            break;
        case EVENT_STAMODE_DHCP_TIMEOUT:
            Serial.printf("Event: EVENT_STAMODE_DHCP_TIMEOUT\n");
            break;
        case EVENT_SOFTAPMODE_PROBEREQRECVED:
            // Serial.printf("Event: EVENT_SOFTAPMODE_PROBEREQRECVED\n");  // dont need to know about every probe request
            break;
        default:
            Serial.printf("Unexpected WiFi event: %d\n", event->event);
            break;
    }
}

