#include <Arduino.h>
#include <ArduinoJson.h>
#include <SimpleList.h>

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#include "painlessMesh.h"
#include "painlessMeshSync.h"


painlessMesh* staticThis;
uint16_t  count = 0;


// general functions
//***********************************************************************
/*void ICACHE_FLASH_ATTR painlessMesh::init( void ) {
    // shut everything down, start with a blank slate.
    debugMsg( STARTUP, "init():\n",    wifi_station_set_auto_connect( 0 ));

    if ( wifi_station_get_connect_status() != STATION_IDLE ) {
        debugMsg( ERROR, "Station is doing something... wierd!? status=%d\n", wifi_station_get_connect_status());
        wifi_station_disconnect();
    }
    wifi_softap_dhcps_stop();
    
    wifi_set_event_handler_cb( wifiEventCb );
    
    staticThis = this;  // provides a way for static callback methods to access "this" object;
    
    // start configuration
    debugMsg( GENERAL, "wifi_set_opmode(STATIONAP_MODE) succeeded? %d\n", wifi_set_opmode( STATIONAP_MODE ) );
    uint8_t  MAC[] = {0,0,0,0,0,0};
    wifi_get_macaddr(SOFTAP_IF, MAC);    
    _nodeId = encodeNodeId(MAC);
    _meshSSID = String( MESH_SSID );
    
    apInit();       // setup AP
    stationInit();  // setup station
    
    debugMsg( GENERAL, "init(): tcp_max_con=%u\n", espconn_tcp_get_max_con() );
}
*/
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::init( String ssid, String password, uint16_t port, _auth_mode authmode, uint8_t channel, phy_mode_t phymode, uint8_t maxtpw, uint8_t hidden, uint8_t maxconn ) {
    // shut everything down, start with a blank slate.
    debugMsg( STARTUP, "init():\n",    wifi_station_set_auto_connect( 0 ));
    
    if ( wifi_station_get_connect_status() != STATION_IDLE ) {
        debugMsg( ERROR, "Station is doing something... wierd!? status=%d\n", wifi_station_get_connect_status());
        wifi_station_disconnect();
    }
    wifi_softap_dhcps_stop();
    
    wifi_set_event_handler_cb( wifiEventCb );

    wifi_set_phy_mode( phymode ); // allow setting PHY_MODE_11G / PHY_MODE_11B
    system_phy_set_max_tpw( maxtpw ); //maximum value of RF Tx Power, unit : 0.25dBm, range [0,82]
    
    staticThis = this;  // provides a way for static callback methods to access "this" object;
    
    // start configuration
    debugMsg( GENERAL, "wifi_set_opmode(STATIONAP_MODE) succeeded? %d\n", wifi_set_opmode( STATIONAP_MODE ) );
    
    _meshSSID = ssid;
    _meshPassword = password;
    _meshPort = port;
    _meshChannel = channel;
    _meshAuthMode = authmode;
    if( password == "" )
        _meshAuthMode = AUTH_OPEN; //if no password ... set auth mode to open
    _meshHidden = hidden;
    _meshMaxConn = maxconn;

    uint8_t MAC[] = {0,0,0,0,0,0};
    wifi_get_macaddr(SOFTAP_IF, MAC);    
    _nodeId = encodeNodeId(MAC);
    
    apInit();       // setup AP
    stationInit();  // setup station
    
    debugMsg( GENERAL, "init(): tcp_max_con=%u\n", espconn_tcp_get_max_con() );
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::update( void ) {
    manageStation();
    manageConnections();
    return;
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendSingle( uint32_t &destId, String &msg ){
    debugMsg( COMMUNICATION, "sendSingle(): dest=%d msg=%s\n", destId, msg.c_str());
    sendMessage( destId, SINGLE, msg );
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendBroadcast( String &msg ) {
    debugMsg( COMMUNICATION, "sendBroadcast(): msg=%s\n", msg.c_str());
    broadcastMessage( _nodeId, BROADCAST, msg );
}
