#include <Arduino.h>
#include <SimpleList.h>
#include <ArduinoJson.h>

#include "easyMesh.h"
#include "easyMeshSync.h"

extern easyMesh* staticThis;
uint32_t timeAdjuster = 0;

// timeSync Functions
//***********************************************************************
uint32_t getNodeTime( void ) {
    return system_get_time() + timeAdjuster;
}

//***********************************************************************
String timeSync::buildTimeStamp( void ) {
    //  meshPrintDebug("buildTimeStamp(): num=%d\n", num);
    
    if ( num > TIME_SYNC_CYCLES )
        meshPrintDebug("buildTimeStamp(): timeSync not started properly\n");
    
    StaticJsonBuffer<75> jsonBuffer;
    JsonObject& timeStampObj = jsonBuffer.createObject();
    times[num] = getNodeTime();
    timeStampObj["time"] = times[num];
    timeStampObj["num"] = num;
    bool remoteAdopt = !adopt;
    timeStampObj["adopt"] = remoteAdopt;
    
    String timeStampStr;
    timeStampObj.printTo( timeStampStr );
    
    //  meshPrintDebug("buildTimeStamp(): timeStamp=%s\n", timeStampStr.c_str() );
    return timeStampStr;
}

//***********************************************************************
bool timeSync::processTimeStamp( String &str ) {
    //    meshPrintDebug("processTimeStamp(): str=%s\n", str.c_str());
    
    DynamicJsonBuffer jsonBuffer(50 );
    JsonObject& timeStampObj = jsonBuffer.parseObject(str);
    
    if ( !timeStampObj.success() ) {
        meshPrintDebug("processTimeStamp(): out of memory1?\n" );
        return false;
    }
    
    num = timeStampObj.get<uint32_t>("num");
    
    times[num] = timeStampObj.get<uint32_t>("time");
    adopt = timeStampObj.get<bool>("adopt");
    
    num++;
    
    if ( num < TIME_SYNC_CYCLES ) {
        str = buildTimeStamp();
        return true;
    }
    else {
        return false;
    }
}

//***********************************************************************
void timeSync::calcAdjustment ( bool odd ) {
    //    meshPrintDebug("calcAdjustment(): odd=%u\n", odd);
    
    uint32_t    bestInterval = 0xFFFFFFFF;
    uint8_t     bestIndex;
    uint32_t    temp;
    
    for (int i = 0; i < TIME_SYNC_CYCLES; i++) {
        //      meshPrintDebug("times[%d]=%u\n", i, times[i]);
        
        if ( i % 2 == odd ) {
            temp = times[i + 2] - times[i];
            
            if ( i < TIME_SYNC_CYCLES - 2 ){
                //            meshPrintDebug("\tinterval=%u\n", temp);
                
                if ( temp < bestInterval ) {
                    bestInterval = temp;
                    bestIndex = i;
                }
            }
        }
    }
    //    meshPrintDebug("best interval=%u, best index=%u\n", bestInterval, bestIndex);
    
    // find number that turns local time into remote time
    uint32_t adopterTime = times[ bestIndex ] + (bestInterval / 2);
    uint32_t adjustment = times[ bestIndex + 1 ] - adopterTime;
    
    //   meshPrintDebug("new calc time=%u, adoptedTime=%u\n", adopterTime + adjustment, times[ bestIndex + 1 ]);
    
    timeAdjuster += adjustment;
}


