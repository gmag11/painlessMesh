#ifndef _PAINLESS_MESH_PLUGIN_OTA_HPP_
#define _PAINLESS_MESH_PLUGIN_OTA_HPP_

#include "painlessmesh/plugin.hpp"

namespace painlessmesh {
namespace plugin {
namespace ota {

class State {
 public:
  TSTRING md5;
#ifdef ESP32
  TSTRING hardware = "ESP32";
#else
  TSTRING hardware = "ESP8266";
#endif
  TSTRING nodeType;
  size_t partNo = 0;
  ota_fn = "/ota_fw.json";

  State(JsonObject jsonObj) {
    md5 = jsonObj["md5"].as<TSTRING>();
    hardware = jsonObj["hardware"].as<TSTRING>();
    nodeType = jsonObj["nodeType"].as<TSTRING>();
  }

  JsonObject addTo(JsonObject&& jsonObj) {
    jsonObj["md5"] = md5;
    jsonObj["hardware"] = hardware;
    jsonObj["nodeType"] = nodeType;
    return jsonObj;
  }

  size_t jsonObjectSize() {
    return JSON_OBJECT_SIZE(3) + md5.length() + hardware.length() +
           nodeType.length();
  }
}

class Announce : public BroadcastPackage {
 public:
  TSTRING md5;
  TSTRING hardware;
  TSTRING nodeType;
  bool forced = false;
  size_t noPart;

  Announce() : BroadcastPackage(10) {}

  Announce(JsonObject jsonObj) : BroadcastPackage(jsonObj) {
    md5 = jsonObj["md5"].as<TSTRING>();
    hardware = jsonObj["hardware"].as<TSTRING>();
    nodeType = jsonObj["nodeType"].as<TSTRING>();
    if (jsonObj.containsKey("forced")) forced = jsonObj["forced"];
    noPart = jsonObj["noPart"];
  }

  JsonObject addTo(JsonObject&& jsonObj) {
    jsonObj = BroadcastPackage::addTo(std::move(jsonObj));
    jsonObj["md5"] = md5;
    jsonObj["hardware"] = hardware;
    jsonObj["nodeType"] = nodeType;
    if (forced) jsonObj["forced"] = forced;
    jsonObj["noPart"] = noPart;
    return jsonObj;
  }

  size_t jsonObjectSize() {
    return JSON_OBJECT_SIZE(noJsonFields + 5) + md5.length() +
           hardware.length() + nodeType.length();
  }

 protected:
  Announce(int type, router::Type routing) : BroadcastPackage(type) {
    this->routing = routing;
  }
};

class DataRequest : public Announce {
 public:
  uint32_t from = 0;
  size_t partNo = 0;

  DataRequest() : Announce(11, router::SINGLE) {}

  DataRequest(JsonObject jsonObj) : Announce(jsonObj) {
    from = jsonObj["from"];
    partNo = jsonObj["partNo"];
  }

  JsonObject addTo(JsonObject&& jsonObj) {
    jsonObj = Announce::addTo(std::move(jsonObj));
    jsonObj["from"] = from;
    jsonObj["partNo"] = partNo;
    return jsonObj;
  }

  size_t jsonObjectSize() {
    return JSON_OBJECT_SIZE(noJsonFields + 5 + 2) + md5.length() +
           hardware.length() + nodeType.length();
  }

 protected:
  DataRequest(int type) : Announce(type, router::SINGLE) {}
};

class Data : public DataRequest {
 public:
  TSTRING data;

  Data() : DataRequest(12) {}

  Data(JsonObject jsonObj) : DataRequest(jsonObj) {
    data = jsonObj["data"].as<TSTRING>();
  }

  JsonObject addTo(JsonObject&& jsonObj) {
    jsonObj = DataRequest::addTo(std::move(jsonObj));
    jsonObj["data"] = data;
    return jsonObj;
  }

