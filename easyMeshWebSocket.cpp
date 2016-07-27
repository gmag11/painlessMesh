// based on https://github.com/dangrie158/ESP-8266-WebSocket

#include <Arduino.h>

extern "C" {
#include "user_interface.h"
#include "espconn.h"

#include "sha1.h"
#include "base64.h"
}
#include "easyMesh.h"
#include "easyMeshWebSocket.h"

espconn webSocketConn;
esp_tcp webSocketTcp;

static void (*wsOnConnectionCallback)(void);
static void (*wsOnMessageCallback)( char *payloadData );

static WSConnection wsConnections[WS_MAXCONN];

void webSocketInit( void ) {
    webSocketConn.type = ESPCONN_TCP;
    webSocketConn.state = ESPCONN_NONE;
    webSocketConn.proto.tcp = &webSocketTcp;
    webSocketConn.proto.tcp->local_port = WEB_SOCKET_PORT;
    espconn_regist_connectcb(&webSocketConn, webSocketConnectCb);
    sint8 ret = espconn_accept(&webSocketConn);
    if ( ret == 0 )
        DEBUG_MSG("webSocket server established on port %d\n", WEB_SOCKET_PORT );
    else
        DEBUG_MSG("webSocket server on port %d FAILED ret=%d\n", WEB_SOCKET_PORT, ret);
    
    return;
}

/*void webSocketSetReceiveCallback( WSOnMessage onMessage ) {
  wsOnMessageCallback = (WSOnMessage)onMessage;
}

void webSocketSetConnectionCallback( WSOnConnection onConnection ) {
  wsOnConnectionCallback = (WSOnConnection)onConnection;
}
*/
void webSocketSetReceiveCallback( void (*onMessage)( char *payloadData) ) {
    wsOnMessageCallback = onMessage;
}

void webSocketSetConnectionCallback( void (*onConnection)(void) ) {
    wsOnConnectionCallback = onConnection;
}


//***********************************************************************
void webSocketConnectCb(void *arg) {
  struct espconn *connection = (espconn *)arg;

  DEBUG_MSG("\n\nmeshWebSocket received connection !!!\n");

    // set time out for this connection in seconds
    espconn_regist_time( connection, 120, 1);
    
  //find an empty slot
  uint8_t slotId = 0;
  while (wsConnections[slotId].connection != NULL && wsConnections[slotId].status != STATUS_CLOSED && slotId < WS_MAXCONN) {
    slotId++;
  }

  DEBUG_MSG("websocketConnectCb slotId=%d\n", slotId);


  if (slotId >= WS_MAXCONN) {
    //no more free slots, close the connection
    DEBUG_MSG("No more free slots for WebSockets!\n");
    espconn_disconnect(connection);
    return;
  }

  //  DEBUG_MSG("websocketConnectCb2\n");

  WSConnection wsConnection;
  wsConnection.status = STATUS_UNINITIALISED;
  wsConnection.connection = connection;
  wsConnection.onMessage = wsOnMessageCallback;
  wsConnections[slotId] = wsConnection;

  //  DEBUG_MSG("websocketConnectCb3\n");

  espconn_regist_recvcb(connection, webSocketRecvCb);
  espconn_regist_sentcb(connection, webSocketSentCb);
  espconn_regist_reconcb(connection, webSocketReconCb);
  espconn_regist_disconcb(connection, webSocketDisconCb);

  //  DEBUG_MSG("leaving websocketConnectCb\n");
}

