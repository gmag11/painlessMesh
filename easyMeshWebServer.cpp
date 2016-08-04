#include <Arduino.h>

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#include "easyMesh.h"
#include "easyMeshWebServer.h"
#include "FS.h"

espconn webServerConn;
esp_tcp webServerTcp;


//***********************************************************************
void webServerInit( void ) {
    webServerConn.type = ESPCONN_TCP;
    webServerConn.state = ESPCONN_NONE;
    webServerConn.proto.tcp = &webServerTcp;
    webServerConn.proto.tcp->local_port = WEB_PORT;
    espconn_regist_connectcb(&webServerConn, webServerConnectCb);
    sint8 ret = espconn_accept(&webServerConn);
    
    SPIFFS.begin(); // start file system for webserver
    
    if ( ret == 0 )
        meshPrintDebug("web server established on port %d\n", WEB_PORT );
    else
        meshPrintDebug("web server on port %d FAILED ret=%d\n", WEB_PORT, ret);
    
    return;
}

//***********************************************************************
void webServerConnectCb(void *arg) {
  struct espconn *newConn = (espconn *)arg;
//  webConnections.push_back( newConn );

//  DEBUG_MSG("web Server received connection !!!\n");

  espconn_regist_recvcb(newConn, webServerRecvCb);
  espconn_regist_sentcb(newConn, webServerSentCb);
  espconn_regist_reconcb(newConn, webServerReconCb);
  espconn_regist_disconcb(newConn, webServerDisconCb);
}

/***********************************************************************/
void webServerRecvCb(void *arg, char *data, unsigned short length) {
    //received some data from webServer connection
    String request( data );
    
    struct espconn *activeConn = (espconn *)arg;
 /*   meshPrintDebug("webServer recv length=%d--->\n", length);
    int i;
    for ( i = 0 ; i < length ; i++ ) {
        meshPrintDebug("%c", data[i] ); //request.c_str());
    }
    meshPrintDebug("\n<---recv i=%d--->\n", i);
  */
    
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
    uint16_t msgLength = 0;
    
    String header;
    String extension = path.substring( path.lastIndexOf(".") + 1 );
    
    File f = SPIFFS.open( path, "r" );
    if ( !f ) {
        meshPrintDebug("webServerRecvCb(): file not found ->%s\n", path.c_str());
        msg = "File-->" + path + "<-- not found - this should be 404\n";
    }
    else {
        meshPrintDebug("path=%s\n", path.c_str() );
        while ( f.available() ) {
            ch = f.read();
            msg.concat( ch );
            msgLength++;
        }
        
        meshPrintDebug("msgLength=%d extention=%s\n", msgLength, extension.c_str() );
        
        header += "HTTP/1.0 200 OK \n";
        header += "Server: SimpleHTTP/0.6 Python/2.7.10\n";
        header += "Date: Wed, 03 Aug 2016 16:58:45 GMT\n";
  
        if ( extension.equals("html") )
            header += "Content-Type: text/html\n";
        else if ( extension.equals("css") )
            header += "Content-Type: text/css\n";
        else if ( extension.equals("js") )
            header += "Content-Type: application/javascript\n";
        else if ( extension.equals("png") )
            header += "Content-Type: image/png\n";
        else if ( extension.equals("jpg") )
            header += "Content-Type: image/jpeg\n";
        else if ( extension.equals("gif") )
            header += "Content-Type: image/gif\n";
        else if ( extension.equals("ico") )
            header += "Content-Type: image/x-icon\n";
        else
            meshPrintDebug("webServerRecvCb(): Wierd file type. path=%s", path.c_str());
        
        header += "Content-Length: ";
        header += msgLength;
        header += "\n\n";

        msg = header + msg;
    }
    
/*    meshPrintDebug("msg=\n" );
    for ( i = 0 ; i < msg.length() ; i++ ) {
        meshPrintDebug("%c", msg.charAt(i) ); //request.c_str());
    }
    meshPrintDebug("<---msg\n" );
  */
    
    espconn_send(activeConn, (uint8*)msg.c_str(), msg.length());
}

/***********************************************************************/
void webServerSentCb(void *arg) {
  //data sent successfully
//  DEBUG_MSG("webServer sent cb \r\n");
  struct espconn *requestconn = (espconn *)arg;
  espconn_disconnect( requestconn );
}

/***********************************************************************/
void webServerDisconCb(void *arg) {
//  DEBUG_MSG("In webServer_server_discon_cb\n");
}

/***********************************************************************/
void webServerReconCb(void *arg, sint8 err) {
//  DEBUG_MSG("In webServer_server_recon_cb err=%d\n", err );
}








