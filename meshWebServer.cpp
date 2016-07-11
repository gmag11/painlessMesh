#include <Arduino.h>
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

#include "meshWebServer.h"
#include "FS.h"

SimpleList<espconn*> webConnections;
uint8_t webCount;

void webServerConnectCb(void *arg) {
  struct espconn *newConn = (espconn *)arg;
  webConnections.push_back( newConn );

//  Serial.printf("meshWebServer received connection !!!\n");

  espconn_regist_recvcb(newConn, webServerRecvCb);
  espconn_regist_sentcb(newConn, webServerSentCb);
  espconn_regist_reconcb(newConn, webServerReconCb);
  espconn_regist_disconcb(newConn, webServerDisconCb);
}

/***********************************************************************/
void webServerRecvCb(void *arg, char *data, unsigned short length) {
  //received some data from webServer connection
  String request( data );
  
//  Serial.printf("In webServer_server_recv_cb count=%d\n", webCount);
  struct espconn *activeConn = (espconn *)arg;
//  Serial.printf("webServer recv"); //--->%s<----\n", request.c_str());

  String get("GET ");
  String path; 

  if ( request.startsWith( get ) ) {
    uint16_t endFileIndex = request.indexOf(" HTTP");
    path = request.substring( get.length(), endFileIndex );
    if( path.equals("/") )
      path = "/index.html";
  }

  String msg = "";
  char ch;

  File f = SPIFFS.open( path, "r" );
  if ( !f ) {
    msg = "File-->" + path + "<-- not found\n";
  }
  else {
//  Serial.printf("path=%s\n", path.c_str() );
    while ( f.available() ) {
      ch = f.read();
      msg.concat( ch );
    }
  }

  //Serial.printf("msg=%s<---\n", msg.c_str() );
    
  espconn_send(activeConn, (uint8*)msg.c_str(), msg.length());
}

/***********************************************************************/
void webServerSentCb(void *arg) {
  //data sent successfully
//  Serial.printf("webServer sent cb \r\n");
  struct espconn *requestconn = (espconn *)arg;
  espconn_disconnect( requestconn );
}

/***********************************************************************/
void webServerDisconCb(void *arg) {
//  Serial.printf("In webServer_server_discon_cb\n");
}

/***********************************************************************/
void webServerReconCb(void *arg, sint8 err) {
//  Serial.printf("In webServer_server_recon_cb err=%d\n", err );
}








