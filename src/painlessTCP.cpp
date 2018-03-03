/*
  Asynchronous TCP library for Espressif MCUs

  Copyright (c) 2016 Hristo Gochkov. All rights reserved.
  This file is part of the esp8266 core for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Arduino.h"

#include "painlessTCP.h"
extern "C"{
#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/inet.h"
#include "lwip/dns.h"
}

#ifdef ESP32
/*
 * TCP/IP Event Task
 * */

typedef enum {
    LWIP_TCP_SENT, LWIP_TCP_RECV, LWIP_TCP_ERROR, LWIP_TCP_POLL
} lwip_event_t;

typedef struct {
        lwip_event_t event;
        void *arg;
        union {
                struct {
                        void * pcb;
                        int8_t err;
                } connected;
                struct {
                        int8_t err;
                } error;
                struct {
                        tcp_pcb * pcb;
                        uint16_t len;
                } sent;
                struct {
                        tcp_pcb * pcb;
                        pbuf * pb;
                        int8_t err;
                } recv;
                struct {
                        tcp_pcb * pcb;
                } poll;
                struct {
                        tcp_pcb * pcb;
                        int8_t err;
                } accept;
                struct {
                        const char * name;
                        ip_addr_t addr;
                } dns;
        };
} lwip_event_packet_t;

static xQueueHandle _tcp_queue;
static TaskHandle_t _tcp_service_task_handle = NULL;

static void _handle_tcp_event(lwip_event_packet_t * e){

    if(e->event == LWIP_TCP_RECV){
        TCPClient::_s_recv(e->arg, e->recv.pcb, e->recv.pb, e->recv.err);
    } else if(e->event == LWIP_TCP_SENT){
        TCPClient::_s_sent(e->arg, e->sent.pcb, e->sent.len);
    } else if(e->event == LWIP_TCP_POLL){
        TCPClient::_s_poll(e->arg, e->poll.pcb);
    } else if(e->event == LWIP_TCP_ERROR){
        TCPClient::_s_error(e->arg, e->error.err);
    }
    free((void*)(e));
}

static void _tcp_service_task(void *pvParameters){
    lwip_event_packet_t * packet = NULL;
    for (;;) {
        if(xQueueReceive(_tcp_queue, &packet, 0) == pdTRUE){
            //dispatch packet
            _handle_tcp_event(packet);
        } else {
            vTaskDelay(1);
        }
    }
    vTaskDelete(NULL);
    _tcp_service_task_handle = NULL;
}
/*
static void _stop_tcp_task(){
    if(_tcp_service_task_handle){
        vTaskDelete(_tcp_service_task_handle);
        _tcp_service_task_handle = NULL;
    }
}
*/
static bool _start_tcp_task(){
    if(!_tcp_queue){
        _tcp_queue = xQueueCreate(32, sizeof(lwip_event_packet_t *));
        if(!_tcp_queue){
            return false;
        }
    }
    if(!_tcp_service_task_handle){
        xTaskCreatePinnedToCore(_tcp_service_task, "tcp_tcp", 8192, NULL, 3, &_tcp_service_task_handle, 1);
        if(!_tcp_service_task_handle){
            return false;
        }
    }
    return true;
}

/*
 * LwIP Callbacks
 * */

static int8_t _tcp_poll(void * arg, struct tcp_pcb * pcb) {
    if(!_tcp_queue){
        return ERR_OK;
    }
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->event = LWIP_TCP_POLL;
    e->arg = arg;
    e->poll.pcb = pcb;
    if (xQueueSend(_tcp_queue, &e, portMAX_DELAY) != pdPASS) {
        free((void*)(e));
    }
    return ERR_OK;
}

static int8_t _tcp_recv(void * arg, struct tcp_pcb * pcb, struct pbuf *pb, int8_t err) {
    if(!_tcp_queue){
        return ERR_OK;
    }
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->event = LWIP_TCP_RECV;
    e->arg = arg;
    e->recv.pcb = pcb;
    e->recv.pb = pb;
    e->recv.err = err;
    if (xQueueSend(_tcp_queue, &e, portMAX_DELAY) != pdPASS) {
        free((void*)(e));
    }
    return ERR_OK;
}

