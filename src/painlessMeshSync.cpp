#include "painlessMesh.h"
#include "painlessMeshSync.h"
#include "painlessMeshJson.h"

#include "time.h"

extern painlessMesh* staticThis;
uint32_t timeAdjuster = 0;

// timeSync Functions
//***********************************************************************
uint32_t ICACHE_FLASH_ATTR painlessMesh::getNodeTime(void) {
    auto base_time = micros();
    uint32_t ret = base_time + timeAdjuster;
    debugMsg(GENERAL, "getNodeTime(): time=%u\n", ret);
    return ret;
}

int ICACHE_FLASH_ATTR timeSync::processTimeStampDelay(
    painlessmesh::protocol::TimeDelay timeDelayPkg) {
  // REFACTOR Do we need to store these values?
  this->timeDelay[0] = timeDelayPkg.msg.t0;
  this->timeDelay[1] = timeDelayPkg.msg.t1;
  this->timeDelay[2] = timeDelayPkg.msg.t2;
  return timeDelayPkg.msg.type;
}

//***********************************************************************
int32_t ICACHE_FLASH_ATTR timeSync::calcAdjustment(uint32_t times[NUMBER_OF_TIMESTAMPS]) {
    staticThis->debugMsg(S_TIME, "calcAdjustment(): Start calculation. t0 = %u, t1 = %u, t2 = %u, t3 = %u\n", times[0], times[1], times[2], times[3]);

    if (times[0] == 0 || times[1] == 0 || times[2] == 0 || times[3] == 0) {
        // if any value is 0 
        staticThis->debugMsg(ERROR, "calcAdjustment(): TimeStamp error.\n");
        return 0x7FFFFFFF; // return max value
    }

    // We use the SNTP protocol https://en.wikipedia.org/wiki/Network_Time_Protocol#Clock_synchronization_algorithm.
    uint32_t offset = ((int32_t)(times[1] - times[0]) / 2) + ((int32_t)(times[2] - times[3]) / 2);


    if (offset < TASK_SECOND && offset > 4)
        timeAdjuster += offset/4; // Take small steps to avoid over correction 
    else 
        timeAdjuster += offset; // Accumulate offset

    staticThis->debugMsg(S_TIME, 
            "calcAdjustment(): Calculated offset %d us.\n", offset);
    staticThis->debugMsg(S_TIME, "calcAdjustment(): New adjuster = %u. New time = %u\n", timeAdjuster, staticThis->getNodeTime());

    return offset; // return offset to decide if sync is OK
}

//***********************************************************************
int32_t ICACHE_FLASH_ATTR timeSync::delayCalc() {
    staticThis->debugMsg(S_TIME, "delayCalc(): Start calculation. t0 = %u, t1 = %u, t2 = %u, t3 = %u\n", timeDelay[0], timeDelay[1], timeDelay[2], timeDelay[3]);

    if (timeDelay[0] == 0 || timeDelay[1] == 0 || timeDelay[2] == 0 || timeDelay[3] == 0) {
        // if any value is 0 
        staticThis->debugMsg(ERROR, "delayCalc(): TimeStamp error.\n");
        return -1; // return max value
    }

    // We use the SNTP protocol https://en.wikipedia.org/wiki/Network_Time_Protocol#Clock_synchronization_algorithm.
    uint32_t tripDelay = ((timeDelay[3] - timeDelay[0]) - (timeDelay[2] - timeDelay[1]))/2;

    staticThis->debugMsg(S_TIME, "delayCalc(): Calculated Network delay %d us\n", tripDelay);

    return tripDelay;
}


