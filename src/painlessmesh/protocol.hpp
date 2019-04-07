#ifndef _PAINLESS_MESH_PROTOCOL_H_
#define _PAINLESS_MESH_PROTOCOL_H_

#include <list>

namespace painlessmesh {
namespace protocol {

#ifndef ARDUINOJSON_VERSION_MAJOR
#include "ArduinoJson.h"
typedef std::string TSTRING;
#endif

enum Type {
  TIME_DELAY = 3,
  TIME_SYNC = 4,
  NODE_SYNC_REQUEST = 5,
  NODE_SYNC_REPLY = 6,
  CONTROL = 7,    // deprecated
  BROADCAST = 8,  // application data for everyone
  SINGLE = 9      // application data for a single node
};

/**
 * Single package
 *
 * Message send to a specific node
 */
class Single {
 public:
  int type = SINGLE;
  uint32_t from;
  uint32_t dest;
  TSTRING msg;

  size_t jsonObjectSize() {
    return JSON_OBJECT_SIZE(4) + round(1.1 * msg.length());
  }
};

/**
 * Broadcast package
 */
class Broadcast {
 public:
  int type = BROADCAST;
  uint32_t from;
  uint32_t dest;
  TSTRING msg;
  size_t jsonObjectSize() {
    return JSON_OBJECT_SIZE(4) + round(1.1 * msg.length());
  }
};

class NodeTree {
 public:
  uint32_t nodeId;
  bool root = false;
  std::list<NodeTree> subs;

  bool operator==(const NodeTree& b) const {
    if (!(this->nodeId == b.nodeId && this->root == b.root &&
          this->subs.size() == b.subs.size()))
      return false;
    auto itA = this->subs.begin();
    auto itB = b.subs.begin();
    for (size_t i = 0; i < this->subs.size(); ++i) {
      if ((*itA) != (*itB)) {
        return false;
      }
      ++itA;
      ++itB;
    }
    return true;
  }

  bool operator!=(const NodeTree& b) const { return !this->operator==(b); }

  size_t jsonObjectSize() {
    size_t base = 1;
    if (root) ++base;
    if (subs.size() > 0) ++base;
    size_t size = JSON_OBJECT_SIZE(base);
    if (subs.size() > 0) size += JSON_ARRAY_SIZE(subs.size());
    for (auto&& s : subs) size += s.jsonObjectSize();
    return size;
  }
};

inline JsonObject addNodeTree(JsonObject&& jsonObj, NodeTree& sub) {
  jsonObj["nodeId"] = sub.nodeId;
  if (sub.root) jsonObj["root"] = sub.root;
  if (sub.subs.size() > 0) {
    JsonArray subsArr = jsonObj.createNestedArray("subs");
    for (auto&& s : sub.subs) {
      JsonObject subObj = subsArr.createNestedObject();
      subObj = addNodeTree(std::move(subObj), s);
    }
  }
  return jsonObj;
}

inline NodeTree extractNodeTree(JsonObject jsonObj) {
  NodeTree pkg;
  pkg.nodeId = jsonObj["nodeId"].as<uint32_t>();
  if (jsonObj.containsKey("root"))
    pkg.root = jsonObj["root"].as<bool>();
  else
    pkg.root = false;
  if (jsonObj.containsKey("subs")) {
    auto jsonArr = jsonObj["subs"].as<JsonArray>();
    for (size_t i = 0; i < jsonArr.size(); ++i) {
      pkg.subs.push_back(extractNodeTree(jsonArr[i]));
    }
  }
  return pkg;
}

/**
 * NodeSyncRequest package
 */
class NodeSyncRequest : public NodeTree {
 public:
  int type = NODE_SYNC_REQUEST;
  uint32_t from;
  uint32_t dest;
  bool operator==(const NodeSyncRequest& b) const {
    if (!(this->from == b.from && this->dest == b.dest)) return false;
    return NodeTree::operator==(b);
  }

  bool operator!=(const NodeSyncRequest& b) const {
    return !this->operator==(b);
  }

