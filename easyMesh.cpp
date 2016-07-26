#include <Arduino.h>
#include <ArduinoJson.h>
#include <SimpleList.h>
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

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
#include "meshSync.h"
#include "easyMeshWebServer.h"
#include "meshWebSocket.h"


extern AnimationController *blipController;

easyMesh* staticThis;
uint16_t  count = 0;


// general functions
//***********************************************************************
void easyMesh::init( void ) {
    // shut everything down, start with a blank slate.
    wifi_station_set_auto_connect( 0 );
    if ( wifi_station_get_connect_status() == STATION_IDLE ) {
        Serial.printf("Station is doing something... wierd!?\n");
        wifi_station_disconnect();
    }
    wifi_softap_dhcps_stop();
    
    wifi_set_event_handler_cb( wifiEventCb );
    
    staticThis = this;  // provides a way for static callback methods to access "this" object;
    
    // start configuration
    Serial.printf("wifi_set_opmode(STATIONAP_MODE) succeeded? %d\n", wifi_set_opmode( STATIONAP_MODE ) );
    
    _chipId = system_get_chip_id();
    _mySSID = String( MESH_PREFIX ) + String( _chipId );
    
    apInit();       // setup AP
    stationInit();  // setup station
    
    os_timer_setfn( &_meshSyncTimer, meshSyncCallback, NULL );
    os_timer_arm( &_meshSyncTimer, SYNC_INTERVAL, 1 );
}

//***********************************************************************
nodeStatusType easyMesh::update( void ) {
    manageStation();
    return _nodeStatus;
}

//***********************************************************************
void easyMesh::setStatus( nodeStatusType newStatus ) {
    _nodeStatus = newStatus;
}



// Syncing functions
//***********************************************************************
void easyMesh::handleHandShake( meshConnection_t *conn, JsonObject& root ) {
    String msg = root["msg"];
    uint32_t remoteChipId = (uint32_t)root["from"];
    
    if ( msg == "Station Handshake") {
        Serial.printf("handleHandShake: recieved station handshake\n");
        
        // check to make sure we are not already connected
        if ( staticThis->findConnection( remoteChipId ) != NULL ) {  //drop this connection
            Serial.printf("We are already connected to this node as Station.  Drop new connection\n");
            espconn_disconnect( conn->esp_conn );
            return;
        }
        //else
        conn->chipId = remoteChipId;
        Serial.printf("sending AP handshake\n");
        staticThis->sendMessage( remoteChipId, HANDSHAKE, "AP Handshake");
        _nodeStatus = CONNECTED;
    }
    else if ( msg == "AP Handshake") {  // add AP chipId to connection
        Serial.printf("handleHandShake: Got AP Handshake\n");
        
        // check to make sure we are not already connected
        if ( staticThis->findConnection( remoteChipId ) != NULL ) {  //drop this connection
            Serial.printf("We are already connected to this node as AP.  Drop new connection\n");
            espconn_disconnect( conn->esp_conn );
            return;
        }
        //else
        conn->chipId = remoteChipId;
        _nodeStatus = CONNECTED;
    }
    else {
        Serial.printf("handleHandShake(): Weird msg\n");
    }
}

//***********************************************************************
void easyMesh::handleMeshSync( meshConnection_t *conn, JsonObject& root ) {
//    Serial.printf("handleMeshSync(): type=%d\n", (int)root["type"] );
    
    String subs = root["subs"];
    conn->subConnections = subs;
//    Serial.printf("subs=%s\n", conn->subConnections.c_str());
    
    if ( (meshPackageType)(int)root["type"] == MESH_SYNC_REQUEST ) {
        String subsJson = staticThis->subConnectionJson( conn );
        staticThis->sendMessage( conn->chipId, MESH_SYNC_REPLY, subsJson );
    }
    else {
        startTimeSync( conn );
    }
}

//***********************************************************************
void easyMesh::meshSyncCallback( void *arg ) {
    Serial.printf("syncCallback():\n");
    
    if ( wifi_station_get_connect_status() == STATION_GOT_IP ) {
        // we are connected as a station find station connection
        SimpleList<meshConnection_t>::iterator connection = staticThis->_connections.begin();
        while ( connection != staticThis->_connections.end() ) {
            if ( connection->esp_conn->proto.tcp->local_port != MESH_PORT ) {
                // found station connection.  Initiate sync
                String subsJson = staticThis->subConnectionJson( connection );
                staticThis->sendMessage( connection->chipId, MESH_SYNC_REQUEST, subsJson );
                break;
            }
            connection++;
        }
    }
}

