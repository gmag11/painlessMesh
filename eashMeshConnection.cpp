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

static void (*receivedCallback)( uint32_t from, String &msg);

extern easyMesh* staticThis;

// connection managment functions
//***********************************************************************
void easyMesh::setReceiveCallback( void(*onReceive)(uint32_t from, String &msg) ) {
    receivedCallback = onReceive;
}

//***********************************************************************
meshConnectionType* easyMesh::findConnection( uint32_t chipId ) {
    //    meshPrintDebug("In findConnection(chipId)\n");
    
    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        //meshPrintDebug("findConnection(chipId): connection-subConnections=%s\n", connection->subConnections.c_str());
        
        if ( connection->chipId == chipId ) {  // check direct connections
            //meshPrintDebug("findConnection(chipId): Found Direct Connection\n");
            return connection;
        }
        
        String chipIdStr(chipId);
        if ( connection->subConnections.indexOf(chipIdStr) != -1 ) { // check sub-connections
            //meshPrintDebug("findConnection(chipId): Found Sub Connection\n");
            return connection;
        }
        
        connection++;
    }
    //meshPrintDebug("findConnection(%d): did not find connection\n", chipId );
    return NULL;
}

//***********************************************************************
meshConnectionType* easyMesh::findConnection( espconn *conn ) {
    //    meshPrintDebug("In findConnection(esp_conn) conn=0x%x\n", conn );
    
    int i=0;
    
    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection->esp_conn == conn ) {
            return connection;
        }
        connection++;
    }
    
    meshPrintDebug("findConnection(espconn): Did not Find\n");
    return NULL;
}

//***********************************************************************
void easyMesh::cleanDeadConnections( void ) {
    meshPrintDebug("In cleanDeadConnections() size=%d\n", _connections.size() );
    
    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection->esp_conn->state == ESPCONN_CLOSE ) {
            connection = _connections.erase( connection );
        } else {
            connection++;
        }
    }
    
    if (_connections.empty()) {
        meshPrintDebug("cleanDeadConnections(): empty\n");
        _nodeStatus = SEARCHING;
    }
    return;
}

//***********************************************************************
String easyMesh::subConnectionJson( meshConnectionType *exclude ) {
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonArray& subArray = jsonBuffer.createArray();
    if ( !subArray.success() )
        meshPrintDebug("subConnectionJson(): ran out of memory 1");
    
    SimpleList<meshConnectionType>::iterator sub = _connections.begin();
    while ( sub != _connections.end() ) {
        if ( sub != exclude && sub->chipId != 0 ) {  //exclude connection that we are working with & anything too new.
            JsonObject& subObj = jsonBuffer.createObject();
            if ( !subObj.success() )
                meshPrintDebug("subConnectionJson(): ran out of memory 2");
            
            subObj["chipId"] = sub->chipId;
            
            if ( sub->subConnections.length() != 0 ) {
                //meshPrintDebug("subConnectionJson(): sub->subConnections=%s\n", sub->subConnections.c_str() );
                
                JsonArray& subs = jsonBuffer.parseArray( sub->subConnections );
                if ( !subs.success() )
                    meshPrintDebug("subConnectionJson(): ran out of memory 3");
                
                subObj["subs"] = subs;
            }
            
            if ( !subArray.add( subObj ) )
                meshPrintDebug("subConnectionJson(): ran out of memory 4");
        }
        sub++;
    }
    
    //adda sub for a WS connection  -- hack!!!!
    //meshPrintDebug("subConnectionJson(): countWsConnections()=%d\n", countWsConnections());
    
    if ( countWsConnections() > 0 ) {
        //meshPrintDebug("subConnectionJson(): adding WS connection\n");
        
        JsonObject& subObj = jsonBuffer.createObject();
        if ( !subObj.success() )
            meshPrintDebug("subConnectionJson(): ran out of memory 5");
        
        subObj["chipId"] = 4444;  // something random - hack!!!
        //        JsonArray& subs = jsonBuffer.parseArray( "[]" );
        //      if ( !subs.success() )
        // meshPrintDebug("subConnectionJson(): ran out of memory 6");
        subObj["subs"] = "[]";//subs;
        
        if ( !subArray.add( subObj ) )
            meshPrintDebug("subConnectionJson(): ran out of memory 4");
    }
    
    String ret;
    subArray.printTo( ret );
    //meshPrintDebug("subConnectionJson(): ret=%s\n", ret.c_str());
    return ret;
}

