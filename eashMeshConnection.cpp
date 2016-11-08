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
    debugMsg( GENERAL, "setReceiveCallback():\n");
    receivedCallback = onReceive;
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::setNewConnectionCallback( void(*onNewConnection)(bool adopt) ) {
    debugMsg( GENERAL, "setNewConnectionCallback():\n");
    newConnectionCallback = onNewConnection;
}

//***********************************************************************
meshConnectionType* ICACHE_FLASH_ATTR easyMesh::closeConnection( meshConnectionType *conn ) {
    // It seems that more should be done here... perhas send off a packette to
    // make an attempt to tell the other node that we are closing this conneciton?
    debugMsg( CONNECTION, "closeConnection(): conn-chipId=%d\n", conn->chipId );
    espconn_disconnect( conn->esp_conn );
    return _connections.erase( conn );
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::manageConnections( void ) {
    debugMsg( GENERAL, "manageConnections():\n");
    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection->lastRecieved + NODE_TIMEOUT < getNodeTime() ) {
            debugMsg( CONNECTION, "manageConnections(): dropping %d NODE_TIMEOUT last=%u node=%u\n", connection->chipId, connection->lastRecieved, getNodeTime() );
 
            connection = closeConnection( connection );
            continue;
        }
        
        if( connection->esp_conn->state == ESPCONN_CLOSE ) {
            debugMsg( CONNECTION, "manageConnections(): dropping %d ESPCONN_CLOSE\n",connection->chipId);
            connection = closeConnection( connection );
            continue;
        }
        
        switch ( connection->nodeSyncStatus ) {
            case NEEDED:           // start a nodeSync
                debugMsg( SYNC, "manageConnections(): start nodeSync with %d\n", connection->chipId);
                startNodeSync( connection );
                connection->nodeSyncStatus = IN_PROGRESS;

            case IN_PROGRESS:
                connection++;
                continue;
        }
        
        switch ( connection->timeSyncStatus ) {
            case NEEDED:
                debugMsg( SYNC, "manageConnections(): starting timeSync with %d\n", connection->chipId);
                startTimeSync( connection );
                connection->timeSyncStatus = IN_PROGRESS;

            case IN_PROGRESS:
                connection++;
                continue;
        }

        if ( connection->newConnection == true ) {  // we should only get here once first nodeSync and timeSync are complete
            newConnectionCallback( adoptionCalc( connection ) );
            connection->newConnection = false;
 
            connection++;
            continue;
        }
        
        // check to see if we've recieved something lately.  Else, flag for new sync.
        // Stagger AP and STA so that they don't try to start a sync at the same time.
        uint32_t nodeTime = getNodeTime();
        if ( connection->nodeSyncRequest == 0 ) { // nodeSync not in progress
            if (    (connection->esp_conn->proto.tcp->local_port == _meshPort  // we are AP
                     &&
                     connection->lastRecieved + ( NODE_TIMEOUT / 2 ) < nodeTime )
                ||
                    (connection->esp_conn->proto.tcp->local_port != _meshPort  // we are the STA
                     &&
                     connection->lastRecieved + ( NODE_TIMEOUT * 3 / 4 ) < nodeTime )
                ) {
                connection->nodeSyncStatus = NEEDED;
            }
        }
        connection++;
    }
}

//***********************************************************************
meshConnectionType* ICACHE_FLASH_ATTR easyMesh::findConnection( uint32_t chipId ) {
    debugMsg( GENERAL, "In findConnection(chipId)\n");
    
    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        
        if ( connection->chipId == chipId ) {  // check direct connections
            debugMsg( GENERAL, "findConnection(chipId): Found Direct Connection\n");
            return connection;
        }
        
        String chipIdStr(chipId);
        if ( connection->subConnections.indexOf(chipIdStr) != -1 ) { // check sub-connections
            debugMsg( GENERAL, "findConnection(chipId): Found Sub Connection\n");
            return connection;
        }
        
        connection++;
    }
    debugMsg( CONNECTION, "findConnection(%d): did not find connection\n", chipId );
    return NULL;
}

//***********************************************************************
meshConnectionType* ICACHE_FLASH_ATTR easyMesh::findConnection( espconn *conn ) {
    debugMsg( GENERAL, "In findConnection(esp_conn) conn=0x%x\n", conn );
    
    int i=0;
    
    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection->esp_conn == conn ) {
            return connection;
        }
        connection++;
    }
    
    debugMsg( CONNECTION, "findConnection(espconn): Did not Find\n");
    return NULL;
}
 