static int8_t _tcp_sent(void * arg, struct tcp_pcb * pcb, uint16_t len) {
    if(!_tcp_queue){
        return ERR_OK;
    }
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->event = LWIP_TCP_SENT;
    e->arg = arg;
    e->sent.pcb = pcb;
    e->sent.len = len;
    if (xQueueSend(_tcp_queue, &e, portMAX_DELAY) != pdPASS) {
        free((void*)(e));
    }
    return ERR_OK;
}

static void _tcp_error(void * arg, int8_t err) {
    if(!_tcp_queue){
        return;
    }
    lwip_event_packet_t * e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
    e->event = LWIP_TCP_ERROR;
    e->arg = arg;
    e->error.err = err;
    if (xQueueSend(_tcp_queue, &e, portMAX_DELAY) != pdPASS) {
        free((void*)(e));
    }
}

/*
 * TCP/IP API Calls
 * */

#include "lwip/priv/tcpip_priv.h"
typedef struct {
    struct tcpip_api_call call;
    tcp_pcb * pcb;
    int8_t err;
    union {
            struct {
                    const char* data;
                    size_t size;
                    uint8_t apiflags;
            } write;
            size_t received;
            struct {
                    ip_addr_t * addr;
                    uint16_t port;
                    tcp_connected_fn cb;
            } connect;
            struct {
                    ip_addr_t * addr;
                    uint16_t port;
            } bind;
            uint8_t backlog;
    };
} tcp_api_call_t;

static err_t _tcp_output_api(struct tcpip_api_call *api_call_msg){
    tcp_api_call_t * msg = (tcp_api_call_t *)api_call_msg;
    msg->err = tcp_output(msg->pcb);
    return msg->err;
}

static esp_err_t _tcp_output(tcp_pcb * pcb) {
    tcp_api_call_t msg;
    msg.pcb = pcb;
    tcpip_api_call(_tcp_output_api, (struct tcpip_api_call*)&msg);
    return msg.err;
}

static err_t _tcp_write_api(struct tcpip_api_call *api_call_msg){
    tcp_api_call_t * msg = (tcp_api_call_t *)api_call_msg;
    msg->err = tcp_write(msg->pcb, msg->write.data, msg->write.size, msg->write.apiflags);
    return msg->err;
}

static esp_err_t _tcp_write(tcp_pcb * pcb, const char* data, size_t size, uint8_t apiflags) {
    tcp_api_call_t msg;
    msg.pcb = pcb;
    msg.write.data = data;
    msg.write.size = size;
    msg.write.apiflags = apiflags;
    tcpip_api_call(_tcp_write_api, (struct tcpip_api_call*)&msg);
    return msg.err;
}

static err_t _tcp_recved_api(struct tcpip_api_call *api_call_msg){
    tcp_api_call_t * msg = (tcp_api_call_t *)api_call_msg;
    msg->err = 0;
    tcp_recved(msg->pcb, msg->received);
    return msg->err;
}

static esp_err_t _tcp_recved(tcp_pcb * pcb, size_t len) {
    tcp_api_call_t msg;
    msg.pcb = pcb;
    msg.received = len;
    tcpip_api_call(_tcp_recved_api, (struct tcpip_api_call*)&msg);
    return msg.err;
}

static err_t _tcp_connect_api(struct tcpip_api_call *api_call_msg){
    tcp_api_call_t * msg = (tcp_api_call_t *)api_call_msg;
    msg->err = tcp_connect(msg->pcb, msg->connect.addr, msg->connect.port, msg->connect.cb);
    return msg->err;
}

static esp_err_t _tcp_connect(tcp_pcb * pcb, ip_addr_t * addr, uint16_t port, tcp_connected_fn cb) {
    tcp_api_call_t msg;
    msg.pcb = pcb;
    msg.connect.addr = addr;
    msg.connect.port = port;
    msg.connect.cb = cb;
    tcpip_api_call(_tcp_connect_api, (struct tcpip_api_call*)&msg);
    return msg.err;
}

