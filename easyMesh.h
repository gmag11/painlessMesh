#ifndef   _EASY_MESH_H_
#define   _EASY_MESH_H_

#include <stdio.h>
#include <stdarg.h>

#include <Arduino.h>
#include <SimpleList.h>
#include <ArduinoJson.h>

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#include "easyMeshSync.h"
#include "easyMeshWebSocket.h"

#define MESH_PREFIX         "mesh"
#define MESH_PASSWORD       "bootyboo"
#define MESH_PORT           4444

#define JSON_BUFSIZE        300 // initial size for the DynamicJsonBuffers.

#define DEBUG_MSG( format, ...) Serial.printf( (format), ##__VA_ARGS__)
//#define DEBUG_MSG(...) meshPrintDebug(__VA_ARGS__)


enum nodeStatusType {
    INITIALIZING        = 0,
    SEARCHING           = 1,
    FOUND_MESH          = 2,
    CONNECTED           = 3
};

enum scanStatusType {
    IDLE       = 0,
    SCANNING   = 1
};

enum meshPackageType {
    STA_HANDSHAKE           = 0,
    AP_HANDSHAKE            = 1,
    MESH_SYNC_REQUEST       = 2,
    MESH_SYNC_REPLY         = 3,
    TIME_SYNC               = 4,
    CONTROL                 = 5
};


struct meshConnection_t {
    espconn         *esp_conn;
    uint32_t        chipId = 0;
    String          subConnections;
    timeSync        time;
};


void inline   meshPrintDebug( const char* format ... ) {
    char str[200];
    
    va_list args;
    va_start(args, format);

    vsnprintf(str, sizeof(str), format, args);
    Serial.print( str );
//    broadcastWsMessage(str, strlen(str), OPCODE_TEXT);
    
    va_end(args);
}


class easyMesh {
public:
    //inline functions
    uint32              getChipId( void ) { return _chipId;};
   
    
    // in easyMesh.cpp
    void                init( void );
    nodeStatusType      update( void );
    void                setStatus( nodeStatusType newStatus ); //must be accessable from callback
    
    // in easyMeshComm.cpp
    //must be accessable from callback
    bool                sendMessage( uint32_t destId, meshPackageType type, String &msg );
    bool                sendMessage( uint32_t destId, meshPackageType type, const char *msg );
    bool                broadcastMessage( meshPackageType type, const char *msg, meshConnection_t *exclude = NULL );
    
    // in easyMeshSync.cpp
    //must be accessable from callback
    void                handleHandShake( meshConnection_t *conn, JsonObject& root );
    
    void                handleMeshSync( meshConnection_t *conn, JsonObject& root );
    void                handleTimeSync( meshConnection_t *conn, JsonObject& root );
    void                startTimeSync( meshConnection_t *conn );
protected:
    static void         meshSyncCallback( void *arg );
    
public:
    // in easyMeshConnection.cpp
    String              subConnectionJson( meshConnection_t *thisConn );
    meshConnection_t*   findConnection( espconn *conn ); //must be accessable from callback
    void                cleanDeadConnections( void ); //must be accessable from callback
    void                tcpConnect( void );     //must be accessable from callback
    bool                connectToBestAP( void );     //must be accessable from callback
    void                setControlCallback( void(*onControl)(ArduinoJson::JsonObject& control));
    uint16_t            connectionCount( meshConnection_t *exclude );
    uint16_t            jsonSubConnCount( String& subConns );
    
    
    // in easyMeshSTA.cpp
    void                manageStation( void );

    // in easyMeshAP.cpp
    void                setWSockRecvCallback( void (*onMessage)(char *payloadData) );
    void                setWSockConnectionCallback( void (*onConnection)(void) );
    
    // should be prototected, but public for debugging
    scanStatusType                  _scanStatus = IDLE;
    nodeStatusType                  _nodeStatus = INITIALIZING;
    SimpleList<bss_info>            _meshAPs;
    SimpleList<meshConnection_t>    _connections;
    
    
protected:
    // in ?
    static void stationScanCb(void *arg, STATUS status);
    static void scanTimerCallback( void *arg );
    void    stationInit( void );
    bool    stationConnect( void );
    void    startStationScan( void );

    void    apInit( void );
    void    tcpServerInit(espconn &serverConn, esp_tcp &serverTcp, espconn_connect_callback connectCb, uint32 port);
    
    // callbacks
    // in easyMeshConnection.cpp
    static void wifiEventCb(System_Event_t *event);
    static void meshConnectedCb(void *arg);
    static void meshSentCb(void *arg);
    static void meshRecvCb(void *arg, char *data, unsigned short length);
    static void meshDisconCb(void *arg);
    static void meshReconCb(void *arg, sint8 err);
    meshConnection_t* findConnection( uint32_t chipId );
    
    // in easyMeshSync.cpp
    bool sendPackage( meshConnection_t *connection, String &package );
    String buildMeshPackage( uint32_t destId, meshPackageType type, String &msg );
    
    uint32_t    _chipId;
    String      _mySSID;
    
    os_timer_t  _scanTimer;
    os_timer_t  _meshSyncTimer;
    
    espconn     _meshServerConn;
    esp_tcp     _meshServerTcp;
    
    espconn     _webServerConn;
    esp_tcp     _webServerTcp;
    
    espconn     _webSocketConn;
    esp_tcp     _webSocketTcp;
    
    espconn     _stationConn;
    esp_tcp     _stationTcp;
    
};


#endif //   _EASY_MESH_H_

