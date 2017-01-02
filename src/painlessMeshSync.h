#ifndef   _MESH_SYNC_H_
#define   _MESH_SYNC_H_

#include <Arduino.h>

#define SCAN_INTERVAL       10000
#define SYNC_INTERVAL       7000
#define TIME_SYNC_INTERVAL  60000000
//#define TIME_SYNC_CYCLES    4 // should (must?) be an even number
#define NUMBER_OF_TIMESTAMS 4

enum timeSyncMessageType_t {
    TIME_SYNC_ERROR = -1,
    TIME_SYNC_REQUEST,
    TIME_REQUEST,
    TIME_RESPONSE
};

//uint32_t getNodeTime( void );

class timeSync {
public:
    uint32_t        times[NUMBER_OF_TIMESTAMS];
    //int8_t          num = -1;
    //bool            adopt;

    String buildTimeStamp(timeSyncMessageType_t timeSyncMessageType, uint32_t originateTS = 0, uint32_t receiveTS = 0, uint32_t transmitTS = 0);
    void calcAdjustment(bool even);
    timeSyncMessageType_t processTimeStamp(String &str);
};

#endif //   _MESH_SYNC_H_