static err_t _tcp_close_api(struct tcpip_api_call *api_call_msg){
    tcp_api_call_t * msg = (tcp_api_call_t *)api_call_msg;
    msg->err = tcp_close(msg->pcb);
    return msg->err;
}

static esp_err_t _tcp_close(tcp_pcb * pcb) {
    tcp_api_call_t msg;
    msg.pcb = pcb;
    tcpip_api_call(_tcp_close_api, (struct tcpip_api_call*)&msg);
    return msg.err;
}

static err_t _tcp_abort_api(struct tcpip_api_call *api_call_msg){
    tcp_api_call_t * msg = (tcp_api_call_t *)api_call_msg;
    msg->err = 0;
    tcp_abort(msg->pcb);
    return msg->err;
}

static esp_err_t _tcp_abort(tcp_pcb * pcb) {
    tcp_api_call_t msg;
    msg.pcb = pcb;
    tcpip_api_call(_tcp_abort_api, (struct tcpip_api_call*)&msg);
    return msg.err;
}

static err_t _tcp_bind_api(struct tcpip_api_call *api_call_msg){
    tcp_api_call_t * msg = (tcp_api_call_t *)api_call_msg;
    msg->err = tcp_bind(msg->pcb, msg->bind.addr, msg->bind.port);
    return msg->err;
}

static esp_err_t _tcp_bind(tcp_pcb * pcb, ip_addr_t * addr, uint16_t port) {
    tcp_api_call_t msg;
    msg.pcb = pcb;
    msg.bind.addr = addr;
    msg.bind.port = port;
    tcpip_api_call(_tcp_bind_api, (struct tcpip_api_call*)&msg);
    return msg.err;
}

static err_t _tcp_listen_api(struct tcpip_api_call *api_call_msg){
    tcp_api_call_t * msg = (tcp_api_call_t *)api_call_msg;
    msg->err = 0;
    msg->pcb = tcp_listen_with_backlog(msg->pcb, msg->backlog);
    return msg->err;
}

static tcp_pcb * _tcp_listen_with_backlog(tcp_pcb * pcb, uint8_t backlog) {
    tcp_api_call_t msg;
    msg.pcb = pcb;
    msg.backlog = backlog?backlog:0xFF;
    tcpip_api_call(_tcp_listen_api, (struct tcpip_api_call*)&msg);
    return msg.pcb;
}
#define _tcp_listen(p) _tcp_listen_with_backlog(p, 0xFF);
#else
#include "espInterface.h"

void ICACHE_FLASH_ATTR log_w(const char* format ...) {
}

void ICACHE_FLASH_ATTR log_e(const char* format ...) {
}

const auto& _tcp_connect = tcp_connect;
const auto& _tcp_err = tcp_err;
const auto& _tcp_write = tcp_write;
const auto& _tcp_output = tcp_output;
const auto& _tcp_recved = tcp_recved;
const auto& _tcp_listen_with_backlog = tcp_listen_with_backlog;
const auto& _tcp_bind = tcp_bind;
const auto& _tcp_close = tcp_close;
const auto& _tcp_abort = tcp_abort;

const auto& _tcp_recv = TCPClient::_s_recv;
const auto& _tcp_sent = TCPClient::_s_sent;
const auto& _tcp_error = TCPClient::_s_error;
const auto& _tcp_poll = TCPClient::_s_poll;
#endif



/*
  Async TCP Client
 */

TCPClient::TCPClient(tcp_pcb* pcb)
: _connect_cb(0)
, _connect_cb_arg(0)
, _discard_cb(0)
, _discard_cb_arg(0)
, _sent_cb(0)
, _sent_cb_arg(0)
, _error_cb(0)
, _error_cb_arg(0)
, _recv_cb(0)
, _recv_cb_arg(0)
, _timeout_cb(0)
, _timeout_cb_arg(0)
, _pcb_busy(false)
, _pcb_sent_at(0)
, _close_pcb(false)
, _ack_pcb(true)
, _rx_last_packet(0)
, _rx_since_timeout(0)
, _ack_timeout(TCP_MAX_ACK_TIME)
, _connect_port(0)
, prev(NULL)
, next(NULL)
, _in_lwip_thread(false)
{
    _pcb = pcb;
    if(_pcb){
        _rx_last_packet = millis();
        tcp_arg(_pcb, this);
        tcp_recv(_pcb, (tcp_recv_fn)&_tcp_recv);
        tcp_sent(_pcb,(tcp_sent_fn) &_tcp_sent);
        tcp_err(_pcb, (tcp_err_fn)&_tcp_error);
        tcp_poll(_pcb, (tcp_poll_fn)&_tcp_poll, 1);
    }
}

