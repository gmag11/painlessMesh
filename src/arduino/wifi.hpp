#ifndef _PAINLESS_MESH_ARDUINO_WIFI_HPP_
#define _PAINLESS_MESH_ARDUINO_WIFI_HPP_

#include "Arduino.h"

#include "painlessmesh/configuration.hpp"

#include "painlessmesh/logger.hpp"
#include "painlessmesh/router.hpp"

#ifdef PAINLESSMESH_ENABLE_ARDUINO_WIFI
#include "painlessMeshSTA.h"

extern painlessmesh::logger::LogClass Log;

namespace painlessmesh {
namespace wifi {
class Mesh {
 public:
  virtual void init(uint32_t nodeId, uint16_t port) = 0;
  void init(TSTRING ssid, TSTRING password, uint16_t port = 5555,
            WiFiMode_t connectMode = WIFI_AP_STA, uint8_t channel = 1,
            uint8_t hidden = 0, uint8_t maxconn = MAX_CONN) {
    using namespace logger;
    // Init random generator seed to generate delay variance
    randomSeed(millis());

    // Shut Wifi down and start with a blank slage
    if (WiFi.status() != WL_DISCONNECTED) WiFi.disconnect();

    Log(STARTUP, "init(): %d\n",
        WiFi.setAutoConnect(false));  // Disable autoconnect
    WiFi.persistent(false);

    // start configuration
    if (!WiFi.mode(connectMode)) {
      Log(GENERAL, "WiFi.mode() false");
    }


    _meshSSID = ssid;
    _meshPassword = password;
    _meshChannel = channel;
    _meshHidden = hidden;
    _meshMaxConn = maxconn;

    uint8_t MAC[] = {0, 0, 0, 0, 0, 0};
    if (WiFi.softAPmacAddress(MAC) == 0) {
      Log(ERROR, "init(): WiFi.softAPmacAddress(MAC) failed.\n");
    }
    uint32_t nodeId = encodeNodeId(MAC);

    this->init(nodeId, port);

    _apIp = IPAddress(0, 0, 0, 0);

    if (connectMode & WIFI_AP) {
      apInit(nodeId);  // setup AP
    }
    if (connectMode & WIFI_STA) {
      this->initStation();
    }
  }

  void init(TSTRING ssid, TSTRING password, Scheduler *baseScheduler,
            uint16_t port = 5555, WiFiMode_t connectMode = WIFI_AP_STA,
            uint8_t channel = 1, uint8_t hidden = 0,
            uint8_t maxconn = MAX_CONN) {
    this->setScheduler(baseScheduler);
    init(ssid, password, port, connectMode, channel, hidden, maxconn);
  }

 protected:
  TSTRING _meshSSID;
  TSTRING _meshPassword;
  uint8_t _meshChannel;
  uint8_t _meshHidden;
  uint8_t _meshMaxConn;

  IPAddress _apIp;
  StationScan stationScan;

  virtual void setScheduler(Scheduler *scheduler) = 0;
  virtual void initStation() = 0;

  void apInit(uint32_t nodeId);
  uint32_t encodeNodeId(const uint8_t *hwaddr);

  friend class StationScan;
};
}  // namespace wifi
};  // namespace painlessmesh
#endif

#endif

