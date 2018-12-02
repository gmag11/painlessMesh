#ifndef   _EASY_MESH_H_
#define   _EASY_MESH_H_


#define _TASK_PRIORITY // Support for layered scheduling priority
#define _TASK_STD_FUNCTION

#include <TaskSchedulerDeclarations.h>
#include <Arduino.h>
#include <list>
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#include <functional>
#include <memory>
using namespace std;
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif // ESP32

#include "painlessMeshSync.h"
#include "painlessMeshSTA.h"
#include "painlessMeshConnection.h"

#define NODE_TIMEOUT         10*TASK_SECOND
#define MIN_FREE_MEMORY      4000 // Minimum free memory, besides here all packets in queue are discarded.
#define MAX_MESSAGE_QUEUE    50 // MAX number of unsent messages in queue. Newer messages are discarded
#define MAX_CONSECUTIVE_SEND 5 // Max message burst

enum meshPackageType {
    TIME_DELAY        = 3,
    TIME_SYNC         = 4,
    NODE_SYNC_REQUEST = 5,
    NODE_SYNC_REPLY   = 6,
    CONTROL           = 7,  //deprecated
    BROADCAST         = 8,  //application data for everyone
    SINGLE            = 9   //application data for a single node
};

template<typename T>
using SimpleList = std::list<T>; // backward compatibility

typedef enum
{
    ERROR         = 1 << 0,
    STARTUP       = 1 << 1,
    MESH_STATUS   = 1 << 2,
    CONNECTION    = 1 << 3,
    SYNC          = 1 << 4,
    S_TIME        = 1 << 5,
    COMMUNICATION = 1 << 6,
    GENERAL       = 1 << 7,
    MSG_TYPES     = 1 << 8,
    REMOTE        = 1 << 9, // not yet implemented
    APPLICATION   = 1 << 10,
    DEBUG         = 1 << 11
} debugType_t;

#ifdef ESP32
#define MAX_CONN 10
#else
#define MAX_CONN 4
#endif // DEBUG

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

    /**
     * Set the node as an root/master node for the mesh
     *
     * This is an optional setting that can speed up mesh formation. 
     * At most one node in the mesh should be a root, or you could
     * end up with multiple subMeshes.
     *
     * We recommend any AP_ONLY nodes (e.g. a bridgeNode) to be set
     * as a root node.
     *
     * If one node is root, then it is also recommended to call painlessMesh::setContainsRoot() on
     * all the nodes in the mesh.
     */
    void setRoot(bool on = true) { root = on; };

    /**
     * The mesh should contains a root node
     *
     * This will cause the mesh to restructure more quickly around the root node. Note that this
     * could have adverse effects if set, while there is no root node present. Also see painlessMesh::setRoot().
     */
    void setContainsRoot(bool on = true) { shouldContainRoot = on; };

    /**
     * Check whether this node is a root node.
     */
    bool isRoot() { return root; };

    // in painlessMeshDebug.cpp
    void                setDebugMsgTypes(uint16_t types);
    void                debugMsg(debugType_t type, const char* format ...);

    // in painlessMesh.cpp
	 					painlessMesh();
    void                init(String ssid, String password, Scheduler *baseScheduler, uint16_t port = 5555, WiFiMode_t connectMode = WIFI_AP_STA, uint8_t channel = 1, uint8_t hidden = 0, uint8_t maxconn = MAX_CONN);
    void                init(String ssid, String password, uint16_t port = 5555, WiFiMode_t connectMode = WIFI_AP_STA, uint8_t channel = 1, uint8_t hidden = 0, uint8_t maxconn = MAX_CONN);
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

    /**
     * Check whether this node is part of a mesh with a root in
     * it.
     */
    bool isRooted();

    // in painlessMeshSync.cpp
    uint32_t            getNodeTime(void);

    // in painlessMeshSTA.cpp
    uint32_t            encodeNodeId(const uint8_t *hwaddr);
    /**
     * Connect (as a station) to a specified network and ip
     *
     * You can pass {0,0,0,0} as IP to have it connect to the gateway
     *
     * This stops the node from scanning for other (non specified) nodes
     * and you should probably also use this node as an anchor: `setAnchor(true)`
     */
    void                stationManual(String ssid, String password, uint16_t port = 0,
                                        IPAddress remote_ip = IPAddress(0,0,0,0));
    bool                setHostname(const char * hostname);
    IPAddress           getStationIP();

    StationScan         stationScan;

    // Rough estimate of the mesh stability (goes from 0-1000)
    size_t              stability = 0;

    // in painlessMeshAP.cpp
    IPAddress           getAPIP();

