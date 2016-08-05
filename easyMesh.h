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
//#include "easyMeshWebSocket.h"

#define MESH_PREFIX         "mesh"
#define MESH_PASSWORD       "bootyboo"
#define MESH_PORT           4444
#define NODE_TIMEOUT        3000000  //uSecs

#define JSON_BUFSIZE        300 // initial size for the DynamicJsonBuffers.

//#define DEBUG_MSG( format, ...) Serial.printf( (format), ##__VA_ARGS__)
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
    TIME_SYNC               = 4,
    NODE_SYNC_REQUEST       = 5,
    NODE_SYNC_REPLY         = 6,
    CONTROL                 = 7,  //deprecated
    BROADCAST               = 8,  //application data for everyone
    SINGLE                  = 9   //application data for a single node
};


struct meshConnectionType {
    espconn         *esp_conn;
    uint32_t        chipId = 0;
    String          subConnections;
    timeSync        time;
    uint32_t        lastRecieved = 0;
    uint32_t        nodeSyncRequest = 0;
    uint32_t        lastTimeSync = 0;
    bool            needsNodeSync = false;
    bool            needsTimeSync = false;
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
    bool                sendSingle( uint32_t &destId, String &msg );
    bool                sendBroadcast( String &msg );
    void                setReceiveCallback( void(*onReceive)(uint32_t from, String &msg) );
protected:
    
    // in easyMeshComm.cpp
    //must be accessable from callback
    bool                sendMessage( uint32_t fromId, uint32_t destId, meshPackageType type, String &msg );
    bool                sendMessage( uint32_t destId, meshPackageType type, String &msg );
    bool                broadcastMessage( uint32_t fromId, meshPackageType type, String &msg, meshConnectionType *exclude = NULL );
    
    // in easyMeshSync.cpp
    //must be accessable from callback
    void                handleNodeSync( meshConnectionType *conn, JsonObject& root );
    void                handleTimeSync( meshConnectionType *conn, JsonObject& root );
    void                startTimeSync( meshConnectionType *conn );

    // in easyMeshConnection.cpp
    String              subConnectionJson( meshConnectionType *exclude );
    meshConnectionType* findConnection( uint32_t chipId );
    meshConnectionType* findConnection( espconn *conn );
    void                cleanDeadConnections( void );
    void                tcpConnect( void );
    bool                connectToBestAP( void );
    uint16_t            jsonSubConnCount( String& subConns );


    // in easyMeshSTA.cpp
    void                manageStation( void );

public:
    uint16_t            connectionCount( meshConnectionType *exclude );

    // in easyMeshAP.cpp

    // should be prototected, but public for debugging
    scanStatusType                  _scanStatus = IDLE;
    nodeStatusType                  _nodeStatus = INITIALIZING;
    SimpleList<bss_info>            _meshAPs;
    SimpleList<meshConnectionType>  _connections;
    
    
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
    
    // in easyMeshSync.cpp
    bool sendPackage( meshConnectionType *connection, String &package );
    String buildMeshPackage( uint32_t fromId, uint32_t destId, meshPackageType type, String &msg );
    
    uint32_t    _chipId;
    String      _mySSID;
    
    os_timer_t  _scanTimer;
    
    espconn     _meshServerConn;
    esp_tcp     _meshServerTcp;
    
    espconn     _stationConn;
    esp_tcp     _stationTcp;
    
};


#endif //   _EASY_MESH_H_