TCPClient::~TCPClient(){
    if(_pcb)
        _close();
}

bool TCPClient::connect(IPAddress ip, uint16_t port){
    if (_pcb){
        log_w("already connected, state %d", _pcb->state);
        return false;
    }
#ifdef ESP32
    if(!_start_tcp_task()){
        log_e("failed to start task");
        return false;
    }

    ip_addr_t addr;
    addr.type = IPADDR_TYPE_V4;
    addr.u_addr.ip4.addr = ip;

    tcp_pcb* pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
#else
    ip_addr_t addr;
    addr.addr = ip;
    tcp_pcb* pcb = tcp_new();
#endif

    if (!pcb){
        log_e("pcb == NULL");
        return false;
    }

    tcp_arg(pcb, this);
    tcp_err(pcb, (tcp_err_fn)&_tcp_error);
    if(_in_lwip_thread){
        tcp_connect(pcb, &addr, port,(tcp_connected_fn)&_s_connected);
    } else {
        _tcp_connect(pcb, &addr, port,(tcp_connected_fn)&_s_connected);
    }
    return true;
}

TCPClient& TCPClient::operator=(const TCPClient& other){
    if (_pcb)
        _close();

    _pcb = other._pcb;
    if (_pcb) {
        _rx_last_packet = millis();
        tcp_arg(_pcb, this);
        tcp_recv(_pcb, (tcp_recv_fn)&_tcp_recv);
        tcp_sent(_pcb, (tcp_sent_fn)&_tcp_sent);
        tcp_err(_pcb, (tcp_err_fn)&_tcp_error);
        tcp_poll(_pcb, (tcp_poll_fn)&_tcp_poll, 1);
    }
    return *this;
}

int8_t TCPClient::_connected(void* pcb, int8_t err){
    _pcb = reinterpret_cast<tcp_pcb*>(pcb);
    if(_pcb){
        _rx_last_packet = millis();
        _pcb_busy = false;
        tcp_recv(_pcb, (tcp_recv_fn)&_tcp_recv);
        tcp_sent(_pcb, (tcp_sent_fn)&_tcp_sent);
        tcp_poll(_pcb, (tcp_poll_fn)&_tcp_poll, 1);
    }
    _in_lwip_thread = true;
    if(_connect_cb)
        _connect_cb(_connect_cb_arg, this);
    _in_lwip_thread = false;
    return ERR_OK;
}

int8_t TCPClient::_close(){
    int8_t err = ERR_OK;
    if(_pcb) {
        //log_i("");
        tcp_arg(_pcb, NULL);
        tcp_sent(_pcb, NULL);
        tcp_recv(_pcb, NULL);
        tcp_err(_pcb, NULL);
        tcp_poll(_pcb, NULL, 0);
        if(_in_lwip_thread){
            err = tcp_close(_pcb);
        } else {
            err = _tcp_close(_pcb);
        }
        if(err != ERR_OK) {
            err = abort();
        }
        _pcb = NULL;
        if(_discard_cb)
            _discard_cb(_discard_cb_arg, this);
    }
    return err;
}

void TCPClient::_error(int8_t err) {
    if(_pcb){
        tcp_arg(_pcb, NULL);
        tcp_sent(_pcb, NULL);
        tcp_recv(_pcb, NULL);
        tcp_err(_pcb, NULL);
        tcp_poll(_pcb, NULL, 0);
        _pcb = NULL;
    }
    if(_error_cb)
        _error_cb(_error_cb_arg, this, err);
    if(_discard_cb)
        _discard_cb(_discard_cb_arg, this);
}