  size_t jsonObjectSize() {
    size_t base = 4;
    if (root) ++base;
    if (subs.size() > 0) ++base;
    size_t size = JSON_OBJECT_SIZE(base);
    if (subs.size() > 0) size += JSON_ARRAY_SIZE(subs.size());
    for (auto&& s : subs) size += s.jsonObjectSize();
    return size;
  }
};

/**
 * NodeSyncReply package
 */
class NodeSyncReply : public NodeSyncRequest {
 public:
  int type = NODE_SYNC_REPLY;
};

struct time_sync_msg_t {
  int type;
  uint32_t t0;
  uint32_t t1;
  uint32_t t2;
};

/**
 * TimeSync package
 */
class TimeSync {
 public:
  int type = TIME_SYNC;
  uint32_t dest;
  uint32_t from;
  time_sync_msg_t msg;
  size_t jsonObjectSize() { return JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(4); }
};

/**
 * TimeDelay package
 */
class TimeDelay : public TimeSync {
 public:
  int type = TIME_DELAY;
};

/**
 * Can store any package variant
 *
 * Internally stores packages as a JsonObject. Main use case is to convert
 * different packages from and to Json (using ArduinoJson).
 */
class Variant {
 public:
#ifdef ARDUINOJSON_ENABLE_STD_STRING
  /**
   * Create Variant object from a json string
   *
   * @param json The json string containing a package
   */
  Variant(std::string json)
      : jsonBuffer(JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(4) +
                   2 * json.length()) {
    error = deserializeJson(jsonBuffer, json,
                            DeserializationOption::NestingLimit(255));
    if (!error) jsonObj = jsonBuffer.as<JsonObject>();
  }

  /**
   * Create Variant object from a json string
   *
   * @param json The json string containing a package
   * @param capacity The capacity to reserve for parsing the string
   */
  Variant(std::string json, size_t capacity) : jsonBuffer(capacity) {
    error = deserializeJson(jsonBuffer, json,
                            DeserializationOption::NestingLimit(255));
    if (!error) jsonObj = jsonBuffer.as<JsonObject>();
  }
#endif

#ifdef ARDUINOJSON_ENABLE_ARDUINO_STRING
  /**
   * Create Variant object from a json string
   *
   * @param json The json string containing a package
   */
  Variant(String json)
      : jsonBuffer(JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(4) +
                   2 * json.length()) {
    error = deserializeJson(jsonBuffer, json,
                            DeserializationOption::NestingLimit(255));
    if (!error) jsonObj = jsonBuffer.as<JsonObject>();
  }

  /**
   * Create Variant object from a json string
   *
   * @param json The json string containing a package
   * @param capacity The capacity to reserve for parsing the string
   */
  Variant(String json, size_t capacity) : jsonBuffer(capacity) {
    error = deserializeJson(jsonBuffer, json,
                            DeserializationOption::NestingLimit(255));
    if (!error) jsonObj = jsonBuffer.as<JsonObject>();
  }
#endif

  /**
   * Create Variant object from a Single package
   *
   * @param single The single package
   */
  Variant(Single single) : jsonBuffer(single.jsonObjectSize()) {
    jsonObj = jsonBuffer.to<JsonObject>();
    jsonObj["type"] = single.type;
    jsonObj["dest"] = single.dest;
    jsonObj["from"] = single.from;
    jsonObj["msg"] = single.msg;
  }

  /**
   * Create Variant object from a Broadcast package
   *
   * @param broadcast The broadcast package
   */
  Variant(Broadcast broadcast) : jsonBuffer(broadcast.jsonObjectSize()) {
    jsonObj = jsonBuffer.to<JsonObject>();
    jsonObj["type"] = broadcast.type;
    jsonObj["dest"] = broadcast.dest;
    jsonObj["from"] = broadcast.from;
    jsonObj["msg"] = broadcast.msg;
  }

  /**
   * Create Variant object from a NodeSyncReply package
   *
   * @param nodeSyncReply The nodeSyncReply package
   */
  Variant(NodeSyncReply nodeSyncReply)
      : jsonBuffer(nodeSyncReply.jsonObjectSize()) {
    jsonObj = jsonBuffer.to<JsonObject>();
    jsonObj["type"] = nodeSyncReply.type;
    jsonObj["dest"] = nodeSyncReply.dest;
    jsonObj["from"] = nodeSyncReply.from;
    jsonObj = addNodeTree(std::move(jsonObj), nodeSyncReply);
  }

