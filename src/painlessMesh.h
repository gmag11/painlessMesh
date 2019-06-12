#ifndef _EASY_MESH_H_
#define _EASY_MESH_H_

#define _TASK_PRIORITY  // Support for layered scheduling priority
#define _TASK_STD_FUNCTION

#include <Arduino.h>
#include <functional>
#include <list>
#include <memory>
#include "painlessmesh/configuration.hpp"
using namespace std;
#ifdef ESP32
#include <AsyncTCP.h>
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif  // ESP32

#include "arduino/wifi.hpp"

#include "painlessMeshConnection.h"
#include "painlessMeshSTA.h"

#include "painlessmesh/buffer.hpp"
#include "painlessmesh/layout.hpp"
#include "painlessmesh/logger.hpp"
#include "painlessmesh/ntp.hpp"
#include "painlessmesh/plugin.hpp"
#include "painlessmesh/protocol.hpp"
#include "painlessmesh/router.hpp"
#include "painlessmesh/tcp.hpp"
using namespace painlessmesh::logger;

#define NODE_TIMEOUT 10 * TASK_SECOND
#define MIN_FREE_MEMORY \
  4000  // Minimum free memory, besides here all packets in queue are discarded.
#define MAX_MESSAGE_QUEUE \
  50  // MAX number of unsent messages in queue. Newer messages are discarded
#define MAX_CONSECUTIVE_SEND 5  // Max message burst

template <typename T>
using SimpleList = std::list<T>;  // backward compatibility

using ConnectionList = std::list<std::shared_ptr<MeshConnection>>;

typedef std::function<void(uint32_t nodeId)> newConnectionCallback_t;
typedef std::function<void(uint32_t nodeId)> droppedConnectionCallback_t;
typedef std::function<void(uint32_t from, TSTRING &msg)> receivedCallback_t;
typedef std::function<void()> changedConnectionsCallback_t;
typedef std::function<void(int32_t offset)> nodeTimeAdjustedCallback_t;
typedef std::function<void(uint32_t nodeId, int32_t delay)> nodeDelayCallback_t;

class painlessMesh
    : public painlessmesh::ntp::MeshTime,
#ifdef PAINLESSMESH_ENABLE_ARDUINO_WIFI
      public painlessmesh::wifi::Mesh,
