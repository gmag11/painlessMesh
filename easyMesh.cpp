#include <Arduino.h>
#include <ArduinoJson.h>
#include <SimpleList.h>
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#include "easyMesh.h"
#include "easyMeshSync.h"

easyMesh* staticThis;
uint16_t  count = 0;


// general functions
//***********************************************************************
void easyMesh::init( void ) {
    // shut everything down, start with a blank slate.
    wifi_station_set_auto_connect( 0 );
    if ( wifi_station_get_connect_status() != STATION_IDLE ) {
        meshPrintDebug("Station is doing something... wierd!? status=%d\n", wifi_station_get_connect_status());
        wifi_station_disconnect();
    }
    wifi_softap_dhcps_stop();
    
    wifi_set_event_handler_cb( wifiEventCb );
    
    staticThis = this;  // provides a way for static callback methods to access "this" object;
    
    // start configuration
    //meshPrintDebug("wifi_set_opmode(STATIONAP_MODE) succeeded? %d\n", wifi_set_opmode( STATIONAP_MODE ) );
    meshPrintDebug("wifi_set_opmode(STATIONAP_MODE) succeeded? %d\n", wifi_set_opmode( STATIONAP_MODE ) );
    
    
    _chipId = system_get_chip_id();
    _mySSID = String( MESH_PREFIX ) + String( _chipId );
    
    apInit();       // setup AP
    stationInit();  // setup station
    
//    os_timer_setfn( &_meshSyncTimer, meshSyncCallback, NULL );
//    os_timer_arm( &_meshSyncTimer, SYNC_INTERVAL, 1 );
}

//***********************************************************************
nodeStatusType easyMesh::update( void ) {
    manageStation();
    
    SimpleList<meshConnectionType>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection->lastRecieved + NODE_TIMEOUT < getNodeTime() ) {
            meshPrintDebug("update(): dropping %d NODE_TIMEOUT last=%d node=%d\n", connection->chipId, connection->lastRecieved, getNodeTime() );
            _connections.erase( connection );
            break;
        }
        
        if( connection->esp_conn->state == ESPCONN_CLOSE ) {
            meshPrintDebug("update(): dropping %d ESPCONN_CLOSE\n", connection->chipId);
            _connections.erase( connection );
            break;
        }
        
        
        // check to see if we've recieved something lately.  Stagger AP and STA 
        if (    (connection->needsNodeSync) == true  ||
                (   ( connection->lastRecieved + ( NODE_TIMEOUT / 2 ) < getNodeTime() ) &&
                    connection->nodeSyncRequest == 0  &&
                    connection->esp_conn->proto.tcp->local_port == MESH_PORT )  || // we are the AP
                (   ( connection->lastRecieved + ( NODE_TIMEOUT * 3 / 4 ) < getNodeTime() ) &&
                    connection->nodeSyncRequest == 0  &&
                    connection->esp_conn->proto.tcp->local_port != MESH_PORT ) // we are the STA

            ) {
            // start a nodeSync
            meshPrintDebug("update(): starting nodeSync with %d needs=%d\n", connection->chipId, connection->needsNodeSync);
            String subs = staticThis->subConnectionJson( connection );
            staticThis->sendMessage( connection->chipId, NODE_SYNC_REQUEST, subs );
            connection->needsNodeSync = false;
            break;
        }
        
        if ( connection->needsTimeSync == true ) {
            meshPrintDebug("update(): starting timeSync with %d\n", connection->chipId);
            startTimeSync( connection );
            connection->needsTimeSync == false;
            break;
        }
        
        connection++;
    }
    
    return _nodeStatus;
}

//***********************************************************************
void easyMesh::setStatus( nodeStatusType newStatus ) {
    _nodeStatus = newStatus;
}



