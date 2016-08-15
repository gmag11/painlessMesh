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
static void (*newConnectionCallback)( bool adopt );

extern easyMesh* staticThis;

// connection managment functions
//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::setReceiveCallback( void(*onReceive)(uint32_t from, String &msg) ) {
    receivedCallback = onReceive;
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::setNewConnectionCallback( void(*onNewConnection)(bool adopt) ) {
    newConnectionCallback = onNewConnection;
}

//***********************************************************************
meshConnectionType* ICACHE_FLASH_ATTR easyMesh::closeConnection( meshConnectionType *conn ) {
    // It seems that more should be done here... perhas send off a packette to
    // make an attempt to tell the other node that we are closing this conneciton?
    espconn_disconnect( conn->esp_conn );
    return _connections.erase( conn );
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::manageConnections( void ) {
    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection->lastRecieved + NODE_TIMEOUT < getNodeTime() ) {
            meshPrintDebug("manageConnections(): dropping %d NODE_TIMEOUT last=%u node=%u\n", connection->chipId, connection->lastRecieved, getNodeTime() );
 
            connection = closeConnection( connection );
            continue;
        }
        
        if( connection->esp_conn->state == ESPCONN_CLOSE ) {
            meshPrintDebug("manageConnections(): dropping %d ESPCONN_CLOSE\n",connection->chipId);
            connection = closeConnection( connection );
            continue;
        }
        
        if ( connection->needsNodeSync == true ) {           // start a nodeSync
            //meshPrintDebug("manageConnections(): start nodeSync with %d\n", connection->chipId);
            startNodeSync( connection );
            connection++;
            continue;
        }
        
        if ( connection->needsTimeSync == true ) {
            meshPrintDebug("manageConnections(): starting timeSync with %d\n", connection->chipId);
            startTimeSync( connection );
            connection->needsTimeSync = false;
 
            connection++;
            continue;
        }

        if ( connection->newConnection == true ) {
            newConnectionCallback( adoptionCalc( connection ) );
            connection->newConnection = false;
 
            connection++;
            continue;
        }
        
        // check to see if we've recieved something lately.  Else, flag. Stagger AP and STA
        uint32_t nodeTime = getNodeTime();
        if ( connection->nodeSyncRequest == 0 ) { // nodeSync not in progress
            if (    (connection->esp_conn->proto.tcp->local_port == MESH_PORT  // we are AP
                     &&
                     connection->lastRecieved + ( NODE_TIMEOUT / 2 ) < nodeTime )
                ||
                    (connection->esp_conn->proto.tcp->local_port != MESH_PORT  // we are the STA
                     &&
                     connection->lastRecieved + ( NODE_TIMEOUT * 3 / 4 ) < nodeTime )
                ) {
                connection->needsNodeSync = true;
            }
        }
        connection++;
    }
}

//***********************************************************************
meshConnectionType* ICACHE_FLASH_ATTR easyMesh::findConnection( uint32_t chipId ) {
  //  meshPrintDebug("In findConnection(chipId)\n");
    
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
 //   meshPrintDebug("findConnection(%d): did not find connection\n", chipId );
    return NULL;
}

