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
/*String ICACHE_FLASH_ATTR timeSync::buildTimeStamp(void) {
    staticThis->debugMsg(SYNC, "buildTimeStamp(): num=%d\n", num);

    if (num > TIME_SYNC_CYCLES)
        // TIME_SYNC_CYCLES should be the same in all nodes. It defines length of times array
        staticThis->debugMsg(ERROR, "buildTimeStamp(): timeSync not started properly\n");

    StaticJsonBuffer<75> jsonBuffer;
    JsonObject& timeStampObj = jsonBuffer.createObject();
    times[num] = staticThis->getNodeTime();
    timeStampObj["time"] = times[num];
    timeStampObj["num"] = num;
    bool remoteAdopt = !adopt; // remote_adopt = !local_adopt
    timeStampObj["adopt"] = remoteAdopt;

    String timeStampStr;
    timeStampObj.printTo(timeStampStr);

    staticThis->debugMsg(SYNC, "buildTimeStamp(): timeStamp=%s\n", timeStampStr.c_str());
    return timeStampStr;
}*/

//***********************************************************************
String ICACHE_FLASH_ATTR timeSync::buildTimeStamp(timeSyncMessageType_t timeSyncMessageType, uint32_t originateTS, uint32_t receiveTS, uint32_t transmitTS) {
    staticThis->debugMsg(SYNC, "buildTimeStamp(): Type = %d, t0 = %d, t1 = %d, t2 = %d\n", timeSyncMessageType, originateTS, receiveTS, transmitTS);
    StaticJsonBuffer<75> jsonBuffer;
    JsonObject& timeStampObj = jsonBuffer.createObject();
    timeStampObj["type"] = timeSyncMessageType;
    if (originateTS > 0)
        timeStampObj["t0"] = originateTS;
    if (receiveTS > 0)
        timeStampObj["t1"] = receiveTS;
    if (transmitTS > 0)
        timeStampObj["t2"] = transmitTS;
    
    String timeStampStr;
    timeStampObj.printTo(timeStampStr);
    staticThis->debugMsg(SYNC, "buildTimeStamp(): timeStamp=%s\n", timeStampStr.c_str());

    return timeStampStr;
}

//***********************************************************************
bool ICACHE_FLASH_ATTR timeSync::processTimeStamp(int timeSyncStatus, String &str, bool ap) {
    staticThis->debugMsg(SYNC, "processTimeStamp(): str=%s\n", str.c_str());

    DynamicJsonBuffer jsonBuffer(50);
    JsonObject& timeStampObj = jsonBuffer.parseObject(str);
    if (!timeStampObj.success()) {
        staticThis->debugMsg(ERROR, "processTimeStamp(): out of memory1?\n");
        return false;
    }
    int8_t numTemp = timeStampObj.get<uint32_t>("num");
    staticThis->debugMsg(SYNC, "processTimeStamp(): local num: %d. recvd num %d. timeSyncStatus: %d. AP: %s\n", num, numTemp, timeSyncStatus, ap ? "true" : "false");
    if (!((timeSyncStatus == IN_PROGRESS) && (num == numTemp) && !ap)) { // Discard colliding messages if I'm STA
        num = numTemp;
        times[num] = timeStampObj.get<uint32_t>("time");
        adopt = timeStampObj.get<bool>("adopt");
        num++; // Increment time sync iteration

        if (num < TIME_SYNC_CYCLES) {
            str = buildTimeStamp();
            return true;
        } else {
            return false;
        }
    } else {
        staticThis->debugMsg(SYNC, "processTimeStamp(): Message collision. Discarded\n");
        return false;
    }
}

