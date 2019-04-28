#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#define ARDUINOJSON_USE_LONG_LONG 1
#include "ArduinoJson.h"
#undef ARDUINOJSON_ENABLE_ARDUINO_STRING
#undef PAINLESSMESH_ENABLE_ARDUINO_STRING
#define PAINLESSMESH_ENABLE_STD_STRING
typedef std::string TSTRING;

#include "catch_utils.hpp"

#include "painlessmesh/layout.hpp"
#include "painlessmesh/protocol.hpp"

using namespace painlessmesh;

SCENARIO("isRoot returns true if the top level Node is the root of the mesh") {
  GIVEN("A nodeTree with root as a top node") {
    std::string rootJson =
        "{\"type\":6,\"root\":true,\"dest\":2428398258,\"from\":3907768579,"
        "\"subs\":[{"
        "\"nodeId\":3959373838,\"subs\":[{\"nodeId\":416992913},{\"nodeId\":"
        "1895675348}]}]}";
    auto variant = protocol::Variant(rootJson);
    auto tree1 = variant.to<protocol::NodeTree>();
    THEN("isRoot returns true") { REQUIRE(layout::isRoot(tree1)); }
  }
  GIVEN("A nodeTree without a root as a top node") {
    std::string jsonTree1 =
        "{\"type\":6,\"dest\":2428398258,\"from\":3907768579,\"nodeId\":"
        "3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{\"nodeId\":"
        "416992913},{\"nodeId\":1895675348,\"root\":true}]}]}";
    auto variant1 = protocol::Variant(jsonTree1);
    auto tree1 = variant1.to<protocol::NodeTree>();
    std::string jsonTree2 =
        "{\"type\":6,\"dest\":2428398258,\"from\":3907768579,\"nodeId\":"
        "3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{\"nodeId\":"
        "416992913},{\"nodeId\":1895675348,\"root\":true}]}]}";
    auto variant2 = protocol::Variant(jsonTree2);
    auto tree2 = variant2.to<protocol::NodeTree>();
    THEN("isRoot returns false") {
      REQUIRE(!layout::isRoot(tree1));
      REQUIRE(!layout::isRoot(tree2));
    }
  }

  GIVEN("A random tree with a root at top level") {
    auto tree1 = createNodeTree(runif(1, 255), 0);
    THEN("isRoot returns true") { REQUIRE(layout::isRoot(tree1)); }
  }

  GIVEN("A random tree with no root at top level") {
    auto noNodes = runif(2, 255);
    auto tree1 = createNodeTree(noNodes, runif(1, noNodes - 1));
    auto tree2 = createNodeTree(runif(1, 255), -1);
    THEN("isRoot returns false") {
      REQUIRE(!layout::isRoot(tree1));
      REQUIRE(!layout::isRoot(tree2));
    }
  }
}

SCENARIO("isRooted returns true if any node in the mesh is the root node") {
  GIVEN("A nodeTree with root as a top node") {
    std::string rootJson =
        "{\"type\":6,\"root\":true,\"dest\":2428398258,\"from\":3907768579,"
        "\"subs\":[{"
        "\"nodeId\":3959373838,\"subs\":[{\"nodeId\":416992913},{\"nodeId\":"
        "1895675348}]}]}";
    auto variant = protocol::Variant(rootJson);
    auto tree1 = variant.to<protocol::NodeTree>();
    THEN("isRooted returns true") { REQUIRE(layout::isRooted(tree1)); }
  }
  GIVEN("A nodeTree with a root some where else") {
    std::string jsonTree1 =
        "{\"type\":6,\"dest\":2428398258,\"from\":3907768579,\"nodeId\":"
        "3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{\"nodeId\":"
        "416992913},{\"nodeId\":1895675348,\"root\":true}]}]}";
    auto variant1 = protocol::Variant(jsonTree1);
    auto tree1 = variant1.to<protocol::NodeTree>();
    THEN("isRooted returns true") { REQUIRE(layout::isRooted(tree1)); }
  }

  GIVEN("A nodeTree without a root any where else") {
    std::string jsonTree2 =
        "{\"type\":6,\"dest\":2428398258,\"from\":3907768579,\"nodeId\":"
        "3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{\"nodeId\":"
        "416992913},{\"nodeId\":1895675348,\"root\":true}]}]}";
    auto variant2 = protocol::Variant(jsonTree2);
    auto tree2 = variant2.to<protocol::NodeTree>();
    THEN("isRooted returns false") { REQUIRE(!layout::isRoot(tree2)); }
  }

  GIVEN("A random tree with a root") {
    auto noNodes = runif(1, 255);
    auto tree1 = createNodeTree(noNodes, runif(0, noNodes - 1));
    THEN("isRooted returns true") { REQUIRE(layout::isRooted(tree1)); }
  }

  GIVEN("A random tree without a root") {
    auto tree1 = createNodeTree(runif(1, 255), -1);
    THEN("isRooted returns false") { REQUIRE(!layout::isRooted(tree1)); }
  }
}

