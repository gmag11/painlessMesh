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
//    meshPrintDebug("buildTimeStamp(): num=%d\n", num);
    
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
    
//    meshPrintDebug("buildTimeStamp(): timeStamp=%s\n", timeStampStr.c_str() );
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
void easyMesh::handleHandShake( meshConnection_t *conn, JsonObject& root ) {
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
void easyMesh::meshSyncCallback( void *arg ) {
    //meshPrintDebug("meshSyncCallback(): entering\n");
    
    if ( wifi_station_get_connect_status() == STATION_GOT_IP ) {
        // we are connected as a station find station connection
        SimpleList<meshConnection_t>::iterator connection = staticThis->_connections.begin();
        while ( connection != staticThis->_connections.end() ) {
            if ( connection->esp_conn->proto.tcp->local_port != MESH_PORT ) {
                // found station connection.  Initiate sync
                String subsJson = staticThis->subConnectionJson( connection );
                meshPrintDebug("meshSyncCallback(): Requesting Sync with %d", connection->chipId );
                staticThis->sendMessage( connection->chipId, MESH_SYNC_REQUEST, subsJson );
                break;
            }
            connection++;
        }
    }
    //meshPrintDebug("meshSyncCallback(): leaving\n");
}

//***********************************************************************
void easyMesh::handleMeshSync( meshConnection_t *conn, JsonObject& root ) {
    meshPrintDebug("handleMeshSync(): type=%d\n", (int)root["type"] );
    
    String subs = root["subs"];
    conn->subConnections = subs;
    //    meshPrintDebug("subs=%s\n", conn->subConnections.c_str());
    
    if ( (meshPackageType)(int)root["type"] == MESH_SYNC_REQUEST ) {
        String subsJson = staticThis->subConnectionJson( conn );
        staticThis->sendMessage( conn->chipId, MESH_SYNC_REPLY, subsJson );
    }
    else {
        startTimeSync( conn );
    }
}

//***********************************************************************
void easyMesh::startTimeSync( meshConnection_t *conn ) {
    meshPrintDebug("startTimeSync():\n");
    // since we are here, we know that we are the STA
    
    if ( conn->time.num > TIME_SYNC_CYCLES ) {
        meshPrintDebug("startTimeSync(): Error timeSync.num not reset conn->time.num=%d\n", conn->time.num );
    }
    
    conn->time.num = 0;
    
    // make the adoption calulation.  Figure out how many nodes I am connected to exclusive of this connection.
    uint16_t mySubCount = 0;
    uint16_t remoteSubCount = 0;
    SimpleList<meshConnection_t>::iterator sub = _connections.begin();
    while ( sub != _connections.end() ) {
        if ( sub != conn ) {  //exclude this connection in the calc.
            mySubCount += ( 1 + jsonSubConnCount( sub->subConnections ) );
        }
        sub++;
    }
    remoteSubCount = jsonSubConnCount( conn->subConnections );
    meshPrintDebug("startTimeSync(): remoteSubCount=%d\n", remoteSubCount);
    conn->time.adopt = ( mySubCount > remoteSubCount ) ? false : true;  // do I adopt the estblished time?
    meshPrintDebug("startTimeSync(): adopt=%d\n", conn->time.adopt);
    
    String timeStamp = conn->time.buildTimeStamp();
    staticThis->sendMessage( conn->chipId, TIME_SYNC, timeStamp );
}

//***********************************************************************
void easyMesh::handleTimeSync( meshConnection_t *conn, JsonObject& root ) {
//    meshPrintDebug("handleTimeSync():\n");
    
    String timeStamp = root["timeStamp"];
    conn->time.processTimeStamp( timeStamp );
    
    if ( conn->time.num < TIME_SYNC_CYCLES ) {
        staticThis->sendMessage( conn->chipId, TIME_SYNC, timeStamp );
    }
    
    uint8_t odd = conn->time.num % 2;
    
    if ( (conn->time.num + odd) >= TIME_SYNC_CYCLES ) {
        if ( conn->time.adopt ) {
            conn->time.calcAdjustment( odd );
        }
    }
}

//***********************************************************************
uint16_t easyMesh::jsonSubConnCount( String& subConns ) {
    meshPrintDebug("jsonSubConnCount(): subConns=%s\n", subConns.c_str() );
    
    uint16_t count = 0;
    
    if ( subConns.length() < 3 )
        return 0;
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonArray& subArray = jsonBuffer.parseArray( subConns );
    
    if ( !subArray.success() ) {
        meshPrintDebug("subConnCount(): out of memory1\n");
    }
    
    String str;
    for ( uint8_t i = 0; i < subArray.size(); i++ ) {
        str = subArray.get<String>(i);
        meshPrintDebug("jsonSubConnCount(): str=%s\n", str.c_str() );
        JsonObject& obj = jsonBuffer.parseObject( str );
        if ( !obj.success() ) {
            meshPrintDebug("subConnCount(): out of memory2\n");
        }
        
        str = obj.get<String>("subs");
        count += ( 1 + jsonSubConnCount( str ) );
    }
    
    return count;
}



