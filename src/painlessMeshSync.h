#ifndef   _MESH_SYNC_H_
#define   _MESH_SYNC_H_

#include <Arduino.h>

//#define TIME_SYNC_INTERVAL  6000000  // Time resync period, in us. 600 sec = 10 min
#define TIME_SYNC_INTERVAL  600000000  // Time resync period, in us. 600 sec = 10 min
#define SYNC_RESPONSE_TIMEOUT 5000000 // Max time to wait for time response. 5 sec
#define NUMBER_OF_TIMESTAMPS 4   // 4 timestamps are needed for time offset calculation
#define MIN_ACCURACY        10000 // Minimum time sync accuracy

enum timeSyncMessageType_t {
    TIME_SYNC_ERROR = -1,
    TIME_SYNC_REQUEST,
    TIME_REQUEST,
    TIME_RESPONSE
};

class timeSync {
public:
    uint32_t              timeDelay[NUMBER_OF_TIMESTAMPS]; // timestamp array

    String                buildTimeStamp(timeSyncMessageType_t timeSyncMessageType, uint32_t originateTS = 0, uint32_t receiveTS = 0, uint32_t transmitTS = 0);
    timeSyncMessageType_t processTimeStampDelay(String &str);
    int32_t               calcAdjustment(uint32_t times[NUMBER_OF_TIMESTAMPS]);
    int32_t               delayCalc();

};

#endif //   _MESH_SYNC_H_

