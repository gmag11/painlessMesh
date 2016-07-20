#ifndef   _MESH_SYNC_H_
#define   _MESH_SYNC_H_

#include <Arduino.h>
#include <SimpleList.h>
#include <ArduinoJson.h>

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
    
#define SCAN_INTERVAL       10000
#define SYNC_INTERVAL       7000
#define TIME_SYNC_CYCLES    10 // should (must?) be an even number

uint32_t getNodeTime( void );

class timeSync {
public:
    uint32_t        times[TIME_SYNC_CYCLES];
    int8_t          num = -1;
    bool            adopt;

    String buildTimeStamp( void );
    bool processTimeStamp( String &str);
    void calcAdjustment ( bool even );
};

#endif //   _MESH_SYNC_H_

