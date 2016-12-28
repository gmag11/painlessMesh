#include <Arduino.h>
#include <SimpleList.h>
#include <ArduinoJson.h>

#include "painlessMesh.h"
#include "painlessMeshSync.h"

extern painlessMesh* staticThis;
uint32_t timeAdjuster = 0;

// timeSync Functions
//***********************************************************************
uint32_t ICACHE_FLASH_ATTR painlessMesh::getNodeTime( void ) {
    uint32_t ret = system_get_time() + timeAdjuster;

    debugMsg( GENERAL, "getNodeTime(): time=%u\n", ret);
    
    return ret;
}

//***********************************************************************
String ICACHE_FLASH_ATTR timeSync::buildTimeStamp( void ) {
    staticThis->debugMsg( SYNC, "buildTimeStamp(): num=%d\n", num);
	staticThis->debugMsg(DEBUG, "buildTimeStamp(): num=%d\n", num);
    
    if ( num > TIME_SYNC_CYCLES ) // German Martin: I don't know why this TIME_SYNC_CYCLES is needed, yet
        staticThis->debugMsg( ERROR, "buildTimeStamp(): timeSync not started properly\n");
    
    StaticJsonBuffer<75> jsonBuffer;
    JsonObject& timeStampObj = jsonBuffer.createObject();
    times[num] = staticThis->getNodeTime();
	staticThis->debugMsg(DEBUG, "buildTimeStamp(): Hora antes de sincronizar: %u\n", times[num]);
    timeStampObj["time"] = times[num];
    timeStampObj["num"] = num;
    bool remoteAdopt = !adopt; // remote_adopt = !local_adopt
    timeStampObj["adopt"] = remoteAdopt;
    
    String timeStampStr;
    timeStampObj.printTo( timeStampStr );
    
    staticThis->debugMsg( SYNC, "buildTimeStamp(): timeStamp=%s\n", timeStampStr.c_str() );
	staticThis->debugMsg(DEBUG, "buildTimeStamp(): timeStamp=%s\n", timeStampStr.c_str());
    return timeStampStr;
}

//***********************************************************************
bool ICACHE_FLASH_ATTR timeSync::processTimeStamp(int timeSyncStatus, String &str, bool ap ) {
    staticThis->debugMsg( SYNC, "processTimeStamp(): str=%s\n", str.c_str());
    
    DynamicJsonBuffer jsonBuffer(50 );
    JsonObject& timeStampObj = jsonBuffer.parseObject(str);
	staticThis->debugMsg(DEBUG, "processTimeStamp(): Objeto JSON creado: %s\n", str.c_str());
    if ( !timeStampObj.success() ) {
        staticThis->debugMsg( ERROR, "processTimeStamp(): out of memory1?\n" );
        return false;
    }
	int8_t numTemp = timeStampObj.get<uint32_t>("num");
	staticThis->debugMsg(DEBUG, "processTimeStamp(): num local: %d. num recibido %d. timeSyncStatus: %d. AP: %s\n", num, numTemp, timeSyncStatus, ap?"true":"false");
	if (!((timeSyncStatus == 2) && (num == numTemp) && !ap)) { // Discard colliding messages if I'm STA
		num = numTemp;
		times[num] = timeStampObj.get<uint32_t>("time");
		adopt = timeStampObj.get<bool>("adopt");
		staticThis->debugMsg(DEBUG, "processTimeStamp(): Se decodifica el TimeStamp recibido. num=%d, time=%u, adopt=%s\n", num, times[num], adopt ? "true" : "false");
		num++; // Increment time sync iteration

		if (num < TIME_SYNC_CYCLES) {
			str = buildTimeStamp();
			return true;
		}
		else {
			return false;
		}
	}
	else {
		staticThis->debugMsg(DEBUG, "processTimeStamp(): Message discarded\n");
		return false;
	}
}

