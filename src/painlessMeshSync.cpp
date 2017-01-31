#include <Arduino.h>
#include <SimpleList.h>
#include <ArduinoJson.h>

#include "painlessMesh.h"
#include "painlessMeshSync.h"

extern painlessMesh* staticThis;
uint32_t timeAdjuster = 0;

// timeSync Functions
//***********************************************************************
uint32_t ICACHE_FLASH_ATTR painlessMesh::getNodeTime(void) {
    uint32_t ret = system_get_time() + timeAdjuster;

    debugMsg(GENERAL, "getNodeTime(): time=%u\n", ret);

    return ret;
}

//***********************************************************************
String ICACHE_FLASH_ATTR timeSync::buildTimeStamp(timeSyncMessageType_t timeSyncMessageType, uint32_t originateTS, uint32_t receiveTS, uint32_t transmitTS) {
    staticThis->debugMsg(S_TIME, "buildTimeStamp(): Type = %u, t0 = %u, t1 = %u, t2 = %u\n", timeSyncMessageType, originateTS, receiveTS, transmitTS);
    StaticJsonBuffer<75> jsonBuffer;
    JsonObject& timeStampObj = jsonBuffer.createObject();
    timeStampObj["type"] = (int)timeSyncMessageType;
    if (originateTS > 0)
        timeStampObj["t0"] = originateTS;
    if (receiveTS > 0)
        timeStampObj["t1"] = receiveTS;
    if (transmitTS > 0)
        timeStampObj["t2"] = transmitTS;

    String timeStampStr;
    timeStampObj.printTo(timeStampStr);
    staticThis->debugMsg(S_TIME, "buildTimeStamp(): timeStamp=%s\n", timeStampStr.c_str());

    return timeStampStr;
}

//***********************************************************************
timeSyncMessageType_t ICACHE_FLASH_ATTR timeSync::processTimeStamp(String &str) {
    // Extracts and fills timestamp values from json
    timeSyncMessageType_t ret = TIME_SYNC_ERROR;

    staticThis->debugMsg(S_TIME, "processTimeStamp(): str=%s\n", str.c_str());

    DynamicJsonBuffer jsonBuffer(75);
    JsonObject& timeStampObj = jsonBuffer.parseObject(str);
    if (!timeStampObj.success()) {
        staticThis->debugMsg(ERROR, "processTimeStamp(): out of memory1?\n");
        return TIME_SYNC_ERROR;
    }

    ret = static_cast<timeSyncMessageType_t>(timeStampObj.get<int>("type"));
    if (ret == TIME_REQUEST || ret == TIME_RESPONSE) {
        times[0] = timeStampObj.get<uint32_t>("t0");
    }
    if (ret == TIME_RESPONSE) {
        times[1] = timeStampObj.get<uint32_t>("t1");
        times[2] = timeStampObj.get<uint32_t>("t2");
    }
    return ret; // return type of sync message

}

//***********************************************************************
timeSyncMessageType_t ICACHE_FLASH_ATTR timeSync::processTimeStampDelay(String &str) {
    // Extracts and fills timestamp values from json
    timeSyncMessageType_t ret = TIME_SYNC_ERROR;

    staticThis->debugMsg(S_TIME, "processTimeStamp(): str=%s\n", str.c_str());

    DynamicJsonBuffer jsonBuffer(75);
    JsonObject& timeStampObj = jsonBuffer.parseObject(str);
    if (!timeStampObj.success()) {
        staticThis->debugMsg(ERROR, "processTimeStamp(): out of memory1?\n");
        return TIME_SYNC_ERROR;
    }

    ret = static_cast<timeSyncMessageType_t>(timeStampObj.get<int>("type"));
    if (ret == TIME_REQUEST || ret == TIME_RESPONSE) {
        timeDelay[0] = timeStampObj.get<uint32_t>("t0");
    }
    if (ret == TIME_RESPONSE) {
        timeDelay[1] = timeStampObj.get<uint32_t>("t1");
        timeDelay[2] = timeStampObj.get<uint32_t>("t2");
    }
    return ret; // return type of sync message

}

