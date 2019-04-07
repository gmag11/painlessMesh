#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#define ARDUINOJSON_USE_LONG_LONG 1
#include "ArduinoJson.h"
#undef ARDUINOJSON_ENABLE_ARDUINO_STRING
typedef std::string TSTRING;

#include "catch_utils.hpp"
#include "painlessmesh/protocol.hpp"

using namespace painlessmesh::protocol;

SCENARIO("A variant knows its type", "[Variant][protocol]") {
  GIVEN("A json string with the type 9 ") {
    std::string str = "{\"type\": 9}";
    WHEN("Passed to a Variant") {
      auto variant = Variant(str);

      THEN("The variant is a Single type") {
        REQUIRE(variant.is<Single>());
        REQUIRE(!variant.is<Broadcast>());
      }
    }
  }

  GIVEN("A json string with the type 8 ") {
    std::string str = "{\"type\": 8}";
    WHEN("Passed to a Variant") {
      auto variant = Variant(str);

      THEN("The variant is a Broadcast type") {
        REQUIRE(!variant.is<Single>());
        REQUIRE(variant.is<Broadcast>());
      }
    }
  }

  GIVEN("A json string with the type 6 ") {
    std::string str = "{\"type\": 6}";
    WHEN("Passed to a Variant") {
      auto variant = Variant(str);

      THEN("The variant is a NodeSyncReply type") {
        REQUIRE(!variant.is<Single>());
        REQUIRE(!variant.is<Broadcast>());
        REQUIRE(variant.is<NodeSyncReply>());
      }
    }
  }

  GIVEN("A json string with the type 5 ") {
    std::string str = "{\"type\": 5}";
    WHEN("Passed to a Variant") {
      auto variant = Variant(str);

      THEN("The variant is a NodeSyncRequest type") {
        REQUIRE(!variant.is<Single>());
        REQUIRE(!variant.is<Broadcast>());
        REQUIRE(!variant.is<NodeSyncReply>());
        REQUIRE(variant.is<NodeSyncRequest>());
      }
    }
  }

  GIVEN("A json string with the type 4 ") {
    std::string str = "{\"type\": 4}";
    WHEN("Passed to a Variant") {
      auto variant = Variant(str);

      THEN("The variant is a TimeSync type") {
        REQUIRE(!variant.is<Single>());
        REQUIRE(!variant.is<Broadcast>());
        REQUIRE(!variant.is<NodeSyncReply>());
        REQUIRE(!variant.is<NodeSyncRequest>());
        REQUIRE(variant.is<TimeSync>());
      }
    }
  }
  GIVEN("A json string with the type 3 ") {
    std::string str = "{\"type\": 3}";
    WHEN("Passed to a Variant") {
      auto variant = Variant(str);

      THEN("The variant is a TimeDelay type") {
        REQUIRE(!variant.is<Single>());
        REQUIRE(!variant.is<Broadcast>());
        REQUIRE(!variant.is<NodeSyncReply>());
        REQUIRE(!variant.is<NodeSyncRequest>());
        REQUIRE(!variant.is<TimeSync>());
        REQUIRE(variant.is<TimeDelay>());
      }
    }
  }
}

