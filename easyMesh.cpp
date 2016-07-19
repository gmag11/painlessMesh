#include <Arduino.h>
#include <ArduinoJson.h>
#include <SimpleList.h>

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

#include "FS.h"
#include "easyMesh.h"
#include "meshSync.h"
#include "meshWebServer.h"
#include "meshWebSocket.h"


easyMesh* staticThis;
uint16_t  count = 0;

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
void easyMesh::update( void ) {
    manageStation();
}

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
void easyMesh::stationInit( void ) {
    startStationScan();
    return;
}

//***********************************************************************
void easyMesh::manageStation( void ) {
    char bestAP[32];
    uint8 stationStatus = wifi_station_get_connect_status();
    
    if ( stationStatus == STATION_GOT_IP ) // everything is up and running
        return;
    
    if ( stationStatus == STATION_IDLE ) {
    }
    
    if ( stationStatus == 2 || stationStatus == 3 || stationStatus == 4 ) {
        Serial.printf("Wierdness in manageStation() %d\n", stationStatus );
    }
}

//***********************************************************************
void easyMesh::setWSockRecvCallback( WSOnMessage onMessage ){
    webSocketSetReceiveCallback( onMessage );
}

//***********************************************************************
void easyMesh::startStationScan( void ) {
    if ( scanStatus != IDLE ) {
        return;
    }
    
    if ( !wifi_station_scan(NULL, stationScanCb) ) {
        Serial.printf("wifi_station_scan() failed!?\n");
        return;
    }
    scanStatus = SCANNING;
    Serial.printf("-->scan started @ %d<--\n", system_get_time());
    return;
}

//***********************************************************************
void easyMesh::scanTimerCallback( void *arg ) {
    staticThis->startStationScan();
}

//***********************************************************************
void easyMesh::stationScanCb(void *arg, STATUS status) {
    char ssid[32];
    bss_info *bssInfo = (bss_info *)arg;
    Serial.printf("-- > scan finished @ % d < --\n", system_get_time());
    staticThis->scanStatus = IDLE;
    
    staticThis->_meshAPs.clear();
    while (bssInfo != NULL) {
        Serial.printf("found : % s, % ddBm", (char*)bssInfo->ssid, (int16_t) bssInfo->rssi );
        if ( strncmp( (char*)bssInfo->ssid, MESH_PREFIX, strlen(MESH_PREFIX) ) == 0 ) {
            Serial.printf(" < ---");
            staticThis->_meshAPs.push_back( *bssInfo );
        }
        Serial.printf("\n");
        bssInfo = STAILQ_NEXT(bssInfo, next);
    }
    Serial.printf("Found % d nodes with MESH_PREFIX = \"%s\"\n", staticThis->_meshAPs.size(), MESH_PREFIX );
    
    staticThis->connectToBestAP();
}

//***********************************************************************
bool easyMesh::connectToBestAP( void ) {
    
    SimpleList<meshConnection_t>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        SimpleList<bss_info>::iterator ap = _meshAPs.begin();
        while( ap != _meshAPs.end() ) {
            String apChipId = (char*)ap->ssid + strlen( MESH_PREFIX);
            Serial.printf("connectToBestAP: sort - ssid=%s, apChipId=%s", ap->ssid, apChipId.c_str());
            
            
            if ( apChipId.toInt() == connection->chipId ) {
                ap = _meshAPs.erase( ap );
                Serial.printf("<--already connected\n");
            }
            else {
                ap++;
                Serial.print("\n");
            }
        }
        connection++;
    }
    
    uint8 statusCode = wifi_station_get_connect_status();
    if ( statusCode != STATION_IDLE ) {
        Serial.printf("connectToBestAP(): station not idle.  code=%d\n", statusCode);
        return false;
    }
    
    if ( staticThis->_meshAPs.empty() ) {  // no meshNodes left in most recent scan
        Serial.printf("connectToBestAP(): no nodes left in list\n");
        // wait 5 seconds and rescan;
        os_timer_setfn( &_scanTimer, scanTimerCallback, NULL );
        os_timer_arm( &_scanTimer, SCAN_INTERVAL, 0 );
        return false;
    }
    
    // if we are here, then we have a list of at least 1 meshAPs.
    // find strongest signal of remaining meshAPs... that is not already connected to our AP.
    SimpleList<bss_info>::iterator bestAP = staticThis->_meshAPs.begin();
    SimpleList<bss_info>::iterator i = staticThis->_meshAPs.begin();
    while ( i != staticThis->_meshAPs.end() ) {
        if ( i->rssi > bestAP->rssi ) {
            bestAP = i;
        }
        ++i;
    }
    
    // connect to bestAP
    Serial.printf("connectToBestAP(): Best AP is %s<---\n", (char*)bestAP->ssid );
    struct station_config stationConf;
    stationConf.bssid_set = 0;
    memcpy(&stationConf.ssid, bestAP->ssid, 32);
    memcpy(&stationConf.password, MESH_PASSWORD, 64);
    wifi_station_set_config(&stationConf);
    wifi_station_connect();
    
    _meshAPs.erase( bestAP );    // drop bestAP from mesh list, so if doesn't work out, we can try the next one
    return true;
}

