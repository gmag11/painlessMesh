#ifndef   _MESH_WEB_SERVER_H_
#define   _MESH_WEB_SERVER_H_

#include <Arduino.h>
#include <SimpleList.h>

extern "C" {
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"
#include "uart.h"

#include "c_types.h"
#include "espconn.h"
#include "mem.h"
}

#define WEB_PORT          80


void webServerConnectCb(void *arg);

void webServerRecvCb(void *arg, char *data, unsigned short length);
void webServerSentCb(void *arg);
void webServerDisconCb(void *arg);
void webServerReconCb(void *arg, sint8 err);

#endif //_MESH_WEB_SERVER_H_