//***********************************************************************
String ICACHE_FLASH_ATTR easyMesh::subConnectionJson( meshConnectionType *exclude ) {
    debugMsg( GENERAL, "subConnectionJson(), exclude=%d\n", exclude->chipId );
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonArray& subArray = jsonBuffer.createArray();
    if ( !subArray.success() )
        debugMsg( ERROR, "subConnectionJson(): ran out of memory 1");
    
    SimpleList<meshConnectionType>::iterator sub = _connections.begin();
    while ( sub != _connections.end() ) {
        if ( sub != exclude && sub->chipId != 0 ) {  //exclude connection that we are working with & anything too new.
            JsonObject& subObj = jsonBuffer.createObject();
            if ( !subObj.success() )
                debugMsg( ERROR, "subConnectionJson(): ran out of memory 2");
            
            subObj["chipId"] = sub->chipId;
            
            if ( sub->subConnections.length() != 0 ) {
                //debugMsg( GENERAL, "subConnectionJson(): sub->subConnections=%s\n", sub->subConnections.c_str() );
                
                JsonArray& subs = jsonBuffer.parseArray( sub->subConnections );
                if ( !subs.success() )
                    debugMsg( ERROR, "subConnectionJson(): ran out of memory 3");
                
                subObj["subs"] = subs;
            }
            
            if ( !subArray.add( subObj ) )
                debugMsg( ERROR, "subConnectionJson(): ran out of memory 4");
        }
        sub++;
    }
    
    String ret;
    subArray.printTo( ret );
    debugMsg( GENERAL, "subConnectionJson(): ret=%s\n", ret.c_str());
    return ret;
}

//***********************************************************************
uint16_t ICACHE_FLASH_ATTR easyMesh::connectionCount( meshConnectionType *exclude ) {
    uint16_t count = 0;
    
    SimpleList<meshConnectionType>::iterator sub = _connections.begin();
    while ( sub != _connections.end() ) {
        if ( sub != exclude ) {  //exclude this connection in the calc.
            count += ( 1 + jsonSubConnCount( sub->subConnections ) );
        }
        sub++;
    }
    
    debugMsg( GENERAL, "connectionCount(): count=%d\n", count);
    return count;
}