//***********************************************************************
void easyMesh::tcpConnect( void ) {
    struct ip_info ipconfig;
    wifi_get_ip_info(STATION_IF, &ipconfig);
    
    if ( wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0 ) {
        Serial.printf("Got local IP=%d.%d.%d.%d\n", IP2STR(&ipconfig.ip) );
        Serial.printf("Dest IP=%d.%d.%d.%d\n", IP2STR( &ipconfig.gw ) );
        
        _stationConn.type = ESPCONN_TCP;
        _stationConn.state = ESPCONN_NONE;
        _stationConn.proto.tcp = &_stationTcp;
        _stationConn.proto.tcp->local_port = espconn_port();
        _stationConn.proto.tcp->remote_port = MESH_PORT;
        os_memcpy(_stationConn.proto.tcp->local_ip, &ipconfig.ip, 4);
        os_memcpy(_stationConn.proto.tcp->remote_ip, &ipconfig.gw, 4);
        
        Serial.printf("conn Print type=%d, state=%d, local_ip=%d.%d.%d.%d, local_port=%d, remote_ip=%d.%d.%d.%d remote_port=%d\n",
                      _stationConn.type,
                      _stationConn.state,
                      IP2STR(_stationConn.proto.tcp->local_ip),
                      _stationConn.proto.tcp->local_port,
                      IP2STR(_stationConn.proto.tcp->remote_ip),
                      _stationConn.proto.tcp->remote_port );
        
        espconn_regist_connectcb(&_stationConn, meshConnectedCb);
        espconn_regist_recvcb(&_stationConn, meshRecvCb);
        espconn_regist_sentcb(&_stationConn, meshSentCb);
        espconn_regist_reconcb(&_stationConn, meshReconCb);
        espconn_regist_disconcb(&_stationConn, meshDisconCb);
        
        sint8  errCode = espconn_connect(&_stationConn);
        if ( errCode != 0 ) {
            Serial.printf("espconn_connect() falied=%d\n", errCode );
        }
    }
    else {
        Serial.printf("ERR: Something un expected in tcpConnect()\n");
    }
    Serial.printf("leaving tcpConnect()\n");
}

//***********************************************************************
bool easyMesh::sendMessage( uint32_t finalDestId, meshPackageType type, String &msg ) {
    //  Serial.printf("In sendMessage()\n");
    
    String package = buildMeshPackage( finalDestId, finalDestId, type, msg );
    
    return sendPackage( findConnection( finalDestId ), package );
}

//***********************************************************************
bool easyMesh::sendMessage( uint32_t finalDestId, meshPackageType type, const char *msg ) {
    String strMsg(msg);
    return sendMessage( finalDestId, type, strMsg );
}

//***********************************************************************
String easyMesh::buildMeshPackage( uint32_t localDestId, uint32_t finalDestId, meshPackageType type, String &msg ) {
    //    Serial.printf("In buildMeshPackage()\n");
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonObject& root = jsonBuffer.createObject();
    root["from"] = _chipId;
    root["localDest"] = localDestId;
    root["finalDest"] = finalDestId;
    root["type"] = (uint8_t)type;
    
    switch( type ) {
        case MESH_SYNC_REQUEST:
        case MESH_SYNC_REPLY:
        {
            Serial.printf("In buildMeshPackage(): msg=%s\n", msg.c_str() );
            JsonArray& subs = jsonBuffer.parseArray( msg );
            if ( !subs.success() ) {
                Serial.printf("buildMeshPackage(): subs = jsonBuffer.parseArray( msg ) failed!");
            }
            root["subs"] = subs;
            break;
        }
        case TIME_SYNC:
            root["timeStamp"] = jsonBuffer.parseObject( msg );
            break;
            
        default:
            root["msg"] = msg;
    }
    
    String ret;
    root.printTo( ret );
    return ret;
}

//***********************************************************************
bool easyMesh::sendPackage( meshConnection_t *connection, String &package ) {
    Serial.printf("Sending package-->%s<--\n", package.c_str() );
    
    sint8 errCode = espconn_send( connection->esp_conn, (uint8*)package.c_str(), package.length() );
    
    if ( errCode == 0 ) {
        // Serial.printf("espconn_send Suceeded\n");
        return true;
    }
    else {
        Serial.printf("espconn_send Failed err=%d\n", errCode );
        return false;
    }
}

//***********************************************************************
meshConnection_t* easyMesh::findConnection( uint32_t chipId ) {
    //    Serial.printf("In findConnection(chipId)\n");
    
    SimpleList<meshConnection_t>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection->chipId == chipId )
            return connection;
        connection++;
    }
    Serial.printf("findConnection(%d): did not find connection\n", chipId );
    return NULL;
}