SCENARIO("A variant can take any package", "[Variant][protocol]") {
  GIVEN("A Single package") {
    auto pkg = createSingle();
    WHEN("Passed to a Variant") {
      auto variant = Variant(pkg);
      THEN("The variant is a Single type") {
        REQUIRE(variant.is<Single>());
        REQUIRE(!variant.is<Broadcast>());
        REQUIRE(!variant.is<NodeSyncReply>());
        REQUIRE(!variant.is<NodeSyncRequest>());
        REQUIRE(!variant.is<TimeSync>());
        REQUIRE(!variant.is<TimeDelay>());
      }

      THEN("The variant can be converted to a Single") {
        auto newPkg = variant.to<Single>();
        REQUIRE(newPkg.dest == pkg.dest);
        REQUIRE(newPkg.from == pkg.from);
        REQUIRE(newPkg.msg == pkg.msg);
        REQUIRE(newPkg.type == pkg.type);
      }
    }
  }

  GIVEN("A Broadcast package") {
    auto pkg = createBroadcast(5);
    WHEN("Passed to a Variant") {
      auto variant = Variant(pkg);
      THEN("The variant is a Broadcast type") {
        REQUIRE(!variant.is<Single>());
        REQUIRE(variant.is<Broadcast>());
        REQUIRE(!variant.is<NodeSyncReply>());
        REQUIRE(!variant.is<NodeSyncRequest>());
        REQUIRE(!variant.is<TimeSync>());
        REQUIRE(!variant.is<TimeDelay>());
      }

      THEN("The variant can be converted to a Broadcast") {
        auto newPkg = variant.to<Broadcast>();
        REQUIRE(newPkg.dest == pkg.dest);
        REQUIRE(newPkg.from == pkg.from);
        REQUIRE(newPkg.msg == pkg.msg);
        REQUIRE(newPkg.type == pkg.type);
      }
    }
  }

  GIVEN("A NodeSyncReply package") {
    auto pkg = createNodeSyncReply(15);
    WHEN("Passed to a Variant") {
      auto variant = Variant(pkg);
      THEN("The variant is a NodeSyncReply type") {
        REQUIRE(!variant.is<Single>());
        REQUIRE(!variant.is<Broadcast>());
        REQUIRE(variant.is<NodeSyncReply>());
        REQUIRE(!variant.is<NodeSyncRequest>());
        REQUIRE(!variant.is<TimeSync>());
        REQUIRE(!variant.is<TimeDelay>());
      }
      THEN("The variant can be converted to a NodeSyncReply") {
        auto newPkg = variant.to<NodeSyncReply>();
        REQUIRE(newPkg.dest == pkg.dest);
        REQUIRE(newPkg.from == pkg.from);
        REQUIRE(newPkg.nodeId == pkg.nodeId);
        REQUIRE(newPkg.root == pkg.root);
        REQUIRE(newPkg.subs.size() == pkg.subs.size());
        REQUIRE(newPkg.type == pkg.type);
        REQUIRE(newPkg == pkg);
      }
    }
  }

  GIVEN("A NodeSyncReply package of random size") {
    auto pkg = createNodeSyncReply();
    WHEN("Passed to a Variant") {
      auto variant = Variant(pkg);

      THEN("The variant throws no error") { REQUIRE(!variant.error); }
      THEN("The variant is a NodeSyncReply type") {
        REQUIRE(!variant.is<Single>());
        REQUIRE(!variant.is<Broadcast>());
        REQUIRE(variant.is<NodeSyncReply>());
        REQUIRE(!variant.is<NodeSyncRequest>());
        REQUIRE(!variant.is<TimeSync>());
        REQUIRE(!variant.is<TimeDelay>());
      }
      THEN("The variant can be converted to a NodeSyncReply") {
        auto newPkg = variant.to<NodeSyncReply>();
        REQUIRE(newPkg.dest == pkg.dest);
        REQUIRE(newPkg.from == pkg.from);
        REQUIRE(newPkg.nodeId == pkg.nodeId);
        REQUIRE(newPkg.root == pkg.root);
        REQUIRE(newPkg.subs.size() == pkg.subs.size());
        REQUIRE(newPkg.type == pkg.type);
        REQUIRE(newPkg == pkg);
      }
    }
  }

  GIVEN("A NodeSyncRequest package") {
    auto pkg = createNodeSyncRequest(5);
    WHEN("Passed to a Variant") {
      auto variant = Variant(pkg);
      THEN("The variant is a NodeSyncRequest type") {
        REQUIRE(!variant.is<Single>());
        REQUIRE(!variant.is<Broadcast>());
        REQUIRE(!variant.is<NodeSyncReply>());
        REQUIRE(variant.is<NodeSyncRequest>());
        REQUIRE(!variant.is<TimeSync>());
        REQUIRE(!variant.is<TimeDelay>());
      }
      THEN("The variant can be converted to a NodeSyncRequest") {
        auto newPkg = variant.to<NodeSyncRequest>();
        REQUIRE(newPkg.dest == pkg.dest);
        REQUIRE(newPkg.from == pkg.from);
        REQUIRE(newPkg.nodeId == pkg.nodeId);
        REQUIRE(newPkg.root == pkg.root);
        REQUIRE(newPkg.subs.size() == pkg.subs.size());
        REQUIRE(newPkg.type == pkg.type);
        REQUIRE(newPkg == pkg);
      }
    }
  }

  GIVEN("A TimeSync package") {
    auto pkg = createTimeSync();
    WHEN("Passed to a Variant") {
      auto variant = Variant(pkg);
      THEN("The variant is a TimeSync type") {
        REQUIRE(!variant.is<Single>());
        REQUIRE(!variant.is<Broadcast>());
        REQUIRE(!variant.is<NodeSyncReply>());
        REQUIRE(!variant.is<NodeSyncRequest>());
        REQUIRE(variant.is<TimeSync>());
        REQUIRE(!variant.is<TimeDelay>());
      }

      THEN("The variant can be converted to a TimeSync") {
        auto newPkg = variant.to<TimeSync>();
        REQUIRE(newPkg.dest == pkg.dest);
        REQUIRE(newPkg.from == pkg.from);
        REQUIRE(newPkg.type == pkg.type);
        REQUIRE(newPkg.msg.type == pkg.msg.type);
        REQUIRE(newPkg.msg.t0 == pkg.msg.t0);
        REQUIRE(newPkg.msg.t1 == pkg.msg.t1);
        REQUIRE(newPkg.msg.t2 == pkg.msg.t2);
      }
    }
  }

  GIVEN("A TimeDelay package") {
    auto pkg = createTimeDelay();
    WHEN("Passed to a Variant") {
      auto variant = Variant(pkg);
      THEN("The variant is a TimeDelay type") {
        REQUIRE(!variant.is<Single>());
        REQUIRE(!variant.is<Broadcast>());
        REQUIRE(!variant.is<NodeSyncReply>());
        REQUIRE(!variant.is<NodeSyncRequest>());
        REQUIRE(!variant.is<TimeSync>());
        REQUIRE(variant.is<TimeDelay>());
      }
      THEN("The variant can be converted to a TimeDelay") {
        auto newPkg = variant.to<TimeDelay>();
        REQUIRE(newPkg.dest == pkg.dest);
        REQUIRE(newPkg.from == pkg.from);
        REQUIRE(newPkg.type == pkg.type);
        REQUIRE(newPkg.msg.type == pkg.msg.type);
        REQUIRE(newPkg.msg.t0 == pkg.msg.t0);
        REQUIRE(newPkg.msg.t1 == pkg.msg.t1);
        REQUIRE(newPkg.msg.t2 == pkg.msg.t2);
      }
    }
  }
}