int8_t TCPClient::_sent(tcp_pcb* pcb, uint16_t len) {
    _rx_last_packet = millis();
    //log_i("%u", len);
    _pcb_busy = false;
    if(_sent_cb)
        _sent_cb(_sent_cb_arg, this, len, (millis() - _pcb_sent_at));
    return ERR_OK;
}

int8_t TCPClient::_recv(tcp_pcb* pcb, pbuf* pb, int8_t err) {
    if(pb == NULL){
        return _close();
    }

    while(pb != NULL){
        _rx_last_packet = millis();
        //we should not ack before we assimilate the data
        //log_i("%u", pb->len);
        //Serial.write((const uint8_t *)pb->payload, pb->len);
        _ack_pcb = true;
        pbuf *b = pb;
        if(_recv_cb)
            _recv_cb(_recv_cb_arg, this, b->payload, b->len);
        if(!_ack_pcb)
            _rx_ack_len += b->len;
        else
            _tcp_recved(pcb, b->len);
        pb = b->next;
        b->next = NULL;
        pbuf_free(b);
    }
    return ERR_OK;
}

int8_t TCPClient::_poll(tcp_pcb* pcb){
    // Close requested
    if(_close_pcb){
        _close_pcb = false;
        _close();
        return ERR_OK;
    }
    uint32_t now = millis();

    // ACK Timeout
    if(_pcb_busy && _ack_timeout && (now - _pcb_sent_at) >= _ack_timeout){
        _pcb_busy = false;
        log_w("ack timeout %d", pcb->state);
        if(_timeout_cb)
            _timeout_cb(_timeout_cb_arg, this, (now - _pcb_sent_at));
        return ERR_OK;
    }
    // RX Timeout
    if(_rx_since_timeout && (now - _rx_last_packet) >= (_rx_since_timeout * 1000)){
        log_w("rx timeout %d", pcb->state);
        _close();
        return ERR_OK;
    }
    // Everything is fine
    if(_poll_cb)
        _poll_cb(_poll_cb_arg, this);
    return ERR_OK;
}

void TCPClient::_dns_found(ip_addr_t *ipaddr){
    _in_lwip_thread = true;
    if(ipaddr){
#ifdef ESP32
        connect(IPAddress(ipaddr->u_addr.ip4.addr), _connect_port);
#else
        connect(IPAddress(ipaddr->addr), _connect_port);
#endif
    } else {
        log_e("dns fail");
        if(_error_cb)
            _error_cb(_error_cb_arg, this, -55);
        if(_discard_cb)
            _discard_cb(_discard_cb_arg, this);
    }
    _in_lwip_thread = false;
}

bool TCPClient::operator==(const TCPClient &other) {
    return _pcb == other._pcb;
}

bool TCPClient::connect(const char* host, uint16_t port){
    ip_addr_t addr;
    err_t err = dns_gethostbyname(host, &addr, (dns_found_callback)&_s_dns_found, this);
    if(err == ERR_OK) {
#ifdef ESP32
        return connect(IPAddress(addr.u_addr.ip4.addr), port);
#else
        return connect(IPAddress(addr.addr), port);
#endif
    } else if(err == ERR_INPROGRESS) {
        _connect_port = port;
        return true;
    }
    log_e("error: %d", err);
    return false;
}

int8_t TCPClient::abort(){
    if(_pcb) {
        log_w("state %d", _pcb->state);
        if(_in_lwip_thread){
            tcp_abort(_pcb);
        } else {
            _tcp_abort(_pcb);
        }
        _pcb = NULL;
    }
    return ERR_ABRT;
}

void TCPClient::close(bool now){
    if(_in_lwip_thread){
        tcp_recved(_pcb, _rx_ack_len);
    } else {
        _tcp_recved(_pcb, _rx_ack_len);
    }
    if(now)
        _close();
    else
        _close_pcb = true;
}

void TCPClient::stop() {
    close(false);
}

