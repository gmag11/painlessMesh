//
//  painlessMeshConnection.cpp
//
//
//  Created by Bill Gray on 7/26/16.
//
//

#include <Arduino.h>
#include <ArduinoJson.h>

#include "painlessMesh.h"

//#include "lwip/priv/tcpip_priv.h"

extern painlessMesh* staticThis;

static temp_buffer_t shared_buffer;

ICACHE_FLASH_ATTR ReceiveBuffer::ReceiveBuffer() {
    buffer = String();
}

void ICACHE_FLASH_ATTR ReceiveBuffer::push(const char * cstr, 
        size_t length, temp_buffer_t &buf) {
    auto data_ptr = cstr;
    do {
        auto len = strnlen(data_ptr, length);
        do {
            auto read_len = min(len, buf.length);
            memcpy(buf.buffer, data_ptr, read_len);
            buf.buffer[read_len] = '\0';
            auto newBuffer = String(buf.buffer);
            buffer.concat(newBuffer);
            len -= newBuffer.length();
            length -= newBuffer.length();
            data_ptr += newBuffer.length()*sizeof(char);
        } while (len > 0);
        if (length > 0) {
            length -= 1;
            data_ptr += 1*sizeof(char);
            if (buffer.length() > 0) { // skip empty buffers
                jsonStrings.push_back(buffer);
                buffer = String();
            }
        }
    } while (length > 0);
    staticThis->debugMsg(COMMUNICATION, "ReceiveBuffer::push(): buffer size=%u, %u\n", jsonStrings.size(), buffer.length());
}

String ICACHE_FLASH_ATTR ReceiveBuffer::front() {
    if (!empty())
        return (*jsonStrings.begin());
    return String();
}

void ICACHE_FLASH_ATTR ReceiveBuffer::pop_front() {
    jsonStrings.pop_front();
}

bool ICACHE_FLASH_ATTR ReceiveBuffer::empty() {
    return jsonStrings.empty();
}

void ICACHE_FLASH_ATTR ReceiveBuffer::clear() {
    jsonStrings.clear();
    buffer = String();
}

ICACHE_FLASH_ATTR SentBuffer::SentBuffer() {};

size_t ICACHE_FLASH_ATTR SentBuffer::requestLength(size_t buffer_length) {
    if (jsonStrings.empty())
        return 0;
    else
        return min(buffer_length - 1, jsonStrings.begin()->length() + 1);
}

void ICACHE_FLASH_ATTR SentBuffer::push(String &message, bool priority) {
    if (priority) 
        jsonStrings.push_front(message);
    else
        jsonStrings.push_back(message);
}

void ICACHE_FLASH_ATTR SentBuffer::read(size_t length, temp_buffer_t &buf) {
    jsonStrings.front().toCharArray(buf.buffer, length + 1);
    last_read_size = length;
}

void ICACHE_FLASH_ATTR SentBuffer::freeRead() {
    staticThis->debugMsg(COMMUNICATION, "SentBuffer::freeRead(): %u\n", last_read_size);
    if (last_read_size == jsonStrings.begin()->length() + 1)
        jsonStrings.pop_front();
    else
        jsonStrings.begin()->remove(0, last_read_size);
    last_read_size = 0;
}

bool ICACHE_FLASH_ATTR SentBuffer::empty() {
    return jsonStrings.empty();
}

void ICACHE_FLASH_ATTR SentBuffer::clear() {
    jsonStrings.clear();
}


void meshRecvCb(void * arg, TCPClient *, void * data, size_t len);
void tcpSentCb(void * arg, TCPClient * tpcb, size_t len, uint32_t time);

