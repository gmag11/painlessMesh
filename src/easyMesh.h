#ifndef   _EASY_MESH_H_
#define   _EASY_MESH_H_

#include <Arduino.h>
#include <SimpleList.h>
#include <ArduinoJson.h>

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#include "easyMeshSync.h"

//#define MESH_PREFIX         "mesh"
//#define MESH_PASSWORD       "bootyboo"
//#define MESH_PORT           4444
#define NODE_TIMEOUT        3000000  //uSecs

#define JSON_BUFSIZE        300 // initial size for the DynamicJsonBuffers.


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

enum syncStatusType {
    NEEDED          = 0,
    REQUESTED       = 1,
    IN_PROGRESS     = 2,
    COMPLETE        = 3
};

enum meshPackageType {
    DROP                    = 3,
    TIME_SYNC               = 4,
    NODE_SYNC_REQUEST       = 5,
    NODE_SYNC_REPLY         = 6,
    CONTROL                 = 7,  //deprecated
    BROADCAST               = 8,  //application data for everyone
    SINGLE                  = 9   //application data for a single node
};



enum debugType {
    ERROR           = 0x0001,
    STARTUP         = 0x0002,
    MESH_STATUS     = 0x0004,
    CONNECTION      = 0x0008,
    SYNC            = 0x0010,
    COMMUNICATION   = 0x0020,
    GENERAL         = 0x0040,
    MSG_TYPES       = 0x0080,
    REMOTE          = 0x0100,  // not yet implemented
    APPLICATION     = 0x0200
    // add types if you like, room for a total of 16 types
};


struct meshConnectionType {
    espconn             *esp_conn;
    uint32_t            chipId = 0;
    String              subConnections;
    timeSync            time;
    uint32_t            lastRecieved = 0;
    bool                newConnection = true;

    syncStatusType      nodeSyncStatus = NEEDED;
    uint32_t            nodeSyncRequest = 0;

    syncStatusType      timeSyncStatus = NEEDED;
    uint32_t            lastTimeSync = 0;
//    bool                needsNodeSync = true;
//    bool                needsTimeSync = false;

    bool                sendReady = true;
    SimpleList<String>  sendQueue;
};


class easyMesh {
public:
    //inline functions
    uint32              getChipId( void ) { return _chipId;};
   
    // in easyMeshDebug.cpp
    void                setDebugMsgTypes( uint16_t types );
    void                debugMsg( debugType type, const char* format ... );
    
    // in easyMesh.cpp
//    void                init( void );
    void                init( String prefix, String password, uint16_t port );

    void                update( void );
    bool                sendSingle( uint32_t &destId, String &msg );
    bool                sendBroadcast( String &msg );
    
    // in easyMeshConnection.cpp
    void                setReceiveCallback( void(*onReceive)(uint32_t from, String &msg) );
    void                setNewConnectionCallback( void(*onNewConnection)(bool adopt) );
    uint16_t            connectionCount( meshConnectionType *exclude = NULL );

    // in easyMeshSync.cpp
    uint32_t            getNodeTime( void );
    
    // should be prototected, but public for debugging
    scanStatusType                  _scanStatus = IDLE;
    nodeStatusType                  _nodeStatus = INITIALIZING;
    SimpleList<bss_info>            _meshAPs;
    SimpleList<meshConnectionType>  _connections;
    
protected:
    
    // in easyMeshComm.cpp
    //must be accessable from callback
    bool                sendMessage( meshConnectionType *conn, uint32_t destId, meshPackageType type, String &msg );
    bool                sendMessage( uint32_t destId, meshPackageType type, String &msg );
    bool                broadcastMessage( uint32_t fromId, meshPackageType type, String &msg, meshConnectionType *exclude = NULL );
    
    bool sendPackage( meshConnectionType *connection, String &package );
    String buildMeshPackage(uint32_t destId, meshPackageType type, String &msg);

    
    // in easyMeshSync.cpp
    //must be accessable from callback
    void                startNodeSync( meshConnectionType *conn );
    void                handleNodeSync( meshConnectionType *conn, JsonObject& root );
    void                startTimeSync( meshConnectionType *conn );
    void                handleTimeSync( meshConnectionType *conn, JsonObject& root );
    bool                adoptionCalc( meshConnectionType *conn );
    
    // in easyMeshConnection.cpp
    void                manageConnections( void );
    String              subConnectionJson( meshConnectionType *exclude );
    meshConnectionType* findConnection( uint32_t chipId );
    meshConnectionType* findConnection( espconn *conn );
    void                cleanDeadConnections( void );
    void                tcpConnect( void );
    bool                connectToBestAP( void );
    uint16_t            jsonSubConnCount( String& subConns );
    meshConnectionType* closeConnection( meshConnectionType *conn );

    // in easyMeshSTA.cpp
    void                manageStation( void );

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
    

    // variables
    uint32_t    _chipId;
    String      _mySSID;
    String      _meshPrefix;
    String      _meshPassword;
    uint16_t    _meshPort;
    
    os_timer_t  _scanTimer;
    
    espconn     _meshServerConn;
    esp_tcp     _meshServerTcp;
    
    espconn     _stationConn;
    esp_tcp     _stationTcp;
};


#endif //   _EASY_MESH_H_

