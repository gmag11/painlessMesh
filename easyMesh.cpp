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