//***********************************************************************
int32_t ICACHE_FLASH_ATTR timeSync::calcAdjustment() {
    staticThis->debugMsg(S_TIME, "calcAdjustment(): Start calculation. t0 = %u, t1 = %u, t2 = %u, t3 = %u\n", times[0], times[1], times[2], times[3]);

    if (times[0] == 0 || times[1] == 0 || times[2] == 0 || times[3] == 0) {
        // if any value is 0 
        staticThis->debugMsg(ERROR, "calcAdjustment(): TimeStamp error.\n");
        return 0xFFFFFFFF; // return max value
    }


    // This calculation algorithm is got from SNTP protocol https://en.wikipedia.org/wiki/Network_Time_Protocol#Clock_synchronization_algorithm
    uint32_t offset = ((int32_t)(times[1] - times[0]) / 2) + ((int32_t)(times[2] - times[3]) / 2);

    uint32_t tripDelay = (times[3] - times[0]) - (times[2] - times[1]);

    timeAdjuster += offset; // Accumulate offset
    staticThis->debugMsg(S_TIME, "calcAdjustment(): Calculated offset %d us. Network delay %d us\n", offset, tripDelay);
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


    // This calculation algorithm is got from SNTP protocol https://en.wikipedia.org/wiki/Network_Time_Protocol#Clock_synchronization_algorithm

    uint32_t tripDelay = ((timeDelay[3] - timeDelay[0]) - (timeDelay[2] - timeDelay[1]))/2;

    staticThis->debugMsg(S_TIME, "delayCalc(): Calculated Network delay %d us\n", tripDelay);

    return tripDelay;
}