//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::handleNodeSync(std::shared_ptr<MeshConnection> conn, JsonObject& root) {
    debugMsg(SYNC, "handleNodeSync(): with %u\n", conn->nodeId);

    meshPackageType message_type = (meshPackageType)(int)root["type"];
    uint32_t        remoteNodeId = root["from"];

    if (remoteNodeId == 0) {
        debugMsg(ERROR, "handleNodeSync(): received invalid remote nodeId\n");
        return;
    }

    if (conn->newConnection) {
        // There is a small but significant probability that we get connected twice to the 
        // same node, e.g. if scanning happened while sub connection data was incomplete.
        auto oldConnection = findConnection(remoteNodeId);
        if (oldConnection) {
            debugMsg(SYNC, "handleNodeSync(): already connected to %u. Closing the new connection \n", remoteNodeId);
            conn->close();
            return;
        }

        // 
        debugMsg(SYNC, "handleNodeSync(): conn->nodeId updated from %u to %u\n", conn->nodeId, remoteNodeId);
        conn->nodeId = remoteNodeId;
        // TODO: Move this to its own function
        newConnectionTask.set(TASK_SECOND, TASK_ONCE, [remoteNodeId]() {
            staticThis->debugMsg(CONNECTION, "newConnectionTask():\n");
            staticThis->debugMsg(CONNECTION, "newConnectionTask(): adding %u now= %u\n", remoteNodeId, staticThis->getNodeTime());
            if (staticThis->newConnectionCallback)
                staticThis->newConnectionCallback(remoteNodeId); // Connection dropped. Signal user            
        });

        _scheduler.addTask(newConnectionTask);
        newConnectionTask.enable();

        // Initially interval is every 10 seconds, 
        // this will slow down to TIME_SYNC_INTERVAL
        // after first succesfull sync
        conn->timeSyncTask.set(10*TASK_SECOND, TASK_FOREVER,
                [conn]() {
            staticThis->debugMsg(S_TIME,
                "timeSyncTask(): %u\n", conn->nodeId);
            staticThis->startTimeSync(conn);
        });
        _scheduler.addTask(conn->timeSyncTask);
        if (conn->station)
            // We are STA, request time immediately
            conn->timeSyncTask.enable();
        else
            // We are the AP, give STA the change to initiate time sync 
            conn->timeSyncTask.enableDelayed();
        conn->newConnection = false;
    }

    if (conn->nodeId != remoteNodeId) {
        debugMsg(SYNC, "handleNodeSync(): Changed nodeId %u, closing connection %u.\n",
                conn->nodeId, remoteNodeId);
        conn->close();
        return;
    }


    // check to see if subs have changed.
    String inComingSubs = root["subs"];
    bool changed = !conn->subConnections.equals(inComingSubs);
    painlessmesh::parseNodeSyncRoot(conn, root, changed);
    if (changed) {  // change in the network
        // Check whether we already know any of the nodes
        // This is necessary to avoid loops.. Not sure if we need to check
        // for both this node and master node, but better safe than sorry
        if ( painlessmesh::stringContainsNumber(inComingSubs, String(conn->nodeId)) || 
                painlessmesh::stringContainsNumber(inComingSubs, String(this->_nodeId))) {
            // This node is also in the incoming subs, so we have a loop
            // Disconnecting to break the loop
            debugMsg(SYNC, "handleNodeSync(): Loop detected, disconnecting %u.\n",
                    remoteNodeId);
            conn->close();
            return;
        }

        debugMsg(SYNC, "handleNodeSync(): Changed connections %u.\n",
                remoteNodeId);
        conn->subConnections = inComingSubs;
        if (changedConnectionsCallback)
            changedConnectionsCallback();

        staticThis->syncSubConnections(conn->nodeId);
    } else {
        stability += min(1000-stability,(size_t)25);
    }
    
    debugMsg(SYNC, "handleNodeSync(): json = %s\n", inComingSubs.c_str());

    switch (message_type) {
    case NODE_SYNC_REQUEST:
    {
        debugMsg(SYNC, "handleNodeSync(): valid NODE_SYNC_REQUEST %u sending NODE_SYNC_REPLY\n", conn->nodeId);
        String myOtherSubConnections = subConnectionJson(conn);
        sendMessage(conn, conn->nodeId, _nodeId, NODE_SYNC_REPLY, myOtherSubConnections, true);
        break;
    }
    case NODE_SYNC_REPLY:
        debugMsg(SYNC, "handleNodeSync(): valid NODE_SYNC_REPLY from %u\n", conn->nodeId);
        break;
    default:
        debugMsg(ERROR, "handleNodeSync(): weird type? %d\n", message_type);
    }
}

