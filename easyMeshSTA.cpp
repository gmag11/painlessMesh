//
//  easyMeshSTA.cpp
//  
//
//  Created by Bill Gray on 7/26/16.
//
//

#include <Arduino.h>
#include <SimpleList.h>

extern "C" {
#include "user_interface.h"
#include "espconn.h"
}

#include "easyMesh.h"


extern easyMesh* staticThis;

// Station functions
//***********************************************************************
void easyMesh::stationInit( void ) {
    startStationScan();
    return;
}

//***********************************************************************
void easyMesh::manageStation( void ) {
    char bestAP[32];
    uint8 stationStatus = wifi_station_get_connect_status();
    
    if ( stationStatus == STATION_GOT_IP ) // everything is up and running
        return;
    
    if ( stationStatus == STATION_IDLE ) {
    }
    
    if ( stationStatus == 2 || stationStatus == 3 || stationStatus == 4 ) {
        DEBUG_MSG("Wierdness in manageStation() %d\n", stationStatus );
    }
}

//***********************************************************************
void easyMesh::startStationScan( void ) {
    if ( _scanStatus != IDLE ) {
        return;
    }
    
    if ( !wifi_station_scan(NULL, stationScanCb) ) {
        DEBUG_MSG("wifi_station_scan() failed!?\n");
        return;
    }
    _scanStatus = SCANNING;
    //    DEBUG_MSG("-->scan started @ %d<--\n", system_get_time());
    return;
}

//***********************************************************************
void easyMesh::scanTimerCallback( void *arg ) {
    staticThis->startStationScan();
}

//***********************************************************************
void easyMesh::stationScanCb(void *arg, STATUS status) {
    char ssid[32];
    bss_info *bssInfo = (bss_info *)arg;
    //   DEBUG_MSG("-- > scan finished @ % d < --\n", system_get_time());
    staticThis->_scanStatus = IDLE;
    
    staticThis->_meshAPs.clear();
    while (bssInfo != NULL) {
        //       DEBUG_MSG("found : % s, % ddBm", (char*)bssInfo->ssid, (int16_t) bssInfo->rssi );
        if ( strncmp( (char*)bssInfo->ssid, MESH_PREFIX, strlen(MESH_PREFIX) ) == 0 ) {
            //         DEBUG_MSG(" < ---");
            staticThis->_meshAPs.push_back( *bssInfo );
        }
        //     DEBUG_MSG("\n");
        bssInfo = STAILQ_NEXT(bssInfo, next);
    }
    //    DEBUG_MSG("Found % d nodes with MESH_PREFIX = \"%s\"\n", staticThis->_meshAPs.size(), MESH_PREFIX );
    
    staticThis->connectToBestAP();
}

//***********************************************************************
bool easyMesh::connectToBestAP( void ) {
    
    SimpleList<meshConnection_t>::iterator connection = _connections.begin();
    while ( connection != _connections.end() ) {
        SimpleList<bss_info>::iterator ap = _meshAPs.begin();
        while( ap != _meshAPs.end() ) {
            String apChipId = (char*)ap->ssid + strlen( MESH_PREFIX);
            //            DEBUG_MSG("connectToBestAP: sort - ssid=%s, apChipId=%s", ap->ssid, apChipId.c_str());
            
            
            if ( apChipId.toInt() == connection->chipId ) {
                ap = _meshAPs.erase( ap );
                //                DEBUG_MSG("<--already connected\n");
            }
            else {
                ap++;
                //              Serial.print("\n");
            }
        }
        connection++;
    }
    
    uint8 statusCode = wifi_station_get_connect_status();
    if ( statusCode != STATION_IDLE ) {
        DEBUG_MSG("connectToBestAP(): station not idle.  code=%d\n", statusCode);
        return false;
    }
    
    if ( staticThis->_meshAPs.empty() ) {  // no meshNodes left in most recent scan
        //      DEBUG_MSG("connectToBestAP(): no nodes left in list\n");
        // wait 5 seconds and rescan;
        os_timer_setfn( &_scanTimer, scanTimerCallback, NULL );
        os_timer_arm( &_scanTimer, SCAN_INTERVAL, 0 );
        return false;
    }
    
    // if we are here, then we have a list of at least 1 meshAPs.
    // find strongest signal of remaining meshAPs... that is not already connected to our AP.
    _nodeStatus = FOUND_MESH;
    SimpleList<bss_info>::iterator bestAP = staticThis->_meshAPs.begin();
    SimpleList<bss_info>::iterator i = staticThis->_meshAPs.begin();
    while ( i != staticThis->_meshAPs.end() ) {
        if ( i->rssi > bestAP->rssi ) {
            bestAP = i;
        }
        ++i;
    }
    
    // connect to bestAP
    //    DEBUG_MSG("connectToBestAP(): Best AP is %s<---\n", (char*)bestAP->ssid );
    struct station_config stationConf;
    stationConf.bssid_set = 0;
    memcpy(&stationConf.ssid, bestAP->ssid, 32);
    memcpy(&stationConf.password, MESH_PASSWORD, 64);
    wifi_station_set_config(&stationConf);
    wifi_station_connect();
    
    _meshAPs.erase( bestAP );    // drop bestAP from mesh list, so if doesn't work out, we can try the next one
    return true;
}

//***********************************************************************
void easyMesh::tcpConnect( void ) {
    struct ip_info ipconfig;
    wifi_get_ip_info(STATION_IF, &ipconfig);
    
    if ( wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0 ) {
        DEBUG_MSG("Got local IP=%d.%d.%d.%d\n", IP2STR(&ipconfig.ip) );
        DEBUG_MSG("Dest IP=%d.%d.%d.%d\n", IP2STR( &ipconfig.gw ) );
        
        _stationConn.type = ESPCONN_TCP;
        _stationConn.state = ESPCONN_NONE;
        _stationConn.proto.tcp = &_stationTcp;
        _stationConn.proto.tcp->local_port = espconn_port();
        _stationConn.proto.tcp->remote_port = MESH_PORT;
        os_memcpy(_stationConn.proto.tcp->local_ip, &ipconfig.ip, 4);
        os_memcpy(_stationConn.proto.tcp->remote_ip, &ipconfig.gw, 4);
        
        DEBUG_MSG("conn Print type=%d, state=%d, local_ip=%d.%d.%d.%d, local_port=%d, remote_ip=%d.%d.%d.%d remote_port=%d\n",
                      _stationConn.type,
                      _stationConn.state,
                      IP2STR(_stationConn.proto.tcp->local_ip),
                      _stationConn.proto.tcp->local_port,
                      IP2STR(_stationConn.proto.tcp->remote_ip),
                      _stationConn.proto.tcp->remote_port );
        
        espconn_regist_connectcb(&_stationConn, meshConnectedCb);
        espconn_regist_recvcb(&_stationConn, meshRecvCb);
        espconn_regist_sentcb(&_stationConn, meshSentCb);
        espconn_regist_reconcb(&_stationConn, meshReconCb);
        espconn_regist_disconcb(&_stationConn, meshDisconCb);
        
        sint8  errCode = espconn_connect(&_stationConn);
        if ( errCode != 0 ) {
            DEBUG_MSG("espconn_connect() falied=%d\n", errCode );
        }
    }
    else {
        DEBUG_MSG("ERR: Something un expected in tcpConnect()\n");
    }
    DEBUG_MSG("leaving tcpConnect()\n");
}
