#ifndef CATCH_UTILS_H_
#define CATCH_UTILS_H_

/*
 * Some helper functions to be used in catch based tests
 */

#include <limits>
#include <random>

#include "painlessmesh/protocol.hpp"

static std::random_device
    rd;  // Will be used to obtain a seed for the random number engine
static std::mt19937 gen(rd());

uint32_t runif(uint32_t from, uint32_t to) {
  std::uniform_int_distribution<uint32_t> distribution(from, to);
  return distribution(gen);
}

uint32_t rbinom(size_t n, double p) {
  std::binomial_distribution<uint32_t> distribution(n, p);
  return distribution(gen);
}

std::string randomString(uint32_t length) {
  std::string str;
  for (uint32_t i = 0; i < length; ++i) {
    char rnd = (char)runif(65, 90);
    str += rnd;
  }
  return str;
}

void randomCString(char* str, uint32_t length) {
  for (uint32_t i = 0; i < length; ++i) {
    char rnd = (char)runif(65, 90);
    str[i] = rnd;
  }
  str[length] = '\0';
}

painlessmesh::protocol::Single createSingle(int length = -1) {
  auto pkg = painlessmesh::protocol::Single();
  pkg.dest = runif(0, std::numeric_limits<uint32_t>::max());
  pkg.from = runif(0, std::numeric_limits<uint32_t>::max());

  if (length < 0) length = runif(0, 4096);
  pkg.msg = randomString(length);
  return pkg;
}

painlessmesh::protocol::Broadcast createBroadcast(int length = -1) {
  auto pkg = painlessmesh::protocol::Broadcast();
  pkg.dest = runif(0, std::numeric_limits<uint32_t>::max());
  pkg.from = runif(0, std::numeric_limits<uint32_t>::max());
  if (length < 0) length = runif(0, 4096);
  pkg.msg = randomString(length);
  return pkg;
}

/*
```
{
  "dest": ...,
  "from": ...,
  "type": ...,
  "subs": [
    {
      "nodeId": ...,
      "root" : true,
      "subs": [
        {
          "nodeId": ...,
          "subs": []
        }
      ]
    }
   ]
}
```
*/
painlessmesh::protocol::NodeTree createNodeTree(int nodes, int contains_root) {
  auto pkg = painlessmesh::protocol::NodeTree();
  pkg.nodeId = runif(0, std::numeric_limits<uint32_t>::max());
  if (contains_root == 0) {
    pkg.root = true;
  }
  --nodes;  // The current node
  --contains_root;
  auto noSubs = runif(1, 5);
  for (uint32_t i = 0; i < noSubs; ++i) {
    if (nodes > 0) {
      if (i == noSubs - 1) {
        pkg.subs.push_back(createNodeTree(nodes, contains_root));
      } else {
        auto newNodes = 1 + rbinom(nodes - 1, 1.0 / noSubs);
        nodes -= newNodes;
        if (newNodes > 0)
          pkg.subs.push_back(createNodeTree(newNodes, contains_root));
        contains_root -= newNodes;
      }
    }
  }
  return pkg;
}

painlessmesh::protocol::NodeSyncReply createNodeSyncReply(
    int nodes = -1, bool contains_root = true) {
  auto pkg = painlessmesh::protocol::NodeSyncReply();
  pkg.dest = runif(0, std::numeric_limits<uint32_t>::max());
  pkg.from = runif(0, std::numeric_limits<uint32_t>::max());
  if (nodes < 0) nodes = runif(1, 254);
  auto rt = -1;
  if (contains_root) rt = runif(0, nodes - 1);
  auto ns = createNodeTree(nodes, rt);
  pkg.subs = ns.subs;
  pkg.nodeId = ns.nodeId;
  pkg.root = ns.root;
  return pkg;
}

painlessmesh::protocol::NodeSyncRequest createNodeSyncRequest(
    int nodes = -1, bool contains_root = true) {
  auto pkg = painlessmesh::protocol::NodeSyncRequest();
  pkg.dest = runif(0, std::numeric_limits<uint32_t>::max());
  pkg.from = runif(0, std::numeric_limits<uint32_t>::max());
  if (nodes < 0) nodes = runif(1, 254);
  auto rt = -1;
  if (contains_root) rt = runif(0, nodes - 1);
  auto ns = createNodeTree(nodes, rt);
  pkg.subs = ns.subs;
  pkg.nodeId = ns.nodeId;
  pkg.root = ns.root;
  return pkg;
}

/*
```
{
    "dest": 887034362,
    "from": 37418,
    "type":4,
    "msg":{
         "type":0
    }
}

{
    "dest": 887034362,
    "from": 37418,
    "type":4,
    "msg":{
        "type":1,
        "t0":32990
    }
}

{
    "dest": 37418,
    "from": 887034362,
    "type":4,
    "msg":{
        "type":2,
        "t0":32990,
        "t1":448585896,
        "t2":448596056,
    }
}
```
*/
painlessmesh::protocol::TimeSync createTimeSync(int type = -1) {
  auto pkg = painlessmesh::protocol::TimeSync();
  pkg.dest = runif(0, std::numeric_limits<uint32_t>::max());
  pkg.from = runif(0, std::numeric_limits<uint32_t>::max());

  if (type < 0) type = runif(0, 2);
  pkg.msg.type = type;
  auto t = runif(0, std::numeric_limits<uint32_t>::max());
  if (type >= 1) pkg.msg.t0 = t;
  if (type >= 2) {
    t += runif(0, 10000);
    pkg.msg.t1 = t;
    t += runif(0, 10000);
    pkg.msg.t2 = t;
  }
  return pkg;
}

painlessmesh::protocol::TimeDelay createTimeDelay(int type = -1) {
  auto pkg = painlessmesh::protocol::TimeDelay();
  pkg.dest = runif(0, std::numeric_limits<uint32_t>::max());
  pkg.from = runif(0, std::numeric_limits<uint32_t>::max());

  if (type < 0) type = runif(0, 2);
  pkg.msg.type = type;
  auto t = runif(0, std::numeric_limits<uint32_t>::max());
  if (type == 1) pkg.msg.t0 = t;
  if (type == 2) {
    t += runif(0, 10000);
    pkg.msg.t1 = t;
    t += runif(0, 10000);
    pkg.msg.t2 = t;
  }
  return pkg;
}

#endif