// easyMesh Syncing functions
//***********************************************************************
void easyMesh::handleHandShake( meshConnectionType *conn, JsonObject& root ) {  //depricated
    //String msg = root["msg"];
    meshPackageType type = (meshPackageType)(int)root["type"];
    
    uint32_t remoteChipId = (uint32_t)root["from"];
    if ( remoteChipId != 0 && findConnection( remoteChipId ) != NULL ) {  //drop this connection
        meshPrintDebug("We are already connected to node %d.  Dropping new connection\n", conn->chipId);
        espconn_disconnect( conn->esp_conn );
        return;
    }
    
    conn->chipId = remoteChipId;  //add this connection
    
    // valid, add subs
    String inComingSubs = root["subs"];
    conn->subConnections = inComingSubs;
    _nodeStatus = CONNECTED;
    
    if ( type == STA_HANDSHAKE ) {
        String outGoingSubs = subConnectionJson( conn );
        sendMessage( conn->chipId, AP_HANDSHAKE, outGoingSubs );
        meshPrintDebug("handleHandShake(): valid STA handshake from %d sending AP handshake\n", conn->chipId );
    }
    else {  // AP connection
        meshPrintDebug("handleHandShake(): valid AP Handshake from %d\n", conn->chipId );
        startTimeSync( conn );
    }
}

//***********************************************************************
void easyMesh::handleNodeSync( meshConnectionType *conn, JsonObject& root ) {
    meshPackageType type = (meshPackageType)(int)root["type"];
    uint32_t        remoteChipId = (uint32_t)root["from"];
    uint32_t        destId = (uint32_t)root["dest"];
    bool reSyncAllSubConnections = false;
    
    
    if( destId == 0 ) { // this is the first NODE_SYNC_REQUEST from a station
        if ( findConnection( remoteChipId ) != NULL ) {  //drop this connection
            meshPrintDebug("handleNodeSync(): Already connected to node %d.  Dropping\n", conn->chipId);
            espconn_disconnect( conn->esp_conn );
            return;
        }
        conn->chipId = remoteChipId;
        reSyncAllSubConnections = true;
    }

    if ( conn->chipId == 0 ) {
        meshPrintDebug("handleNodeSync(): Recieved new connection %d\n", remoteChipId );
        conn->chipId = remoteChipId;  //add this connection
        reSyncAllSubConnections = true;
    } else if ( conn->chipId != remoteChipId ) {
        meshPrintDebug("handleNodeSync(): chipIds don't match? %d %d\n", conn->chipId, remoteChipId );
    }
    
    
    // if we are here this is a valid request or responce.
    String inComingSubs = root["subs"];
    if ( !conn->subConnections.equals( inComingSubs ) ) {  // change in the network
        reSyncAllSubConnections = true;
        conn->subConnections = inComingSubs;
    }
    
    if ( type == NODE_SYNC_REQUEST ) {
        meshPrintDebug("handleNodeSync(): valid NODE_SYNC_REQUEST %d sending NODE_SYNC_REPLY\n", conn->chipId );
        String myOtherSubConnections = subConnectionJson( conn );
        sendMessage( conn->chipId, NODE_SYNC_REPLY, myOtherSubConnections );
    }
    else if ( type == NODE_SYNC_REPLY ){
        meshPrintDebug("handleNodeSync(): valid NODE_SYNC_REPLY from %d\n", conn->chipId );
        conn->nodeSyncRequest = 0;  //reset nodeSyncRequest Timer
        if ( conn->lastTimeSync == 0 )
            startTimeSync( conn );
    }
    else {
        meshPrintDebug("handleNodeSync(): weird type? %d\n", type );
    }
    
    conn->needsNodeSync = false;  // mark this connection nodeSync'd
    
    if ( reSyncAllSubConnections == true ) {
        SimpleList<meshConnectionType>::iterator connection = _connections.begin();
        while ( connection != _connections.end() ) {
            if ( connection != conn ) {  // exclude this connection
                connection->needsNodeSync = true;
            }
            connection++;
        }
    }
    meshPrintDebug("handleNodeSync(): leaving\n" );
}

//***********************************************************************
void easyMesh::handleMeshSync( meshConnectionType *conn, JsonObject& root ) { //depricated
    meshPrintDebug("handleMeshSync(): type=%d\n", (int)root["type"] );
    
    String subs = root["subs"];
    conn->subConnections = subs;
    //    meshPrintDebug("subs=%s\n", conn->subConnections.c_str());
    
    if ( (meshPackageType)(int)root["type"] == MESH_SYNC_REQUEST ) {
        String subsJson = staticThis->subConnectionJson( conn );
        staticThis->sendMessage( conn->chipId, MESH_SYNC_REPLY, subsJson );
        meshPrintDebug("handleMeshSync(): subJson=%s", subsJson.c_str() );
        
    }
    else {
        startTimeSync( conn );
    }
}

