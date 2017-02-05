#ifndef   _EASY_MESH_H_
#define   _EASY_MESH_H_

#ifndef ESP8266
#error Only ESP8266 platform is allowed
#endif // !ESP8266


#include <Arduino.h>
#include <SimpleList.h>
#include <ArduinoJson.h>
#include <functional>
using namespace std;

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}


#include "painlessMeshSync.h"

#define NODE_TIMEOUT        3000000  //uSecs
#define JSON_BUFSIZE        300 // initial size for the DynamicJsonBuffers.
#define MIN_FREE_MEMORY     16000 // Minimum free memory, besides here all packets in queue are discarded.
#define MAX_MESSAGE_QUEUE   50 // MAX number of unsent messages in queue. Newer messages are discarded
#define MAX_CONSECUTIVE_SEND 5 // Max message busrt


enum nodeStatusType {
    INITIALIZING = 0,
    SEARCHING = 1,
    FOUND_MESH = 2,
    CONNECTED = 3
};

enum nodeMode {
    AP_ONLY,
    STA_ONLY,
    STA_AP
};

enum scanStatusType {
    IDLE = 0,
    SCANNING = 1
};

enum syncStatusType {
    NEEDED = 0,
    REQUESTED = 1,
    IN_PROGRESS = 2,
    COMPLETE = 3
};

enum meshPackageType {
    TIME_DELAY = 3,
    TIME_SYNC = 4,
    NODE_SYNC_REQUEST = 5,
    NODE_SYNC_REPLY = 6,
    CONTROL = 7,  //deprecated
    BROADCAST = 8,  //application data for everyone
    SINGLE = 9   //application data for a single node
};

typedef int debugType;

#define ERROR 1
#define STARTUP 1<<1
#define MESH_STATUS 1<<2
#define CONNECTION 1<<3
#define SYNC 1<<4
#define S_TIME 1<<5
#define COMMUNICATION 1<<6
#define GENERAL 1<<7
#define MSG_TYPES 1<<8
#define REMOTE 1<<9  // not yet implemented
#define APPLICATION 1<<10
#define DEBUG 1<<11

struct meshConnectionType {
    espconn             *esp_conn;
    uint32_t            nodeId = 0;
    String              subConnections;
    timeSync            time;
    uint32_t            lastReceived = 0;
    bool                newConnection = true;

    syncStatusType      nodeSyncStatus = NEEDED;
    uint32_t            nodeSyncLastRequested = 0;

    syncStatusType      timeSyncStatus = NEEDED;
    uint32_t            timeSyncLastRequested = 0; // Timestamp to be compared in manageConnections() to check response for timeout
    uint32_t            timeDelayLastRequested = 0; // Timestamp to be compared in manageConnections() to check response for timeout
    uint32_t            lastTimeSync = 0; // Timestamp to trigger periodic time sync
    uint32_t            nextTimeSyncPeriod = 0; // 

    bool                sendReady = true;
    SimpleList<String>  sendQueue;
};

typedef std::function<void(uint32_t nodeId)> newConnectionCallback_t;
typedef std::function<void(uint32_t from, String &msg)> receivedCallback_t;
typedef std::function<void()> changedConnectionsCallback_t;
typedef std::function<void(int32_t offset)> nodeTimeAdjustedCallback_t;
typedef std::function<void(uint32_t nodeId, int32_t delay)> nodeDelayCallback_t;

class painlessMesh {
public:
    //inline functions
    uint32_t            getNodeId(void) { return _nodeId; };

    // in painlessMeshDebug.cpp
    void                setDebugMsgTypes(uint16_t types);
    void                debugMsg(debugType type, const char* format ...);

    // in painlessMesh.cpp
    void                init(String ssid, String password, uint16_t port = 5555, enum nodeMode connectMode = STA_AP, _auth_mode authmode = AUTH_WPA2_PSK, uint8_t channel = 1, phy_mode_t phymode = PHY_MODE_11G, uint8_t maxtpw = 82, uint8_t hidden = 0, uint8_t maxconn = 4);
    void                update(void);
    bool                sendSingle(uint32_t &destId, String &msg);
    bool                sendBroadcast(String &msg);
    bool                startDelayMeas(uint32_t nodeId);