//***********************************************************************
void webSocketRecvCb(void *arg, char *data, unsigned short len) {
  espconn *esp_connection = (espconn*)arg;

  //received some data from webSocket connection
  //DEBUG_MSG("In webSocketRecvCb\n");
  //  DEBUG_MSG("webSocket recv--->%s<----\n", data);

  WSConnection *wsConnection = getWsConnection(esp_connection);
  if (wsConnection == NULL) {
    DEBUG_MSG("webSocket Heh?\n");
    return;
  }

  //get the first occurrence of the key identifier
  char *key = os_strstr(data, WS_KEY_IDENTIFIER);

  //  DEBUG_MSG("key-->%s<--\n", key );

  if (key != NULL) {
    // ------------------------ Handle the Handshake ------------------------
    //    DEBUG_MSG("In Handle the Handshake\n");
    //Skip the identifier (that contains the space already)
    key += os_strlen(WS_KEY_IDENTIFIER);

    //   DEBUG_MSG("keynow-->%s<--\n", key);

    //the key ends at the newline
    char *endSequence = os_strstr(key, HTML_HEADER_LINEEND);
    //    DEBUG_MSG("endSequency-->%s<--\n", endSequence);

    if (endSequence != NULL) {
      int keyLastChar = endSequence - key;
      //we can throw away all the other data, only the key is interesting
      key[keyLastChar] = '\0';
      //      DEBUG_MSG("keyTrimmed-->%s<--\n", key);

      char acceptKey[100];
      createWsAcceptKey(key, acceptKey, 100);

      //     DEBUG_MSG("acceptKey-->%s<--\n", acceptKey);

      //now construct our message and send it back to the client
      char responseMessage[strlen(WS_RESPONSE) + 100];
      os_sprintf(responseMessage, WS_RESPONSE, acceptKey);

      //      DEBUG_MSG("responseMessage-->%s<--\n", responseMessage);

      //send the response
      espconn_sent(esp_connection, (uint8_t *)responseMessage, strlen(responseMessage));
      wsConnection->status = STATUS_OPEN;

      //call the connection callback
      if (wsOnConnectionCallback != NULL) {
        //       DEBUG_MSG("Handle the Handshake 5\n");
        wsOnConnectionCallback();
      }
    }
  } else {
    // ------------------------ Handle a Frame ------------------------
    //    DEBUG_MSG("In Handle a Frame\n");

    WSFrame frame;
    parseWsFrame(data, &frame);

    if (frame.isMasked) {
      unmaskWsPayload(frame.payloadData, frame.payloadLength, frame.maskingKey);
    } else {
      //we are the server, and need to shut down the connection
      //if we receive an unmasked packet
      //      DEBUG_MSG("frame.isMasked=false closing connection\n");
      closeWsConnection(wsConnection);
      return;
    }

//    DEBUG_MSG("frame.payloadData-->%s<--\n", frame.payloadData);

    if (frame.opcode == OPCODE_PING) {
      //      DEBUG_MSG("frame.opcode=OPCODE_PING\n");
      sendWsMessage(wsConnection, frame.payloadData, frame.payloadLength, FLAG_FIN | OPCODE_PONG);
      return;
    }

    if (frame.opcode == OPCODE_CLOSE) {
      //gracefully shut down the connection
      //      DEBUG_MSG("frame.opcode=OPCODE_CLOSE, closeing connection\n");
      closeWsConnection(wsConnection);
      return;
    }

    if (wsConnection->onMessage != NULL) {
      wsConnection->onMessage(frame.payloadData);
    }
  }
  //  DEBUG_MSG("Leaving webSocketRecvCb\n");
}


//***********************************************************************
static void ICACHE_FLASH_ATTR unmaskWsPayload(char *maskedPayload, uint32_t payloadLength, uint32_t maskingKey) {
  //the algorith described in IEEE RFC 6455 Section 5.3
  //TODO: this should decode the payload 4-byte wise and do the remainder afterwards
  for (int i = 0; i < payloadLength; i++) {
    int j = i % 4;
    maskedPayload[i] = maskedPayload[i] ^ ((uint8_t *)&maskingKey)[j];
  }
}


//***********************************************************************
static void ICACHE_FLASH_ATTR parseWsFrame(char *data, WSFrame *frame) {
  frame->flags = (*data) & FLAGS_MASK;
  frame->opcode = (*data) & OPCODE_MASK;
  //next byte
  data += 1;
  frame->isMasked = (*data) & IS_MASKED;
  frame->payloadLength = (*data) & PAYLOAD_MASK;

  //next byte
  data += 1;

  if (frame->payloadLength == 126) {
    os_memcpy(&frame->payloadLength, data, sizeof(uint16_t));
    data += sizeof(uint16_t);
  } else if (frame->payloadLength == 127) {
    os_memcpy(&frame->payloadLength, data, sizeof(uint64_t));
    data += sizeof(uint64_t);
  }

  if (frame->isMasked) {
    os_memcpy(&frame->maskingKey, data, sizeof(uint32_t));
    data += sizeof(uint32_t);
  }

  frame->payloadData = data;
}


//***********************************************************************
WSConnection *ICACHE_FLASH_ATTR getWsConnection(struct espconn *connection) {
//  DEBUG_MSG("In getWsConnecition\n");
  for (int slotId = 0; slotId < WS_MAXCONN; slotId++) {
//    DEBUG_MSG("slotId=%d, ws.conn*=%x, espconn*=%x<--  ", slotId, wsConnections[slotId].connection, connection);

    //    DEBUG_MSG("ws.connIP=%d.%d.%d.%d espconnIP=%d.%d.%d.%d --- ", IP2STR( wsConnections[slotId].connection->proto.tcp->remote_ip), IP2STR( connection->proto.tcp->remote_ip) );
//    DEBUG_MSG("ws.connIP=%x espconnIP=%x\n", *(uint32_t*)wsConnections[slotId].connection->proto.tcp->remote_ip, *(uint32_t*)connection->proto.tcp->remote_ip ) ;

    //   if (wsConnections[slotId].connection == connection) {
    if (*(uint32_t*)wsConnections[slotId].connection->proto.tcp->remote_ip == *(uint32_t*)connection->proto.tcp->remote_ip ) {
 //     DEBUG_MSG("Leaving getWsConnecition slotID=%d\n", slotId);
      return wsConnections + slotId;
    }
  }

//  DEBUG_MSG("Leaving getWsConnecition w/ NULL\n");
  return NULL;
}

