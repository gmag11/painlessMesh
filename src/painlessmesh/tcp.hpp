#ifndef _PAINLESS_MESH_TCP_HPP_
#define _PAINLESS_MESH_TCP_HPP_

#include <list>

#include "Arduino.h"
#include "painlessmesh/configuration.hpp"

#include "painlessmesh/logger.hpp"

namespace painlessmesh {
namespace tcp {
inline uint32_t encodeNodeId(const uint8_t *hwaddr) {
  using namespace painlessmesh::logger;
  Log(GENERAL, "encodeNodeId():\n");
  uint32_t value = 0;

  value |= hwaddr[2] << 24;  // Big endian (aka "network order"):
  value |= hwaddr[3] << 16;
  value |= hwaddr[4] << 8;
  value |= hwaddr[5];
  return value;
}

template <class T, class M>
void initServer(AsyncServer &server, M &mesh) {
  using namespace logger;
  server.setNoDelay(true);

  server.onClient(
      [&mesh](void *arg, AsyncClient *client) {
        if (mesh.semaphoreTake()) {
          Log(CONNECTION, "New AP connection incoming\n");
          auto conn = std::make_shared<T>(client, &mesh, false);
          conn->initTasks();
          conn->initTCPCallbacks();
          mesh.subs.push_back(conn);
          mesh.semaphoreGive();
        }
      },
      NULL);
  server.begin();
}

template <class T, class M>
void connect(AsyncClient &client, IPAddress ip, uint16_t port, M &mesh) {
  using namespace logger;
  client.onError([&mesh](void *, AsyncClient *client, int8_t err) {
    if (mesh.semaphoreTake()) {
      Log(CONNECTION, "tcp_err(): error trying to connect %d\n", err);
      mesh.droppedConnectionCallbacks.execute(0, true);
      mesh.semaphoreGive();
    }
  });

  client.onConnect(
      [&mesh](void *, AsyncClient *client) {
        if (mesh.semaphoreTake()) {
          Log(CONNECTION, "New STA connection incoming\n");
          auto conn = std::make_shared<T>(client, &mesh, true);
          conn->initTasks();
          conn->initTCPCallbacks();
          mesh.subs.push_back(conn);
          mesh.semaphoreGive();
        }
      },
      NULL);

  client.connect(ip, port);
}
}  // namespace tcp
}  // namespace painlessmesh
#endif