//***********************************************************************
uint16_t ICACHE_FLASH_ATTR easyMesh::jsonSubConnCount( String& subConns ) {
    debugMsg( GENERAL, "jsonSubConnCount(): subConns=%s\n", subConns.c_str() );
    
    uint16_t count = 0;
    
    if ( subConns.length() < 3 )
        return 0;
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonArray& subArray = jsonBuffer.parseArray( subConns );
    
    if ( !subArray.success() ) {
        debugMsg( ERROR, "subConnCount(): out of memory1\n");
    }
    
    String str;
    for ( uint8_t i = 0; i < subArray.size(); i++ ) {
        str = subArray.get<String>(i);
        debugMsg( GENERAL, "jsonSubConnCount(): str=%s\n", str.c_str() );
        JsonObject& obj = jsonBuffer.parseObject( str );
        if ( !obj.success() ) {
            debugMsg( ERROR, "subConnCount(): out of memory2\n");
        }
        
        str = obj.get<String>("subs");
        count += ( 1 + jsonSubConnCount( str ) );
    }
    
    debugMsg( CONNECTION, "jsonSubConnCount(): leaving count=%d\n", count );
    
    return count;
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::meshConnectedCb(void *arg) {
    staticThis->debugMsg( CONNECTION, "meshConnectedCb(): new meshConnection !!!\n");
    meshConnectionType newConn;
    newConn.esp_conn = (espconn *)arg;
    espconn_set_opt( newConn.esp_conn, ESPCONN_NODELAY );  // removes nagle, low latency, but soaks up bandwidth
    newConn.lastRecieved = staticThis->getNodeTime();
    
    espconn_regist_recvcb(newConn.esp_conn, meshRecvCb);
    espconn_regist_sentcb(newConn.esp_conn, meshSentCb);
    espconn_regist_reconcb(newConn.esp_conn, meshReconCb);
    espconn_regist_disconcb(newConn.esp_conn, meshDisconCb);

    staticThis->_connections.push_back( newConn );
    
    if( newConn.esp_conn->proto.tcp->local_port != staticThis->_meshPort ) { // we are the station, start nodeSync
        staticThis->debugMsg( CONNECTION, "meshConnectedCb(): we are STA, start nodeSync\n");
        staticThis->startNodeSync( staticThis->_connections.end() - 1 );
        newConn.timeSyncStatus = NEEDED;
    }
    else
        staticThis->debugMsg( CONNECTION, "meshConnectedCb(): we are AP\n");
    
    staticThis->debugMsg( GENERAL, "meshConnectedCb(): leaving\n");
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::meshRecvCb(void *arg, char *data, unsigned short length) {
    meshConnectionType *receiveConn = staticThis->findConnection( (espconn *)arg );

    staticThis->debugMsg( COMMUNICATION, "meshRecvCb(): data=%s fromId=%d\n", data, receiveConn->chipId );
    
    if ( receiveConn == NULL ) {
        staticThis->debugMsg( ERROR, "meshRecvCb(): recieved from unknown connection 0x%x ->%s<-\n", arg, data);
        staticThis->debugMsg( ERROR, "dropping this msg... see if we recover?\n");
        return;
    }


    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonObject& root = jsonBuffer.parseObject( data );
    if (!root.success()) {   // Test if parsing succeeded.
        staticThis->debugMsg( ERROR, "meshRecvCb: parseObject() failed. data=%s<--\n", data);
        return;
    }
    
    staticThis->debugMsg( GENERAL, "Recvd from %d-->%s<--\n", receiveConn->chipId, data);

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
            staticThis->debugMsg( ERROR, "meshRecvCb(): unexpected json, root[\"type\"]=%d", (int)root["type"]);
            return;
    }
    
    // record that we've gotten a valid package
    receiveConn->lastRecieved = staticThis->getNodeTime();
    return;
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::meshSentCb(void *arg) {
    staticThis->debugMsg( GENERAL, "meshSentCb():\n");    //data sent successfully
    espconn *conn = (espconn*)arg;
    meshConnectionType *meshConnection = staticThis->findConnection( conn );
    
    if ( meshConnection == NULL ) {
        staticThis->debugMsg( ERROR, "meshSentCb(): err did not find meshConnection? Likely it was dropped for some reason\n");
        return;
    }
    
    if ( !meshConnection->sendQueue.empty() ) {
        String package = *meshConnection->sendQueue.begin();
        meshConnection->sendQueue.pop_front();
        sint8 errCode = espconn_send( meshConnection->esp_conn, (uint8*)package.c_str(), package.length() );
        //connection->sendReady = false;
        
        if ( errCode != 0 ) {
            staticThis->debugMsg( ERROR, "meshSentCb(): espconn_send Failed err=%d\n", errCode );
        }
    }
    else {
        meshConnection->sendReady = true;
    }
}
//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::meshDisconCb(void *arg) {
    struct espconn *disConn = (espconn *)arg;
    
    staticThis->debugMsg( CONNECTION, "meshDisconCb(): ");
    
    //test to see if this connection was on the STATION interface by checking the local port
    if ( disConn->proto.tcp->local_port == staticThis->_meshPort ) {
        staticThis->debugMsg( CONNECTION, "AP connection.  No new action needed. local_port=%d\n", disConn->proto.tcp->local_port);
    }
    else {
        staticThis->debugMsg( CONNECTION, "Station Connection! Find new node. local_port=%d\n", disConn->proto.tcp->local_port);
        // should start up automatically when station_status changes to IDLE
        wifi_station_disconnect();
    }
    
    return;
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::meshReconCb(void *arg, sint8 err) {
    staticThis->debugMsg( ERROR, "In meshReconCb(): err=%d\n", err );
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::wifiEventCb(System_Event_t *event) {
    switch (event->event) {
        case EVENT_STAMODE_CONNECTED:
            staticThis->debugMsg( CONNECTION, "wifiEventCb(): EVENT_STAMODE_CONNECTED ssid=%s\n", (char*)event->event_info.connected.ssid );
            break;
        case EVENT_STAMODE_DISCONNECTED:
            staticThis->debugMsg( CONNECTION, "wifiEventCb(): EVENT_STAMODE_DISCONNECTED\n");
            staticThis->connectToBestAP();
            break;
        case EVENT_STAMODE_AUTHMODE_CHANGE:
            staticThis->debugMsg( CONNECTION, "wifiEventCb(): EVENT_STAMODE_AUTHMODE_CHANGE\n");
            break;
        case EVENT_STAMODE_GOT_IP:
            staticThis->debugMsg( CONNECTION, "wifiEventCb(): EVENT_STAMODE_GOT_IP\n");
            staticThis->tcpConnect();
            break;
            
        case EVENT_SOFTAPMODE_STACONNECTED:
            staticThis->debugMsg( CONNECTION, "wifiEventCb(): EVENT_SOFTAPMODE_STACONNECTED\n");
            break;
            
        case EVENT_SOFTAPMODE_STADISCONNECTED:
            staticThis->debugMsg( CONNECTION, "wifiEventCb(): EVENT_SOFTAPMODE_STADISCONNECTED\n");
            break;
        case EVENT_STAMODE_DHCP_TIMEOUT:
            staticThis->debugMsg( CONNECTION, "wifiEventCb(): EVENT_STAMODE_DHCP_TIMEOUT\n");
            break;
        case EVENT_SOFTAPMODE_PROBEREQRECVED:
            // debugMsg( GENERAL, "Event: EVENT_SOFTAPMODE_PROBEREQRECVED\n");  // dont need to know about every probe request
            break;
        default:
            staticThis->debugMsg( ERROR, "Unexpected WiFi event: %d\n", event->event);
            break;
    }
}