SCENARIO("We can get the size of the mesh") {
  GIVEN("A tree with a set size") {
    std::string jsonTree =
        "{\"type\":6,\"dest\":2428398258,\"from\":3907768579,\"nodeId\":"
        "3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{\"nodeId\":"
        "416992913},{\"nodeId\":1895675348,\"root\":true}]}]}";
    auto variant = protocol::Variant(jsonTree);
    auto tree = variant.to<protocol::NodeTree>();
    THEN("Size returns the correct size") { REQUIRE(layout::size(tree) == 4); }
  }
  GIVEN("A random tree with a set size") {
    auto noNodes = runif(1, 255);
    auto tree = createNodeTree(noNodes, runif(-1, noNodes - 1));
    THEN("Size returns the correct size") {
      REQUIRE(layout::size(tree) == noNodes);
    }
  }
}

SCENARIO("We can confirm whether a mesh contains specific nodes") {
  GIVEN("A tree with known nodes") {
    std::string jsonTree =
        "{\"type\":6,\"dest\":2428398258,\"from\":3107768579,\"nodeId\":"
        "3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{\"nodeId\":"
        "416992913},{\"nodeId\":1895675348,\"root\":true}]}]}";
    auto variant = protocol::Variant(jsonTree);
    auto tree = variant.to<protocol::NodeTree>();
    THEN(
        "contains should return the true when it contains a node, false "
        "otherwise") {
      REQUIRE(layout::contains(tree, 1895675348));
      REQUIRE(layout::contains(tree, 3907768579));
      REQUIRE(layout::contains(tree, 3959373838));
      REQUIRE(!layout::contains(tree, 0));
      REQUIRE(!layout::contains(tree, 2428398258));
      REQUIRE(!layout::contains(tree, 3107768579));
    }
  }
}

SCENARIO("A layout neighbour knows when to update its sub") {
  GIVEN("A Neighbour") {
    std::string jsonTree =
        "{\"type\":6,\"dest\":2428398258,\"from\":3107768579,\"nodeId\":"
        "3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{\"nodeId\":"
        "416992913},{\"nodeId\":1895675348,\"root\":true}]}]}";
    auto variant = protocol::Variant(jsonTree);
    auto tree = variant.to<layout::Neighbour>();
    // auto neighbour = std::interpret_cast<layout::Neighbour*>(pTree);
    auto neighbour = tree;
    THEN("When passed the same tree updateSubs() will return false") {
      REQUIRE(!neighbour.updateSubs(tree));
    }

    auto tree1 = createNodeTree(runif(2, 5), -1);
    tree1.nodeId = neighbour.nodeId;
    THEN("When passing a different tree it will get updated") {
      REQUIRE(neighbour.updateSubs(tree1));
      REQUIRE(tree1 == neighbour);
    }

    THEN("When current nodeId is zero then updateSubs() will return true") {
      neighbour.nodeId = 0;
      REQUIRE(neighbour.updateSubs(tree));
      REQUIRE(neighbour == tree);
      REQUIRE(neighbour.nodeId == tree.nodeId);
    }
  }
}

SCENARIO("findRoute works as expected with different types of connections") {
  GIVEN("A layout with Neighbour shared ptrs") {
    auto layout = layout::Layout<layout::Neighbour>();
    std::string jsonTree1 =
        "{\"type\":6,\"dest\":2428398258,\"from\":3107768579,\"nodeId\":"
        "3907768579,\"subs\":[{\"nodeId\":3959373838,\"subs\":[{\"nodeId\":"
        "416992913},{\"nodeId\":1895675348}]}]}";
    auto variant1 = protocol::Variant(jsonTree1);
    auto tree1 =
        std::make_shared<layout::Neighbour>(variant1.to<layout::Neighbour>());

    std::string jsonTree2 =
        "{\"type\":6,\"dest\":2428398258,\"from\":3107768579,\"nodeId\":"
        "3907768580,\"subs\":[{\"nodeId\":3959373839,\"subs\":[{\"nodeId\":"
        "416992914},{\"nodeId\":1895675349}]}]}";
    auto variant2 = protocol::Variant(jsonTree2);
    auto tree2 =
        std::make_shared<layout::Neighbour>(variant2.to<layout::Neighbour>());

    std::string jsonTree3 =
        "{\"type\":6,\"dest\":2428398258,\"from\":3107768579,\"nodeId\":"
        "3907768581,\"subs\":[{\"nodeId\":3959373840,\"subs\":[{\"nodeId\":"
        "416992915},{\"nodeId\":1895675350}]}]}";
    auto variant3 = protocol::Variant(jsonTree3);
    auto tree3 =
        std::make_shared<layout::Neighbour>(variant3.to<layout::Neighbour>());

    layout.subs.push_back(tree1);
    layout.subs.push_back(tree2);
    layout.subs.push_back(tree3);

    layout.nodeId = runif(1, 1000);

    THEN("findRoute works") {
      auto rt = layout::findRoute<layout::Neighbour>(layout, 1895675350);
      REQUIRE(rt->nodeId == 3907768581);
      rt = layout::findRoute<layout::Neighbour>(layout, 1895675351);
      REQUIRE(!rt);
    }

    THEN("It can be converted to a NodeTree") {
      auto lay = layout.asNodeTree();
      auto nt = protocol::NodeTree();
      nt.nodeId = lay.nodeId;
      for (auto &&s : lay.subs) {
        nt.subs.push_back(s);
      }
      REQUIRE(nt == lay);
    }
  }
}