#endif
      public painlessmesh::plugin::PackageHandler<MeshConnection> {
 public:
  void init(uint32_t nodeId, uint16_t port);
  void init(Scheduler *scheduler, uint32_t nodeId, uint16_t port);
  // TODO: move these to wifi::Mesh
#ifdef PAINLESSMESH_ENABLE_ARDUINO_WIFI
  void initStation();
  // These should be removable when things compile correctly?
  void init(TSTRING ssid, TSTRING password, Scheduler *baseScheduler,
            uint16_t port = 5555, WiFiMode_t connectMode = WIFI_AP_STA,
            uint8_t channel = 1, uint8_t hidden = 0,
            uint8_t maxconn = MAX_CONN) {
    using namespace painlessmesh;
    wifi::Mesh::init(ssid, password, baseScheduler, port, connectMode, channel,
                     hidden, maxconn);
  }

  void init(TSTRING ssid, TSTRING password, uint16_t port = 5555,
            WiFiMode_t connectMode = WIFI_AP_STA, uint8_t channel = 1,
            uint8_t hidden = 0, uint8_t maxconn = MAX_CONN) {
    using namespace painlessmesh;
    wifi::Mesh::init(ssid, password, port, connectMode, channel, hidden,
                     maxconn);
  }
#endif
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
   * If one node is root, then it is also recommended to call
   * painlessMesh::setContainsRoot() on all the nodes in the mesh.
   */
  void setRoot(bool on = true) { root = on; };

  /**
   * The mesh should contains a root node
   *
   * This will cause the mesh to restructure more quickly around the root node.
   * Note that this could have adverse effects if set, while there is no root
   * node present. Also see painlessMesh::setRoot().
   */
  void setContainsRoot(bool on = true) { shouldContainRoot = on; };

  /**
   * Check whether this node is a root node.
   */
  bool isRoot() { return root; };

  // in painlessMeshDebug.cpp
  void setDebugMsgTypes(uint16_t types);

  // in painlessMesh.cpp
  painlessMesh();
  /**
   * Disconnect and stop this node
   */
  void stop();
  void update(void);
  bool sendSingle(uint32_t destId, TSTRING msg);
  bool sendBroadcast(TSTRING msg, bool includeSelf = false);
  bool startDelayMeas(uint32_t nodeId);

  // in painlessMeshConnection.cpp
  void onReceive(receivedCallback_t onReceive);
  void onNewConnection(newConnectionCallback_t onNewConnection);
  void onDroppedConnection(droppedConnectionCallback_t onDroppedConnection);
  void onChangedConnections(changedConnectionsCallback_t onChangedConnections);
  void onNodeTimeAdjusted(nodeTimeAdjustedCallback_t onTimeAdjusted);
  void onNodeDelayReceived(nodeDelayCallback_t onDelayReceived);

  bool isConnected(uint32_t nodeId) {
    return painlessmesh::router::findRoute<MeshConnection>((*this), nodeId) !=
           NULL;
  }

  std::list<uint32_t> getNodeList(bool includeSelf = false);

  /**
   * Return a json representation of the current mesh layout
   */
  inline TSTRING subConnectionJson(bool pretty = false) {
    return this->asNodeTree().toString(pretty);
  }

  // in painlessMeshSTA.cpp
  /**
   * Connect (as a station) to a specified network and ip
   *
   * You can pass {0,0,0,0} as IP to have it connect to the gateway
   *
   * This stops the node from scanning for other (non specified) nodes
   * and you should probably also use this node as an anchor: `setAnchor(true)`
   */
  void stationManual(TSTRING ssid, TSTRING password, uint16_t port = 0,
                     IPAddress remote_ip = IPAddress(0, 0, 0, 0));
  bool setHostname(const char *hostname);
  IPAddress getStationIP();

  // Rough estimate of the mesh stability (goes from 0-1000)
  size_t stability = 0;

  // in painlessMeshAP.cpp
  IPAddress getAPIP();

#if __cplusplus > 201103L
  [[deprecated(
      "Use of the internal scheduler will be deprecated, please use an user "
      "provided scheduler instead (See the startHere example).")]]
#endif
  Scheduler &scheduler = _scheduler;

#ifndef UNITY  // Make everything public in unit test mode
 protected:
#endif
  void setScheduler(Scheduler *baseScheduler) {
    baseScheduler->setHighPriorityScheduler(&this->_scheduler);
    isExternalScheduler = true;
  }

  painlessmesh::router::MeshCallbackList<MeshConnection> callbackList;
  void startTimeSync(std::shared_ptr<MeshConnection> conn);

  bool adoptionCalc(std::shared_ptr<MeshConnection> conn);

  // in painlessMeshConnection.cpp
  // void                cleanDeadConnections(void); // Not implemented. Needed?
  void tcpConnect(void);
  bool closeConnectionSTA();

  void eraseClosedConnections();

  // in painlessMeshAP.cpp

  void tcpServerInit();

  // callbacks
  // in painlessMeshConnection.cpp
  void eventHandleInit();

  // Callback functions
  newConnectionCallback_t newConnectionCallback;
  droppedConnectionCallback_t droppedConnectionCallback;
  changedConnectionsCallback_t changedConnectionsCallback;
  nodeTimeAdjustedCallback_t nodeTimeAdjustedCallback;
  nodeDelayCallback_t nodeDelayReceivedCallback;
#ifdef ESP32
  SemaphoreHandle_t xSemaphore = NULL;

  WiFiEventId_t eventScanDoneHandler;
  WiFiEventId_t eventSTAStartHandler;
  WiFiEventId_t eventSTADisconnectedHandler;
  WiFiEventId_t eventSTAGotIPHandler;
#elif defined(ESP8266)
  WiFiEventHandler eventSTAConnectedHandler;
  WiFiEventHandler eventSTADisconnectedHandler;
  WiFiEventHandler eventSTAAuthChangeHandler;
  WiFiEventHandler eventSTAGotIPHandler;
  WiFiEventHandler eventSoftAPConnectedHandler;
  WiFiEventHandler eventSoftAPDisconnectedHandler;
#endif  // ESP8266
  uint16_t _meshPort;

  AsyncServer *_tcpListener;

  bool _station_got_ip = false;

  bool isExternalScheduler = false;

  /// Is the node a root node
  bool shouldContainRoot;

  Scheduler _scheduler;
  Task droppedConnectionTask;
  Task newConnectionTask;

  /**
   * Wrapper function for ESP32 semaphore function
   *
   * Waits for the semaphore to be available and then returns true
   *
   * Always return true on ESP8266
   */
  bool semaphoreTake();
  /**
   * Wrapper function for ESP32 semaphore give function
   *
   * Does nothing on ESP8266 hardware
   */
  void semaphoreGive();

  friend class StationScan;
  friend class MeshConnection;
  friend void onDataCb(void *, AsyncClient *, void *, size_t);
  friend void tcpSentCb(void *, AsyncClient *, size_t, uint32_t);
  friend void meshRecvCb(void *, AsyncClient *, void *, size_t);
  friend void painlessmesh::ntp::handleTimeSync<painlessMesh, MeshConnection>(
      painlessMesh &, painlessmesh::protocol::TimeSync,
      std::shared_ptr<MeshConnection>, uint32_t);
  friend void painlessmesh::ntp::handleTimeDelay<painlessMesh, MeshConnection>(
      painlessMesh &, painlessmesh::protocol::TimeDelay,
      std::shared_ptr<MeshConnection>, uint32_t);
  friend void
  painlessmesh::router::handleNodeSync<painlessMesh, MeshConnection>(
      painlessMesh &, protocol::NodeTree, std::shared_ptr<MeshConnection> conn);
  friend void painlessmesh::tcp::initServer<MeshConnection, painlessMesh>(
      AsyncServer &, painlessMesh &);
  friend void painlessmesh::tcp::connect<MeshConnection, painlessMesh>(
      AsyncClient &, IPAddress, uint16_t, painlessMesh &);
};

#endif  //   _EASY_MESH_H_