//***********************************************************************
void ICACHE_FLASH_ATTR timeSync::calcAdjustment( bool odd ) {
    staticThis->debugMsg( SYNC, "calcAdjustment(): odd=%u\n", odd);
	staticThis->debugMsg(DEBUG, "calcAdjustment(): odd=%u\n", odd);
    
    uint32_t    bestInterval = 0xFFFFFFFF; // Max 32 bit value
    uint8_t     bestIndex;
    uint32_t    temp;
    
    for (int i = 0; i < TIME_SYNC_CYCLES; i++) {
        //      debugMsg( GENERAL, "calcAdjustment(): times[%d]=%u\n", i, times[i]);
		staticThis->debugMsg(DEBUG, "calcAdjustment(): times[%d] --> %u. Odd = %s\n", i, times[i], (i % 2 == odd)?"true":"false");

        if ( i % 2 == odd ) {
			if (odd)
				temp = times[i - 1] - times[i]; // ??
			else
				temp = times[i + 1] - times[i]; // ??
			staticThis->debugMsg(DEBUG, "calcAdjustment(): %d is %s\n", i, odd?"odd":"even");

            //if ( i < TIME_SYNC_CYCLES - 2 ){ // If TIME_SYNC_CYCLES is 2 this never happens
			if (i < TIME_SYNC_CYCLES - 1) {
                //            debugMsg( GENERAL, "\tinterval=%u\n", temp);

                if ( temp < bestInterval ) {
                    bestInterval = temp;
                    bestIndex = i;
                }
				staticThis->debugMsg(DEBUG, "calcAdjustment(): best interval calculation. Value so far %u\n", bestInterval);

            }
			
        }
    }
    staticThis->debugMsg( SYNC, "calcAdjustment(): best interval=%u, best index=%u\n", bestInterval, bestIndex);
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
void ICACHE_FLASH_ATTR painlessMesh::startNodeSync( meshConnectionType *conn ) {

    debugMsg( SYNC, "startNodeSync(): with %d\n", conn->nodeId);
	String subs = subConnectionJson( conn );
	sendMessage( conn, conn->nodeId, NODE_SYNC_REQUEST, subs );
	conn->nodeSyncRequest = getNodeTime();
	conn->nodeSyncStatus = IN_PROGRESS;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::handleNodeSync( meshConnectionType *conn, JsonObject& root ) {
    debugMsg( SYNC, "handleNodeSync(): with %d\n", conn->nodeId );
    
    meshPackageType type = (meshPackageType)(int)root["type"];
    uint32_t        remoteNodeId = (uint32_t)root["from"];
    uint32_t        destId = (uint32_t)root["dest"];
    bool            reSyncAllSubConnections = false;
    
    if( (destId == 0) && (findConnection( remoteNodeId ) != NULL) ) {
        // this is the first NODE_SYNC_REQUEST from a station
        // is we are already connected drop this connection
        debugMsg( SYNC, "handleNodeSync(): Already connected to node %d.  Dropping\n", conn->nodeId);
        closeConnection( conn );
        return;
    }

    if ( conn->nodeId != remoteNodeId ) {
        debugMsg( SYNC, "handleNodeSync(): conn->nodeId updated from %d to %d\n", conn->nodeId, remoteNodeId );
        conn->nodeId = remoteNodeId;

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
            debugMsg( SYNC, "handleNodeSync(): valid NODE_SYNC_REQUEST %d sending NODE_SYNC_REPLY\n", conn->nodeId );
            String myOtherSubConnections = subConnectionJson( conn );
            sendMessage( conn, conn->nodeId, NODE_SYNC_REPLY, myOtherSubConnections );
            break;
        }
        case NODE_SYNC_REPLY:
            debugMsg( SYNC, "handleNodeSync(): valid NODE_SYNC_REPLY from %d\n", conn->nodeId );
			debugMsg(DEBUG, "handleNodeSync(): valid NODE_SYNC_REPLY from %d\n", conn->nodeId);
            conn->nodeSyncRequest = 0;  //reset nodeSyncRequest Timer  ????
			if (conn->lastTimeSync == 0) {
				debugMsg(DEBUG, "handleNodeSync(): timeSyncStatus changed to NEEDED\n");
				//startTimeSync(conn);
				//conn->timeSyncStatus = IN_PROGRESS;
				conn->timeSyncStatus = NEEDED;
			}
            break;
        default:
            debugMsg( ERROR, "handleNodeSync(): weird type? %d\n", type );
    }
    
    if ( reSyncAllSubConnections == true ) {
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
void ICACHE_FLASH_ATTR painlessMesh::startTimeSync( meshConnectionType *conn ) {
    debugMsg( SYNC, "startTimeSync(): with %d\n", conn->nodeId );
	debugMsg(DEBUG, "startTimeSync(): with %d, local port: %d\n", conn->nodeId, conn->esp_conn->proto.tcp->local_port);
	debugMsg(DEBUG, "startTimeSync(): timeSyncStatus changed to IN_PROGRESS\n", conn->nodeId);
	conn->timeSyncStatus = IN_PROGRESS;

	if (conn->time.num > TIME_SYNC_CYCLES) {
		debugMsg(ERROR, "startTimeSync(): Error timeSync.num not reset conn->time.num=%d\n", conn->time.num);
	}

	conn->time.num = 0; // First time num is 0.

	conn->time.adopt = adoptionCalc(conn); // should I adopt other party's time?
	//   debugMsg( GENERAL, "startTimeSync(): remoteSubCount=%d adopt=%d\n", remoteSubCount, conn->time.adopt);

	String timeStamp = conn->time.buildTimeStamp();
	staticThis->sendMessage(conn, conn->nodeId, TIME_SYNC, timeStamp);
	debugMsg(DEBUG, "startTimeSync(): Enviado mensaje %s a %u. timeSyncStatus: %d\n", timeStamp.c_str(), conn->nodeId, conn->timeSyncStatus);
}

//***********************************************************************
bool ICACHE_FLASH_ATTR painlessMesh::adoptionCalc( meshConnectionType *conn ) {
    // make the adoption calulation.  Figure out how many nodes I am connected to exclusive of this connection.
    
    uint16_t mySubCount = connectionCount( conn );  //exclude this connection.
    uint16_t remoteSubCount = jsonSubConnCount( conn->subConnections );
    
    bool ret = ( mySubCount > remoteSubCount ) ? false : true;
    
    debugMsg( GENERAL, "adoptionCalc(): mySubCount=%d remoteSubCount=%d ret = %d\n", mySubCount, remoteSubCount, ret );
	debugMsg(DEBUG, "adoptionCalc(): Subconexiones locales %d. Subconexiones remotas %d. Resultado = %s\n", mySubCount, remoteSubCount, ret ? "true":"false");
    
    return ret;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::handleTimeSync( meshConnectionType *conn, JsonObject& root ) {
    
    String timeStamp = root["msg"];
    debugMsg( SYNC, "handleTimeSync(): with %d in timestamp=%s\n", conn->nodeId, timeStamp.c_str() );
	debugMsg(DEBUG, "handleTimeSync(): Recibido mensaje TIME_SYNC desde %d con timestamp local = %u\n", conn->nodeId, getNodeTime());
	debugMsg(DEBUG, "handleTimeSync(): timeStamp = %s\n", timeStamp.c_str());
	debugMsg(DEBUG, "handleTimeSync(): ip_local: %d.%d.%d.%d, puerto local = %d\n", IP2STR(conn->esp_conn->proto.tcp->local_ip), conn->esp_conn->proto.tcp->local_port);
	debugMsg(DEBUG, "handleTimeSync(): ip_remota: %d.%d.%d.%d, puerto remoto = %d\n", IP2STR(conn->esp_conn->proto.tcp->remote_ip), conn->esp_conn->proto.tcp->remote_port);
    
    bool shouldAnswer = conn->time.processTimeStamp( conn->timeSyncStatus, timeStamp , conn->esp_conn->proto.tcp->local_port == _meshPort );  //verifies timeStamp and UPDATES it with a new one.

    debugMsg( SYNC, "handleTimeSync(): with %d out timestamp=%s\n", conn->nodeId, timeStamp.c_str() );
	debugMsg(DEBUG, "handleTimeSync(): con %d. Timestamp remoto=%u\n", conn->nodeId, conn->time.times[conn->time.num]);

    
    if ( conn->time.num < TIME_SYNC_CYCLES && shouldAnswer ) { // Answer last valid message is sync cycles are still missing
		debugMsg(DEBUG, "startTimeSync(): Enviado mensaje %s a %u. timeSyncStatus: %d\n", timeStamp.c_str(), conn->nodeId, conn->timeSyncStatus);
        staticThis->sendMessage( conn, conn->nodeId, TIME_SYNC, timeStamp );
    }
    
    uint8_t odd = conn->time.num % 2; // 1 if num is odd, 0 if even
    
    if ( (conn->time.num + odd) >= TIME_SYNC_CYCLES ) {   // timeSync completed
		staticThis->debugMsg(DEBUG, "handleTimeSync(): timeSync completed. num = %d\n", conn->time.num);
        if ( conn->time.adopt ) { // if I have to adopt
            conn->time.calcAdjustment(odd);
            
            // flag all connections for re-timeSync
            SimpleList<meshConnectionType>::iterator connection = _connections.begin();
            while ( connection != _connections.end() ) {
                if ( connection != conn ) {  // exclude this connection
                    connection->timeSyncStatus = NEEDED;
					staticThis->debugMsg(DEBUG, "handleTimeSync(): timeSyncStatus changed to NEEDED\n");
                }
                connection++;
            }
        }
        conn->lastTimeSync = getNodeTime();
        conn->timeSyncStatus = COMPLETE;
    }
}






