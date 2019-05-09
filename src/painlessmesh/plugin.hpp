#ifndef _PAINLESS_MESH_PLUGIN_HPP_
#define _PAINLESS_MESH_PLUGIN_HPP_

#ifndef ARDUINOJSON_VERSION_MAJOR
#include "ArduinoJson.h"
typedef std::string TSTRING;
#endif
#include "painlessmesh/router.hpp"

namespace painlessmesh {
namespace plugin {

class SinglePackage : public protocol::PackageInterface {
 public:
  uint32_t from;
  uint32_t dest;
  router::Type routing;
  int type;
  int noJsonFields = 4;

  SinglePackage(int type) : routing(router::SINGLE), type(type) {}

  SinglePackage(JsonObject jsonObj) {
    from = jsonObj["from"];
    dest = jsonObj["dest"];
    type = jsonObj["type"];
    routing = static_cast<router::Type>(jsonObj["routing"].as<int>());
  }

  JsonObject addTo(JsonObject&& jsonObj) {
    jsonObj["from"] = from;
    jsonObj["dest"] = dest;
    jsonObj["routing"] = static_cast<int>(routing);
    jsonObj["type"] = type;
    return jsonObj;
  }
};

class BroadcastPackage : public protocol::PackageInterface {
 public:
  uint32_t from;
  router::Type routing;
  int type;
  int noJsonFields = 3;

  BroadcastPackage(int type) : routing(router::BROADCAST), type(type) {}

  BroadcastPackage(JsonObject jsonObj) {
    from = jsonObj["from"];
    type = jsonObj["type"];
    routing = static_cast<router::Type>(jsonObj["routing"].as<int>());
  }

  JsonObject addTo(JsonObject&& jsonObj) {
    jsonObj["from"] = from;
    jsonObj["routing"] = static_cast<int>(routing);
    jsonObj["type"] = type;
    return jsonObj;
  }
};

/**
 * Handle different plugins
 *
 * Responsible for
 * - having a list of plugin types
 * - the functions defined to handle the different plugin types
 * - tasks?
 */
template <typename T>
class PluginHandler : public layout::Layout<T> {
 public:
  bool sendPackage(protocol::PackageInterface *pkg) {
    auto variant = protocol::Variant(pkg);
    // if single or neighbour with direction
    if (variant.routing() == router::SINGLE ||
        (variant.routing() == router::NEIGHBOUR && variant.dest() != 0)) {
      return router::send(variant, (*this));
      ;
    }

    // if broadcast or neighbour without direction
    if (variant.routing() == router::BROADCAST ||
        (variant.routing() == router::NEIGHBOUR && variant.dest() == 0)) {
      auto i = router::broadcast(variant, (*this), 0);
      if (i > 0) return true;
      return false;
    }
    return false;
  }

  router::MeshCallbackList<T> callbackList;
};

}  // namespace plugin
}  // namespace painlessmesh
#endif