//***********************************************************************
uint16_t easyMesh::connectionCount( meshConnectionType *exclude ) {
    //    meshPrintDebug("connectionCount():\n");
    uint16_t count = 0;
    
    SimpleList<meshConnectionType>::iterator sub = _connections.begin();
    while ( sub != _connections.end() ) {
        if ( sub != exclude ) {  //exclude this connection in the calc.
            count += ( 1 + jsonSubConnCount( sub->subConnections ) );
        }
        sub++;
    }
    
    if ( countWsConnections() > 0 ) {  //this is a hack.  It will report multiple ws on different nodes as multiple connections
        count++;
    }
    
    //meshPrintDebug("connectionCount(): count=%d\n", count);
    return count;
}

//***********************************************************************
uint16_t easyMesh::jsonSubConnCount( String& subConns ) {
    //    meshPrintDebug("jsonSubConnCount(): subConns=%s\n", subConns.c_str() );
    
    uint16_t count = 0;
    
    if ( subConns.length() < 3 )
        return 0;
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonArray& subArray = jsonBuffer.parseArray( subConns );
    
    if ( !subArray.success() ) {
        meshPrintDebug("subConnCount(): out of memory1\n");
    }
    
    String str;
    for ( uint8_t i = 0; i < subArray.size(); i++ ) {
        str = subArray.get<String>(i);
        //meshPrintDebug("jsonSubConnCount(): str=%s\n", str.c_str() );
        JsonObject& obj = jsonBuffer.parseObject( str );
        if ( !obj.success() ) {
            meshPrintDebug("subConnCount(): out of memory2\n");
        }
        
        str = obj.get<String>("subs");
        count += ( 1 + jsonSubConnCount( str ) );
    }
    
    //meshPrintDebug("jsonSubConnCount(): leaving count=%d\n", count );
    
    return count;
}


//***********************************************************************
void easyMesh::meshConnectedCb(void *arg) {
    meshPrintDebug("meshConnectedCb(): new meshConnection !!!\n");
    meshConnectionType newConn;
    newConn.esp_conn = (espconn *)arg;
    newConn.lastRecieved = getNodeTime();
    staticThis->_connections.push_back( newConn );
    
    espconn_regist_recvcb(newConn.esp_conn, meshRecvCb);
    espconn_regist_sentcb(newConn.esp_conn, meshSentCb);
    espconn_regist_reconcb(newConn.esp_conn, meshReconCb);
    espconn_regist_disconcb(newConn.esp_conn, meshDisconCb);
    
    if( newConn.esp_conn->proto.tcp->local_port != MESH_PORT ) { // we are the station, send station handshake
        meshPrintDebug("meshConnectedCb(): we are the STA\n");
        String subs = staticThis->subConnectionJson( &newConn );
        staticThis->sendMessage( 0, NODE_SYNC_REQUEST, subs );
    }
}

