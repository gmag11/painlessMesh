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

//***********************************************************************
String ICACHE_FLASH_ATTR timeSync::buildTimeStamp(timeSyncMessageType_t timeSyncMessageType, uint32_t originateTS, uint32_t receiveTS, uint32_t transmitTS) {
    staticThis->debugMsg(S_TIME, "buildTimeStamp(): Type = %u, t0 = %u, t1 = %u, t2 = %u\n", timeSyncMessageType, originateTS, receiveTS, transmitTS);
#if ARDUINOJSON_VERSION_MAJOR==6
    StaticJsonDocument<75> jsonBuffer;
    JsonObject timeStampObj = jsonBuffer.to<JsonObject>();
#else
    StaticJsonBuffer<75> jsonBuffer;
    JsonObject& timeStampObj = jsonBuffer.createObject();
#endif
    timeStampObj["type"] = (int)timeSyncMessageType;
    if (originateTS > 0)
        timeStampObj["t0"] = originateTS;
    if (receiveTS > 0)
        timeStampObj["t1"] = receiveTS;
    if (transmitTS > 0)
        timeStampObj["t2"] = transmitTS;

    String timeStampStr;
#if ARDUINOJSON_VERSION_MAJOR==6
    serializeJson(timeStampObj, timeStampStr);
#else
    timeStampObj.printTo(timeStampStr);
#endif
    staticThis->debugMsg(S_TIME, "buildTimeStamp(): timeStamp=%s\n", timeStampStr.c_str());

    return timeStampStr;
}

