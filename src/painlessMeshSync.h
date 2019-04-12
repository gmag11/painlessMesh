#ifndef   _MESH_SYNC_H_
#define   _MESH_SYNC_H_

#include "Arduino.h"

#ifndef TIME_SYNC_INTERVAL
#define TIME_SYNC_INTERVAL  1*TASK_MINUTE  // Time resync period
#endif

#define NUMBER_OF_TIMESTAMPS 4   // 4 timestamps are needed for time offset calculation

#ifndef TIME_SYNC_ACCURACY
#define TIME_SYNC_ACCURACY   5000 // Minimum time sync accuracy (5ms
#endif

// Forward declaration
namespace painlessmesh {
namespace protocol {
class TimeDelay;
}
}  // namespace painlessmesh

class timeSync {
public:
    uint32_t              timeDelay[NUMBER_OF_TIMESTAMPS]; // timestamp array

    int processTimeStampDelay(painlessmesh::protocol::TimeDelay timeDelay);
    int32_t               calcAdjustment(uint32_t times[NUMBER_OF_TIMESTAMPS]);
    int32_t               delayCalc();

};

#endif //   _MESH_SYNC_H_

