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
//    DEBUG_MSG("buildTimeStamp(): num=%d\n", num);
    
    if ( num > TIME_SYNC_CYCLES )
        DEBUG_MSG("buildTimeStamp(): timeSync not started properly\n");
    
    StaticJsonBuffer<75> jsonBuffer;
    JsonObject& timeStampObj = jsonBuffer.createObject();
    times[num] = getNodeTime();
    timeStampObj["time"] = times[num];
    timeStampObj["num"] = num;
    bool remoteAdopt = !adopt;
    timeStampObj["adopt"] = remoteAdopt;
    
    String timeStampStr;
    timeStampObj.printTo( timeStampStr );
    
//    DEBUG_MSG("buildTimeStamp(): timeStamp=%s\n", timeStampStr.c_str() );
    return timeStampStr;
}

//***********************************************************************
bool timeSync::processTimeStamp( String &str ) {
//    DEBUG_MSG("processTimeStamp(): str=%s\n", str.c_str());
    
    DynamicJsonBuffer jsonBuffer(50 );
    JsonObject& timeStampObj = jsonBuffer.parseObject(str);
    
    if ( !timeStampObj.success() ) {
        DEBUG_MSG("processTimeStamp(): out of memory1?\n" );
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
//    DEBUG_MSG("calcAdjustment(): odd=%u\n", odd);

    uint32_t    bestInterval = 0xFFFFFFFF;
    uint8_t     bestIndex;
    uint32_t    temp;

    for (int i = 0; i < TIME_SYNC_CYCLES; i++) {
  //      DEBUG_MSG("times[%d]=%u\n", i, times[i]);
        
        if ( i % 2 == odd ) {
            temp = times[i + 2] - times[i];
            
            if ( i < TIME_SYNC_CYCLES - 2 ){
    //            DEBUG_MSG("\tinterval=%u\n", temp);
                
                if ( temp < bestInterval ) {
                    bestInterval = temp;
                    bestIndex = i;
                }
            }
        }
    }
//    DEBUG_MSG("best interval=%u, best index=%u\n", bestInterval, bestIndex);
    
    // find number that turns local time into remote time
    uint32_t adopterTime = times[ bestIndex ] + (bestInterval / 2);
    uint32_t adjustment = times[ bestIndex + 1 ] - adopterTime;
    
 //   DEBUG_MSG("new calc time=%u, adoptedTime=%u\n", adopterTime + adjustment, times[ bestIndex + 1 ]);

    timeAdjuster += adjustment;
}



// easyMesh Syncing functions
//***********************************************************************
void easyMesh::handleHandShake( meshConnection_t *conn, JsonObject& root ) {
    String msg = root["msg"];
    uint32_t remoteChipId = (uint32_t)root["from"];
    
    if ( msg == "Station Handshake") {
        DEBUG_MSG("handleHandShake(): recieved station handshake\n");
        
        // check to make sure we are not already connected
        if ( staticThis->findConnection( remoteChipId ) != NULL ) {  //drop this connection
            DEBUG_MSG("We are already connected to this node as Station.  Drop new connection\n");
            espconn_disconnect( conn->esp_conn );
            return;
        }
        //else
        conn->chipId = remoteChipId;
        DEBUG_MSG("sending AP handshake\n");
        staticThis->sendMessage( remoteChipId, HANDSHAKE, "AP Handshake");
        _nodeStatus = CONNECTED;
    }
    else if ( msg == "AP Handshake") {  // add AP chipId to connection
        DEBUG_MSG("handleHandShake(): received AP Handshake\n");
        
        // check to make sure we are not already connected
        if ( staticThis->findConnection( remoteChipId ) != NULL ) {  //drop this connection
            DEBUG_MSG("We are already connected to this node as AP.  Drop new connection\n");
            espconn_disconnect( conn->esp_conn );
            return;
        }
        //else
        conn->chipId = remoteChipId;
        _nodeStatus = CONNECTED;
    }
    else {
        DEBUG_MSG("handleHandShake(): Weird msg\n");
    }
}

//***********************************************************************
void easyMesh::meshSyncCallback( void *arg ) {
    DEBUG_MSG("meshSyncCallback(): entering\n");
    
    if ( wifi_station_get_connect_status() == STATION_GOT_IP ) {
        // we are connected as a station find station connection
        SimpleList<meshConnection_t>::iterator connection = staticThis->_connections.begin();
        while ( connection != staticThis->_connections.end() ) {
            if ( connection->esp_conn->proto.tcp->local_port != MESH_PORT ) {
                // found station connection.  Initiate sync
                String subsJson = staticThis->subConnectionJson( connection );
                DEBUG_MSG("meshSyncCallback(): Requesting Sync with %d", connection->chipId );
                staticThis->sendMessage( connection->chipId, MESH_SYNC_REQUEST, subsJson );
                break;
            }
            connection++;
        }
    }
    DEBUG_MSG("meshSyncCallback(): leaving\n");
}


//***********************************************************************
void easyMesh::handleMeshSync( meshConnection_t *conn, JsonObject& root ) {
    DEBUG_MSG("handleMeshSync(): type=%d\n", (int)root["type"] );
    
    String subs = root["subs"];
    conn->subConnections = subs;
    //    DEBUG_MSG("subs=%s\n", conn->subConnections.c_str());
    
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
    DEBUG_MSG("startTimeSync():\n");
    // since we are here, we know that we are the STA
    
    if ( conn->time.num > TIME_SYNC_CYCLES ) {
        DEBUG_MSG("startTimeSync(): Error timeSync.num not reset conn->time.num=%d\n", conn->time.num );
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
    conn->time.adopt = ( mySubCount > remoteSubCount ) ? false : true;  // do I adopt the estblished time?
    DEBUG_MSG("startTimeSync(): adopt=%d\n", conn->time.adopt);
    
    String timeStamp = conn->time.buildTimeStamp();
    staticThis->sendMessage( conn->chipId, TIME_SYNC, timeStamp );
}

//***********************************************************************
void easyMesh::handleTimeSync( meshConnection_t *conn, JsonObject& root ) {
    DEBUG_MSG("handleTimeSync():\n");
    
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
    uint16_t count = 0;
    
    if ( subConns.length() < 3 )
        return 0;
    
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonArray& subArray = jsonBuffer.parseArray( subConns );
    
    if ( !subArray.success() ) {
        DEBUG_MSG("subConnCount(): out of memory1\n");
    }
    
    String str;
    
    for ( uint8_t i = 0; i < subArray.size(); i++ ) {
        str = subArray.get<String>(i);
        DEBUG_MSG("jsonSubConnCount(): str=%s\n", str.c_str() );
        JsonObject& obj = jsonBuffer.parseObject( str );
        if ( !obj.success() ) {
            DEBUG_MSG("subConnCount(): out of memory2\n");
        }
        
        str = obj.get<String>("subs");
        count += ( 1 + jsonSubConnCount( str ) );
    }
    
    return count;
}

//***********************************************************************
String easyMesh::subConnectionJson( meshConnection_t *thisConn ) {
    DynamicJsonBuffer jsonBuffer( JSON_BUFSIZE );
    JsonArray& subArray = jsonBuffer.createArray();
    if ( !subArray.success() )
        DEBUG_MSG("subConnectionJson(): ran out of memory 1");
    
    SimpleList<meshConnection_t>::iterator sub = _connections.begin();
    while ( sub != _connections.end() ) {
        if ( sub != thisConn ) {  //exclude the connection that we are working with.
            JsonObject& subObj = jsonBuffer.createObject();
            if ( !subObj.success() )
                DEBUG_MSG("subConnectionJson(): ran out of memory 2");
            
            subObj["chipId"] = sub->chipId;
            
            if ( sub->subConnections.length() != 0 ) {
                DEBUG_MSG("subConnectionJson(): sub->subConnections=%s\n", sub->subConnections.c_str() );
                
                JsonArray& subs = jsonBuffer.parseArray( sub->subConnections );
                if ( !subs.success() )
                    DEBUG_MSG("subConnectionJson(): ran out of memory 3");
                
                subObj["subs"] = subs;
            }
            
            if ( !subArray.add( subObj ) )
                DEBUG_MSG("subConnectionJson(): ran out of memory 4");
        }
        sub++;
    }
    
    String ret;
    subArray.printTo( ret );
    return ret;
}


