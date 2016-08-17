#include <Arduino.h>
#include <SimpleList.h>
#include <ArduinoJson.h>

#include "easyMesh.h"
#include "easyMeshSync.h"

extern easyMesh* staticThis;
uint32_t timeAdjuster = 0;

// timeSync Functions
//***********************************************************************
uint32_t ICACHE_FLASH_ATTR easyMesh::getNodeTime( void ) {
    return system_get_time() + timeAdjuster;
}

//***********************************************************************
String ICACHE_FLASH_ATTR timeSync::buildTimeStamp( void ) {
    //  meshPrintDebug("buildTimeStamp(): num=%d\n", num);
    
    if ( num > TIME_SYNC_CYCLES )
        meshPrintDebug("buildTimeStamp(): timeSync not started properly\n");
    
    StaticJsonBuffer<75> jsonBuffer;
    JsonObject& timeStampObj = jsonBuffer.createObject();
    times[num] = staticThis->getNodeTime();
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
bool ICACHE_FLASH_ATTR timeSync::processTimeStamp( String &str ) {
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
void ICACHE_FLASH_ATTR timeSync::calcAdjustment ( bool odd ) {
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
void ICACHE_FLASH_ATTR easyMesh::startNodeSync( meshConnectionType *conn ) {
    //meshPrintDebug("startNodeSync(): with %u\n", conn->chipId);

    String subs = subConnectionJson( conn );
    sendMessage( conn, conn->chipId, NODE_SYNC_REQUEST, subs );
    conn->nodeSyncRequest = getNodeTime();
    conn->needsNodeSync = false;
    
    //meshPrintDebug("startNodeSync(): leaving\n");
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::handleNodeSync( meshConnectionType *conn, JsonObject& root ) {
    meshPackageType type = (meshPackageType)(int)root["type"];
    uint32_t        remoteChipId = (uint32_t)root["from"];
    uint32_t        destId = (uint32_t)root["dest"];
    bool reSyncAllSubConnections = false;
    
    if( (destId == 0) && (findConnection( remoteChipId ) != NULL) ) {
        // this is the first NODE_SYNC_REQUEST from a station
        // is we are already connected drop this connection
        meshPrintDebug("handleNodeSync(): Already connected to node %d.  Dropping\n", conn->chipId);
        closeConnection( conn );
        return;
    }

    if ( conn->chipId != remoteChipId ) {
        meshPrintDebug("handleNodeSync(): conn->chipId updated from %d to %d\n", conn->chipId, remoteChipId );
        conn->chipId = remoteChipId;

    }
    
    // check to see if subs have changed.
    String inComingSubs = root["subs"];
    if ( !conn->subConnections.equals( inComingSubs ) ) {  // change in the network
        reSyncAllSubConnections = true;
        conn->subConnections = inComingSubs;
    }
    
    switch ( type ) {
        case NODE_SYNC_REQUEST:
        {
            //meshPrintDebug("handleNodeSync(): valid NODE_SYNC_REQUEST %d sending NODE_SYNC_REPLY\n", conn->chipId );
            String myOtherSubConnections = subConnectionJson( conn );
            sendMessage( conn, _chipId, NODE_SYNC_REPLY, myOtherSubConnections );
            break;
        }
        case NODE_SYNC_REPLY:
            //meshPrintDebug("handleNodeSync(): valid NODE_SYNC_REPLY from %d\n", conn->chipId );
            conn->nodeSyncRequest = 0;  //reset nodeSyncRequest Timer  ????
            if ( conn->lastTimeSync == 0 )
                startTimeSync( conn );
            break;
        default:
            meshPrintDebug("handleNodeSync(): weird type? %d\n", type );
    }
    
    if ( reSyncAllSubConnections == true ) {
        SimpleList<meshConnectionType>::iterator connection = _connections.begin();
        while ( connection != _connections.end() ) {
            connection->needsNodeSync = true;
            connection++;
        }
    }
    
    conn->needsNodeSync = false;  // mark this connection nodeSync'd

    
    //meshPrintDebug("handleNodeSync(): leaving\n" );
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::startTimeSync( meshConnectionType *conn ) {
    meshPrintDebug("startTimeSync(): with %d\n", conn->chipId );
    
    if ( conn->time.num > TIME_SYNC_CYCLES ) {
        meshPrintDebug("startTimeSync(): Error timeSync.num not reset conn->time.num=%d\n", conn->time.num );
    }
    
    conn->time.num = 0;
    
    
    conn->time.adopt = adoptionCalc( conn ); // do I adopt the estblished time?
    //   meshPrintDebug("startTimeSync(): remoteSubCount=%d adopt=%d\n", remoteSubCount, conn->time.adopt);
    
    String timeStamp = conn->time.buildTimeStamp();
    staticThis->sendMessage( conn, _chipId, TIME_SYNC, timeStamp );
    
    meshPrintDebug("startTimeSync(): Leaving\n");
}

//***********************************************************************
bool ICACHE_FLASH_ATTR easyMesh::adoptionCalc( meshConnectionType *conn ) {
    // make the adoption calulation.  Figure out how many nodes I am connected to exclusive of this connection.
    uint16_t mySubCount = connectionCount( conn );  //exclude this connection.
    uint16_t remoteSubCount = jsonSubConnCount( conn->subConnections );
    
    return ( mySubCount > remoteSubCount ) ? false : true;
}

//***********************************************************************
void ICACHE_FLASH_ATTR easyMesh::handleTimeSync( meshConnectionType *conn, JsonObject& root ) {
    
    String timeStamp = root["msg"];
//    meshPrintDebug("handleTimeSync(): with %d in timestamp=%s\n", conn->chipId, timeStamp.c_str());
    
    conn->time.processTimeStamp( timeStamp );  //varifies timeStamp and updates it with a new one.

//    meshPrintDebug("handleTimeSync(): with %d out timestamp=%s\n", conn->chipId, timeStamp.c_str());

    
    if ( conn->time.num < TIME_SYNC_CYCLES ) {
        staticThis->sendMessage( conn, _chipId, TIME_SYNC, timeStamp );
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




