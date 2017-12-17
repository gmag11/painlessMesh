#ifndef   _EASY_MESH_H_
#define   _EASY_MESH_H_

#define _TASK_STD_FUNCTION

#include <painlessScheduler.h>
#include <Arduino.h>
#include <list>
#include <ArduinoJson.h>
#include <functional>
#include <memory>
using namespace std;
#include "espInterface.h"
#include "painlessTCP.h"

#include "painlessMeshSync.h"
#include "painlessMeshSTA.h"
#include "painlessMeshConnection.h"

#define NODE_TIMEOUT        10*TASK_SECOND
#define MIN_FREE_MEMORY     16000 // Minimum free memory, besides here all packets in queue are discarded.
#define MAX_MESSAGE_QUEUE   50 // MAX number of unsent messages in queue. Newer messages are discarded
#define MAX_CONSECUTIVE_SEND 5 // Max message busrt

enum nodeMode {
    AP_ONLY = WIFI_MODE_AP,
    STA_ONLY = WIFI_MODE_STA,
    STA_AP = WIFI_MODE_APSTA
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

template<typename T>
using SimpleList = std::list<T>; // backward compatibility

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

using ConnectionList = std::list<std::shared_ptr<MeshConnection>>;

typedef std::function<void(uint32_t nodeId)> newConnectionCallback_t;
typedef std::function<void(uint32_t nodeId)> droppedConnectionCallback_t;
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
#ifdef ESP32
    void                init(String ssid, String password, uint16_t port = 5555, enum nodeMode connectMode = STA_AP, wifi_auth_mode_t authmode = WIFI_AUTH_WPA2_PSK, uint8_t channel = 1, uint8_t phymode = WIFI_PROTOCOL_11G, uint8_t maxtpw = 82, uint8_t hidden = 0, uint8_t maxconn = 10);
#else
    void                init(String ssid, String password, uint16_t port = 5555, enum nodeMode connectMode = STA_AP, wifi_auth_mode_t authmode = WIFI_AUTH_WPA2_PSK, uint8_t channel = 1, uint8_t phymode = WIFI_PROTOCOL_11G, uint8_t maxtpw = 82, uint8_t hidden = 0, uint8_t maxconn = 4);
#endif
    /**
     * Disconnect and stop this node
     */
    void                stop();
    void                update(void);
    bool                sendSingle(uint32_t &destId, String &msg);
    bool                sendBroadcast(String &msg, bool includeSelf = false);
    bool                startDelayMeas(uint32_t nodeId);

    // in painlessMeshConnection.cpp
    void                onReceive(receivedCallback_t  onReceive);
    void                onNewConnection(newConnectionCallback_t onNewConnection);
    void                onDroppedConnection(droppedConnectionCallback_t onDroppedConnection);
    void                onChangedConnections(changedConnectionsCallback_t onChangedConnections);
    void                onNodeTimeAdjusted(nodeTimeAdjustedCallback_t onTimeAdjusted);
    void                onNodeDelayReceived(nodeDelayCallback_t onDelayReceived);
    String              subConnectionJson() { return subConnectionJson(NULL); }
    bool                isConnected(uint32_t nodeId) { return findConnection(nodeId) != NULL; }
    std::list<uint32_t> getNodeList();

    // in painlessMeshSync.cpp
    uint32_t            getNodeTime(void);

    // in painlessMeshSTA.cpp
    uint32_t            encodeNodeId(uint8_t *hwaddr);
    /// Connect (as a station) to a specified network and ip
    /// You can pass {0,0,0,0} as IP to have it connect to the gateway
    void stationManual(String ssid, String password, uint16_t port = 0,
        uint8_t * remote_ip = NULL);
    bool setHostname(const char * hostname);
    ip4_addr_t getStationIP();

    Scheduler scheduler;
    StationScan stationScan;

    // Rough estimate of the mesh stability (goes from 0-1000)
    size_t stability = 0;

#ifndef UNITY // Make everything public in unit test mode
protected:
#endif
    // in painlessMeshComm.cpp
    //must be accessable from callback
    bool                sendMessage(std::shared_ptr<MeshConnection> conn, uint32_t destId, uint32_t fromId, meshPackageType type, String &msg, bool priority = false);
    bool                sendMessage(uint32_t destId, uint32_t fromId, meshPackageType type, String &msg, bool priority = false);
    bool                broadcastMessage(uint32_t fromId, meshPackageType type, String &msg, std::shared_ptr<MeshConnection> exclude = NULL);

    String              buildMeshPackage(uint32_t destId, uint32_t fromId, meshPackageType type, String &msg);


    // in painlessMeshSync.cpp
    //must be accessable from callback
    void                handleNodeSync(std::shared_ptr<MeshConnection> conn, JsonObject& root);
    void                startTimeSync(std::shared_ptr<MeshConnection> conn);
    void                handleTimeSync(std::shared_ptr<MeshConnection> conn, JsonObject& root, uint32_t receivedAt);
    void                handleTimeDelay(std::shared_ptr<MeshConnection> conn, JsonObject& root, uint32_t receivedAt);
    bool                adoptionCalc(std::shared_ptr<MeshConnection> conn);

    // in painlessMeshConnection.cpp
    //void                cleanDeadConnections(void); // Not implemented. Needed?
    void                tcpConnect(void);
    bool closeConnectionSTA(); 

    void                eraseClosedConnections();

    String              subConnectionJson(std::shared_ptr<MeshConnection> exclude);
    String              subConnectionJsonHelper(
                            ConnectionList &connections,
                            uint32_t exclude = 0);
    size_t              approxNoNodes(); // estimate of numbers of node
    size_t              approxNoNodes(String &subConns); // estimate of numbers of node
    shared_ptr<MeshConnection> findConnection(uint32_t nodeId, uint32_t exclude = 0);
    shared_ptr<MeshConnection> findConnection(TCPClient *conn);

    // in painlessMeshAP.cpp
    void                apInit(void);

    void                tcpServerInit();

    // callbacks
    // in painlessMeshConnection.cpp
    static int         espWifiEventCb(void * ctx, system_event_t *event);

    // Callback functions
    newConnectionCallback_t         newConnectionCallback;
    droppedConnectionCallback_t     droppedConnectionCallback;
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
    wifi_auth_mode_t  _meshAuthMode;
    uint8_t     _meshHidden;
    uint8_t     _meshMaxConn;

    ConnectionList  _connections;

    TCPServer  *_tcpListener;

    bool         _station_got_ip = false;

    Task droppedConnectionTask;
    Task newConnectionTask;

    friend class StationScan;
    friend class MeshConnection;
    friend void onDataCb(void * arg, TCPClient *client, void *data, size_t len);
};

#endif //   _EASY_MESH_H_