bool TCPClient::free(){
    if(!_pcb)
        return true;
    if(_pcb->state == 0 || _pcb->state > 4)
        return true;
    return false;
}

size_t TCPClient::space(){
    if((_pcb != NULL) && (_pcb->state == 4)){
        return tcp_sndbuf(_pcb);
    }
    return 0;
}

size_t TCPClient::write(const char* data) {
    if(data == NULL)
        return 0;
    return write(data, strlen(data));
}

size_t TCPClient::write(const char* data, size_t size, uint8_t apiflags) {
    size_t will_send = add(data, size, apiflags);
    if(!will_send || !send())
        return 0;
    return will_send;
}


size_t TCPClient::add(const char* data, size_t size, uint8_t apiflags) {
    if(!_pcb || size == 0 || data == NULL)
        return 0;
    size_t room = space();
    if(!room)
        return 0;
    size_t will_send = (room < size) ? room : size;
    int8_t err = ERR_OK;
    if(_in_lwip_thread){
        err = tcp_write(_pcb, data, will_send, apiflags);
    } else {
        err = _tcp_write(_pcb, data, will_send, apiflags);
    }
    if(err != ERR_OK)
        return 0;
    return will_send;
}

bool TCPClient::send(){
    int8_t err = ERR_OK;
    if(_in_lwip_thread){
        err = tcp_output(_pcb);
    } else {
        err = _tcp_output(_pcb);
    }
    if(err == ERR_OK){
        _pcb_busy = true;
        _pcb_sent_at = millis();
        return true;
    }
    return false;
}

size_t TCPClient::ack(size_t len){
    if(len > _rx_ack_len)
        len = _rx_ack_len;
    if(len){
        if(_in_lwip_thread){
            tcp_recved(_pcb, len);
        } else {
            _tcp_recved(_pcb, len);
        }
    }
    _rx_ack_len -= len;
    return len;
}

// Operators

TCPClient & TCPClient::operator+=(const TCPClient &other) {
    if(next == NULL){
        next = (TCPClient*)(&other);
        next->prev = this;
    } else {
        TCPClient *c = next;
        while(c->next != NULL) c = c->next;
        c->next =(TCPClient*)(&other);
        c->next->prev = c;
    }
    return *this;
}

void TCPClient::setRxTimeout(uint32_t timeout){
    _rx_since_timeout = timeout;
}

uint32_t TCPClient::getRxTimeout(){
    return _rx_since_timeout;
}

uint32_t TCPClient::getAckTimeout(){
    return _ack_timeout;
}

void TCPClient::setAckTimeout(uint32_t timeout){
    _ack_timeout = timeout;
}

void TCPClient::setNoDelay(bool nodelay){
    if(!_pcb)
        return;
    if(nodelay)
        tcp_nagle_disable(_pcb);
    else
        tcp_nagle_enable(_pcb);
}

bool TCPClient::getNoDelay(){
    if(!_pcb)
        return false;
    return tcp_nagle_disabled(_pcb);
}

uint16_t TCPClient::getMss(){
    if(_pcb)
        return tcp_mss(_pcb);
    return 0;
}

uint32_t TCPClient::getRemoteAddress() {
    if(!_pcb)
        return 0;
#ifdef ESP32
    return _pcb->remote_ip.u_addr.ip4.addr;
#else
    return _pcb->remote_ip.addr;
#endif

}

uint16_t TCPClient::getRemotePort() {
    if(!_pcb)
        return 0;
    return _pcb->remote_port;
}

uint32_t TCPClient::getLocalAddress() {
    if(!_pcb)
        return 0;
#ifdef ESP32
    return _pcb->local_ip.u_addr.ip4.addr;
#else
    return _pcb->local_ip.addr;
#endif
}

uint16_t TCPClient::getLocalPort() {
    if(!_pcb)
        return 0;
    return _pcb->local_port;
}

IPAddress TCPClient::remoteIP() {
    return IPAddress(getRemoteAddress());
}

uint16_t TCPClient::remotePort() {
    return getRemotePort();
}

IPAddress TCPClient::localIP() {
    return IPAddress(getLocalAddress());
}

