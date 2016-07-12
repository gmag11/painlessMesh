#include <Arduino.h>
#include <ArduinoJson.h>

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
#include "meshWebServer.h"
#include "meshWebSocket.h"


easyMesh* staticThis;
uint16_t  count = 0;

//***********************************************************************
void easyMesh::init( void ) {
  // shut everything down, start with a blank slate.
  staticThis = this;

  wifi_station_set_auto_connect( false );
  wifi_station_disconnect();
  wifi_softap_dhcps_stop();

  // start configuration
  Serial.printf("wifi_set_opmode(STATIONAP_MODE) succeeded? %d\n", wifi_set_opmode( STATIONAP_MODE ) );
  wifi_set_event_handler_cb( wifiEventCb );

  _chipId = system_get_chip_id();
  _mySSID = String( MESH_PREFIX ) + String( _chipId );

  // setup AP
  apInit();
}

//***********************************************************************

void easyMesh::update( void ) {
 // manageStation();
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
}


/***********************************************************************/
bool easyMesh::findBestAP( char *buffer ) {
  if ( staticThis->_meshAPs.empty() ) {           // we need a new scan
    if ( staticThis->scanStatus == SCANNING ) {   // scan in progress, wait until it finishes
      return false;
    }

    // else start scan
    Serial.printf("-->scan started @ %d<--\n", system_get_time());
    if ( !wifi_station_scan(NULL, stationScanCb) ) {
      Serial.printf("wifi_station_scan() failed!?\n");
      return false;
    }
    staticThis->scanStatus = SCANNING;
    return false;
  }

  // if we are here, then we have a list of at least 1 meshAPs. find strongest signal of remaining meshAPs
  SimpleList<bss_info>::iterator bestAP = staticThis->_meshAPs.begin();
  SimpleList<bss_info>::iterator i = staticThis->_meshAPs.begin();
  while ( i != staticThis->_meshAPs.end() ) {
    if ( i->rssi > bestAP->rssi ) {
      bestAP = i;
    }
    ++i;
  }
  strcpy( buffer, (char*)bestAP->ssid );
  Serial.printf("Best AP is %s<---\n", buffer );
  return true;
}

/***********************************************************************/
void easyMesh::manageStation( void ) {
  char bestAP[32];
  uint8 stationStatus = wifi_station_get_connect_status();

  if ( stationStatus == STATION_GOT_IP ) // everything is up and running
    return;

  if ( stationStatus == STATION_IDLE ) {
    if (findBestAP( bestAP ) ) {
      // connect to bestAP
      struct station_config stationConf;
      stationConf.bssid_set = 0;
      memcpy(&stationConf.ssid, bestAP, 32);
      memcpy(&stationConf.password, MESH_PASSWORD, 64);
      wifi_station_set_config(&stationConf);
      wifi_station_connect();
      return;
    }
  }

  if ( stationStatus == 2 || stationStatus == 3 || stationStatus == 4 ) {
    Serial.printf("Wierdness in manageStation() %d\n", stationStatus );
  }
}

/***********************************************************************/
void easyMesh::stationScanCb(void *arg, STATUS status) {
  char ssid[32];
  bss_info *bssInfo = (bss_info *)arg;
  Serial.printf("-- > scan finished @ % d < --\n", system_get_time());

  staticThis->_meshAPs.clear();
  while (bssInfo != NULL) {
    Serial.printf("found : % s, % ddBm", (char*)bssInfo->ssid, (int16_t) bssInfo->rssi );
    if ( strncmp( (char*)bssInfo->ssid, MESH_PREFIX, strlen(MESH_PREFIX) ) == 0 ) {
      Serial.printf(" < -- -");
      staticThis->_meshAPs.push_back( *bssInfo );
    }
    Serial.printf("\n");
    bssInfo = STAILQ_NEXT(bssInfo, next);
  }
  Serial.printf("Found % d nodes with MESH_PREFIX = \"%s\"\n", staticThis->_meshAPs.size(), MESH_PREFIX );

  staticThis->scanStatus = IDLE;
}


/***********************************************************************/
void easyMesh::tcpConnect( void ) {
  struct ip_info ipconfig;
  wifi_get_ip_info(STATION_IF, &ipconfig);

  if ( wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0 ) {
    Serial.printf("\nGot local IP=%d.%d.%d.%d\n", IP2STR(&ipconfig.ip) );
    Serial.printf("Dest IP=%d.%d.%d.%d\n", IP2STR( &ipconfig.gw ) );

    espconn conn;
    esp_tcp tcp;

    conn.type = ESPCONN_TCP;
    conn.state = ESPCONN_NONE;
    conn.proto.tcp = &tcp;
    conn.proto.tcp->local_port = espconn_port();
    conn.proto.tcp->remote_port = 80;
    os_memcpy(conn.proto.tcp->local_ip, &ipconfig.ip, 4);
    os_memcpy(conn.proto.tcp->remote_ip, &ipconfig.gw, 4);

    Serial.printf("conn Print type=%d, state=%d, local_ip=%d.%d.%d.%d, local_port=%d, remote_ip=%d.%d.%d.%d remote_port=%d\n",
                  conn.type,
                  conn.state,
                  IP2STR(conn.proto.tcp->local_ip),
                  conn.proto.tcp->local_port,
                  IP2STR(conn.proto.tcp->remote_ip),
                  conn.proto.tcp->remote_port );

        espconn_regist_connectcb(&conn, meshConnectedCb);
        espconn_regist_recvcb(&conn, meshRecvCb);
        espconn_regist_sentcb(&conn, meshSentCb);
        espconn_regist_reconcb(&conn, meshReconCb);
        espconn_regist_disconcb(&conn, meshDisconCb);
    
    sint8  errCode = espconn_connect(&conn);
    if ( errCode != 0 ) {
      Serial.printf("espconn_connect() falied=%d\n", errCode );
    }
  }
  else {
    Serial.printf("ERR: Something un expected in tcpConnect()\n");
  }
  Serial.printf("leaving tcpConnect()\n");
}