#if __cplusplus > 201103L
    [[deprecated("Use of the internal scheduler will be deprecated, please use an user provided scheduler instead (See the startHere example).")]]
#endif
    Scheduler &scheduler = _scheduler;

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

    // Update other connections of a change
    void                syncSubConnections(uint32_t changedId);

    // in painlessMeshConnection.cpp
    //void                cleanDeadConnections(void); // Not implemented. Needed?
    void                tcpConnect(void);
    bool                closeConnectionSTA(); 

    void                eraseClosedConnections();

    String              subConnectionJson(std::shared_ptr<MeshConnection> exclude);
    String              subConnectionJsonHelper(ConnectionList &connections, uint32_t exclude = 0);
    
    size_t              approxNoNodes(); // estimate of numbers of node
    size_t              approxNoNodes(String &subConns); // estimate of numbers of node
    
    shared_ptr<MeshConnection> findConnection(uint32_t nodeId, uint32_t exclude = 0);
    shared_ptr<MeshConnection> findConnection(AsyncClient *conn);

    std::list<uint32_t> getNodeList(String &subConnections);

    // in painlessMeshAP.cpp
    void                apInit(void);

    void                tcpServerInit();

    // callbacks
    // in painlessMeshConnection.cpp
    void                eventHandleInit();

    // Callback functions
    newConnectionCallback_t         newConnectionCallback;
    droppedConnectionCallback_t     droppedConnectionCallback;
    receivedCallback_t              receivedCallback;
    changedConnectionsCallback_t    changedConnectionsCallback;
    nodeTimeAdjustedCallback_t      nodeTimeAdjustedCallback;
    nodeDelayCallback_t             nodeDelayReceivedCallback;
#ifdef ESP32
    WiFiEventId_t eventScanDoneHandler;
    WiFiEventId_t eventSTAStartHandler;
    WiFiEventId_t eventSTADisconnectedHandler;
    WiFiEventId_t eventSTAGotIPHandler;
#elif defined(ESP8266)
    WiFiEventHandler  eventSTAConnectedHandler;
    WiFiEventHandler  eventSTADisconnectedHandler;
    WiFiEventHandler  eventSTAAuthChangeHandler;
    WiFiEventHandler  eventSTAGotIPHandler;
    WiFiEventHandler  eventSoftAPConnectedHandler;
    WiFiEventHandler  eventSoftAPDisconnectedHandler;
#endif // ESP8266

    uint32_t          _nodeId;
    String            _meshSSID;
    String            _meshPassword;
    uint16_t          _meshPort;
    uint8_t           _meshChannel;
    uint8_t           _meshHidden;
    uint8_t           _meshMaxConn;

    IPAddress         _apIp;

    ConnectionList    _connections;

    AsyncServer       *_tcpListener;

    bool              _station_got_ip = false;

    bool              isExternalScheduler = false;

    /// Is the node a root node
    bool root;
    bool shouldContainRoot;

    Scheduler         _scheduler;
    Task              droppedConnectionTask;
    Task              newConnectionTask;

    friend class StationScan;
    friend class MeshConnection;
    friend void  onDataCb(void * arg, AsyncClient *client, void *data, size_t len);
};

#endif //   _EASY_MESH_H_
