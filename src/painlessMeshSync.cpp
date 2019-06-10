#include "painlessMesh.h"

#include "time.h"

extern LogClass Log;

void ICACHE_FLASH_ATTR
painlessMesh::startTimeSync(std::shared_ptr<MeshConnection> conn) {
  using namespace painlessmesh;
  Log(S_TIME, "startTimeSync(): with %u\n", conn->nodeId);
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
  router::send<protocol::TimeSync, MeshConnection>(timeSync, conn, true);
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
  bool ap = !conn->station;

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