//***********************************************************************
void easyMesh::meshRecvCb(void *arg, char *data, unsigned short length) {
    meshConnectionType *receiveConn = staticThis->findConnection( (espconn *)arg );
    
    //meshPrintDebug("Recvd from %d-->%s<--\n", receiveConn->chipId, data);
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonObject& root = jsonBuffer.parseObject( data );
    if (!root.success()) {   // Test if parsing succeeded.
        meshPrintDebug("meshRecvCb: parseObject() failed. data=%s<--\n", data);
        return;
    }
    
    if ( receiveConn == NULL ) {
        meshPrintDebug("meshRecvCb(): recieved from unknown connection 0x%x ->%s<-\n", arg, data);
        uint32_t remoteId = (uint32_t)root["from"];
        receiveConn = staticThis->findConnection( remoteId );
        meshPrintDebug("meshRecvCb(): tried lookup by Id=%d, got this pointer 0x%x\n", remoteId, receiveConn);
        meshPrintDebug("dropping this msg... see if we recover?\n", remoteId, receiveConn);
        return;
    }
    
    String msg = root["msg"];
    
    switch( (meshPackageType)(int)root["type"] ) {
        case NODE_SYNC_REQUEST:
        case NODE_SYNC_REPLY:
            staticThis->handleNodeSync( receiveConn, root );
            break;
        
        case TIME_SYNC:
            staticThis->handleTimeSync( receiveConn, root );
            break;
    
        case SINGLE:
            if ( (uint32_t)root["dest"] == staticThis->getChipId() ) {  // msg for us!
                receivedCallback( (uint32_t)root["from"], msg);
            } else {                                                    // pass it along
                staticThis->sendMessage( (uint32_t)root["from"], (uint32_t)root["dest"], SINGLE, msg );  //this is ineffiecnt
            }
            break;
        
        case BROADCAST:
            staticThis->broadcastMessage( (uint32_t)root["from"], BROADCAST, msg, receiveConn);
            receivedCallback( (uint32_t)root["from"], msg);
            break;
     
        default:
            meshPrintDebug("meshRecvCb(): unexpected json, root[\"type\"]=%d", (int)root["type"]);
            return;
    }
    
    // record that we've gotten a valid package
    receiveConn->lastRecieved = getNodeTime();
    return;
}

//***********************************************************************
void easyMesh::meshSentCb(void *arg) {
    //    meshPrintDebug("In meshSentCb\r\n");    //data sent successfully
}

//***********************************************************************
void easyMesh::meshDisconCb(void *arg) {
    struct espconn *disConn = (espconn *)arg;
    
    meshPrintDebug("meshDisconCb: ");
    
    // remove this connection from _connections
    staticThis->cleanDeadConnections();
    
    //test to see if this connection was on the STATION interface by checking the local port
    if ( disConn->proto.tcp->local_port == MESH_PORT ) {
        meshPrintDebug("AP connection.  No new action needed. local_port=%d\n", disConn->proto.tcp->local_port);
    }
    else {
        meshPrintDebug("Station Connection! Find new node. local_port=%d\n", disConn->proto.tcp->local_port);
        wifi_station_disconnect();
    }
    
    return;
}

//***********************************************************************
void easyMesh::meshReconCb(void *arg, sint8 err) {
    meshPrintDebug("In meshReconCb err=%d\n", err );
}

//***********************************************************************
void easyMesh::wifiEventCb(System_Event_t *event) {
    switch (event->event) {
        case EVENT_STAMODE_CONNECTED:
            meshPrintDebug("Event: EVENT_STAMODE_CONNECTED ssid=%s\n", (char*)event->event_info.connected.ssid );
            break;
        case EVENT_STAMODE_DISCONNECTED:
            meshPrintDebug("Event: EVENT_STAMODE_DISCONNECTED\n");
            staticThis->connectToBestAP();
            break;
        case EVENT_STAMODE_AUTHMODE_CHANGE:
            meshPrintDebug("Event: EVENT_STAMODE_AUTHMODE_CHANGE\n");
            break;
        case EVENT_STAMODE_GOT_IP:
            meshPrintDebug("Event: EVENT_STAMODE_GOT_IP\n");
            staticThis->tcpConnect();
            break;
            
        case EVENT_SOFTAPMODE_STACONNECTED:
            meshPrintDebug("Event: EVENT_SOFTAPMODE_STACONNECTED\n");
            break;
            
        case EVENT_SOFTAPMODE_STADISCONNECTED:
            meshPrintDebug("Event: EVENT_SOFTAPMODE_STADISCONNECTED\n");
            break;
        case EVENT_STAMODE_DHCP_TIMEOUT:
            meshPrintDebug("Event: EVENT_STAMODE_DHCP_TIMEOUT\n");
            break;
        case EVENT_SOFTAPMODE_PROBEREQRECVED:
            // meshPrintDebug("Event: EVENT_SOFTAPMODE_PROBEREQRECVED\n");  // dont need to know about every probe request
            break;
        default:
            meshPrintDebug("Unexpected WiFi event: %d\n", event->event);
            break;
    }
}