//***********************************************************************
void ICACHE_FLASH_ATTR timeSync::calcAdjustment(bool odd) {
    staticThis->debugMsg(DEBUG, "calcAdjustment(): odd=%u\n", odd);

    uint32_t    bestInterval = 0xFFFFFFFF; // Max 32 bit value
    uint8_t     bestIndex;
    uint32_t    temp;

    for (int i = 0; i < TIME_SYNC_CYCLES; i++) {
        //      debugMsg( GENERAL, "calcAdjustment(): times[%d]=%u\n", i, times[i]);
        staticThis->debugMsg(DEBUG, "calcAdjustment(): times[%d] --> %u. Odd = %s\n", i, times[i], (i % 2 == odd) ? "true" : "false");

        if (i % 2 == odd) {
            if (odd)
                temp = times[i - 1] - times[i]; // ??
            else
                temp = times[i + 1] - times[i]; // ??
            staticThis->debugMsg(DEBUG, "calcAdjustment(): %d is %s\n", i, odd ? "odd" : "even");

            //if ( i < TIME_SYNC_CYCLES - 2 ){ // If TIME_SYNC_CYCLES is 2 this never happens
            if (i < TIME_SYNC_CYCLES - 1) {
                //            debugMsg( GENERAL, "\tinterval=%u\n", temp);

                if (temp < bestInterval) {
                    bestInterval = temp;
                    bestIndex = i;
                }
                staticThis->debugMsg(DEBUG, "calcAdjustment(): best interval calculation. Value so far %u\n", bestInterval);

            }

        }
    }
    staticThis->debugMsg(DEBUG, "calcAdjustment(): best interval=%u, best index=%u\n", bestInterval, bestIndex);
    // find number that turns local time into remote time
    //uint32_t adopterTime = times[ bestIndex ] + (bestInterval / 2);
    //uint32_t adjustment = times[ bestIndex + 1 ] - adopterTime;
    uint32_t adjustment = bestInterval;
    timeAdjuster += adjustment - 80000;  // calculation delay aprox. 160000. Should calculate this automatically

    //staticThis->debugMsg( SYNC, "new calc time=%u, adoptedTime=%u\n", adopterTime + adjustment, times[ bestIndex + 1 ]);
    staticThis->debugMsg(DEBUG, "calcAdjustment(): new time=%u\n", staticThis->getNodeTime());

}