ICACHE_FLASH_ATTR MeshConnection::MeshConnection(TCPClient *client_ptr, painlessMesh *pMesh, bool is_station) {
    station = is_station;
    mesh = pMesh;
    client = client_ptr;

    client->setNoDelay(true);
    client->setRxTimeout(NODE_TIMEOUT/TASK_SECOND);

    //tcp_arg(pcb, static_cast<void*>(this));
    auto arg = static_cast<void*>(this);
    client->onData(meshRecvCb, arg); 

    client->onAck(tcpSentCb, arg); 

    if (station) { // we are the station, start nodeSync
        staticThis->debugMsg(CONNECTION, "meshConnectedCb(): we are STA\n");
    } else {
        staticThis->debugMsg(CONNECTION, "meshConnectedCb(): we are AP\n");
    }

    client->onError([](void * arg, TCPClient *client, int8_t err) {
        staticThis->debugMsg(CONNECTION, "tcp_err(): MeshConnection %s\n", client->errorToString(err));
    }, arg);

    client->onDisconnect([](void *arg, TCPClient *client) {
        if (arg == NULL) {
            staticThis->debugMsg(CONNECTION, "onDisconnect(): MeshConnection NULL\n");
            if (client->connected())
                client->close(true);
            return;
        }
        auto conn = static_cast<MeshConnection*>(arg);
        staticThis->debugMsg(CONNECTION, "onDisconnect():\n");
        staticThis->debugMsg(CONNECTION, "onDisconnect(): dropping %u now= %u\n", conn->nodeId, staticThis->getNodeTime());
        conn->close();
    }, arg);

    auto syncInterval = NODE_TIMEOUT/2;
    if (!station)
        syncInterval = NODE_TIMEOUT*2;

    this->nodeSyncTask.set(
            syncInterval, TASK_FOREVER, [this](){
        staticThis->debugMsg(SYNC, "nodeSyncTask(): \n");
        staticThis->debugMsg(SYNC, "nodeSyncTask(): request with %u\n", 
                this->nodeId);
        auto saveConn = staticThis->findConnection(client);
        String subs = staticThis->subConnectionJson(saveConn);
        staticThis->sendMessage(saveConn, this->nodeId, 
                staticThis->_nodeId, NODE_SYNC_REQUEST, subs, true);
    });
    staticThis->scheduler.addTask(this->nodeSyncTask);
    if (station)
        this->nodeSyncTask.enable();
    else
        this->nodeSyncTask.enableDelayed();

    receiveBuffer = ReceiveBuffer();
    readBufferTask.set(100*TASK_MILLISECOND, TASK_FOREVER, [this]() {
        if (!this->receiveBuffer.empty()) {
            String frnt = this->receiveBuffer.front();
            this->handleMessage(frnt, staticThis->getNodeTime());
            this->receiveBuffer.pop_front();
            if (!this->receiveBuffer.empty())
                this->readBufferTask.forceNextIteration();
        }
    });
    staticThis->scheduler.addTask(readBufferTask);
    readBufferTask.enableDelayed();
    
    staticThis->debugMsg(GENERAL, "MeshConnection(): leaving\n");
}

ICACHE_FLASH_ATTR MeshConnection::~MeshConnection() {
    staticThis->debugMsg(CONNECTION, "~MeshConnection():\n");
    this->close();
    if (!client->freeable()) {
        mesh->debugMsg(CONNECTION, "~MeshConnection(): Closing pcb\n");
        client->close(true);
    }
    client->abort();
    delete client;
    /*if (esp_conn)
        espconn_disconnect(esp_conn);*/
}

void ICACHE_FLASH_ATTR MeshConnection::close() {
    if (!connected)
        return;
    
    staticThis->debugMsg(CONNECTION, "MeshConnection::close().\n");
    this->connected = false;

    this->timeSyncTask.setCallback(NULL);
    this->nodeSyncTask.setCallback(NULL);
    this->readBufferTask.setCallback(NULL);

    auto nodeId = this->nodeId;

    mesh->droppedConnectionTask.set(TASK_SECOND, TASK_ONCE, [nodeId]() {
        staticThis->debugMsg(CONNECTION, "closingTask():\n");
        staticThis->debugMsg(CONNECTION, "closingTask(): dropping %u now= %u\n", nodeId, staticThis->getNodeTime());
       if (staticThis->changedConnectionsCallback)
            staticThis->changedConnectionsCallback(); // Connection dropped. Signal user
       if (staticThis->droppedConnectionCallback)
            staticThis->droppedConnectionCallback(nodeId); // Connection dropped. Signal user
       for (auto &&connection : staticThis->_connections) {
           if (connection->nodeId != nodeId) { // Exclude current
               connection->nodeSyncTask.forceNextIteration();
           }
       }
       staticThis->stability /= 2;
    });
    mesh->scheduler.addTask(staticThis->droppedConnectionTask);
    mesh->droppedConnectionTask.enable();

    if (client->connected()) {
        mesh->debugMsg(CONNECTION, "close(): Closing pcb\n");
        client->close();
    }

    if (station) {
        staticThis->debugMsg(CONNECTION, "close(): call esp_wifi_disconnect().\n");
        esp_wifi_disconnect();
    }

    receiveBuffer.clear();
    sentBuffer.clear();

    mesh->eraseClosedConnections();

    if (station && mesh->_station_got_ip)
        mesh->_station_got_ip = false;

    this->nodeId = 0;
}