/***********************************************************************/
void easyMesh::tcpServerInit(espconn & serverConn, esp_tcp & serverTcp, espconn_connect_callback connectCb, uint32 port) {
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

String easyMesh::buildMeshPackage( uint32_t localDestId, uint32_t finalDestId, String & msg ) {
  Serial.printf("In buildMeshPackage()\n");

  StaticJsonBuffer<200> jsonBuffer;
  char sendBuffer[200];

  JsonObject& root = jsonBuffer.createObject();
  root["from"] = _chipId;
  root["localDest"] = localDestId;
  root["finalDest"] = finalDestId;
  root["msg"] = msg;

  root.printTo( sendBuffer, sizeof( sendBuffer ) );

  return String( sendBuffer );
}

/***********************************************************************/
bool easyMesh::sendMessage( uint32_t finalDestId, String & msg ) {
  Serial.printf("In sendMessage()\n");

  String package = buildMeshPackage( finalDestId, finalDestId, msg );

  return sendPackage( findConnection( finalDestId ), package );
}

/***********************************************************************/
bool easyMesh::sendHandshake( meshConnection_t *connection ) {
  Serial.printf("In sendHandshake()\n");
  
  String handshakeMsg("Handshake Msg");
  String package = buildMeshPackage( 0, 0, handshakeMsg );

  return sendPackage( connection, package );
}


/***********************************************************************/
bool easyMesh::sendPackage( meshConnection_t *connection, String & package ) {
  Serial.printf("Sending package=%s<--\n", package.c_str() );

  sint8 errCode = espconn_send( connection->esp_conn, (uint8*)package.c_str(), package.length() );

  if ( errCode == 0 )
    return true;
  else {
    Serial.printf("espconn_send Failed err=%d\n", errCode );
    return false;
  }
}
/***********************************************************************/

meshConnection_t* easyMesh::findConnection( uint32_t chipId ) {
  Serial.printf("In findConnection()\n");
  
  SimpleList<meshConnection_t>::iterator connection = _connections.begin();
  while ( connection != _connections.end() ) {
    if ( connection->chipId != chipId )
      return connection;
  }
  return NULL;
}
/***********************************************************************/
void easyMesh::meshConnectedCb(void *arg) {
  Serial.printf("new meshConnection !!!\n");
  meshConnection_t newConn;
  newConn.esp_conn = (espconn *)arg;
  //  struct espconn *newConn = (espconn *)arg;
  staticThis->_connections.push_back( newConn );

  Serial.printf("new meshConnection !!!\n");

  espconn_regist_recvcb(newConn.esp_conn, meshRecvCb);
  espconn_regist_sentcb(newConn.esp_conn, meshSentCb);
  espconn_regist_reconcb(newConn.esp_conn, meshReconCb);
  espconn_regist_disconcb(newConn.esp_conn, meshDisconCb);

  staticThis->sendHandshake( &newConn );
}

/***********************************************************************/
void easyMesh::meshRecvCb(void *arg, char *packageData, unsigned short length) {
  Serial.printf("In meshRecvCb recvd-->%s<--", packageData);
  struct espconn *receiveConn = (espconn *)arg;
} 

/***********************************************************************/
void easyMesh::meshSentCb(void *arg) {
  //data sent successfully
  Serial.printf("In meshSentCb\r\n");
  //  struct espconn *requestconn = (espconn *)arg;
  //  espconn_disconnect( requestconn );
}

/***********************************************************************/
void easyMesh::meshDisconCb(void *arg) {
  Serial.printf("In meshDisconCb\n");
}

/***********************************************************************/
void easyMesh::meshReconCb(void *arg, sint8 err) {
  Serial.printf("In meshReconCb err=%d\n", err );
}

/***********************************************************************/
void easyMesh::wifiEventCb(System_Event_t *event) {
  switch (event->event) {
    case EVENT_STAMODE_CONNECTED:
      Serial.printf("Event: EVENT_STAMODE_CONNECTED ssid=%s\n", (char*)event->event_info.connected.ssid );
      break;
    case EVENT_STAMODE_DISCONNECTED:
      Serial.printf("Event: EVENT_STAMODE_DISCONNECTED\n");
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