uint16_t TCPClient::localPort() {
    return getLocalPort();
}

uint8_t TCPClient::state() {
    if(!_pcb)
        return 0;
    return _pcb->state;
}

bool TCPClient::connected(){
    if (!_pcb)
        return false;
    return _pcb->state == 4;
}

bool TCPClient::connecting(){
    if (!_pcb)
        return false;
    return _pcb->state > 0 && _pcb->state < 4;
}

bool TCPClient::disconnecting(){
    if (!_pcb)
        return false;
    return _pcb->state > 4 && _pcb->state < 10;
}

bool TCPClient::disconnected(){
    if (!_pcb)
        return true;
    return _pcb->state == 0 || _pcb->state == 10;
}

bool TCPClient::freeable(){
    if (!_pcb)
        return true;
    return _pcb->state == 0 || _pcb->state > 4;
}

bool TCPClient::canSend(){
    return space() > 0;
}


// Callback Setters

void TCPClient::onConnect(TCPConnectHandler cb, void* arg){
    _connect_cb = cb;
    _connect_cb_arg = arg;
}

void TCPClient::onDisconnect(TCPConnectHandler cb, void* arg){
    _discard_cb = cb;
    _discard_cb_arg = arg;
}

void TCPClient::onAck(TCPAckHandler cb, void* arg){
    _sent_cb = cb;
    _sent_cb_arg = arg;
}

void TCPClient::onError(TCPErrorHandler cb, void* arg){
    _error_cb = cb;
    _error_cb_arg = arg;
}

void TCPClient::onData(TCPDataHandler cb, void* arg){
    _recv_cb = cb;
    _recv_cb_arg = arg;
}

void TCPClient::onTimeout(TCPTimeoutHandler cb, void* arg){
    _timeout_cb = cb;
    _timeout_cb_arg = arg;
}

void TCPClient::onPoll(TCPConnectHandler cb, void* arg){
    _poll_cb = cb;
    _poll_cb_arg = arg;
}


void TCPClient::_s_dns_found(const char * name, ip_addr_t * ipaddr, void * arg){
    reinterpret_cast<TCPClient*>(arg)->_dns_found(ipaddr);
}

int8_t TCPClient::_s_poll(void * arg, struct tcp_pcb * pcb) {
    reinterpret_cast<TCPClient*>(arg)->_poll(pcb);
    return ERR_OK;
}

int8_t TCPClient::_s_recv(void * arg, struct tcp_pcb * pcb, struct pbuf *pb, int8_t err) {
    reinterpret_cast<TCPClient*>(arg)->_recv(pcb, pb, err);
    return ERR_OK;
}

int8_t TCPClient::_s_sent(void * arg, struct tcp_pcb * pcb, uint16_t len) {
    reinterpret_cast<TCPClient*>(arg)->_sent(pcb, len);
    return ERR_OK;
}

void TCPClient::_s_error(void * arg, int8_t err) {
    reinterpret_cast<TCPClient*>(arg)->_error(err);
}

int8_t TCPClient::_s_connected(void * arg, void * pcb, int8_t err){
    reinterpret_cast<TCPClient*>(arg)->_connected(pcb, err);
    return ERR_OK;
}

const char * TCPClient::errorToString(int8_t error){
    switch(error){
        case 0: return "OK";
        case -1: return "Out of memory error";
        case -2: return "Buffer error";
        case -3: return "Timeout";
        case -4: return "Routing problem";
        case -5: return "Operation in progress";
        case -6: return "Illegal value";
        case -7: return "Operation would block";
        case -8: return "Connection aborted";
        case -9: return "Connection reset";
        case -10: return "Connection closed";
        case -11: return "Not connected";
        case -12: return "Illegal argument";
        case -13: return "Address in use";
        case -14: return "Low-level netif error";
        case -15: return "Already connected";
        case -55: return "DNS failed";
        default: return "UNKNOWN";
    }
}