bool ICACHE_FLASH_ATTR MeshConnection::addMessage(String &message, bool priority) {
    /*
    mesh->debugMsg(DEBUG, "No connections: %u, sentMessages: %u, receiveMessages: %u, station: %u, canSend: %u, nodeId: %u\n",
            mesh->_connections.size(), sentBuffer.jsonStrings.size(), receiveBuffer.jsonStrings.size(), station, client->canSend(), nodeId);
    if (receiveBuffer.jsonStrings.size() > 3)
        mesh->debugMsg(DEBUG, "Msg %s\n", message.c_str());
    */
    if (ESP.getFreeHeap() - message.length() >= MIN_FREE_MEMORY) { // If memory heap is enough, queue the message
        if (priority) {
            sentBuffer.push(message, priority);
            mesh->debugMsg(COMMUNICATION, "addMessage(): Package sent to queue beginning -> %d , FreeMem: %d\n", sentBuffer.jsonStrings.size(), ESP.getFreeHeap());
        } else {
            if (sentBuffer.jsonStrings.size() < MAX_MESSAGE_QUEUE) {
                sentBuffer.push(message, priority);
                staticThis->debugMsg(COMMUNICATION, "addMessage(): Package sent to queue end -> %d , FreeMem: %d\n", sentBuffer.jsonStrings.size(), ESP.getFreeHeap());
            } else {
                staticThis->debugMsg(ERROR, "addMessage(): Message queue full -> %d , FreeMem: %d\n", sentBuffer.jsonStrings.size(), ESP.getFreeHeap());
                if (client->canSend())
                    writeNext();
                return false;
            }
        }
        if (client->canSend())
            writeNext();
        return true;
    } else {
        //connection->sendQueue.clear(); // Discard all messages if free memory is low
        staticThis->debugMsg(DEBUG, "addMessage(): Memory low, message was discarded\n");
        if (client->canSend())
            writeNext();
        return false;
    }
}

bool ICACHE_FLASH_ATTR MeshConnection::writeNext() {
    if (sentBuffer.empty()) {
        staticThis->debugMsg(COMMUNICATION, "writeNext(): sendQueue is empty\n");
        return false;
    }
    auto len = sentBuffer.requestLength(shared_buffer.length);
    auto snd_len = client->space();
    if (len > snd_len)
        len = snd_len;
    if (len > 0) {
        sentBuffer.read(len, shared_buffer);
        auto written = client->write(shared_buffer.buffer, len, 1);
        if (written == len) {
            staticThis->debugMsg(COMMUNICATION, "writeNext(): Package sent = %s\n", shared_buffer.buffer);
            client->send(); // TODO only do this for priority messages
            sentBuffer.freeRead();
            writeNext();
            return true;
        } else if (written == 0) {
            staticThis->debugMsg(COMMUNICATION, "writeNext(): tcp_write Failed node=%u. Resending later\n", nodeId);
            return false;
        } else {
            staticThis->debugMsg(ERROR, "writeNext(): Less written than requested. Please report bug on the issue tracker\n");
            return false;
        }
    } else {
        staticThis->debugMsg(COMMUNICATION, "writeNext(): tcp_sndbuf not enough space\n");
        return false;
    }

}

// connection managment functions
//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::onReceive(receivedCallback_t  cb) {
    debugMsg(GENERAL, "onReceive():\n");
    receivedCallback = cb;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::onNewConnection(newConnectionCallback_t cb) {
    debugMsg(GENERAL, "onNewConnection():\n");
    newConnectionCallback = cb;
}