// painlessMesh Syncing functions
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::startNodeSync(meshConnectionType *conn) {

    debugMsg(SYNC, "startNodeSync(): with %d\n", conn->nodeId);
    String subs = subConnectionJson(conn);
    sendMessage(conn, conn->nodeId, _nodeId, NODE_SYNC_REQUEST, subs, true);
    //conn->nodeSyncLastRequested = getNodeTime();
    conn->nodeSyncLastRequested = system_get_time(); // Using nodeTime may cause problems when node updates time.
    conn->nodeSyncStatus = IN_PROGRESS;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::handleNodeSync(meshConnectionType *conn, JsonObject& root) {
    debugMsg(SYNC, "handleNodeSync(): with %d\n", conn->nodeId);

    meshPackageType message_type = (meshPackageType)(int)root["type"];
    uint32_t        remoteNodeId = (uint32_t)root["from"];
    uint32_t        destId = (uint32_t)root["dest"];
    bool            reSyncAllSubConnections = false;

    if ((destId == 0) && (findConnection(remoteNodeId) != NULL)) {
        // this is the first NODE_SYNC_REQUEST from a station
        // is we are already connected drop this connection
        debugMsg(SYNC, "handleNodeSync(): Already connected to node %d.  Dropping\n", conn->nodeId);
        closeConnection(conn);
        return;
    }

    if (conn->nodeId != remoteNodeId) {
        debugMsg(SYNC, "handleNodeSync(): conn->nodeId updated from %d to %d\n", conn->nodeId, remoteNodeId);
        conn->nodeId = remoteNodeId;

    }

    // check to see if subs have changed.
    String inComingSubs = root["subs"];
    if (!conn->subConnections.equals(inComingSubs)) {  // change in the network
        reSyncAllSubConnections = true;
        conn->subConnections = inComingSubs;
        if (changedConnectionsCallback)
            changedConnectionsCallback();
    }
    String tempstr;
    root.printTo(tempstr);
    debugMsg(SYNC, "handleNodeSync(): json = %s\n", tempstr.c_str());

    switch (message_type) {
    case NODE_SYNC_REQUEST:
    {
        debugMsg(SYNC, "handleNodeSync(): valid NODE_SYNC_REQUEST %d sending NODE_SYNC_REPLY\n", conn->nodeId);
        String myOtherSubConnections = subConnectionJson(conn);
        sendMessage(conn, conn->nodeId, _nodeId, NODE_SYNC_REPLY, myOtherSubConnections, true);
        break;
    }
    case NODE_SYNC_REPLY:
        debugMsg(SYNC, "handleNodeSync(): valid NODE_SYNC_REPLY from %d\n", conn->nodeId);
        conn->nodeSyncLastRequested = 0;  //reset nodeSyncLastRequested Timer
        if (conn->lastTimeSync == 0) {
            debugMsg(S_TIME, "handleNodeSync(): timeSyncStatus changed to NEEDED (1)\n");
            //startTimeSync(conn); // TimeSync is started on manageConnections() because status is set to NEEDED
            //conn->timeSyncStatus = IN_PROGRESS;
            conn->timeSyncStatus = NEEDED;
        }
        break;
    default:
        debugMsg(ERROR, "handleNodeSync(): weird type? %d\n", message_type);
    }

    if (reSyncAllSubConnections == true) {
        SimpleList<meshConnectionType>::iterator connection = _connections.begin();
        while (connection != _connections.end()) {
            if (connection != conn) { // Exclude current
                connection->nodeSyncStatus = NEEDED;
            }
            connection++;
        }
    }

    conn->nodeSyncStatus = COMPLETE;  // mark this connection nodeSync'd
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::startTimeSync(meshConnectionType *conn, boolean checkAdopt) {
    boolean adopt = true; // default, adopt time
    String timeStamp;

    debugMsg(S_TIME, "startTimeSync(): with %d, local port: %d\n", conn->nodeId, conn->esp_conn->proto.tcp->local_port);

    if (checkAdopt) {
        adopt = adoptionCalc(conn);
    }
    if (adopt) {
        timeStamp = conn->time.buildTimeStamp(TIME_REQUEST, getNodeTime()); // Ask other party its time
        debugMsg(S_TIME, "startTimeSync(): timeSyncStatus changed to IN_PROGRESS\n", conn->nodeId);
        conn->timeSyncStatus = IN_PROGRESS;
    } else {
        timeStamp = conn->time.buildTimeStamp(TIME_SYNC_REQUEST); // Tell other party to ask me the time
        debugMsg(S_TIME, "startTimeSync(): timeSyncStatus changed to COMPLETE\n", conn->nodeId);
        conn->timeSyncStatus = COMPLETE;
    }
    sendMessage(conn, conn->nodeId, _nodeId, TIME_SYNC, timeStamp, true);
    conn->timeSyncLastRequested = system_get_time(); // It is compared in manageConnections() to check response for timeout
                                                     // Using nodeTime may cause problems when node updates time because counter discontinuity.
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::adoptionCalc(meshConnectionType *conn) {
    // make the adoption calulation. Figure out how many nodes I am connected to exclusive of this connection.

    uint16_t mySubCount = connectionCount(conn);  //exclude this connection.
    uint16_t remoteSubCount = jsonSubConnCount(conn->subConnections);
    bool ap = conn->esp_conn->proto.tcp->local_port == _meshPort;

    // ToDo. Simplify this logic
    bool ret = (mySubCount > remoteSubCount) ? false : true;
    if (mySubCount == remoteSubCount && ap) { // in case of withdraw, ap wins
        ret = false;
    }

    debugMsg(S_TIME, "adoptionCalc(): mySubCount=%d remoteSubCount=%d role=%s adopt=%s\n", mySubCount, remoteSubCount, ap ? "AP" : "STA", ret ? "true" : "false");

    return ret;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::handleTimeSync(meshConnectionType *conn, JsonObject& root, uint32_t receivedAt) {
    String timeStamp = root["msg"];
    debugMsg(S_TIME, "handleTimeSync(): with %d in timestamp=%s\n", conn->nodeId, timeStamp.c_str());
    debugMsg(S_TIME, "handleTimeSync(): local ip: %d.%d.%d.%d, local port = %d\n", IP2STR(conn->esp_conn->proto.tcp->local_ip), conn->esp_conn->proto.tcp->local_port);
    debugMsg(S_TIME, "handleTimeSync(): remote ip: %d.%d.%d.%d, remote port = %d\n", IP2STR(conn->esp_conn->proto.tcp->remote_ip), conn->esp_conn->proto.tcp->remote_port);

    timeSyncMessageType_t timeSyncMessageType = conn->time.processTimeStamp(timeStamp); // Extract timestamps and get type of message

    String t_stamp;

    switch (timeSyncMessageType) {
    case (TIME_SYNC_REQUEST):  // Other party request me to ask it for time
        debugMsg(S_TIME, "handleTimeSync(): Received requesto to start TimeSync. Status = %d\n", conn->timeSyncStatus);
        if (conn->timeSyncStatus != IN_PROGRESS) {
            startTimeSync(conn, false); // Start time sync only if I was not syncing yet
        } else {
            debugMsg(S_TIME, "handleTimeSync(): Already syncing. Request ignored\n");
        }
        break;

    case (TIME_REQUEST):

        conn->timeSyncStatus == IN_PROGRESS;
        debugMsg(S_TIME, "handleTimeSync(): TIME REQUEST received. T0 = %d\n", conn->time.times[0]);

        // Build time response
        t_stamp = conn->time.buildTimeStamp(TIME_RESPONSE, conn->time.times[0], receivedAt, getNodeTime());
        staticThis->sendMessage(conn, conn->nodeId, _nodeId, TIME_SYNC, t_stamp, true);

        debugMsg(S_TIME, "handleTimeSync(): Response sent %s\n", t_stamp.c_str());
        debugMsg(S_TIME, "handleTimeSync(): timeSyncStatus with %d changed to COMPLETE\n", conn->nodeId);

        // After response is sent I assume sync is completed
        conn->timeSyncStatus == COMPLETE;
        conn->lastTimeSync = getNodeTime();
        break;

    case (TIME_RESPONSE):
        debugMsg(S_TIME, "handleTimeSync(): TIME RESPONSE received.\n");
        if (conn->timeSyncStatus == IN_PROGRESS) {
            conn->time.times[3] = receivedAt; // Calculate fourth timestamp (response received time)

            int32_t offset = conn->time.calcAdjustment(); // Adjust time and get calculated offset

            // flag all connections for re-timeSync
            if (nodeTimeAdjustedCallback) {
                nodeTimeAdjustedCallback(offset);
            }

            SimpleList<meshConnectionType>::iterator connection = _connections.begin();
            while (connection != _connections.end()) {
                if (connection != conn) {  // exclude this connection
                    connection->timeSyncStatus = NEEDED;
                    debugMsg(S_TIME, "handleTimeSync(): timeSyncStatus with %d changed to NEEDED (2)\n", connection->nodeId);
                }
                connection++;
            }

            if (offset < MIN_ACCURACY && offset > -MIN_ACCURACY) {
                // mark complete only if offset was less than 10 ms
                conn->timeSyncStatus = COMPLETE;
                debugMsg(S_TIME, "handleTimeSync(): timeSyncStatus with %d changed to COMPLETE\n", connection->nodeId);
            } else {
                // Iterate sync procedure if accuracy was not enough
                conn->timeSyncStatus = NEEDED;
                debugMsg(S_TIME, "handleTimeSync(): timeSyncStatus with %d changed to NEEDED (3)\n", connection->nodeId);

            }
            conn->lastTimeSync = getNodeTime();
        } else {
            debugMsg(S_TIME, "handleTimeSync(): TIME RESPONSE ignored.\n");
            conn->timeSyncStatus == COMPLETE;
        }

        break;
    }

    debugMsg(S_TIME, "handleTimeSync(): ----------------------------------\n");

}

void ICACHE_FLASH_ATTR painlessMesh::handleTimeDelay(meshConnectionType *conn, JsonObject& root, uint32_t receivedAt) {
    String timeStamp = root["msg"];
    uint32_t from = root["from"];
    debugMsg(S_TIME, "handleTimeDelay(): from %u in timestamp = %s\n", from, timeStamp.c_str());

    timeSyncMessageType_t timeSyncMessageType = conn->time.processTimeStampDelay(timeStamp); // Extract timestamps and get type of message

    String t_stamp;

    switch (timeSyncMessageType) {

    case (TIME_REQUEST):

        //conn->timeSyncStatus == IN_PROGRESS;
        debugMsg(S_TIME, "handleTimeDelay(): TIME REQUEST received. T0 = %d\n", conn->time.times[0]);

        // Build time response
        t_stamp = conn->time.buildTimeStamp(TIME_RESPONSE, conn->time.timeDelay[0], receivedAt, getNodeTime());
        staticThis->sendMessage(conn, from, _nodeId, TIME_DELAY, t_stamp);

        debugMsg(S_TIME, "handleTimeDelay(): Response sent %s\n", t_stamp.c_str());

        // After response is sent I assume sync is completed
        //conn->timeSyncStatus == COMPLETE;
        //conn->lastTimeSync = getNodeTime();
        break;

    case (TIME_RESPONSE):
        debugMsg(S_TIME, "handleTimeDelay(): TIME RESPONSE received.\n");
        conn->time.timeDelay[3] = receivedAt; // Calculate fourth timestamp (response received time)

        int32_t delay = conn->time.delayCalc(); // Adjust time and get calculated offset
        debugMsg(S_TIME, "handleTimeDelay(): Delay is %d\n", delay);

        //conn->timeSyncStatus == COMPLETE;

        if (nodeDelayReceivedCallback)
            nodeDelayReceivedCallback(from, delay);


        break;
    }

    debugMsg(S_TIME, "handleTimeSync(): ----------------------------------\n");

}




