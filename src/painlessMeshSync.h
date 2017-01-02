#ifndef   _MESH_SYNC_H_
#define   _MESH_SYNC_H_

#include <Arduino.h>

#define SCAN_INTERVAL       10000
#define SYNC_INTERVAL       7000
#define TIME_SYNC_INTERVAL  60000000
#define TIME_SYNC_CYCLES    2 // should (must?) be an even number

enum timeSyncMessageType_t {
    TIME_SYNC_REQUEST,
    TIME_REQUEST,
    TIME_RESPONSE
};

//uint32_t getNodeTime( void );

class timeSync {
public:
    uint32_t        times[TIME_SYNC_CYCLES];
    int8_t          num = -1;
    bool            adopt;

    String buildTimeStamp(timeSyncMessageType_t timeSyncMessageType, uint32_t originateTS = 0, uint32_t receiveTS = 0, uint32_t transmitTS = 0);
    bool processTimeStamp(int timeSyncStatus, String &str, bool ap);
    void calcAdjustment(bool even);
};

#endif //   _MESH_SYNC_H_