  /**
   * Create Variant object from a NodeSyncRequest package
   *
   * @param nodeSyncRequest The nodeSyncRequest package
   */
  Variant(NodeSyncRequest nodeSyncRequest)
      : jsonBuffer(nodeSyncRequest.jsonObjectSize()) {
    jsonObj = jsonBuffer.to<JsonObject>();
    jsonObj["type"] = nodeSyncRequest.type;
    jsonObj["dest"] = nodeSyncRequest.dest;
    jsonObj["from"] = nodeSyncRequest.from;
    jsonObj = addNodeTree(std::move(jsonObj), nodeSyncRequest);
  }

  /**
   * Create Variant object from a TimeSync package
   *
   * @param timeSync The timeSync package
   */
  Variant(TimeSync timeSync) : jsonBuffer(timeSync.jsonObjectSize()) {
    jsonObj = jsonBuffer.to<JsonObject>();
    jsonObj["type"] = timeSync.type;
    jsonObj["dest"] = timeSync.dest;
    jsonObj["from"] = timeSync.from;
    auto msgObj = jsonObj.createNestedObject("msg");
    msgObj["type"] = timeSync.msg.type;
    if (timeSync.msg.type == 1) msgObj["t0"] = timeSync.msg.t0;
    if (timeSync.msg.type == 2) {
      msgObj["t1"] = timeSync.msg.t1;
      msgObj["t2"] = timeSync.msg.t2;
    }
  }

  /**
   * Create Variant object from a TimeDelay package
   *
   * @param timeDelay The timeDelay package
   */
  Variant(TimeDelay timeDelay) : jsonBuffer(timeDelay.jsonObjectSize()) {
    jsonObj = jsonBuffer.to<JsonObject>();
    jsonObj["type"] = timeDelay.type;
    jsonObj["dest"] = timeDelay.dest;
    jsonObj["from"] = timeDelay.from;
    auto msgObj = jsonObj.createNestedObject("msg");
    msgObj["type"] = timeDelay.msg.type;
    if (timeDelay.msg.type == 1) msgObj["t0"] = timeDelay.msg.t0;
    if (timeDelay.msg.type == 2) {
      msgObj["t1"] = timeDelay.msg.t1;
      msgObj["t2"] = timeDelay.msg.t2;
    }
  }

  /**
   * Whether this package is of the given type
   */
  template <typename T>
  inline bool is() {
    return false;
  }

  /**
   * Convert Variant to the given type
   */
  template <typename T>
  inline T to() {
    return T();
  }

#ifdef ARDUINOJSON_ENABLE_STD_STRING
  /**
   * Print a variant to a string
   *
   * @return A json representation of the string
   */
  void printTo(std::string& str) { serializeJson(jsonObj, str); }
#endif

#ifdef ARDUINOJSON_ENABLE_ARDUINO_STRING
  /**
   * Print a variant to a string
   *
   * @return A json representation of the string
   */
  void printTo(String& str) { serializeJson(jsonObj, str); }
#endif

  DeserializationError error = DeserializationError::Ok;