    // in painlessMeshConnection.cpp
    void                onReceive(receivedCallback_t  onReceive);
    void                onNewConnection(newConnectionCallback_t onNewConnection);
    void                onChangedConnections(changedConnectionsCallback_t onChangedConnections);
    void                onNodeTimeAdjusted(nodeTimeAdjustedCallback_t onTimeAdjusted);
    void                onNodeDelayReceived(nodeDelayCallback_t onDelayReceived);
    uint16_t            connectionCount(meshConnectionType *exclude = NULL);
    String              subConnectionJson() { return subConnectionJson(NULL); }
    bool                isConnected(uint32_t nodeId) { return findConnection(nodeId) != NULL; }
    SimpleList<uint32_t> getNodeList();

    // in painlessMeshSync.cpp
    uint32_t            getNodeTime(void);

#ifndef UNITY // Make everything public in unit test mode
protected:
#endif

    // in painlessMeshComm.cpp
    //must be accessable from callback
    bool                sendMessage(meshConnectionType *conn, uint32_t destId, uint32_t fromId, meshPackageType type, String &msg, bool priority = false);
    bool                sendMessage(uint32_t destId, uint32_t fromId, meshPackageType type, String &msg, bool priority = false);
    bool                broadcastMessage(uint32_t fromId, meshPackageType type, String &msg, meshConnectionType *exclude = NULL);

    bool                sendPackage(meshConnectionType *connection, String &package, bool priority = false);
    String              buildMeshPackage(uint32_t destId, uint32_t fromId, meshPackageType type, String &msg);


    // in painlessMeshSync.cpp
    //must be accessable from callback
    void                startNodeSync(meshConnectionType *conn);
    void                handleNodeSync(meshConnectionType *conn, JsonObject& root);
    void                startTimeSync(meshConnectionType *conn, boolean checkAdopt = true);
    void                handleTimeSync(meshConnectionType *conn, JsonObject& root, uint32_t receivedAt);
    void                handleTimeDelay(meshConnectionType *conn, JsonObject& root, uint32_t receivedAt);
    bool                adoptionCalc(meshConnectionType *conn);

    // in painlessMeshConnection.cpp
    void                manageConnections(void);
    meshConnectionType* findConnection(uint32_t nodeId);
    meshConnectionType* findConnection(espconn *conn);
    //void                cleanDeadConnections(void); // Not implemented. Needed?
    void                tcpConnect(void);
    bool                connectToBestAP(void);
    uint16_t            jsonSubConnCount(String& subConns);
    meshConnectionType* closeConnection(meshConnectionType *conn);
    String              subConnectionJson(meshConnectionType *exclude);


    // in painlessMeshSTA.cpp
    void                manageStation(void);
    static void         stationScanCb(void *arg, STATUS status);
    static void         scanTimerCallback(void *arg);
    void                stationInit(void);
    //bool                stationConnect(void); // Not implemented. Needed?
    void                startStationScan(void);
    uint32_t            encodeNodeId(uint8_t *hwaddr);

    // in painlessMeshAP.cpp
    void                apInit(void);
    void                tcpServerInit(espconn &serverConn, esp_tcp &serverTcp, espconn_connect_callback connectCb, uint32 port);

    // callbacks
    // in painlessMeshConnection.cpp
    static void         wifiEventCb(System_Event_t *event);
    static void         meshConnectedCb(void *arg);
    static void         meshSentCb(void *arg);
    static void         meshRecvCb(void *arg, char *data, unsigned short length);
    static void         meshDisconCb(void *arg);
    static void         meshReconCb(void *arg, sint8 err);

    // Callback functions
    newConnectionCallback_t         newConnectionCallback;
    receivedCallback_t              receivedCallback;
    changedConnectionsCallback_t    changedConnectionsCallback;
    nodeTimeAdjustedCallback_t      nodeTimeAdjustedCallback;
    nodeDelayCallback_t             nodeDelayReceivedCallback;

    // variables
    uint32_t    _nodeId;
    String      _meshSSID;
    String      _meshPassword;
    uint16_t    _meshPort;
    uint8_t     _meshChannel;
    _auth_mode  _meshAuthMode;
    uint8_t     _meshHidden;
    uint8_t     _meshMaxConn;

    scanStatusType                  _scanStatus = IDLE; // STA scanning status
    nodeStatusType                  _nodeStatus = INITIALIZING;
    SimpleList<bss_info>            _meshAPs;
    SimpleList<meshConnectionType>  _connections;

    os_timer_t  _scanTimer;

    espconn     _meshServerConn;
    esp_tcp     _meshServerTcp;

    espconn     _stationConn;
    esp_tcp     _stationTcp;
};


#endif //   _EASY_MESH_H_
