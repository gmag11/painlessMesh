#include "painlessMeshJson.h"

#include "painlessMesh.h"

bool ICACHE_FLASH_ATTR painlessmesh::subRooted(const String &subs) {
    auto id = subs.indexOf("root");
    if (id == 0 || (id > 0 &&
                // Space or { or tab or \n " or '
                (subs[id - 1] == 32 || subs[id - 1] == 123 || 
                 subs[id - 1] == 9 || subs[id - 1] == 10 || 
                 subs[id - 1] == 34 || subs[id - 1] == 39))) {
        // 4 + 4 + 4 is length of "root", + optional white space + length of "true"
        id = subs.lastIndexOf("true", id + 4 + 4 + 4);
        if (id >= 0)
            return true;
    }
    return false;
}

// Check whether a string contains a numeric substring as a complete number
//
// "a:800" does contain "800", but does not contain "80"
bool ICACHE_FLASH_ATTR painlessmesh::stringContainsNumber(const String &subConnections,
                                             const String & nodeIdStr, int from) {
    auto index = subConnections.indexOf(nodeIdStr, from);
    if (index == -1)
        return false;
    // Check that the preceding and following characters are not a number
    else if (index > 0 &&
             index + nodeIdStr.length() + 1 < subConnections.length() &&
             // Preceding character is not a number
             (subConnections.charAt(index - 1) < '0' ||
             subConnections.charAt(index - 1) > '9') &&
             // Following character is not a number
             (subConnections.charAt(index + nodeIdStr.length() + 1) < '0' ||
             subConnections.charAt(index + nodeIdStr.length() + 1) > '9')
             ) {
        return true;
    } else { // Check whether the nodeid occurs further in the subConnections string
        return stringContainsNumber(subConnections, nodeIdStr,
                                    index + nodeIdStr.length());
    }
    return false;
}

bool ICACHE_FLASH_ATTR painlessmesh::parseNodeSyncRoot(std::shared_ptr<MeshConnection> conn, JsonObject& jsonObj, bool checkSubs) {
    if (jsonObj.containsKey("root") && jsonObj["root"].as<bool>()) {
        conn->root = true;
        return true;
    }
    conn->root = false;

    if (checkSubs) {
        String inComingSubs = jsonObj["subs"];
        if (subRooted(inComingSubs)) {
            conn->rooted = true;
            return true;
        } else {
            conn->rooted = false;
        }
    }
    return conn->rooted;
}