SCENARIO("NodeSyncReply is backwards compatible", "[Variant][protocol]") {
  GIVEN("A json string without a base nodeId") {
    std::string old =
        "{\"type\":6,\"dest\":2428398258,\"from\":3907768579,\"subs\":[{"
        "\"nodeId\":3959373838,\"subs\":[{\"nodeId\":416992913},{\"nodeId\":"
        "1895675348,\"root\":true}]}]}";
    std::string withId =
        "{\"type\":6,\"dest\":2428398258,\"from\":3907768579,\"nodeId\":"
        "3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{\"nodeId\":"
        "416992913},{\"nodeId\":1895675348,\"root\":true}]}]}";
    WHEN("Converted to a NodeSyncReply") {
      auto variant = Variant(old);
      auto nsr = variant.to<NodeSyncReply>();
      auto variantId = Variant(withId);
      auto nsrId = variantId.to<NodeSyncReply>();
      THEN("NodeId is set to from") {
        REQUIRE(nsr.from == nsr.nodeId);
        REQUIRE(nsr == nsrId);
      }
    }

    WHEN("Converted to a NodeTree") {
      auto variant = Variant(old);
      auto ns = variant.to<NodeTree>();
      auto variantId = Variant(withId);
      auto nsId = variantId.to<NodeTree>();
      auto variantReply = Variant(withId);
      auto nsrId = variantId.to<NodeSyncReply>();
      THEN("NodeId is set to from value") {
        REQUIRE(nsrId.from == ns.nodeId);
        REQUIRE(nsrId.nodeId == nsId.nodeId);
        REQUIRE(nsrId.subs == ns.subs);
      }
    }
  }

  GIVEN("A json string with root explicitly set to false") {
    std::string old =
        "{\"type\":6,\"dest\":2428398258,\"from\":3907768579,\"nodeId\":"
        "3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{\"nodeId\":"
        "416992913},{\"nodeId\":1895675348,\"root\":true}]}]}";
    std::string withId =
        "{\"type\":6,\"dest\":2428398258,\"root\":false,\"from\":3907768579,"
        "\"nodeId\":3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{"
        "\"nodeId\":416992913},{\"nodeId\":1895675348,\"root\":true}]}]}";
    WHEN("Converted to a NodeSyncReply") {
      auto variant = Variant(old);
      auto nsr = variant.to<NodeSyncReply>();
      auto variantId = Variant(withId);
      auto nsrId = variantId.to<NodeSyncReply>();
      THEN("NodeId is set to from") { REQUIRE(nsr == nsrId); }
    }
  }

  GIVEN("A json string with subs explicitly set to empty") {
    std::string old =
        "{\"type\":6,\"dest\":2428398258,\"from\":3907768579,\"nodeId\":"
        "3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{\"nodeId\":"
        "416992913},{\"nodeId\":1895675348,\"root\":true,\"subs\":[]}]}]}";
    std::string withId =
        "{\"type\":6,\"dest\":2428398258,\"from\":3907768579,\"nodeId\":"
        "3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{\"nodeId\":"
        "416992913},{\"nodeId\":1895675348,\"root\":true}]}]}";
    WHEN("Converted to a NodeSyncReply") {
      auto variant = Variant(old);
      auto nsr = variant.to<NodeSyncReply>();
      auto variantId = Variant(withId);
      auto nsrId = variantId.to<NodeSyncReply>();
      THEN("NodeId is set to from") { REQUIRE(nsr == nsrId); }
    }
  }
}