  size_t jsonObjectSize() {
    return JSON_OBJECT_SIZE(noJsonFields + 5 + 2 + 1) + md5.length() +
           hardware.length() + nodeType.length() + data.length();
  }
};

/**
if (isJSON && root.containsKey("plugin") &&
        String("ota").equals(root["plugin"].as<String>())) {
        Serial.printf("startHere: Received OTA msg from %u msg=%s\n", from,
                      msg.c_str());

        if (currentFW.nodeType.equals(root["nodeType"].as<String>()) &&
            currentFW.hardware.equals(root["hardware"].as<String>())) {
            if (String("version").equals(root["type"].as<String>())) {
                if (currentFW.md5.equals(root["md5"].as<String>()) ||
                    updateFW.md5.equals(root["md5"].as<String>()))
                    return;  // Announced version already known
                else {
                    // Setup new updatedFW
                    updateFW = currentFW;
                    updateFW.md5 = root["md5"].as<String>();
                    updateFW.partNo = 0;
                    updateFW.noPart = root["noPart"].as<size_t>();

                    // Setup otaDataRequestTask (enableIfNot)
                    otaDataRequestTask.set(30 * TASK_SECOND, 5, [from]() {
#if ARDUINOJSON_VERSION_MAJOR == 6
                        DynamicJsonDocument jsonBuffer(256);
                        auto req = jsonBuffer.to<JsonObject>();
#else
                        DynamicJsonBuffer jsonBuffer;
                        JsonObject& req = jsonBuffer.createObject();
#endif
                        createDataRequest(req, updateFW);
                        String msg;
#if ARDUINOJSON_VERSION_MAJOR == 6
                        serializeJson(req, msg);
#else
                        req.printTo(msg);
#endif
                        uint32_t cpyFrom = from;
                        mesh.sendSingle(cpyFrom, msg);
                    });
                    otaDataRequestTask.enableIfNot();
                    otaDataRequestTask.forceNextIteration();
                    Serial.printf("Requesting firmware update\n");
                }
            } else if (String("data").equals(root["type"].as<String>()) &&
                       updateFW.partNo == root["partNo"].as<size_t>()) {
                size_t partNo = root["partNo"];
                size_t noPart = root["noPart"];
                if (partNo == 0) {
                    String otaMD5 = root["md5"].as<String>();
#ifdef ESP32
                    uint32_t maxSketchSpace = UPDATE_SIZE_UNKNOWN;
#else
                    uint32_t maxSketchSpace =
                        (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
#endif
                    Serial.printf("Sketch size %d\n", maxSketchSpace);
                    if (Update.isRunning()) {
                        Update.end(false);
                    }
                    if (!Update.begin(
                            maxSketchSpace)) {  // start with max available size
                        Serial.println("handleOTA(): OTA start failed!");
                        Update.printError(Serial);
                        Update.end();
                    } else {
                        Update.setMD5(otaMD5.c_str());
                    }
                }
                //    write data
                auto b64data = root["data"].as<std::string>();
                auto b64Data = base64_decode(b64data);

                if (Update.write((uint8_t *)b64Data.c_str(),
                                 b64Data.length()) != b64Data.length()) {
                    Serial.println("handleOTA(): OTA write failed!");
                    Update.printError(Serial);
                    Update.end();
                    return;
                }

                if (partNo == noPart - 1) {
                    //       check md5, reboot
                    if (Update.end(true)) {  // true to set the size to the
                                             // current progress
                        Serial.printf("Update.MD5: %s\n",
                                      Update.md5String().c_str());
                        auto file = SPIFFS.open(OTA_FN, "w");
#if ARDUINOJSON_VERSION_MAJOR == 6
                        DynamicJsonDocument jsonBuffer(1024);
                        auto req = jsonBuffer.to<JsonObject>();
#else
                        DynamicJsonBuffer jsonBuffer;
                        JsonObject &req = jsonBuffer.createObject();
#endif
                        createDataRequest(req, updateFW);
                        String msg;
#if ARDUINOJSON_VERSION_MAJOR == 6
                        serializeJson(req, msg);
#else
                        req.printTo(msg);
#endif
                        file.print(msg);
                        file.close();

                        Serial.println("handleOTA(): OTA Success!");
                        ESP.restart();
                    } else {
                        Serial.println("handleOTA(): OTA failed!");
                        Update.printError(Serial);
                    }
                    otaDataRequestTask.disable();
                } else {
                    ++updateFW.partNo;
                    otaDataRequestTask.setIterations(5);
                    otaDataRequestTask.forceNextIteration();
                }
            }
        }
    }
    Serial.printf("ota: Received from %u msg=%s\n", from, msg.c_str());
}
*/


template <class T>
void addPackageCallback(Scheduler& scheduler, plugin::PackageHandler<T>& mesh,
                        TSTRING nodeType = "") {
  auto currentFW = std::make_shared<State>();
  auto updateFW = std::make_shared<State>();
#ifdef ESP32
  SPIFFS.begin(true);  // Start the SPI Flash Files System
#else
  SPIFFS.begin();  // Start the SPI Flash Files System
#endif
  if (SPIFFS.exists(currentFW.ota_fn)) {
    auto file = SPIFFS.open(currentFW.ota_fn, "r");
    TSTRING msg = "";
    while (file.available()) {
      msg += (char)file.read();
    }
    auto var = protocol::Variant(msg);
    currentFW = var.to<State>();
  }

  // Add request task in disabled state with [updateFW], to know
  // the partNo (and others) to request

  mesh.onPackage(10, [currentFW, updateFW](protocol::Variant variant) {
      // convert variant to Announce
      // Check if we want the update
      // if not then return false
      // if yes then change the relevant fields in updateFW and 
      // enable the request task 
      });

  mesh.onPackage(11, [currentFW](protocol::Variant variant) {
      // Data request
      // Log as unhandled for now)
      });

  mesh.onPackage(12, [currentFW, updateFW](protocol::Variant variant) {
      // Check whether it is a new part
      // If so write, update updateFW and restart the request task iterations
      // If last part then write ota_fn and reboot
      });
}

}  // namespace ota
}  // namespace plugin
}  // namespace painlessmesh

#endif

