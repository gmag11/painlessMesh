#include "painlessMesh.h"

#include "time.h"

extern painlessMesh* staticThis;
extern LogClass Log;

void ICACHE_FLASH_ATTR
painlessMesh::startTimeSync(std::shared_ptr<MeshConnection> conn) {
  Log(S_TIME, "startTimeSync(): with %u, local port: %d\n", conn->nodeId,
      conn->client->getLocalPort());
  auto adopt = adoptionCalc(conn);
  painlessmesh::protocol::TimeSync timeSync;
  if (adopt) {
    timeSync =
        painlessmesh::protocol::TimeSync(nodeId, conn->nodeId, getNodeTime());
    Log(S_TIME, "startTimeSync(): Requesting %u to adopt our time\n",
        conn->nodeId);
  } else {
    timeSync = painlessmesh::protocol::TimeSync(nodeId, conn->nodeId);
    Log(S_TIME, "startTimeSync(): Requesting time from %u\n", conn->nodeId);
  }
  send<painlessmesh::protocol::TimeSync>(conn, timeSync, true);
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::adoptionCalc(std::shared_ptr<MeshConnection> conn) {
  using namespace painlessmesh;
  if (conn == NULL)  // Missing connection
    return false;
  // make the adoption calulation. Figure out how many nodes I am connected to
  // exclusive of this connection.

  // We use length as an indicator for how many subconnections both nodes have
  uint16_t mySubCount = layout::size(layout::excludeRoute(
      this->asNodeTree(), conn->nodeId));           // exclude this connection.
  uint16_t remoteSubCount = layout::size((*conn));  // exclude this connection.
  bool ap = conn->client->getLocalPort() == _meshPort;

  // ToDo. Simplify this logic
  bool ret = (mySubCount > remoteSubCount) ? false : true;
  if (mySubCount == remoteSubCount && ap) {  // in case of draw, ap wins
    ret = false;
    }

    Log(S_TIME,
        "adoptionCalc(): mySubCount=%d remoteSubCount=%d role=%s adopt=%s\n",
        mySubCount, remoteSubCount, ap ? "AP" : "STA", ret ? "true" : "false");

    return ret;
}

void ICACHE_FLASH_ATTR painlessMesh::syncSubConnections(uint32_t changedId) {
  Log(SYNC, "syncSubConnections(): changedId = %u\n", changedId);
  for (auto&& connection : this->subs) {
    if (connection->connected && !connection->newConnection &&
        connection->nodeId != 0 &&
        connection->nodeId != changedId) {  // Exclude current
      connection->nodeSyncTask.forceNextIteration();
    }
    }
    staticThis->stability /= 2;
}
