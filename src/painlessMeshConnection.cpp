//
//  painlessMeshConnection.cpp
//
//
//  Created by Bill Gray on 7/26/16.
//
//

#include "painlessMeshConnection.h"
#include "painlessMesh.h"

#include "painlessmesh/logger.hpp"
using namespace painlessmesh;

//#include "lwip/priv/tcpip_priv.h"

extern LogClass Log;

static painlessmesh::buffer::temp_buffer_t shared_buffer;

ICACHE_FLASH_ATTR MeshConnection::MeshConnection(
    AsyncClient *client_ptr, painlessmesh::Mesh<MeshConnection> *pMesh,
    bool is_station) {
  using namespace painlessmesh;
  station = is_station;
  mesh = pMesh;
  client = client_ptr;

  client->setNoDelay(true);
  client->setRxTimeout(NODE_TIMEOUT / TASK_SECOND);

  if (station) {  // we are the station, start nodeSync
    Log(CONNECTION, "meshConnectedCb(): we are STA\n");
  } else {
    Log(CONNECTION, "meshConnectedCb(): we are AP\n");
  }

  Log(GENERAL, "MeshConnection(): leaving\n");
}

ICACHE_FLASH_ATTR MeshConnection::~MeshConnection() {
  Log(CONNECTION, "~MeshConnection():\n");
  this->close();
  if (!client->freeable()) {
    Log(CONNECTION, "~MeshConnection(): Closing pcb\n");
    client->close(true);
  }
  client->abort();
  if (!station) delete client;
}

void MeshConnection::initTCPCallbacks() {
  using namespace logger;
  auto arg = static_cast<void *>(this);
  client->onData(
      [self = this->shared_from_this()](void *arg, AsyncClient *client,
                                        void *data, size_t len) {
        using namespace logger;
        if (self->mesh->semaphoreTake()) {
          Log(COMMUNICATION, "onData(): fromId=%u\n", self ? self->nodeId : 0);

          self->receiveBuffer.push(static_cast<const char *>(data), len,
                                   shared_buffer);

          // Signal that we are done
          self->client->ack(len);
          self->readBufferTask.forceNextIteration();

          self->mesh->semaphoreGive();
        }
      },
      arg);

  client->onAck(
      [self = this->shared_from_this()](void *arg, AsyncClient *client,
                                        size_t len, uint32_t time) {
        using namespace logger;
        if (self->mesh->semaphoreTake()) {
          if (self == NULL) {
            Log(COMMUNICATION, "onAck(): no valid connection found\n");
          }
          self->sentBufferTask.forceNextIteration();
          self->mesh->semaphoreGive();
        }
      },
      arg);

  client->onError(
      [self = this->shared_from_this()](void *arg, AsyncClient *client,
                                        int8_t err) {
        if (self->mesh->semaphoreTake()) {
          // When AsyncTCP gets an error it will call both
          // onError and onDisconnect
          // so we handle this in the onDisconnect callback
          Log(CONNECTION, "tcp_err(): MeshConnection %s\n",
              client->errorToString(err));
          self->mesh->semaphoreGive();
        }
      },
      arg);

  client->onDisconnect(
      [self = this->shared_from_this()](void *arg, AsyncClient *client) {
        if (self->mesh->semaphoreTake()) {
          if (arg == NULL) {
            Log(CONNECTION, "onDisconnect(): MeshConnection NULL\n");
            if (client->connected()) client->close(true);
            return;
          }
          auto conn = static_cast<MeshConnection *>(arg);
          Log(CONNECTION, "onDisconnect():\n");
          Log(CONNECTION, "onDisconnect(): dropping %u now= %u\n", conn->nodeId,
              self->mesh->getNodeTime());
          conn->close();
          self->mesh->semaphoreGive();
        }
      },
      arg);
}

void MeshConnection::initTasks() {
  using namespace logger;
  auto syncInterval = NODE_TIMEOUT * 0.75;
  if (!station) syncInterval = syncInterval * 4;

  this->nodeSyncTask.set(
      syncInterval, TASK_FOREVER, [self = this->shared_from_this()]() {
        Log(SYNC, "nodeSyncTask(): request with %u\n", self->nodeId);
        router::send<protocol::NodeSyncRequest, MeshConnection>(
            self->request(self->mesh->asNodeTree()), self);
      });
  mesh->mScheduler->addTask(this->nodeSyncTask);
  if (station)
    this->nodeSyncTask.enable();
  else
    this->nodeSyncTask.enableDelayed();
  // this->nodeSyncTask.enable();

  receiveBuffer = painlessmesh::buffer::ReceiveBuffer<TSTRING>();
  readBufferTask.set(100 * TASK_MILLISECOND, TASK_FOREVER,
                     [self = this->shared_from_this()]() {
                       Log(GENERAL, "readBufferTask()\n");
                       if (!self->receiveBuffer.empty()) {
                         TSTRING frnt = self->receiveBuffer.front();
                         self->receiveBuffer.pop_front();
                         if (!self->receiveBuffer.empty())
                           self->readBufferTask.forceNextIteration();
                         router::routePackage<MeshConnection>(
                             (*self->mesh), self->shared_from_this(), frnt,
                             self->mesh->callbackList,
                             self->mesh->getNodeTime());
                       }
                     });
  mesh->mScheduler->addTask(readBufferTask);
  readBufferTask.enableDelayed();

  sentBufferTask.set(
      500 * TASK_MILLISECOND, TASK_FOREVER,
      [self = this->shared_from_this()]() {
        Log(GENERAL, "sentBufferTask()\n");
        if (!self->sentBuffer.empty() && self->client->canSend()) {
          self->writeNext();
          self->sentBufferTask.forceNextIteration();
        }
      });
  mesh->mScheduler->addTask(sentBufferTask);
  sentBufferTask.enableDelayed();
}

