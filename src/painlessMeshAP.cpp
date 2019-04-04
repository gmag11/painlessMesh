//
//  painlessMeshAP.cpp
//  
//
//  Created by Bill Gray on 7/26/16.
// 
//

#include <Arduino.h>

#include "painlessMesh.h"

extern painlessMesh* staticThis;

// AP functions
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::apInit(void) {

    _apIp = IPAddress(10, (_nodeId & 0xFF00) >> 8, (_nodeId & 0xFF), 1);
    IPAddress netmask(255,255,255,0);

    WiFi.softAPConfig(_apIp, _apIp, netmask);
    WiFi.softAP(_meshSSID.c_str(), _meshPassword.c_str(), _meshChannel, _meshHidden, _meshMaxConn);

    // establish AP tcpServers
    tcpServerInit();
}

IPAddress ICACHE_FLASH_ATTR painlessMesh::getAPIP()
{
    return _apIp;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::tcpServerInit() {
    debugMsg(GENERAL, "tcpServerInit():\n");

    _tcpListener = new AsyncServer(_meshPort);
    _tcpListener->setNoDelay(true);

    _tcpListener->onClient([](void * arg, AsyncClient *client) {
#ifdef ESP32
                if( xSemaphoreTake( staticThis->xSemaphore, ( TickType_t ) 10 ) == pdTRUE )
                {
#endif
        staticThis->debugMsg(CONNECTION, "New AP connection incoming\n");
        auto conn = std::make_shared<MeshConnection>(client, staticThis, false);
        staticThis->_connections.push_back(conn);
#ifdef ESP32
        xSemaphoreGive( staticThis->xSemaphore );
    } else {
        staticThis->debugMsg(ERROR, "onConnect(): Cannot get semaphore\n");
    }
#endif
    }, NULL);

    _tcpListener->begin();

    debugMsg(STARTUP, "AP tcp server established on port %d\n", _meshPort);
    return;
}