void ICACHE_FLASH_ATTR painlessMesh::onDroppedConnection(droppedConnectionCallback_t cb) {
    debugMsg(GENERAL, "onDroppedConnection():\n");
    droppedConnectionCallback = cb;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::onChangedConnections(changedConnectionsCallback_t cb) {
    debugMsg(GENERAL, "onChangedConnections():\n");
    changedConnectionsCallback = cb;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::onNodeTimeAdjusted(nodeTimeAdjustedCallback_t cb) {
    debugMsg(GENERAL, "onNodeTimeAdjusted():\n");
    nodeTimeAdjustedCallback = cb;
}

//***********************************************************************
void ICACHE_FLASH_ATTR painlessMesh::onNodeDelayReceived(nodeDelayCallback_t cb) {
    debugMsg(GENERAL, "onNodeDelayReceived():\n");
    nodeDelayReceivedCallback = cb;
}

void ICACHE_FLASH_ATTR painlessMesh::eraseClosedConnections() {
    auto connection = _connections.begin();
    while (connection != _connections.end()) {
        if (!(*connection)->connected) {
            connection = _connections.erase(connection);
            debugMsg(CONNECTION, "eraseClosedConnections():\n");
        } else {
            ++connection;
        }
    }
}

bool ICACHE_FLASH_ATTR painlessMesh::closeConnectionSTA()
{
    auto connection = _connections.begin();
    while (connection != _connections.end()) {
        if ((*connection)->station) {
            // We found the STA connection, close it
            (*connection)->close();
            return true;
        }
        ++connection;
    }
    return false;
}

// Check whether a string contains a numeric substring as a complete number
//
// "a:800" does contain "800", but does not contain "80"
bool ICACHE_FLASH_ATTR  stringContainsNumber(const String &subConnections,
                                             const String & nodeIdStr, int from = 0) {
    auto index = subConnections.indexOf(nodeIdStr, from);
    if (index == -1)
        return false;
    // Check that the preceding and following characters are not a number
    else if (index > 0 &&
             index + nodeIdStr.length() + 1 < subConnections.length() &&
             // Preceding character is not a number
             (subConnections.charAt(index - 1) < '0' ||
             subConnections.charAt(index - 1) > '9') &&
             // Following character is not a number
             (subConnections.charAt(index + nodeIdStr.length() + 1) < '0' ||
             subConnections.charAt(index + nodeIdStr.length() + 1) > '9')
             ) {
        return true;
    } else { // Check whether the nodeid occurs further in the subConnections string
        return stringContainsNumber(subConnections, nodeIdStr,
                                    index + nodeIdStr.length());
    }
    return false;
}

//***********************************************************************
// Search for a connection to a given nodeID
std::shared_ptr<MeshConnection> ICACHE_FLASH_ATTR painlessMesh::findConnection(uint32_t nodeId, uint32_t exclude) {
    debugMsg(GENERAL, "In findConnection(nodeId)\n");

    for (auto &&connection : _connections) {
        if (connection->nodeId == exclude) {
            debugMsg(GENERAL, "findConnection(%u): Skipping excluded connection\n", nodeId);
            continue;
        }

        if (connection->nodeId == nodeId) {  // check direct connections
            debugMsg(GENERAL, "findConnection(%u): Found Direct Connection\n", nodeId);
            return connection;
        }

        if (stringContainsNumber(connection->subConnections,
            String(nodeId))) { // check sub-connections
            debugMsg(GENERAL, "findConnection(%u): Found Sub Connection through %u\n", nodeId, connection->nodeId);
            return connection;
        }
    }
    debugMsg(CONNECTION, "findConnection(%u): did not find connection\n", nodeId);
    return NULL;
}

//***********************************************************************
std::shared_ptr<MeshConnection>  ICACHE_FLASH_ATTR painlessMesh::findConnection(TCPClient *client) {
    debugMsg(GENERAL, "In findConnection(esp_conn) conn=0x%x\n", client);

    for (auto &&connection : _connections) {
        if ((*connection->client) == (*client)) {
            return connection;
        }
    }

    debugMsg(CONNECTION, "findConnection(espconn): Did not Find\n");
    return NULL;
}

//***********************************************************************
String ICACHE_FLASH_ATTR painlessMesh::subConnectionJson(std::shared_ptr<MeshConnection> exclude) {
    if (exclude == NULL)
        return subConnectionJsonHelper(_connections);
    else
        return subConnectionJsonHelper(_connections, exclude->nodeId);
}

//***********************************************************************
String ICACHE_FLASH_ATTR painlessMesh::subConnectionJsonHelper(
        ConnectionList &connections,
        uint32_t exclude) {
    if (exclude != 0)
        debugMsg(GENERAL, "subConnectionJson(), exclude=%u\n", exclude);

    String ret = "[";
    for (auto &&sub : connections) {
        if (!sub->connected) {
            debugMsg(ERROR, "subConnectionJsonHelper(): Found closed connection %u\n", 
                    sub->nodeId);
        } else if (sub->nodeId != exclude && sub->nodeId != 0) {  //exclude connection that we are working with & anything too new.
            if (ret.length() > 1)
                ret += String(",");
            ret += String("{\"nodeId\":") + String(sub->nodeId) +
                String(",\"subs\":") + sub->subConnections + String("}");
        }
    }
    ret += String("]");

    debugMsg(GENERAL, "subConnectionJson(): ret=%s\n", ret.c_str());
    return ret;
}

// Calculating the actual number of connected nodes is fairly expensive,
// this calculates a cheap approximation
size_t ICACHE_FLASH_ATTR painlessMesh::approxNoNodes() {
    debugMsg(GENERAL, "approxNoNodes()\n");
    auto sc = subConnectionJson();
    return approxNoNodes(sc);
}

size_t ICACHE_FLASH_ATTR painlessMesh::approxNoNodes(String &subConns) {
    return max((long int) 1,round(subConns.length()/30.0));
}

//***********************************************************************
std::list<uint32_t> ICACHE_FLASH_ATTR painlessMesh::getNodeList() {
    std::list<uint32_t> nodeList;

    String nodeJson = subConnectionJson();

    int index = 0;

    while ((uint) index < nodeJson.length()) {
        uint comma = 0;
        index = nodeJson.indexOf("\"nodeId\":");
        if (index == -1)
            break;
        comma = nodeJson.indexOf(',', index);
        String temp = nodeJson.substring(index + 9, comma);
        uint32_t id = strtoul(temp.c_str(), NULL, 10);
        nodeList.push_back(id);
        index = comma + 1;
        nodeJson = nodeJson.substring(index);
    }

    return nodeList;

}

//***********************************************************************
void ICACHE_FLASH_ATTR tcpSentCb(void * arg, TCPClient * client, size_t len, uint32_t time) {
    if (arg == NULL) {
        staticThis->debugMsg(COMMUNICATION, "tcpSentCb(): no valid connection found\n");
    }
    auto conn = static_cast<MeshConnection*>(arg);
    conn->writeNext();
}

void ICACHE_FLASH_ATTR meshRecvCb(void * arg, TCPClient *client, void * data, size_t len) {
    if (arg == NULL) {
        staticThis->debugMsg(COMMUNICATION, "meshRecvCb(): no valid connection found\n");
    }
    auto receiveConn = static_cast<MeshConnection*>(arg);

    staticThis->debugMsg(COMMUNICATION, "meshRecvCb(): fromId=%u\n", receiveConn ? receiveConn->nodeId : 0);

    receiveConn->receiveBuffer.push(static_cast<const char*>(data), len, shared_buffer);

    // Signal that we are done
    client->ack(len); // ackLater?
    //client->ackLater();
    //tcp_recved(receiveConn->pcb, total_length);

    receiveConn->readBufferTask.forceNextIteration(); 
}

void ICACHE_FLASH_ATTR MeshConnection::handleMessage(String &buffer, uint32_t receivedAt) {
    staticThis->debugMsg(COMMUNICATION, "meshRecvCb(): Recvd from %u-->%s<--\n", this->nodeId, buffer.c_str());

    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(buffer.c_str(), 100);
    if (!root.success()) {   // Test if parsing succeeded.
        staticThis->debugMsg(ERROR, "meshRecvCb(): parseObject() failed. total_length=%d, data=%s<--\n", buffer.length(), buffer.c_str());
        return;
    }

    String msg = root["msg"];
    meshPackageType t_message = (meshPackageType)(int)root["type"];

    staticThis->debugMsg(COMMUNICATION, "meshRecvCb(): lastRecieved=%u fromId=%u type=%d\n", staticThis->getNodeTime(), this->nodeId, t_message);

    auto rConn = staticThis->findConnection(this->client);
    switch (t_message) {
    case NODE_SYNC_REQUEST:
    case NODE_SYNC_REPLY:
        // TODO: These findConnections are not the most efficient way of doing it.
        staticThis->handleNodeSync(rConn, root);
        break;

    case TIME_SYNC:
        staticThis->handleTimeSync(rConn, root, receivedAt);
        break;

    case SINGLE:
    case TIME_DELAY:
        if ((uint32_t)root["dest"] == staticThis->getNodeId()) {  // msg for us!
            if (t_message == TIME_DELAY) {
                staticThis->handleTimeDelay(rConn, root, receivedAt);
            } else {
                if (staticThis->receivedCallback)
                    staticThis->receivedCallback((uint32_t)root["from"], msg);
            }
        } else {                                                    // pass it along
            String tempStr;
            root.printTo(tempStr);
            auto conn = staticThis->findConnection((uint32_t)root["dest"], this->nodeId);
            if (conn) {
                conn->addMessage(tempStr);
                staticThis->debugMsg(COMMUNICATION, "meshRecvCb(): Message %s to %u forwarded through %u\n", tempStr.c_str(), (uint32_t)root["dest"], conn->nodeId);
            }
        }
        break;

    case BROADCAST:
        staticThis->broadcastMessage((uint32_t)root["from"], BROADCAST, msg, rConn);
        if (staticThis->receivedCallback)
            staticThis->receivedCallback((uint32_t)root["from"], msg);
        break;

    default:
        staticThis->debugMsg(ERROR, "meshRecvCb(): unexpected json, root[\"type\"]=%d", (int)root["type"]);
    }
    return;
}

//***********************************************************************
// Wifi event handler
int ICACHE_FLASH_ATTR painlessMesh::espWifiEventCb(void * ctx, system_event_t *event) {
    switch (event->event_id) {
    case SYSTEM_EVENT_SCAN_DONE:
        staticThis->debugMsg(CONNECTION, "espWifiEventCb(): SYSTEM_EVENT_SCAN_DONE\n");
        // Call the same thing original callback called
        staticThis->stationScan.scanComplete();
        break;
    case SYSTEM_EVENT_STA_START:
        staticThis->stationScan.task.forceNextIteration();
        staticThis->debugMsg(CONNECTION, "espWifiEventCb(): SYSTEM_EVENT_STA_START\n");
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        staticThis->debugMsg(CONNECTION, "espWifiEventCb(): SYSTEM_EVENT_STA_CONNECTED\n");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        staticThis->_station_got_ip = false;
        staticThis->debugMsg(CONNECTION, "espWifiEventCb(): SYSTEM_EVENT_STA_DISCONNECTED\n");
        //staticThis->closeConnectionSTA();
#ifdef ESP8266
        esp_wifi_disconnect(); // Make sure we are disconnected
#endif
        staticThis->stationScan.connectToAP(); // Search for APs and connect to the best one
        break;
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
        staticThis->debugMsg(CONNECTION, "espWifiEventCb(): SYSTEM_EVENT_STA_AUTHMODE_CHANGE\n");
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        staticThis->_station_got_ip = true;
        staticThis->debugMsg(CONNECTION, "espWifiEventCb(): SYSTEM_EVENT_STA_GOT_IP\n");
        staticThis->tcpConnect(); // Connect to TCP port
        break;

    case SYSTEM_EVENT_AP_STACONNECTED:
        staticThis->debugMsg(CONNECTION, "espWifiEventCb(): SYSTEM_EVENT_AP_STACONNECTED\n");
        break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:
        staticThis->debugMsg(CONNECTION, "espWifiEventCb(): SYSTEM_EVENT_AP_STADISCONNECTED\n");
        break;

    default:
        staticThis->debugMsg(ERROR, "Unhandled WiFi event: %d\n", event->event_id);
        break;
    }
    return ESP_OK;
}