//***********************************************************************
meshConnection_t* easyMesh::findConnection( espconn *conn ) {
    //    Serial.printf("In findConnection(esp_conn) conn=0x%x\n", conn );
    
    int i=0;
    
    SimpleList<meshConnection_t>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        if ( connection->esp_conn == conn ) {
            return connection;
        }
        connection++;
    }
    
    Serial.printf("findConnection(espconn) Failed");
    return NULL;
}

//***********************************************************************
void easyMesh::cleanDeadConnections( void ) {
    //Serial.printf("In cleanDeadConnections() size=%d\n", _connections.size() );
    
    int i=0;
    
    SimpleList<meshConnection_t>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        /*Serial.printf("i=%d esp_conn=0x%x type=%d state=%d\n",
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
        
        i++;
    }
    return;
}

//***********************************************************************
void easyMesh::meshConnectedCb(void *arg) {
    Serial.printf("new meshConnection !!!\n");
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
    Serial.printf("In meshRecvCb recvd*-->%s<--*\n", data);
    meshConnection_t *receiveConn = staticThis->findConnection( (espconn *)arg );
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonObject& root = jsonBuffer.parseObject( data );
    if (!root.success()) {   // Test if parsing succeeded.
        Serial.printf("meshRecvCb: parseObject() failed. data=%s<--\n", data);
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
            Serial.printf("meshRecvCb(): unexpected json root[\"type\"]=%d", (int)root["type"]);
    }
    return;
}

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
    }
    else if ( msg == "AP Handshake") {  // add AP chipId to connection
        Serial.printf("handleHandShake: Got AP Handshake\n");
        
        // check to make sure we are not already connected
        if ( staticThis->findConnection( remoteChipId ) != NULL ) {  //drop this connection
            Serial.printf("We are already connected to this node as AP.  Drop new connection\n");
            espconn_disconnect( conn->esp_conn );
            return;
        }
        // else
        conn->chipId = remoteChipId;
    }
    else {
        Serial.printf("handleHandShake(): Weird msg\n");
    }
}

//***********************************************************************
void easyMesh::handleMeshSync( meshConnection_t *conn, JsonObject& root ) {
    Serial.printf("handleMeshSync(): type=%d\n", (int)root["type"] );
    
    String subs = root["subs"];
    conn->subConnections = subs;
    Serial.printf("subs=%s\n", conn->subConnections.c_str());
    
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

//***********************************************************************
void easyMesh::handleControl( meshConnection_t *conn, JsonObject& root ) {
    Serial.printf("handleControl():\n");
}

//***********************************************************************
void easyMesh::meshSentCb(void *arg) {
    Serial.printf("In meshSentCb\r\n");    //data sent successfully
}

//***********************************************************************
void easyMesh::meshDisconCb(void *arg) {
    struct espconn *disConn = (espconn *)arg;
    
    Serial.printf("meshDisconCb: ");
    
    // remove this connection from _connections
    staticThis->cleanDeadConnections();
    
    //test to see if this connection was on the STATION interface by checking the local port
    if ( disConn->proto.tcp->local_port == MESH_PORT ) {
        Serial.printf("AP connection.  All good! local_port=%d\n", disConn->proto.tcp->local_port);
    }
    else {
        Serial.printf("Station Connection! Find new node. local_port=%d\n", disConn->proto.tcp->local_port);
        wifi_station_disconnect();
    }
    
    return;
}

//***********************************************************************
void easyMesh::meshReconCb(void *arg, sint8 err) {
    Serial.printf("In meshReconCb err=%d\n", err );
}

//***********************************************************************
void easyMesh::wifiEventCb(System_Event_t *event) {
    switch (event->event) {
        case EVENT_STAMODE_CONNECTED:
            Serial.printf("Event: EVENT_STAMODE_CONNECTED ssid=%s\n", (char*)event->event_info.connected.ssid );
            break;
        case EVENT_STAMODE_DISCONNECTED:
            Serial.printf("Event: EVENT_STAMODE_DISCONNECTED\n");
            staticThis->connectToBestAP();
            break;
        case EVENT_STAMODE_AUTHMODE_CHANGE:
            Serial.printf("Event: EVENT_STAMODE_AUTHMODE_CHANGE\n");
            break;
        case EVENT_STAMODE_GOT_IP:
            Serial.printf("Event: EVENT_STAMODE_GOT_IP\n");
            staticThis->tcpConnect();
            break;
            
        case EVENT_SOFTAPMODE_STACONNECTED:
            Serial.printf("Event: EVENT_SOFTAPMODE_STACONNECTED\n");
            break;
            
        case EVENT_SOFTAPMODE_STADISCONNECTED:
            Serial.printf("Event: EVENT_SOFTAPMODE_STADISCONNECTED\n");
            break;
        case EVENT_STAMODE_DHCP_TIMEOUT:
            Serial.printf("Event: EVENT_STAMODE_DHCP_TIMEOUT\n");
            break;
        case EVENT_SOFTAPMODE_PROBEREQRECVED:
            // Serial.printf("Event: EVENT_SOFTAPMODE_PROBEREQRECVED\n");  // dont need to know about every probe request
            break;
        default:
            Serial.printf("Unexpected WiFi event: %d\n", event->event);
            break;
    }
}


