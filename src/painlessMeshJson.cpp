#include "painlessMeshJson.h"

#include "painlessMesh.h"

extern LogClass Log;

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