SCENARIO("NodeSyncReply supports the == operator", "[Variant][protocol]") {
  GIVEN("Different NodeSyncReplies") {
    auto pkg1 = createNodeSyncReply(5);
    auto pkg2 = createNodeSyncReply(5);

    // Same subs different base
    auto pkg3 = createNodeSyncReply(5);
    auto pkg4 = createNodeSyncReply(5);
    pkg4.subs = pkg3.subs;

    // Same base different subs
    auto pkg5 = pkg4;
    pkg5.subs = pkg1.subs;
    THEN("They are not equal") {
      REQUIRE(pkg1 != pkg2);
      REQUIRE(pkg2 == pkg2);

      REQUIRE(pkg3 != pkg4);
      REQUIRE(pkg3.subs == pkg4.subs);

      REQUIRE(pkg5 != pkg4);
      REQUIRE(pkg5.subs != pkg4.subs);
    }
  }
}

SCENARIO("A variant can printTo a package", "[Variant][protocol]") {
  GIVEN("A NodeSyncReply package printed to a string using Variant") {
    auto pkg = createNodeSyncReply(5);
    std::string str;
    auto variant = Variant(pkg);
    variant.printTo(str);
    THEN("It can be converted back into an identical pkg") {
      auto variant = Variant(str);
      auto pkg2 = variant.to<NodeSyncReply>();
      REQUIRE(pkg2 == pkg);
    }
  }
}

SCENARIO("The Variant type properly carries over errors",
         "[Variant][protocol][error]") {
  GIVEN("A large and small NodeSyncReply pkg") {
    auto large_pkg = createNodeSyncReply(100);
    auto large_variant = Variant(large_pkg);
    std::string large_json;
    large_variant.printTo(large_json);

    auto small_pkg = createNodeSyncReply(5);
    auto small_variant = Variant(small_pkg);
    std::string small_json;
    small_variant.printTo(small_json);

    THEN("It carries over the ArduinoJson error") {
      auto large_var = Variant(large_json, 1024);
      REQUIRE(large_var.error);
      auto small_var = Variant(small_json, 1024);
      REQUIRE(!small_var.error);
    }
  }
}
