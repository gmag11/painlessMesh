//
//  painlessMeshAP.cpp
//  
//
//  Created by Bill Gray on 7/26/16.
// 
//

#include <Arduino.h>

#include "painlessMesh.h"

extern LogClass Log;

// AP functions
//***********************************************************************
void ICACHE_FLASH_ATTR painlessmesh::wifi::Mesh::apInit(uint32_t nodeId) {
  _apIp = IPAddress(10, (nodeId & 0xFF00) >> 8, (nodeId & 0xFF), 1);
  IPAddress netmask(255, 255, 255, 0);

  WiFi.softAPConfig(_apIp, _apIp, netmask);
  WiFi.softAP(_meshSSID.c_str(), _meshPassword.c_str(), _meshChannel,
              _meshHidden, _meshMaxConn);
}

IPAddress ICACHE_FLASH_ATTR painlessMesh::getAPIP()
{
    return _apIp;
}

