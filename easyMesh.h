#ifndef   _EASY_MESH_H_
#define   _EASY_MESH_H_

#include <Arduino.h>
#include <SimpleList.h>

#include "meshSync.h"
#include "easyMeshWebSocket.h"

#define MESH_PREFIX         "mesh"
#define MESH_PASSWORD       "bootyboo"
#define MESH_PORT           4444

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

enum meshPackageType {
    HANDSHAKE               = 0,
    MESH_SYNC_REQUEST       = 1,
    MESH_SYNC_REPLY         = 2,
    TIME_SYNC               = 3,
    CONTROL                 = 4
};


struct meshConnection_t {
    espconn         *esp_conn;
    uint32_t        chipId = 0;
    String          subConnections;
    timeSync        time;
};



class easyMesh {
public:
    void                init( void );
    nodeStatusType      update( void );
    void                manageStation( void );
    void                setWSockRecvCallback( WSOnMessage onMessage );
    void                setWSockConnectionCallback( WSOnConnection onConnection );
    
    //must be accessable from callback
    bool                sendMessage( uint32_t finalDestId, meshPackageType type, String &msg );
    bool                sendMessage( uint32_t finalDestId, meshPackageType type, const char *msg );
    bool                broadcastMessage( meshPackageType type, const char *msg );
    void                tcpConnect( void );
    bool                connectToBestAP( void );
    meshConnection_t*   findConnection( espconn *conn );
    void                cleanDeadConnections( void );
    void                handleHandShake( meshConnection_t *conn, JsonObject& root );
    void                handleMeshSync( meshConnection_t *conn, JsonObject& root );
    void                handleControl( meshConnection_t *conn, JsonObject& root );
    void                handleTimeSync( meshConnection_t *conn, JsonObject& root );
    String              subConnectionJson( meshConnection_t *thisConn );
    void                startTimeSync( meshConnection_t *conn );
    uint16_t            jsonSubConnCount( String& subConns );
    void                setStatus( nodeStatusType newStatus );
    uint32              getChipId( void ) { return _chipId;};
    
    // should be prototected, but public for debugging
    scanStatusType                  _scanStatus = IDLE;
    nodeStatusType                  _nodeStatus = INITIALIZING;
    SimpleList<bss_info>            _meshAPs;
    SimpleList<meshConnection_t>    _connections;
    
    
protected:
    void    apInit( void );
    void    stationInit( void );
    void    tcpServerInit(espconn &serverConn, esp_tcp &serverTcp, espconn_connect_callback connectCb, uint32 port);
    
    bool    stationConnect( void );
    void    startStationScan( void );
    
    // callbacks
    static void wifiEventCb(System_Event_t *event);
    static void meshConnectedCb(void *arg);
    static void meshSentCb(void *arg);
    static void meshRecvCb(void *arg, char *data, unsigned short length);
    static void meshDisconCb(void *arg);
    static void meshReconCb(void *arg, sint8 err);
    static void stationScanCb(void *arg, STATUS status);
    static void scanTimerCallback( void *arg );
    static void meshSyncCallback( void *arg );
    
    meshConnection_t* findConnection( uint32_t chipId );
    
    String buildMeshPackage( uint32_t localDestId, uint32_t finalDestId, meshPackageType type, String &msg );
    bool sendPackage( meshConnection_t *connection, String &package );
    
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

