#pragma once

#include <functional>
#include <cstring>
#include "Arduino.h"

#define ASYNC_WRITE_FLAG_COPY \
  0x01  // will allocate new buffer to hold the data while sending (else will
        // hold reference to the data given)
#define ASYNC_WRITE_FLAG_MORE \
  0x02  // will not send PSH flag, meaning that there should be more data to be
        // sent before the application should react.

class AsyncClient;

typedef std::function<void(void*, AsyncClient*)> AcConnectHandler;
typedef std::function<void(void*, AsyncClient*, size_t len, uint32_t time)>
    AcAckHandler;
typedef std::function<void(void*, AsyncClient*, int8_t error)> AcErrorHandler;
typedef std::function<void(void*, AsyncClient*, void* data, size_t len)>
    AcDataHandler;
typedef std::function<void(void*, AsyncClient*, struct pbuf* pb)>
    AcPacketHandler;
typedef std::function<void(void*, AsyncClient*, uint32_t time)>
    AcTimeoutHandler;

class AsyncServer;

class AsyncClient {
 public:
  AsyncClient() {}

  AsyncClient(AsyncServer* server) : mServer(server) {}

  void setNoDelay(bool nodelay) {}
  void setRxTimeout(uint32_t timeout) {}
  void onData(AcDataHandler cb, void* arg = 0) {_recv_cb = cb;}
  void onAck(AcAckHandler cb, void* arg = 0) { _sent_cb = cb; }
  void onError(AcErrorHandler cb, void* arg = 0) {}
  void onDisconnect(AcConnectHandler cb, void* arg = 0) {_discard_cb = cb;}
  void onConnect(AcConnectHandler cb, void* arg = 0) { _connect_cb = cb; }

  const char* errorToString(int err) { return ""; }

  bool connected() { return mOther; }

  bool canSend() { return true; }
  void ack(int len) {
    if (_sent_cb) 
      _sent_cb(NULL, this, len, 0);
  }
  void close(bool now = false) {
    /*if (mOther && mOther->_discard_cb) {
      mOther->_discard_cb(NULL, this);
      mOther = NULL;
    }
    mServer = NULL;*/
  }
  bool connect(IPAddress ip, uint16_t port);
  size_t space() { return 1000; }
  bool send() { return true; }
  size_t write(const char* data, size_t size,
               uint8_t apiflags = ASYNC_WRITE_FLAG_COPY) {
    char* cpy[size];
    memcpy(&cpy, data, size);
    void * arg  = NULL;
    if (mOther && mOther->_recv_cb) {
      mOther->_recv_cb(arg, mOther, cpy, size);
    }
    return size;
  }

  bool freeable() { return true; }
  int8_t abort() { return 0; }
  bool free() { return true; }

  bool operator==(const AsyncClient &other) {
    return mOther == other.mOther;
  }

 protected:
  AsyncServer* mServer = NULL;
  AsyncClient* mOther = NULL;
  AcConnectHandler _connect_cb = NULL;
  AcConnectHandler _discard_cb = NULL;
  AcDataHandler _recv_cb = NULL;
  AcAckHandler _sent_cb = NULL;
};

class AsyncServer : public AsyncClient {
 public:
  AsyncServer() {}
  AsyncServer(uint16_t port) {}
  void onClient(AcConnectHandler cb, void* arg = 0) { _connect_cb = cb; }
  void begin() {}
};

typedef enum {
    WL_NO_SHIELD        = 255,   // for compatibility with WiFi Shield library
    WL_IDLE_STATUS      = 0,
    WL_NO_SSID_AVAIL    = 1,
    WL_SCAN_COMPLETED   = 2,
    WL_CONNECTED        = 3,
    WL_CONNECT_FAILED   = 4,
    WL_CONNECTION_LOST  = 5,
    WL_DISCONNECTED     = 6
} wl_status_t;

class WiFiClass {
 public:
  void disconnect() {}
  auto status() {
    return WL_CONNECTED;
  }
};

class ESPClass {
 public:
  size_t getFreeHeap() { return 1e6; }
};

inline bool AsyncClient::connect(IPAddress ip, uint16_t port) {
  this->mOther = new AsyncClient();
  this->mOther->mOther = this;
  void * arg  = NULL;
  if (mServer->_connect_cb)
    mServer->_connect_cb(arg, this->mOther);
  if (this->_connect_cb)
    this->_connect_cb(arg, this);
  return true;
}