//***********************************************************************
meshConnectionType* ICACHE_FLASH_ATTR easyMesh::findConnection( espconn *conn ) {
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
String ICACHE_FLASH_ATTR easyMesh::subConnectionJson( meshConnectionType *exclude ) {
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
    
    String ret;
    subArray.printTo( ret );
    //meshPrintDebug("subConnectionJson(): ret=%s\n", ret.c_str());
    return ret;
}

//***********************************************************************
uint16_t ICACHE_FLASH_ATTR easyMesh::connectionCount( meshConnectionType *exclude ) {
    //    meshPrintDebug("connectionCount():\n");
    uint16_t count = 0;
    
    SimpleList<meshConnectionType>::iterator sub = _connections.begin();
    while ( sub != _connections.end() ) {
        if ( sub != exclude ) {  //exclude this connection in the calc.
            count += ( 1 + jsonSubConnCount( sub->subConnections ) );
        }
        sub++;
    }
    
    //meshPrintDebug("connectionCount(): count=%d\n", count);
    return count;
}

//***********************************************************************
uint16_t ICACHE_FLASH_ATTR easyMesh::jsonSubConnCount( String& subConns ) {
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
void ICACHE_FLASH_ATTR easyMesh::meshConnectedCb(void *arg) {
    meshPrintDebug("meshConnectedCb(): new meshConnection !!!\n");
    meshConnectionType newConn;
    newConn.esp_conn = (espconn *)arg;
    espconn_set_opt( newConn.esp_conn, ESPCONN_NODELAY );  // removes nagle, low latency, but soaks up bandwidth
    newConn.lastRecieved = getNodeTime();
    
    espconn_regist_recvcb(newConn.esp_conn, meshRecvCb);
    espconn_regist_sentcb(newConn.esp_conn, meshSentCb);
    espconn_regist_reconcb(newConn.esp_conn, meshReconCb);
    espconn_regist_disconcb(newConn.esp_conn, meshDisconCb);

    staticThis->_connections.push_back( newConn );
    
    if( newConn.esp_conn->proto.tcp->local_port != MESH_PORT ) { // we are the station, start nodeSync
//        meshPrintDebug("meshConnectedCb():: chipId=%u chipId=%u\n", newConn.chipId, (staticThis->_connections.end() - 1) ->chipId);
        staticThis->startNodeSync( staticThis->_connections.end() - 1 );
        newConn.needsTimeSync = true;
    }
    
    meshPrintDebug("meshConnectedCb(): leaving\n");
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::meshRecvCb(void *arg, char *data, unsigned short length) {
    meshConnectionType *receiveConn = staticThis->findConnection( (espconn *)arg );
    
    if ( receiveConn == NULL ) {
        meshPrintDebug("meshRecvCb(): recieved from unknown connection 0x%x ->%s<-\n", arg, data);
        meshPrintDebug("dropping this msg... see if we recover?\n");
        return;
    }
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonObject& root = jsonBuffer.parseObject( data );
    if (!root.success()) {   // Test if parsing succeeded.
        meshPrintDebug("meshRecvCb: parseObject() failed. data=%s<--\n", data);
        return;
    }
    
    //meshPrintDebug("Recvd from %d-->%s<--\n", receiveConn->chipId, data);

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
                //staticThis->sendMessage( (uint32_t)root["dest"], (uint32_t)root["from"], SINGLE, msg );  //this is ineffiecnt
                String tempStr( data );
                staticThis->sendPackage( staticThis->findConnection( (uint32_t)root["dest"] ), tempStr );
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
void ICACHE_FLASH_ATTR easyMesh::meshSentCb(void *arg) {
    //    meshPrintDebug("In meshSentCb\r\n");    //data sent successfully
    espconn *conn = (espconn*)arg;
    meshConnectionType *meshConnection = staticThis->findConnection( conn );
    
    if ( meshConnection == NULL ) {
        meshPrintDebug("meshSentCb(): err did not find meshConnection? Likely it was dropped for some reason\n");
        return;
    }
    
    if ( !meshConnection->sendQueue.empty() ) {
        String package = *meshConnection->sendQueue.begin();
        meshConnection->sendQueue.pop_front();
        sint8 errCode = espconn_send( meshConnection->esp_conn, (uint8*)package.c_str(), package.length() );
        //connection->sendReady = false;
        
        if ( errCode != 0 ) {
            meshPrintDebug("meshSentCb(): espconn_send Failed err=%d\n", errCode );
        }
    }
    else {
        meshConnection->sendReady = true;
    }
}
//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::meshDisconCb(void *arg) {
    struct espconn *disConn = (espconn *)arg;
    
    meshPrintDebug("meshDisconCb: ");
    
    // remove this connection from _connections
    //staticThis->cleanDeadConnections();
    
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
void ICACHE_FLASH_ATTR easyMesh::meshReconCb(void *arg, sint8 err) {
    meshPrintDebug("In meshReconCb(): err=%d\n", err );
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::wifiEventCb(System_Event_t *event) {
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

