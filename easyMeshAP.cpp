//
//  easyMeshAP.cpp
//  
//
//  Created by Bill Gray on 7/26/16.
//
//

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

#include "FS.h"  // for SPIFFS
#include "easyMesh.h"



// AP functions
//***********************************************************************
void easyMesh::apInit( void  ) {
    String password( MESH_PASSWORD );
    
    ip_addr ip, netmask;
    IP4_ADDR( &ip, 192, 168, ( _chipId & 0xFF ), 1);
    IP4_ADDR( &netmask, 255, 255, 255, 0);
    
    ip_info ipInfo;
    ipInfo.ip = ip;
    ipInfo.gw = ip;
    ipInfo.netmask = netmask;
    if ( !wifi_set_ip_info( SOFTAP_IF, &ipInfo ) ) {
        Serial.printf("wifi_set_ip_info() failed\n");
    }
    
    Serial.printf("Starting AP with SSID=%s IP=%d.%d.%d.%d GW=%d.%d.%d.%d NM=%d.%d.%d.%d\n",
                  _mySSID.c_str(),
                  IP2STR( &ipInfo.ip ),
                  IP2STR( &ipInfo.gw ),
                  IP2STR( &ipInfo.netmask ) );
    
    
    softap_config apConfig;
    wifi_softap_get_config( &apConfig );
    
    memset( apConfig.ssid, 0, 32 );
    memset( apConfig.password, 0, 64);
    memcpy( apConfig.ssid, _mySSID.c_str(), _mySSID.length());
    memcpy( apConfig.password, password.c_str(), password.length() );
    apConfig.authmode = AUTH_WPA2_PSK;
    apConfig.ssid_len = _mySSID.length();
    apConfig.beacon_interval = 100;
    apConfig.max_connection = 4; // how many stations can connect to ESP8266 softAP at most.
    
    wifi_softap_set_config(&apConfig);// Set ESP8266 softap config .
    if ( !wifi_softap_dhcps_start() )
        Serial.printf("DHCP server failed\n");
    else
        Serial.printf("DHCP server started\n");
    
    // establish AP tcpServers
    tcpServerInit( _meshServerConn, _meshServerTcp, meshConnectedCb, MESH_PORT );

    tcpServerInit( _webServerConn, _webServerTcp, webServerConnectCb, WEB_PORT );

    tcpServerInit( _webSocketConn, _webSocketTcp, webSocketConnectCb, WEB_SOCKET_PORT );
    
    SPIFFS.begin(); // start file system for webserver
}

//***********************************************************************
void easyMesh::tcpServerInit(espconn &serverConn, esp_tcp &serverTcp, espconn_connect_callback connectCb, uint32 port) {
    serverConn.type = ESPCONN_TCP;
    serverConn.state = ESPCONN_NONE;
    serverConn.proto.tcp = &serverTcp;
    serverConn.proto.tcp->local_port = port;
    espconn_regist_connectcb(&serverConn, connectCb);
    sint8 ret = espconn_accept(&serverConn);
    if ( ret == 0 )
        Serial.printf("AP tcp server established on port %d\n", port );
    else
        Serial.printf("AP tcp server on port %d FAILED ret=%d\n", port, ret);
    
    return;
}

//***********************************************************************
void easyMesh::setWSockRecvCallback( WSOnMessage onMessage ){
    webSocketSetReceiveCallback( onMessage );
}

//***********************************************************************
void easyMesh::setWSockConnectionCallback( WSOnConnection onConnection ){
    webSocketSetConnectionCallback( onConnection );
}