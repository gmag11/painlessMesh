#ifndef   _PAINLESS_MESH_JSON_H_
#define   _PAINLESS_MESH_JSON_H_

#include <memory>

#define ARDUINOJSON_USE_LONG_LONG 1
#include "Arduino.h"
#include "ArduinoJson.h"

class MeshConnection;

namespace painlessmesh {
    /// Check whether subConnections string contains 'root : true'
    bool subRooted(const String &subs);

    /**
     * Check whether a json string contains a numeric substring as a complete number
     *
     * "a:800" does contain "800", but does not contain "80"
     */
    bool stringContainsNumber(const String &subConnections,
            const String & nodeIdStr, int from = 0);

    /** 
     * Parse the message for root or rooted values and set them on the conn
     *
     * return true if root or rooted, false otherwise
     */
    bool parseNodeSyncRoot(std::shared_ptr<MeshConnection> conn, JsonObject& jsonObj, bool checkSubs = true);
};
#endif


