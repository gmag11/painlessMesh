#include "painlessMesh.h"

#ifdef PAINLESSMESH_ENABLE_ARDUINO_WIFI
// TODO: Is this really needed here?
#include "lwip/init.h"
#endif

LogClass Log;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
ICACHE_FLASH_ATTR painlessMesh::painlessMesh() {}
#pragma GCC diagnostic pop

void painlessMesh::init(uint32_t id, uint16_t port) {
  nodeId = id;

  _meshPort = port;

#ifdef ESP32
  xSemaphore = xSemaphoreCreateMutex();
#endif

  // establish AP tcpServers
  tcpServerInit();
  eventHandleInit();

  _scheduler.enableAll();

  // Add package handlers
  callbackList =
      painlessmesh::ntp::addPackageCallback(std::move(callbackList), (*this));
  callbackList = painlessmesh::router::addPackageCallback(
      std::move(callbackList), (*this));
}

void painlessMesh::init(Scheduler *scheduler, uint32_t id, uint16_t port) {
  this->setScheduler(scheduler);
}

#ifdef PAINLESSMESH_ENABLE_ARDUINO_WIFI
void painlessMesh::initStation() {
  stationScan.init(this, _meshSSID, _meshPassword, _meshPort);
  _scheduler.addTask(stationScan.task);
  stationScan.task.enable();
}
#endif

void ICACHE_FLASH_ATTR painlessMesh::stop() {
  // remove all WiFi events
#ifdef ESP32
  WiFi.removeEvent(eventScanDoneHandler);
  WiFi.removeEvent(eventSTAStartHandler);
  WiFi.removeEvent(eventSTADisconnectedHandler);
  WiFi.removeEvent(eventSTAGotIPHandler);
#elif defined(ESP8266)
  eventSTAConnectedHandler = WiFiEventHandler();
  eventSTADisconnectedHandler = WiFiEventHandler();
  eventSTAAuthChangeHandler = WiFiEventHandler();
  eventSTAGotIPHandler = WiFiEventHandler();
  eventSoftAPConnectedHandler = WiFiEventHandler();
  eventSoftAPDisconnectedHandler = WiFiEventHandler();
#endif  // ESP32

  // Close all connections
  while (subs.size() > 0) {
    auto connection = subs.begin();
    (*connection)->close();
  }

  // Stop scanning task
#ifdef PAINLESSMESH_ENABLE_ARDUINO_WIFI
  stationScan.task.setCallback(NULL);
  _scheduler.deleteTask(stationScan.task);
#endif

  // Note that this results in the droppedConnections not to be signalled
  // We might want to change this later
  newConnectionTask.setCallback(NULL);
  _scheduler.deleteTask(newConnectionTask);
  droppedConnectionTask.setCallback(NULL);
  _scheduler.deleteTask(droppedConnectionTask);

  // Shutdown wifi hardware
  if (WiFi.status() != WL_DISCONNECTED) WiFi.disconnect();
}

//***********************************************************************
// do nothing if user have other Scheduler, they have to run their scheduler in
// loop not this library
void ICACHE_FLASH_ATTR painlessMesh::update(void) {
  if (isExternalScheduler == false) {
    if (semaphoreTake()) {
      _scheduler.execute();
      semaphoreGive();
    }
  }
  return;
}

bool ICACHE_FLASH_ATTR painlessMesh::semaphoreTake(void) {
#ifdef ESP32
  return xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE;
#else
  return true;
#endif
}

void ICACHE_FLASH_ATTR painlessMesh::semaphoreGive(void) {
#ifdef ESP32
  xSemaphoreGive(xSemaphore);
#endif
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendSingle(uint32_t &destId,
                                                TSTRING &msg) {
  Log(COMMUNICATION, "sendSingle(): dest=%u msg=%s\n", destId, msg.c_str());
  auto single = painlessmesh::protocol::Single(this->nodeId, destId, msg);
  return painlessmesh::router::send<MeshConnection>(single, (*this));
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::sendBroadcast(TSTRING &msg,
                                                   bool includeSelf) {
  using namespace painlessmesh;
  Log(COMMUNICATION, "sendBroadcast(): msg=%s\n", msg.c_str());
  auto pkg = painlessmesh::protocol::Broadcast(this->nodeId, 0, msg);
  auto success =
      router::broadcast<protocol::Broadcast, MeshConnection>(pkg, (*this), 0);
  if (success && includeSelf) {
    auto variant = protocol::Variant(pkg);
    callbackList.execute(pkg.type, pkg, NULL, 0);
  }
  if (success > 0) return true;
  return false;
}

bool ICACHE_FLASH_ATTR painlessMesh::startDelayMeas(uint32_t nodeID) {
  using namespace painlessmesh;
  Log(S_TIME, "startDelayMeas(): NodeId %u\n", nodeID);
  auto conn = painlessmesh::router::findRoute<MeshConnection>((*this), nodeID);
  if (!conn) return false;
  return router::send<protocol::TimeDelay, MeshConnection>(
      protocol::TimeDelay(this->nodeId, nodeID, getNodeTime()), conn);
}

void ICACHE_FLASH_ATTR painlessMesh::setDebugMsgTypes(uint16_t newTypes) {
  Log.setLogLevel(newTypes);
}

void ICACHE_FLASH_ATTR painlessMesh::tcpServerInit() {
  Log(GENERAL, "tcpServerInit():\n");

  _tcpListener = new AsyncServer(_meshPort);
  _tcpListener->setNoDelay(true);

  _tcpListener->onClient(
      [this](void *arg, AsyncClient *client) {
        if (this->semaphoreTake()) {
          Log(CONNECTION, "New AP connection incoming\n");
          auto conn = std::make_shared<MeshConnection>(client, this, false);
          conn->initTCPCallbacks();
          conn->initTasks();
          this->subs.push_back(conn);
          this->semaphoreGive();
        }
      },
      NULL);

  _tcpListener->begin();

  Log(STARTUP, "AP tcp server established on port %d\n", _meshPort);
  return;
}
