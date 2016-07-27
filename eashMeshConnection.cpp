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
#include "user_interface.h"
#include "espconn.h"
}

#include "easyMesh.h"

extern easyMesh* staticThis;

// connection managment functions

//***********************************************************************
meshConnection_t* easyMesh::findConnection( uint32_t chipId ) {
    //    DEBUG_MSG("In findConnection(chipId)\n");
    
    SimpleList<meshConnection_t>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        DEBUG_MSG("findConnection(chipId): connection-subConnections=%s\n", connection->subConnections.c_str());
        
        if ( connection->chipId == chipId ) {  // check direct connections
            DEBUG_MSG("findConnection(chipId): Found Direct Connection\n");
            return connection;
        }
        
        String chipIdStr(chipId);
        if ( connection->subConnections.indexOf(chipIdStr) != -1 ) { // check sub-connections
            DEBUG_MSG("findConnection(chipId): Found Sub Connection\n");
            return connection;
        }
        
        connection++;
    }
    DEBUG_MSG("findConnection(%d): did not find connection\n", chipId );
    return NULL;
}

//***********************************************************************
meshConnection_t* easyMesh::findConnection( espconn *conn ) {
    //    DEBUG_MSG("In findConnection(esp_conn) conn=0x%x\n", conn );
    
    int i=0;
    
    SimpleList<meshConnection_t>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection->esp_conn == conn ) {
            return connection;
        }
        connection++;
    }
    
    DEBUG_MSG("findConnection(espconn) Failed");
    return NULL;
}

//***********************************************************************
void easyMesh::cleanDeadConnections( void ) {
    //DEBUG_MSG("In cleanDeadConnections() size=%d\n", _connections.size() );
    
    //int i=0;
    
    SimpleList<meshConnection_t>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        /*DEBUG_MSG("i=%d esp_conn=0x%x type=%d state=%d\n",
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
    DEBUG_MSG("new meshConnection !!!\n");
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
    //    DEBUG_MSG("In meshRecvCb recvd*-->%s<--*\n", data);
    meshConnection_t *receiveConn = staticThis->findConnection( (espconn *)arg );
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonObject& root = jsonBuffer.parseObject( data );
    if (!root.success()) {   // Test if parsing succeeded.
        DEBUG_MSG("meshRecvCb: parseObject() failed. data=%s<--\n", data);
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
            DEBUG_MSG("meshRecvCb(): unexpected json root[\"type\"]=%d", (int)root["type"]);
    }
    return;
}

//***********************************************************************
void easyMesh::meshSentCb(void *arg) {
    //    DEBUG_MSG("In meshSentCb\r\n");    //data sent successfully
}

//***********************************************************************
void easyMesh::meshDisconCb(void *arg) {
    struct espconn *disConn = (espconn *)arg;
    
    DEBUG_MSG("meshDisconCb: ");
    
    // remove this connection from _connections
    staticThis->cleanDeadConnections();
    
    //test to see if this connection was on the STATION interface by checking the local port
    if ( disConn->proto.tcp->local_port == MESH_PORT ) {
        DEBUG_MSG("AP connection.  No cleanup needed. local_port=%d\n", disConn->proto.tcp->local_port);
    }
    else {
        DEBUG_MSG("Station Connection! Find new node. local_port=%d\n", disConn->proto.tcp->local_port);
        wifi_station_disconnect();
    }
    
    return;
}

//***********************************************************************
void easyMesh::meshReconCb(void *arg, sint8 err) {
    DEBUG_MSG("In meshReconCb err=%d\n", err );
}

//***********************************************************************
void easyMesh::wifiEventCb(System_Event_t *event) {
    switch (event->event) {
        case EVENT_STAMODE_CONNECTED:
            DEBUG_MSG("Event: EVENT_STAMODE_CONNECTED ssid=%s\n", (char*)event->event_info.connected.ssid );
            break;
        case EVENT_STAMODE_DISCONNECTED:
            DEBUG_MSG("Event: EVENT_STAMODE_DISCONNECTED\n");
            staticThis->connectToBestAP();
            break;
        case EVENT_STAMODE_AUTHMODE_CHANGE:
            DEBUG_MSG("Event: EVENT_STAMODE_AUTHMODE_CHANGE\n");
            break;
        case EVENT_STAMODE_GOT_IP:
            DEBUG_MSG("Event: EVENT_STAMODE_GOT_IP\n");
            staticThis->tcpConnect();
            break;
            
        case EVENT_SOFTAPMODE_STACONNECTED:
            DEBUG_MSG("Event: EVENT_SOFTAPMODE_STACONNECTED\n");
            break;
            
        case EVENT_SOFTAPMODE_STADISCONNECTED:
            DEBUG_MSG("Event: EVENT_SOFTAPMODE_STADISCONNECTED\n");
            break;
        case EVENT_STAMODE_DHCP_TIMEOUT:
            DEBUG_MSG("Event: EVENT_STAMODE_DHCP_TIMEOUT\n");
            break;
        case EVENT_SOFTAPMODE_PROBEREQRECVED:
            // DEBUG_MSG("Event: EVENT_SOFTAPMODE_PROBEREQRECVED\n");  // dont need to know about every probe request
            break;
        default:
            DEBUG_MSG("Unexpected WiFi event: %d\n", event->event);
            break;
    }
}