//***********************************************************************
void easyMesh::meshSyncCallback( void *arg ) {
    //meshPrintDebug("meshSyncCallback(): entering\n");
    
    if ( wifi_station_get_connect_status() == STATION_GOT_IP ) {
        // we are connected as a station find station connection
        SimpleList<meshConnectionType>::iterator connection = staticThis->_connections.begin();
        while ( connection != staticThis->_connections.end() ) {
            if ( connection->esp_conn->proto.tcp->local_port != MESH_PORT ) {
                // found station connection.  Initiate sync
                String subsJson = staticThis->subConnectionJson( connection );
                meshPrintDebug("meshSyncCallback(): Requesting Sync with %d subJson=%s", connection->chipId, subsJson.c_str() );
                staticThis->sendMessage( connection->chipId, MESH_SYNC_REQUEST, subsJson );
                break;
            }
            connection++;
        }
    }
    //meshPrintDebug("meshSyncCallback(): leaving\n");
}

//***********************************************************************
void easyMesh::startTimeSync( meshConnectionType *conn ) {
    //  meshPrintDebug("startTimeSync():\n");
    // since we are here, we know that we are the STA
    
    if ( conn->time.num > TIME_SYNC_CYCLES ) {
        meshPrintDebug("startTimeSync(): Error timeSync.num not reset conn->time.num=%d\n", conn->time.num );
    }
    
    conn->time.num = 0;
    
    // make the adoption calulation.  Figure out how many nodes I am connected to exclusive of this connection.
    uint16_t mySubCount = connectionCount( conn );
    uint16_t remoteSubCount = jsonSubConnCount( conn->subConnections );
    
    /*    SimpleList<meshConnectionType>::iterator sub = _connections.begin();
     while ( sub != _connections.end() ) {
     if ( sub != conn ) {  //exclude this connection in the calc.
     mySubCount += ( 1 + jsonSubConnCount( sub->subConnections ) );
     }
     sub++;
     }
     */
    
    conn->time.adopt = ( mySubCount > remoteSubCount ) ? false : true;  // do I adopt the estblished time?
    //   meshPrintDebug("startTimeSync(): remoteSubCount=%d adopt=%d\n", remoteSubCount, conn->time.adopt);
    
    String timeStamp = conn->time.buildTimeStamp();
    staticThis->sendMessage( conn->chipId, TIME_SYNC, timeStamp );
    //   meshPrintDebug("startTimeSync(): Leaving\n");
}

//***********************************************************************
void easyMesh::handleTimeSync( meshConnectionType *conn, JsonObject& root ) {
    //    meshPrintDebug("handleTimeSync():\n");
    
    String timeStamp = root["timeStamp"];
    conn->time.processTimeStamp( timeStamp );  //varifies timeStamp and updates it with a new one.
    
    if ( conn->time.num < TIME_SYNC_CYCLES ) {
        staticThis->sendMessage( conn->chipId, TIME_SYNC, timeStamp );
    }
    
    uint8_t odd = conn->time.num % 2;
    
    if ( (conn->time.num + odd) >= TIME_SYNC_CYCLES ) {   // timeSync completed
        if ( conn->time.adopt ) {
            conn->time.calcAdjustment( odd );
            
            // flag all connections for re-timeSync
            SimpleList<meshConnectionType>::iterator connection = _connections.begin();
            while ( connection != _connections.end() ) {
                if ( connection != conn ) {  // exclude this connection
                    connection->needsTimeSync = true;
                }
                connection++;
            }
        }
        conn->lastTimeSync = getNodeTime();
        conn->needsTimeSync = false;
    }
}