// painlessMesh Syncing functions
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::startNodeSync(meshConnectionType *conn) {

    debugMsg(SYNC, "startNodeSync(): with %d\n", conn->nodeId);
    String subs = subConnectionJson(conn);
    sendMessage(conn, conn->nodeId, NODE_SYNC_REQUEST, subs);
    conn->nodeSyncRequest = getNodeTime();
    conn->nodeSyncStatus = IN_PROGRESS;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::handleNodeSync(meshConnectionType *conn, JsonObject& root) {
    debugMsg(SYNC, "handleNodeSync(): with %d\n", conn->nodeId);

    meshPackageType type = (meshPackageType) (int) root["type"];
    uint32_t        remoteNodeId = (uint32_t) root["from"];
    uint32_t        destId = (uint32_t) root["dest"];
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
    }

    switch (type) {
    case NODE_SYNC_REQUEST:
    {
        debugMsg(SYNC, "handleNodeSync(): valid NODE_SYNC_REQUEST %d sending NODE_SYNC_REPLY\n", conn->nodeId);
        String myOtherSubConnections = subConnectionJson(conn);
        sendMessage(conn, conn->nodeId, NODE_SYNC_REPLY, myOtherSubConnections);
        break;
    }
    case NODE_SYNC_REPLY:
        debugMsg(SYNC, "handleNodeSync(): valid NODE_SYNC_REPLY from %d\n", conn->nodeId);
        conn->nodeSyncRequest = 0;  //reset nodeSyncRequest Timer  ????
        if (conn->lastTimeSync == 0) {
            debugMsg(SYNC, "handleNodeSync(): timeSyncStatus changed to NEEDED\n");
            //startTimeSync(conn);
            //conn->timeSyncStatus = IN_PROGRESS;
            conn->timeSyncStatus = NEEDED;
        }
        break;
    default:
        debugMsg(ERROR, "handleNodeSync(): weird type? %d\n", type);
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
/*void ICACHE_FLASH_ATTR painlessMesh::startTimeSync(meshConnectionType *conn) {
    debugMsg(SYNC, "startTimeSync(): with %d, local port: %d\n", conn->nodeId, conn->esp_conn->proto.tcp->local_port);
    debugMsg(SYNC, "startTimeSync(): timeSyncStatus changed to IN_PROGRESS\n", conn->nodeId);
    conn->timeSyncStatus = IN_PROGRESS;

    if (conn->time.num > TIME_SYNC_CYCLES) {
        debugMsg(ERROR, "startTimeSync(): Error timeSync.num not reset conn->time.num=%d\n", conn->time.num);
    }

    conn->time.num = 0; // First time num is 0.

    conn->time.adopt = adoptionCalc(conn); // should I adopt other party's time?
    //   debugMsg( GENERAL, "startTimeSync(): remoteSubCount=%d adopt=%d\n", remoteSubCount, conn->time.adopt);

    String timeStamp = conn->time.buildTimeStamp();
    staticThis->sendMessage(conn, conn->nodeId, TIME_SYNC, timeStamp);
}*/

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::startTimeSync(meshConnectionType *conn, boolean checkAdopt) {
    boolean adopt = true;
    String timeStamp;

    debugMsg(SYNC, "startTimeSync(): with %d, local port: %d\n", conn->nodeId, conn->esp_conn->proto.tcp->local_port);
    debugMsg(SYNC, "startTimeSync(): timeSyncStatus changed to IN_PROGRESS\n", conn->nodeId);
    conn->timeSyncStatus = IN_PROGRESS;

    if (checkAdopt){
        adopt = adoptionCalc(conn);
    }
    if (adopt) {
        timeStamp = conn->time.buildTimeStamp(TIME_REQUEST, getNodeTime());
    } else {
        timeStamp = conn->time.buildTimeStamp(TIME_SYNC_REQUEST);
    }
    sendMessage(conn, conn->nodeId, TIME_SYNC, timeStamp);
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::adoptionCalc(meshConnectionType *conn) {
    // make the adoption calulation.  Figure out how many nodes I am connected to exclusive of this connection.

    uint16_t mySubCount = connectionCount(conn);  //exclude this connection.
    uint16_t remoteSubCount = jsonSubConnCount(conn->subConnections);
    bool ap = conn->esp_conn->proto.tcp->local_port == _meshPort;

    bool ret = (mySubCount > remoteSubCount) ? false : true;

    debugMsg(GENERAL, "adoptionCalc(): mySubCount=%d remoteSubCount=%d role=%s adopt=%s\n", mySubCount, remoteSubCount, ap ? "AP" : "STA", ret ? "true" : "false");

    return ret;
}

//***********************************************************************
//* Time sync protocol is used to let all nodes in the mesh share a common
//* clock, in order to be able to do synchronized tasks.
//*
//* - A node builds a timestamp with its local system clock and order 0.
//*    Then it sends it to AP.
//* - It adds a flag indicating if receving peer should update its clock.
//*    The flag will be true for the node with less connections and
//*    subconnections. In case of an equal value, the node acting as STA
//*    will update its clock.
//* - When timestamp is received order (num) is increased and a new timestamp
//*    is generated
//* - If num is less than TIME_SYNC_CYCLES then it sends new generated
//*    timestamp.
//* - An even value of num is asigned to requests and orders are assigned
//*    with an odd value.
//* - If TIME_SYNC_CYCLES is reached, or TIME_SYNC_CYCLES in case of a
//*    received request, the node that has been assigned to adopt new time
//*    will search in the time structure the lower difference into consecutive
//*    values.
//* - This value will be added to timeAdjuster variable, that is added to
//*    local system clock every time meshClock is asked.
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::handleTimeSync(meshConnectionType *conn, JsonObject& root) {

    String timeStamp = root["msg"];
    debugMsg(SYNC, "handleTimeSync(): with %d in timestamp=%s\n", conn->nodeId, timeStamp.c_str());
    debugMsg(SYNC, "handleTimeSync(): ip_local: %d.%d.%d.%d, puerto local = %d\n", IP2STR(conn->esp_conn->proto.tcp->local_ip), conn->esp_conn->proto.tcp->local_port);
    debugMsg(SYNC, "handleTimeSync(): ip_remota: %d.%d.%d.%d, puerto remoto = %d\n", IP2STR(conn->esp_conn->proto.tcp->remote_ip), conn->esp_conn->proto.tcp->remote_port);

    bool shouldAnswer = conn->time.processTimeStamp(conn->timeSyncStatus, timeStamp, conn->esp_conn->proto.tcp->local_port == _meshPort);  //verifies timeStamp and UPDATES it with a new one.

    debugMsg(SYNC, "handleTimeSync(): with %d out timestamp=%s\n", conn->nodeId, timeStamp.c_str());

    if (conn->time.num < TIME_SYNC_CYCLES && shouldAnswer) { // Answer last valid message is sync cycles are still missing
        staticThis->sendMessage(conn, conn->nodeId, TIME_SYNC, timeStamp);
    }

    uint8_t odd = conn->time.num % 2; // 1 if num is odd, 0 if even

    if ((conn->time.num + odd) >= TIME_SYNC_CYCLES) {   // timeSync completed
        debugMsg(SYNC, "handleTimeSync(): timeSync completed. num = %d\n", conn->time.num);
        if (conn->time.adopt) { // if I have to adopt
            conn->time.calcAdjustment(odd);

            // flag all connections for re-timeSync
            SimpleList<meshConnectionType>::iterator connection = _connections.begin();
            while (connection != _connections.end()) {
                if (connection != conn) {  // exclude this connection
                    connection->timeSyncStatus = NEEDED;
                    debugMsg(SYNC, "handleTimeSync(): timeSyncStatus with %u changed to NEEDED\n", connection->nodeId);
                }
                connection++;
            }
        }
        conn->lastTimeSync = getNodeTime();
        conn->timeSyncStatus = COMPLETE;
        debugMsg(SYNC, "handleTimeSync(): timeSyncStatus changed to COMPLETE\n");
        debugMsg(SYNC, "handleTimeSync(): ----------------------------------\n");
    }
}