//***********************************************************************
void easyMesh::startTimeSync( meshConnection_t *conn ) {
    Serial.printf("startTimeSync():\n");
    // since we are here, we know that we are the STA
    
    if ( conn->time.num > TIME_SYNC_CYCLES ) {
        Serial.printf("startTimeSync(): Error timeSync.num not reset conn->time.num=%d\n", conn->time.num );
    }
    
    conn->time.num = 0;
    
    // make the adoption calulation.  Figure out how many nodes I am connected to exclusive of this connection.
    uint16_t mySubCount = 0;
    uint16_t remoteSubCount = 0;
    SimpleList<meshConnection_t>::iterator sub = _connections.begin();
    while ( sub != _connections.end() ) {
        if ( sub != conn ) {  //exclude this connection in the calc.
            mySubCount += ( 1 + jsonSubConnCount( sub->subConnections ) );
        }
        sub++;
    }
    remoteSubCount = jsonSubConnCount( conn->subConnections );
    conn->time.adopt = ( mySubCount > remoteSubCount ) ? false : true;  // do I adopt the estblished time?
    Serial.printf("startTimeSync(): adopt=%d\n", conn->time.adopt);
    
    String timeStamp = conn->time.buildTimeStamp();
    staticThis->sendMessage( conn->chipId, TIME_SYNC, timeStamp );
}

//***********************************************************************
void easyMesh::handleTimeSync( meshConnection_t *conn, JsonObject& root ) {
//    Serial.printf("handleTimeSync():\n");

    String timeStamp = root["timeStamp"];
    conn->time.processTimeStamp( timeStamp );
    
    if ( conn->time.num < TIME_SYNC_CYCLES ) {
        staticThis->sendMessage( conn->chipId, TIME_SYNC, timeStamp );
    }
  
    uint8_t odd = conn->time.num % 2;
    
    if ( (conn->time.num + odd) >= TIME_SYNC_CYCLES ) {
        if ( conn->time.adopt ) {
            conn->time.calcAdjustment( odd );
        }
    }
}

//***********************************************************************
uint16_t easyMesh::jsonSubConnCount( String& subConns ) {
    uint16_t count = 0;
    
    if ( subConns.length() < 3 )
        return 0;
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonArray& subArray = jsonBuffer.parseArray( subConns );
    
    if ( !subArray.success() ) {
        Serial.printf("subConnCount(): out of memory1\n");
    }
    
    String str;
    
    for ( uint8_t i = 0; i < subArray.size(); i++ ) {
        str = subArray.get<String>(i);
        Serial.printf("jsonSubConnCount(): str=%s\n", str.c_str() );
        JsonObject& obj = jsonBuffer.parseObject( str );
        if ( !obj.success() ) {
            Serial.printf("subConnCount(): out of memory2\n");
        }
        
        str = obj.get<String>("subs");
        count += ( 1 + jsonSubConnCount( str ) );
    }
    
    return count;
}

//***********************************************************************
String easyMesh::subConnectionJson( meshConnection_t *thisConn ) {
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonArray& subArray = jsonBuffer.createArray();
    if ( !subArray.success() )
        Serial.printf("subConnectionJson(): ran out of memory 1");
    
    SimpleList<meshConnection_t>::iterator sub = _connections.begin();
    while ( sub != _connections.end() ) {
        if ( sub != thisConn ) {  //exclude the connection that we are working with.
            JsonObject& subObj = jsonBuffer.createObject();
            if ( !subObj.success() )
                Serial.printf("subConnectionJson(): ran out of memory 2");
            
            subObj["chipId"] = sub->chipId;
            
            if ( sub->subConnections.length() != 0 ) {
                Serial.printf("subConnectionJson(): sub->subConnections=%s\n", sub->subConnections.c_str() );
                
                JsonArray& subs = jsonBuffer.parseArray( sub->subConnections );
                if ( !subs.success() )
                    Serial.printf("subConnectionJson(): ran out of memory 3");
                
                subObj["subs"] = subs;
            }
            
            if ( !subArray.add( subObj ) )
                Serial.printf("subConnectionJson(): ran out of memory 4");
        }
        sub++;
    }
    
    String ret;
    subArray.printTo( ret );
    return ret;
}








// control functions
//***********************************************************************
void easyMesh::handleControl( meshConnection_t *conn, JsonObject& root ) {
    Serial.printf("handleControl():");
    
    String control = root["control"];
    
    DynamicJsonBuffer jsonBuffer(50 );
    JsonObject& controlObj = jsonBuffer.parseObject(control);
    
    if ( !controlObj.success() ) {
        Serial.printf("handleControl(): out of memory1?\n" );
        return;
    }
    
    blipController->hue = controlObj.get<float>("one");
    
    String temp;
    controlObj.printTo(temp);
    Serial.printf("control=%s, controlObj=%s, one=%d\n", control.c_str(), temp.c_str(), blipController->hue );
}