const char * TCPClient::stateToString(){
    switch(state()){
        case 0: return "Closed";
        case 1: return "Listen";
        case 2: return "SYN Sent";
        case 3: return "SYN Received";
        case 4: return "Established";
        case 5: return "FIN Wait 1";
        case 6: return "FIN Wait 2";
        case 7: return "Close Wait";
        case 8: return "Closing";
        case 9: return "Last ACK";
        case 10: return "Time Wait";
        default: return "UNKNOWN";
    }
}

/*
  Async TCP Server
 */
struct pending_pcb {
        tcp_pcb* pcb;
        pbuf *pb;
        struct pending_pcb * next;
};

TCPServer::TCPServer(IPAddress addr, uint16_t port)
: _port(port)
, _addr(addr)
, _noDelay(false)
, _in_lwip_thread(false)
, _pcb(0)
, _connect_cb(0)
, _connect_cb_arg(0)
{}

TCPServer::TCPServer(uint16_t port)
: _port(port)
, _addr((uint32_t) IPADDR_ANY)
, _noDelay(false)
, _in_lwip_thread(false)
, _pcb(0)
, _connect_cb(0)
, _connect_cb_arg(0)
{}

TCPServer::~TCPServer(){
    end();
}

void TCPServer::onClient(TCPConnectHandler cb, void* arg){
    _connect_cb = cb;
    _connect_cb_arg = arg;
}

int8_t TCPServer::_s_accept(void * arg, tcp_pcb * pcb, int8_t err){
    reinterpret_cast<TCPServer*>(arg)->_accept(pcb, err);
    return ERR_OK;
}

int8_t TCPServer::_accept(tcp_pcb* pcb, int8_t err){
    tcp_accepted(_pcb);
    if(_connect_cb){

        if (_noDelay)
            tcp_nagle_disable(pcb);
        else
            tcp_nagle_enable(pcb);

        TCPClient *c = new TCPClient(pcb);
        if(c){
            _in_lwip_thread = true;
            c->_in_lwip_thread = true;
            _connect_cb(_connect_cb_arg, c);
            c->_in_lwip_thread = false;
            _in_lwip_thread = false;
            return ERR_OK;
        }
    }
    if(tcp_close(pcb) != ERR_OK){
        tcp_abort(pcb);
    }
    log_e("FAIL");
    return ERR_OK;
}

void TCPServer::begin(){
    if(_pcb)
        return;

    int8_t err;
#ifdef ESP32
    if(!_start_tcp_task()){
        log_e("failed to start task");
        return;
    }
    _pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!_pcb){
        log_e("_pcb == NULL");
        return;
    }

    ip_addr_t local_addr;
    local_addr.type = IPADDR_TYPE_V4;
    local_addr.u_addr.ip4.addr = (uint32_t) _addr;
#else
    _pcb = tcp_new();
    if (!_pcb){
        log_e("_pcb == NULL");
        return;
    }

    ip_addr_t local_addr;
    local_addr.addr = (uint32_t) _addr;
#endif
    err = _tcp_bind(_pcb, &local_addr, _port);

    if (err != ERR_OK) {
        _tcp_close(_pcb);
        log_e("bind error: %d", err);
        return;
    }

    //static uint8_t backlog = 5;
    //_pcb = _tcp_listen_with_backlog(_pcb, backlog);
#ifdef ESP32
    _pcb = _tcp_listen(_pcb);
#else
    _pcb = tcp_listen(_pcb);
#endif

    if (!_pcb) {
        log_e("listen_pcb == NULL");
        return;
    }
    tcp_arg(_pcb, (void*) this);
    tcp_accept(_pcb, (tcp_accept_fn)&_s_accept);
}

void TCPServer::end(){
    if(_pcb){
        tcp_arg(_pcb, NULL);
        tcp_accept(_pcb, NULL);
        if(_in_lwip_thread){
            tcp_abort(_pcb);
        } else {
            _tcp_abort(_pcb);
        }
        _pcb = NULL;
    }
}

void TCPServer::setNoDelay(bool nodelay){
    _noDelay = nodelay;
}

bool TCPServer::getNoDelay(){
    return _noDelay;
}

uint8_t TCPServer::status(){
    if (!_pcb)
        return 0;
    return _pcb->state;
}