//***********************************************************************
static int ICACHE_FLASH_ATTR createWsAcceptKey(const char *key, char *buffer, int bufferSize) {
  sha1nfo s;

  char concatenatedBuffer[512];
  concatenatedBuffer[0] = '\0';
  //concatenate the key and the GUID
  os_strcat(concatenatedBuffer, key);
  os_strcat(concatenatedBuffer, WS_GUID);

  //build the sha1 hash
  sha1_init(&s);
  sha1_write(&s, concatenatedBuffer, strlen(concatenatedBuffer));
  uint8_t *hash = sha1_result(&s);

  return base64_encode(20, hash, bufferSize, buffer);
}


//***********************************************************************
void closeWsConnection(WSConnection * connection) {
  //  DEBUG_MSG("In closeWsConnection\n");

  char closeMessage[CLOSE_MESSAGE_LENGTH] = CLOSE_MESSAGE;
  espconn_sent(connection->connection, (uint8_t *)closeMessage, sizeof(closeMessage));
  connection->status = STATUS_CLOSED;
  return;
}

//***********************************************************************
void ICACHE_FLASH_ATTR broadcastWsMessage(const char *payload, uint32_t payloadLength, uint8_t options) {
  //  DEBUG_MSG("broadcaseWsMessage-->%s<-- payloadLength=%d\n", payload, payloadLength);
  for (int slotId = 0; slotId < WS_MAXCONN; slotId++) {
    WSConnection connection = wsConnections[slotId];
    if (connection.connection != NULL && connection.status == STATUS_OPEN) {
      sendWsMessage(&connection, payload, payloadLength, options);
    }
  }
}

//***********************************************************************
void ICACHE_FLASH_ATTR sendWsMessage(WSConnection *connection,
                                     const char *payload,
                                     uint32_t payloadLength,
                                     uint8_t options) {
  //  DEBUG_MSG("sendWsMessage-->%s<-- payloadLength=%d\n", payload,payloadLength);

  uint8_t payloadLengthField[9];
  uint8_t payloadLengthFieldLength = 0;

  if (payloadLength > ((1 << 16) - 1)) {
    payloadLengthField[0] = 127;
    os_memcpy(payloadLengthField + 1, &payloadLength, sizeof(uint32_t));
    payloadLengthFieldLength = sizeof(uint32_t) + 1;
  } else if (payloadLength > ((1 << 8) - 1)) {
    payloadLengthField[0] = 126;
    os_memcpy(payloadLengthField + 1, &payloadLength, sizeof(uint16_t));
    payloadLengthFieldLength = sizeof(uint16_t) + 1;
  } else {
    payloadLengthField[0] = payloadLength;
    payloadLengthFieldLength = 1;
  }

  uint64_t maximumPossibleMessageSize = 14 + payloadLength; //14 bytes is the biggest frame header size
  char message[maximumPossibleMessageSize];
  message[0] = FLAG_FIN | options;

  os_memcpy(message + 1, &payloadLengthField, payloadLengthFieldLength);
  os_memcpy(message + 1 + payloadLengthFieldLength, payload, strlen(payload));

  espconn_sent(connection->connection, (uint8_t *)&message, payloadLength + 1 + payloadLengthFieldLength);
}




//***********************************************************************
void webSocketSentCb(void *arg) {
  //data sent successfully
  //DEBUG_MSG("webSocket sent cb \r\n");
  struct espconn *requestconn = (espconn *)arg;
  //  espconn_disconnect( requestconn );
}

/***********************************************************************/
void webSocketDisconCb(void *arg) {
  espconn *esp_connection = (espconn*)arg;

  WSConnection *wsConn = getWsConnection( esp_connection);
  if ( wsConn != NULL ) {
    wsConn->status = STATUS_CLOSED;
    DEBUG_MSG("Leaving webSocket_server_discon_cb found\n");
    return;
  }

  DEBUG_MSG("Leaving webSocket_server_discon_cb  didn't find\n");
  return;
}

/***********************************************************************/
void webSocketReconCb(void *arg, sint8 err) {
  DEBUG_MSG("In webSocket_server_recon_cb err=%d\n", err );
}





