//
//  easyMeshAP.cpp
//  
//
//  Created by Bill Gray on 7/26/16.
//
//

#include <Arduino.h>

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#include "easyMesh.h"


// AP functions
//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::apInit( void  ) {
//    String password( MESH_PASSWORD );
    
    ip_addr ip, netmask;
    IP4_ADDR( &ip, 192, 168, ( _chipId & 0xFF ), 1);
    IP4_ADDR( &netmask, 255, 255, 255, 0);
    
    ip_info ipInfo;
    ipInfo.ip = ip;
    ipInfo.gw = ip;
    ipInfo.netmask = netmask;
    if ( !wifi_set_ip_info( SOFTAP_IF, &ipInfo ) ) {
        debugMsg( ERROR, "wifi_set_ip_info() failed\n");
    }
    
    debugMsg( STARTUP, "apInit(): Starting AP with SSID=%s IP=%d.%d.%d.%d GW=%d.%d.%d.%d NM=%d.%d.%d.%d\n",
                  _mySSID.c_str(),
                  IP2STR( &ipInfo.ip ),
                  IP2STR( &ipInfo.gw ),
                  IP2STR( &ipInfo.netmask ) );
    
    softap_config apConfig;
    wifi_softap_get_config( &apConfig );
    
    memset( apConfig.ssid, 0, 32 );
    memset( apConfig.password, 0, 64);
    memcpy( apConfig.ssid, _mySSID.c_str(), _mySSID.length());
    memcpy( apConfig.password, _meshPassword.c_str(), _meshPassword.length() );
    apConfig.authmode = AUTH_WPA2_PSK;
    apConfig.ssid_len = _mySSID.length();
    apConfig.beacon_interval = 100;
    apConfig.max_connection = 4; // how many stations can connect to ESP8266 softAP at most.
    
    wifi_softap_set_config(&apConfig);// Set ESP8266 softap config .
    if ( !wifi_softap_dhcps_start() )
        debugMsg( ERROR, "DHCP server failed\n");
    else
        debugMsg( STARTUP, "DHCP server started\n");
    
    // establish AP tcpServers
    tcpServerInit( _meshServerConn, _meshServerTcp, meshConnectedCb, _meshPort );
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::tcpServerInit(espconn &serverConn, esp_tcp &serverTcp, espconn_connect_callback connectCb, uint32 port) {
    
    debugMsg( GENERAL, "tcpServerInit():\n");
    
    serverConn.type = ESPCONN_TCP;
    serverConn.state = ESPCONN_NONE;
    serverConn.proto.tcp = &serverTcp;
    serverConn.proto.tcp->local_port = port;
    espconn_regist_connectcb(&serverConn, connectCb);
    sint8 ret = espconn_accept(&serverConn);
    if ( ret == 0 )
        debugMsg( STARTUP, "AP tcp server established on port %d\n", port );
    else
        debugMsg( ERROR, "AP tcp server on port %d FAILED ret=%d\n", port, ret);
    
    return;
}
