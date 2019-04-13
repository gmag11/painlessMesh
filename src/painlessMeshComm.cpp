//
//  painlessMeshComm.cpp
//
//
//  Created by Bill Gray on 7/26/16.
//
//
#include "painlessMesh.h"

extern painlessMesh* staticThis;

bool ICACHE_FLASH_ATTR painlessMesh::sendNodeSync(
    std::shared_ptr<MeshConnection> conn, uint32_t destId, uint32_t fromId,
    painlessmesh::protocol::Type type, String &msg, bool priority) {
  using namespace painlessmesh;
  debugMsg(COMMUNICATION,
           "sendNodeSync(conn): conn-nodeId=%u destId=%u type=%d msg=%s\n",
           conn->nodeId, destId, (uint8_t)type, msg.c_str());

  String package;
  DynamicJsonDocument jsonBuffer(1024 + 2 * msg.length());
  JsonObject jsonObj = jsonBuffer.to<JsonObject>();

  jsonObj["dest"] = destId;
  jsonObj["from"] = fromId;
  jsonObj["type"] = (uint8_t)type;

  switch (type) {
    case protocol::NODE_SYNC_REQUEST:
    case protocol::NODE_SYNC_REPLY: {
      jsonObj["subs"] = serialized(msg);
      if (this->isRoot()) jsonObj["root"] = true;
      break;
    }
    default:
      debugMsg(ERROR, "sendNodeSync(conn): unsupported type=%d\n", conn->nodeId,
               (uint8_t)type);
  }

  serializeJson(jsonObj, package);

  return conn->addMessage(package, priority);
}

bool ICACHE_FLASH_ATTR
painlessMesh::broadcastMessage(painlessmesh::protocol::Broadcast pkg,
                               std::shared_ptr<MeshConnection> exclude) {
  // send a message to every node on the mesh
  bool errCode = false;

  if (exclude != NULL)
    debugMsg(COMMUNICATION,
             "broadcastMessage(): from=%u type=%d, msg=%s exclude=%u\n",
             pkg.from, pkg.type, pkg.msg.c_str(), exclude->nodeId);
  else
    debugMsg(COMMUNICATION,
             "broadcastMessage(): from=%u type=%d, msg=%s exclude=NULL\n",
             pkg.from, pkg.type, pkg.msg.c_str());

  if (_connections.size() > 0)
    errCode = true;  // Assume true if at least one connections
  for (auto &&connection : _connections) {
    if (!exclude || connection->nodeId != exclude->nodeId) {
      pkg.dest = connection->nodeId;
      if (!send<painlessmesh::protocol::Broadcast>(connection, pkg))
        errCode = false;  // If any error return 0
    }
  }
  return errCode;
}