//***********************************************************************
timeSyncMessageType_t ICACHE_FLASH_ATTR timeSync::processTimeStampDelay(String &str) {
    // Extracts and fills timestamp values from json
    timeSyncMessageType_t ret = TIME_SYNC_ERROR;

    staticThis->debugMsg(S_TIME, "processTimeStamp(): str=%s\n", str.c_str());

#if ARDUINOJSON_VERSION_MAJOR==6
    DynamicJsonDocument jsonBuffer(256);
    DeserializationError error = deserializeJson(jsonBuffer, str);
    if (error) {
        staticThis->debugMsg(ERROR, "processTimeStamp(): out of memory1?\n");
        return TIME_SYNC_ERROR;
    }
    JsonObject timeStampObj = jsonBuffer.as<JsonObject>();
#else
    DynamicJsonBuffer jsonBuffer(75);
    JsonObject& timeStampObj = jsonBuffer.parseObject(str);
    if (!timeStampObj.success()) {
        staticThis->debugMsg(ERROR, "processTimeStamp(): out of memory1?\n");
        return TIME_SYNC_ERROR;
    }
#endif

    ret = static_cast<timeSyncMessageType_t>(timeStampObj["type"].as<int>());
    if (ret == TIME_REQUEST || ret == TIME_RESPONSE) {
        timeDelay[0] = timeStampObj["t0"].as<uint32_t>();
    }
    if (ret == TIME_RESPONSE) {
        timeDelay[1] = timeStampObj["t1"].as<uint32_t>();
        timeDelay[2] = timeStampObj["t2"].as<uint32_t>();
    }
    return ret; // return type of sync message

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

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::startTimeSync(std::shared_ptr<MeshConnection> conn) {
    String timeStamp;

    debugMsg(S_TIME, "startTimeSync(): with %u, local port: %d\n", conn->nodeId, conn->client->getLocalPort());
    auto adopt = adoptionCalc(conn);
    if (adopt) {
        timeStamp = conn->time.buildTimeStamp(TIME_REQUEST, getNodeTime()); // Ask other party its time
        debugMsg(S_TIME, "startTimeSync(): Requesting %u to adopt our time\n", conn->nodeId);
    } else {
        timeStamp = conn->time.buildTimeStamp(TIME_SYNC_REQUEST); // Tell other party to ask me the time
        debugMsg(S_TIME, "startTimeSync(): Requesting time from %u\n", conn->nodeId);
    }
    sendMessage(conn, conn->nodeId, _nodeId, TIME_SYNC, timeStamp, true);
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
void ICACHE_FLASH_ATTR painlessMesh::handleTimeSync(std::shared_ptr<MeshConnection> conn, JsonObject& root, uint32_t receivedAt) {
    auto timeSyncMessageType = static_cast<timeSyncMessageType_t>(root["msg"]["type"].as<int>());
    String msg;

    switch (timeSyncMessageType) {
    case (TIME_SYNC_ERROR):
        debugMsg(S_TIME, "handleTimeSync(): Received time sync error. Restarting time sync.\n");
        conn->timeSyncTask.forceNextIteration();
        break;
    case (TIME_SYNC_REQUEST):  // Other party request me to ask it for time
        debugMsg(S_TIME, "handleTimeSync(): Received requesto to start TimeSync with node: %u\n", conn->nodeId);
        root["msg"]["type"] = static_cast<int>(TIME_REQUEST);
        root["msg"]["t0"] = getNodeTime();
        msg = root["msg"].as<String>();
        staticThis->sendMessage(conn, conn->nodeId, _nodeId, TIME_SYNC, msg, true);
        break;

    case (TIME_REQUEST):
        root["msg"]["type"] = static_cast<int>(TIME_RESPONSE);
        root["msg"]["t1"] = receivedAt;
        root["msg"]["t2"] = getNodeTime();
        msg = root["msg"].as<String>();
        staticThis->sendMessage(conn, conn->nodeId, _nodeId, TIME_SYNC, msg, true);

        // Build time response
        debugMsg(S_TIME, "handleTimeSync(): Response sent %s\n", msg.c_str());
        debugMsg(S_TIME, "handleTimeSync(): timeSyncStatus with %u completed\n", conn->nodeId);

        // After response is sent I assume sync is completed
        conn->timeSyncTask.delay(TIME_SYNC_INTERVAL);
        break;

    case (TIME_RESPONSE):
        debugMsg(S_TIME, "handleTimeSync(): TIME RESPONSE received.\n");
        uint32_t times[NUMBER_OF_TIMESTAMPS] = {
            root["msg"]["t0"],
            root["msg"]["t1"],
            root["msg"]["t2"],
            receivedAt};

        int32_t offset = conn->time.calcAdjustment(times); // Adjust time and get calculated offset

        // flag all connections for re-timeSync
        if (nodeTimeAdjustedCallback) {
            nodeTimeAdjustedCallback(offset);
        }

        if (offset < TIME_SYNC_ACCURACY && offset > -TIME_SYNC_ACCURACY) {
            // mark complete only if offset was less than 10 ms
            conn->timeSyncTask.delay(TIME_SYNC_INTERVAL);
            debugMsg(S_TIME, "handleTimeSync(): timeSyncStatus with %u completed\n", conn->nodeId);

            // Time has changed, update other nodes
            for (auto &&connection : _connections) {
                if (connection->nodeId != conn->nodeId) {  // exclude this connection
                    connection->timeSyncTask.forceNextIteration();
                    debugMsg(S_TIME, "handleTimeSync(): timeSyncStatus with %u brought forward\n", connection->nodeId);
                }
            }
        } else {
            // Iterate sync procedure if accuracy was not enough
            conn->timeSyncTask.delay(200*TASK_MILLISECOND); // Small delay
            debugMsg(S_TIME, "handleTimeSync(): timeSyncStatus with %u needs further tries\n", conn->nodeId);

        }
        break;
    }

    debugMsg(S_TIME, "handleTimeSync(): ----------------------------------\n");

}

void ICACHE_FLASH_ATTR painlessMesh::handleTimeDelay(std::shared_ptr<MeshConnection> conn, JsonObject& root, uint32_t receivedAt) {
    String timeStamp = root["msg"];
    uint32_t from = root["from"];
    debugMsg(S_TIME, "handleTimeDelay(): from %u in timestamp = %s\n", from, timeStamp.c_str());

    timeSyncMessageType_t timeSyncMessageType = conn->time.processTimeStampDelay(timeStamp); // Extract timestamps and get type of message

    String t_stamp;

    switch (timeSyncMessageType) {
    case (TIME_SYNC_ERROR):
        debugMsg(ERROR, "handleTimeDelay(): Error in requesting time delay. Please try again.\n");
        break;

    case (TIME_REQUEST):
        //conn->timeSyncStatus == IN_PROGRESS;
        debugMsg(S_TIME, "handleTimeDelay(): TIME REQUEST received.\n");

        // Build time response
        t_stamp = conn->time.buildTimeStamp(TIME_RESPONSE, conn->time.timeDelay[0], receivedAt, getNodeTime());
        staticThis->sendMessage(conn, from, _nodeId, TIME_DELAY, t_stamp);

        debugMsg(S_TIME, "handleTimeDelay(): Response sent %s\n", t_stamp.c_str());

        // After response is sent I assume sync is completed
        //conn->timeSyncStatus == COMPLETE;
        //conn->lastTimeSync = getNodeTime();
        break;

    case (TIME_RESPONSE):
        {
            debugMsg(S_TIME, "handleTimeDelay(): TIME RESPONSE received.\n");
            conn->time.timeDelay[3] = receivedAt; // Calculate fourth timestamp (response received time)

            int32_t delay = conn->time.delayCalc(); // Adjust time and get calculated offset
            debugMsg(S_TIME, "handleTimeDelay(): Delay is %d\n", delay);

            //conn->timeSyncStatus == COMPLETE;

            if (nodeDelayReceivedCallback)
                nodeDelayReceivedCallback(from, delay);
        }
        break;

    default:
        debugMsg(S_TIME, "handleTimeDelay(): Unknown timeSyncMessageType received. Ignoring for now.\n");
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
