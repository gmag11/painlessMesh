#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#define ARDUINOJSON_USE_LONG_LONG 1
#include "ArduinoJson.h"
#undef ARDUINOJSON_ENABLE_ARDUINO_STRING
typedef std::string TSTRING;

#include "catch_utils.hpp"
#include "painlessmesh/plugin.hpp"

using namespace painlessmesh;

class NamePackage : public plugin::Package {};

// We will test this by implementing the named mesh as a plugin
SCENARIO("We want to implement a named mesh as a custom type") {
  // Note that every plugin will have a unique id (which is also the package
  // type) that can be used to get access to handling functions, data etc.

  // Every plugin has one (or more) class inheriting from plugin::package

  // PluginHandler will be inherited into the painlessMesh class
  // and is responsible for routing

  // Note data will be included in the handling functions
  // [&data](plugin::Package)

  // We should have a namedMesh, which implements sendSingle("name",msg);
  // Then allow people to do class Mesh : public painlessMesh, public NamedMesh

  // Would be nice to be able to override onReceive callback, so we can do
  // something else
}

// Maybe also add ota implementation

// NOTE routing in painlessMesh handleMessage needs to be improved:
// If variant.routing == NEIGHBOUR -> handle and don't sent on
// If variant.routing == SINGLE -> if variant.dest is this nodeID: handle, else
// sent on to its destination if variant.routing == BROADCAST -> handle and sent
// on We can check if routing key exists, otherwise for old messages we can
// hardcode it, with most being NEIGHBOUR except for the SINGLE message type and
// BROADCAST msg type
// Later we could maybe add a multicast type. This would be relatively easy by
// having destination be an vector of nodeIDs.. although would need to calculate
// the set of nodes contained in each route.

// We may need to implement a general Plugin class, which can be passed to
// the handler and will be hold the different packages and tasks.
// This will come out when I try to add actual plugins such as namedmesh
// and ota