 private:
  DynamicJsonDocument jsonBuffer;
  JsonObject jsonObj;
};

template <>
inline bool Variant::is<Single>() {
  return jsonObj["type"].as<int>() == SINGLE;
}

template <>
inline bool Variant::is<Broadcast>() {
  return jsonObj["type"].as<int>() == BROADCAST;
}

template <>
inline bool Variant::is<NodeSyncReply>() {
  return jsonObj["type"].as<int>() == NODE_SYNC_REPLY;
}

template <>
inline bool Variant::is<NodeSyncRequest>() {
  return jsonObj["type"].as<int>() == NODE_SYNC_REQUEST;
}

template <>
inline bool Variant::is<TimeSync>() {
  return jsonObj["type"].as<int>() == TIME_SYNC;
}

template <>
inline bool Variant::is<TimeDelay>() {
  return jsonObj["type"].as<int>() == TIME_DELAY;
}

template <>
inline Single Variant::to<Single>() {
  auto pkg = Single();
  pkg.dest = jsonObj["dest"].as<uint32_t>();
  pkg.from = jsonObj["from"].as<uint32_t>();
  pkg.msg = jsonObj["msg"].as<TSTRING>();
  return pkg;
}

template <>
inline Broadcast Variant::to<Broadcast>() {
  auto pkg = Broadcast();
  pkg.dest = jsonObj["dest"].as<uint32_t>();
  pkg.from = jsonObj["from"].as<uint32_t>();
  pkg.msg = jsonObj["msg"].as<TSTRING>();
  return pkg;
}

template <>
inline NodeSyncReply Variant::to<NodeSyncReply>() {
  auto pkg = NodeSyncReply();
  pkg.dest = jsonObj["dest"].as<uint32_t>();
  pkg.from = jsonObj["from"].as<uint32_t>();
  if (jsonObj.containsKey("root")) pkg.root = jsonObj["root"].as<bool>();
  if (jsonObj.containsKey("nodeId"))
    pkg.nodeId = jsonObj["nodeId"].as<uint32_t>();
  else
    pkg.nodeId = pkg.from;

  if (jsonObj.containsKey("subs")) {
    auto jsonArr = jsonObj["subs"].as<JsonArray>();
    for (size_t i = 0; i < jsonArr.size(); ++i) {
      pkg.subs.push_back(extractNodeTree(jsonArr[i]));
    }
  }

  return pkg;
}

template <>
inline NodeTree Variant::to<NodeTree>() {
  auto pkg = NodeTree();
  if (jsonObj.containsKey("root")) pkg.root = jsonObj["root"].as<bool>();
  if (jsonObj.containsKey("nodeId"))
    pkg.nodeId = jsonObj["nodeId"].as<uint32_t>();
  else
    pkg.nodeId = jsonObj["from"].as<uint32_t>();

  if (jsonObj.containsKey("subs")) {
    auto jsonArr = jsonObj["subs"].as<JsonArray>();
    for (size_t i = 0; i < jsonArr.size(); ++i) {
      pkg.subs.push_back(extractNodeTree(jsonArr[i]));
    }
  }

  return pkg;
}

template <>
inline NodeSyncRequest Variant::to<NodeSyncRequest>() {
  auto pkg = NodeSyncRequest();
  pkg.dest = jsonObj["dest"].as<uint32_t>();
  pkg.from = jsonObj["from"].as<uint32_t>();
  if (jsonObj.containsKey("root"))
    pkg.root = jsonObj["root"].as<bool>();
  else
    pkg.root = false;
  if (jsonObj.containsKey("nodeId"))
    pkg.nodeId = jsonObj["nodeId"].as<uint32_t>();
  else
    pkg.nodeId = pkg.from;

  if (jsonObj.containsKey("subs")) {
    auto jsonArr = jsonObj["subs"].as<JsonArray>();
    for (size_t i = 0; i < jsonArr.size(); ++i) {
      pkg.subs.push_back(extractNodeTree(jsonArr[i]));
    }
  }

  return pkg;
}

template <>
inline TimeSync Variant::to<TimeSync>() {
  auto pkg = TimeSync();
  pkg.dest = jsonObj["dest"].as<uint32_t>();
  pkg.from = jsonObj["from"].as<uint32_t>();
  pkg.msg.type = jsonObj["msg"]["type"].as<int>();
  pkg.msg.t0 = jsonObj["msg"]["t0"].as<uint32_t>();
  pkg.msg.t1 = jsonObj["msg"]["t1"].as<uint32_t>();
  pkg.msg.t2 = jsonObj["msg"]["t2"].as<uint32_t>();

  return pkg;
}

template <>
inline TimeDelay Variant::to<TimeDelay>() {
  auto pkg = TimeDelay();
  pkg.dest = jsonObj["dest"].as<uint32_t>();
  pkg.from = jsonObj["from"].as<uint32_t>();
  pkg.msg.type = jsonObj["msg"]["type"].as<int>();
  pkg.msg.t0 = jsonObj["msg"]["t0"].as<uint32_t>();
  pkg.msg.t1 = jsonObj["msg"]["t1"].as<uint32_t>();
  pkg.msg.t2 = jsonObj["msg"]["t2"].as<uint32_t>();

  return pkg;
}
}  // namespace protocol
}  // namespace painlessmesh
#endif
