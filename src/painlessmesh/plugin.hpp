#ifndef _PAINLESS_MESH_PLUGIN_HPP_
#define _PAINLESS_MESH_PLUGIN_HPP_

#ifndef ARDUINOJSON_VERSION_MAJOR
#include "ArduinoJson.h"
typedef std::string TSTRING;
#endif

namespace painlessmesh {
namespace plugin {

class Package {
 public:
  uint32_t from = 0;
  uint32_t dest = 0;
  int type = 0;

  Package(JsonObject jsonObj);

  virtual JsonObject addTo(JsonObject&& jsonObj);
};

/**
 * Handle different plugins
 *
 * Responsible for 
 * - having a list of plugin types
 * - the functions defined to handle the different plugin types
 * - tasks?
 */
class PluginHandler {
};

}  // namespace protocol
}  // namespace painlessmesh
#endif