void ICACHE_FLASH_ATTR MeshConnection::close() {
    if (!connected)
        return;

    Log(CONNECTION, "MeshConnection::close().\n");
    this->connected = false;

    this->timeSyncTask.setCallback(NULL);
    this->nodeSyncTask.setCallback(NULL);
    this->readBufferTask.setCallback(NULL);
    this->sentBufferTask.setCallback(NULL);
    this->client->onDisconnect(NULL, NULL);
    this->client->onError(NULL, NULL);

    mesh->addTask(
        (*mesh->mScheduler), TASK_SECOND, TASK_ONCE,
        [mesh = this->mesh, nodeId = this->nodeId]() {
          Log(CONNECTION, "closingTask():\n");
          Log(CONNECTION, "closingTask(): dropping %u now= %u\n", nodeId,
              mesh->getNodeTime());
          if (mesh->changedConnectionsCallback)
            mesh->changedConnectionsCallback();  // Connection dropped. Signal
          // user
          if (mesh->droppedConnectionCallback)
            mesh->droppedConnectionCallback(
                nodeId);  // Connection dropped. Signal user
          layout::syncLayout<MeshConnection>((*mesh), nodeId);
        });

    if (client->connected()) {
      Log(CONNECTION, "close(): Closing pcb\n");
      client->close();
    }

    // TODO: This should be handled by the mesh.onDisconnect callback/event
    if (station && WiFi.status() == WL_CONNECTED) {
      Log(CONNECTION, "close(): call WiFi.disconnect().\n");
      WiFi.disconnect();
    }

    receiveBuffer.clear();
    sentBuffer.clear();

    this->nodeId = 0;
    mesh->eraseClosedConnections();
    Log(CONNECTION, "MeshConnection::close() Done.\n");
}

bool ICACHE_FLASH_ATTR MeshConnection::addMessage(TSTRING &message,
                                                  bool priority) {
  if (ESP.getFreeHeap() - message.length() >=
      MIN_FREE_MEMORY) {  // If memory heap is enough, queue the message
    if (priority) {
      sentBuffer.push(message, priority);
      Log(COMMUNICATION,
          "addMessage(): Package sent to queue beginning -> %d , "
          "FreeMem: %d\n",
          sentBuffer.size(), ESP.getFreeHeap());
    } else {
      if (sentBuffer.size() < MAX_MESSAGE_QUEUE) {
        sentBuffer.push(message, priority);
        Log(COMMUNICATION,
            "addMessage(): Package sent to queue end -> %d , FreeMem: "
            "%d\n",
            sentBuffer.size(), ESP.getFreeHeap());
      } else {
        Log(ERROR, "addMessage(): Message queue full -> %d , FreeMem: %d\n",
            sentBuffer.size(), ESP.getFreeHeap());
        sentBufferTask.forceNextIteration();
        return false;
      }
    }
    sentBufferTask.forceNextIteration();
    return true;
  } else {
    // connection->sendQueue.clear(); // Discard all messages if free memory is
    // low
    Log(DEBUG, "addMessage(): Memory low, message was discarded\n");
    sentBufferTask.forceNextIteration();
    return false;
  }
}

bool ICACHE_FLASH_ATTR MeshConnection::writeNext() {
    if (sentBuffer.empty()) {
      Log(COMMUNICATION, "writeNext(): sendQueue is empty\n");
      return false;
    }
    auto len = sentBuffer.requestLength(shared_buffer.length);
    auto snd_len = client->space();
    if (len > snd_len)
        len = snd_len;
    if (len > 0) {
      // sentBuffer.read(len, shared_buffer);
      // auto written = client->write(shared_buffer.buffer, len, 1);
      auto data_ptr = sentBuffer.readPtr(len);
      auto written = client->write(data_ptr, len, 1);
      if (written == len) {
        Log(COMMUNICATION, "writeNext(): Package sent = %s\n",
            shared_buffer.buffer);
        client->send();  // TODO only do this for priority messages
        sentBuffer.freeRead();
        sentBufferTask.forceNextIteration();
        return true;
        } else if (written == 0) {
          Log(COMMUNICATION,
              "writeNext(): tcp_write Failed node=%u. Resending later\n",
              nodeId);
          return false;
        } else {
          Log(ERROR,
              "writeNext(): Less written than requested. Please report bug on "
              "the issue tracker\n");
          return false;
        }
    } else {
      Log(COMMUNICATION, "writeNext(): tcp_sndbuf not enough space\n");
      return false;
    }

}