void ICACHE_FLASH_ATTR
painlessMesh::startTimeSync(std::shared_ptr<MeshConnection> conn) {
  debugMsg(S_TIME, "startTimeSync(): with %u, local port: %d\n", conn->nodeId,
           conn->client->getLocalPort());
  auto adopt = adoptionCalc(conn);
  painlessmesh::protocol::TimeSync timeSync;
  if (adopt) {
    timeSync =
        painlessmesh::protocol::TimeSync(_nodeId, conn->nodeId, getNodeTime());
    debugMsg(S_TIME, "startTimeSync(): Requesting %u to adopt our time\n",
             conn->nodeId);
  } else {
    timeSync = painlessmesh::protocol::TimeSync(_nodeId, conn->nodeId);
    debugMsg(S_TIME, "startTimeSync(): Requesting time from %u\n",
             conn->nodeId);
  }
  send<painlessmesh::protocol::TimeSync>(conn, timeSync, true);
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::adoptionCalc(std::shared_ptr<MeshConnection> conn) {
    if (conn == NULL) // Missing connection
        return false;
    // make the adoption calulation. Figure out how many nodes I am connected to exclusive of this connection.

    // We use length as an indicator for how many subconnections both nodes have
    uint16_t mySubCount = subConnectionJson(conn).length();  //exclude this connection.
    uint16_t remoteSubCount = conn->subConnections.length();
    bool ap = conn->client->getLocalPort() == _meshPort;

    // ToDo. Simplify this logic
    bool ret = (mySubCount > remoteSubCount) ? false : true;
    if (mySubCount == remoteSubCount && ap) { // in case of draw, ap wins
        ret = false;
    }

    debugMsg(S_TIME, "adoptionCalc(): mySubCount=%d remoteSubCount=%d role=%s adopt=%s\n", mySubCount, remoteSubCount, ap ? "AP" : "STA", ret ? "true" : "false");

    return ret;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::handleTimeSync(
    std::shared_ptr<MeshConnection> conn,
    painlessmesh::protocol::TimeSync timeSync, uint32_t receivedAt) {
  switch (timeSync.msg.type) {
    case (painlessmesh::protocol::TIME_SYNC_ERROR):
      debugMsg(ERROR,
               "handleTimeSync(): Received time sync error. Restarting time "
               "sync.\n");
      conn->timeSyncTask.forceNextIteration();
      break;
    case (painlessmesh::protocol::TIME_SYNC_REQUEST):  // Other party request me
                                                       // to ask it for time
      debugMsg(S_TIME,
               "handleTimeSync(): Received requesto to start TimeSync with "
               "node: %u\n",
               conn->nodeId);
      timeSync.reply(getNodeTime());
      staticThis->send<painlessmesh::protocol::TimeSync>(conn, timeSync, true);
      break;

    case (painlessmesh::protocol::TIME_REQUEST):
      timeSync.reply(receivedAt, getNodeTime());
      staticThis->send<painlessmesh::protocol::TimeSync>(conn, timeSync, true);

      debugMsg(S_TIME, "handleTimeSync(): timeSyncStatus with %u completed\n",
               conn->nodeId);

      // After response is sent I assume sync is completed
      conn->timeSyncTask.delay(TIME_SYNC_INTERVAL);
      break;

    case (painlessmesh::protocol::TIME_REPLY): {
      debugMsg(S_TIME, "handleTimeSync(): TIME RESPONSE received.\n");
      uint32_t times[NUMBER_OF_TIMESTAMPS] = {timeSync.msg.t0, timeSync.msg.t1,
                                              timeSync.msg.t2, receivedAt};

      int32_t offset = conn->time.calcAdjustment(
          times);  // Adjust time and get calculated offset

      // flag all connections for re-timeSync
      if (nodeTimeAdjustedCallback) {
        nodeTimeAdjustedCallback(offset);
      }

      if (offset < TIME_SYNC_ACCURACY && offset > -TIME_SYNC_ACCURACY) {
        // mark complete only if offset was less than 10 ms
        conn->timeSyncTask.delay(TIME_SYNC_INTERVAL);
        debugMsg(S_TIME, "handleTimeSync(): timeSyncStatus with %u completed\n",
                 conn->nodeId);

        // Time has changed, update other nodes
        for (auto&& connection : _connections) {
          if (connection->nodeId != conn->nodeId) {  // exclude this connection
            connection->timeSyncTask.forceNextIteration();
            debugMsg(
                S_TIME,
                "handleTimeSync(): timeSyncStatus with %u brought forward\n",
                connection->nodeId);
          }
        }
      } else {
        // Iterate sync procedure if accuracy was not enough
        conn->timeSyncTask.delay(200 * TASK_MILLISECOND);  // Small delay
        debugMsg(
            S_TIME,
            "handleTimeSync(): timeSyncStatus with %u needs further tries\n",
            conn->nodeId);
      }
      break;
    }
    default:
      debugMsg(ERROR, "handleTimeSync(): unkown type %u, %u\n",
               timeSync.msg.type, painlessmesh::protocol::TIME_SYNC_REQUEST);
      break;
  }

  debugMsg(S_TIME, "handleTimeSync(): ----------------------------------\n");
}

void ICACHE_FLASH_ATTR painlessMesh::handleTimeDelay(
    std::shared_ptr<MeshConnection> conn,
    painlessmesh::protocol::TimeDelay timeDelay, uint32_t receivedAt) {
  debugMsg(S_TIME, "handleTimeDelay(): from %u in timestamp\n", timeDelay.from);

  conn->time.processTimeStampDelay(timeDelay);

  switch (timeDelay.msg.type) {
    case (painlessmesh::protocol::TIME_SYNC_ERROR):
      debugMsg(ERROR,
               "handleTimeDelay(): Error in requesting time delay. Please try "
               "again.\n");
      break;

    case (painlessmesh::protocol::TIME_REQUEST):
      // conn->timeSyncStatus == IN_PROGRESS;
      debugMsg(S_TIME, "handleTimeDelay(): TIME REQUEST received.\n");

      // Build time response
      timeDelay.reply(receivedAt, getNodeTime());
      staticThis->send<painlessmesh::protocol::TimeDelay>(conn, timeDelay);
      break;

    case (painlessmesh::protocol::TIME_REPLY): {
      debugMsg(S_TIME, "handleTimeDelay(): TIME RESPONSE received.\n");
      conn->time.timeDelay[3] =
          receivedAt;  // Calculate fourth timestamp (response received time)

      int32_t delay =
          conn->time.delayCalc();  // Adjust time and get calculated offset
      debugMsg(S_TIME, "handleTimeDelay(): Delay is %d\n", delay);

      // conn->timeSyncStatus == COMPLETE;

      if (nodeDelayReceivedCallback)
        nodeDelayReceivedCallback(timeDelay.from, delay);
    } break;

    default:
      debugMsg(ERROR,
               "handleTimeDelay(): Unknown timeSyncMessageType received. "
               "Ignoring for now.\n");
  }

  debugMsg(S_TIME, "handleTimeSync(): ----------------------------------\n");
}

void ICACHE_FLASH_ATTR painlessMesh::syncSubConnections(uint32_t changedId) {
    debugMsg(SYNC, "syncSubConnections(): changedId = %u\n", changedId);
    for (auto &&connection : _connections) {
        if (connection->connected &&
                !connection->newConnection &&
                connection->nodeId != 0 && 
                connection->nodeId != changedId) { // Exclude current
            connection->nodeSyncTask.forceNextIteration();
        }
    }
    staticThis->stability /= 2;
}
