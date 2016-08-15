#include <Arduino.h>
#include <ArduinoJson.h>
#include <easyMesh.h>
#include <easyWebServer.h>
#include <easyWebSocket.h>

#include "animations.h"

easyMesh  mesh;  // mesh global

// animation globals
extern NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> strip;
extern NeoPixelAnimator animations; // NeoPixel animation management object
extern AnimationController controllers[]; 

os_timer_t  yerpTimer;

void setup() {
  Serial.begin( 115200 );

  mesh.init();
  mesh.setReceiveCallback( &receivedCallback );
  mesh.setNewConnectionCallback( &newConnectionCallback );

  webServerInit();

  webSocketInit();
  webSocketSetReceiveCallback( &wsReceiveCallback );
  webSocketSetConnectionCallback( &wsConnectionCallback );

  meshPrintDebug("\nIn setup() my chipId=%d\n", mesh.getChipId());

  strip.Begin();
  strip.Show();

  animationsInit();

  os_timer_setfn( &yerpTimer, yerpCb, NULL );
  os_timer_arm( &yerpTimer, 1000, 1 );
}

void loop() {
  mesh.update();

  static uint16_t previousConnections;
  uint16_t numConnections = mesh.connectionCount();
  if( countWsConnections() > 0 )
    numConnections++;

  if ( previousConnections != numConnections ) {
    meshPrintDebug("loop(): numConnections=%d\n", numConnections);

    if ( numConnections == 0 ) {
      controllers[smoothIdx].nextAnimation = searchingIdx;
      controllers[searchingIdx].nextAnimation = searchingIdx;
      controllers[searchingIdx].hue[0] = 0.0f;
    } else {
      controllers[searchingIdx].nextAnimation = smoothIdx;
      controllers[smoothIdx].nextAnimation = smoothIdx;
    }

    sendWsControl();

    previousConnections = numConnections;
  }

  animations.UpdateAnimations();
  strip.Show();
}

void yerpCb( void *arg ) {
  static int yerpCount;
  int connCount = 0;

  String msg = "Yerp=";
  msg += yerpCount++;

  meshPrintDebug("msg-->%s<-- stationStatus=%u numConnections=%u\n", msg.c_str(), wifi_station_get_connect_status(), mesh.connectionCount( NULL ) );

  SimpleList<meshConnectionType>::iterator connection = mesh._connections.begin();
  while ( connection != mesh._connections.end() ) {
    meshPrintDebug("\tconn#%d, chipId=%d subs=%s\n", connCount++, connection->chipId, connection->subConnections.c_str() );
    connection++;
  }

  // send ping to webSockets
  String ping("ping");
  broadcastWsMessage(ping.c_str(), ping.length(), OPCODE_TEXT);
  //sendWsControl();
}

void newConnectionCallback( bool adopt ) {
  if ( adopt == false ) {
    String control = buildControl();
    mesh.sendBroadcast( control );
  }
}

void receivedCallback( uint32_t from, String &msg ) {
  meshPrintDebug("receivedCallback():\n");

  DynamicJsonBuffer jsonBuffer(50);
  JsonObject& control = jsonBuffer.parseObject( msg );

  broadcastWsMessage(msg.c_str(), msg.length(), OPCODE_TEXT);

  meshPrintDebug("control=%s\n", msg.c_str());

  for ( int i = 0; i < ( mesh.connectionCount( NULL ) + 1 ); i++) {
    float hue = control[String(i)];
    controllers[smoothIdx].hue[i] = hue;
  }
}

void wsConnectionCallback( void ) {
  meshPrintDebug("wsConnectionCallback():\n");
}

void wsReceiveCallback( char *payloadData ) {
  meshPrintDebug("wsReceiveCallback(): payloadData=%s\n", payloadData );

  String msg( payloadData );
  mesh.sendBroadcast( msg );

  if ( strcmp( payloadData, "wsOpened") == 0) {  // hack to give the browser time to get the ws up and running
    meshPrintDebug("wsReceiveCallback(): received wsOpened\n" );
    sendWsControl();
    return;
  }

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& control = jsonBuffer.parseObject(payloadData);

  if (!control.success()) {   // Test if parsing succeeded.
    meshPrintDebug("wsReceiveCallback(): parseObject() failed. payload=%s<--\n", payloadData);
    return;
  }
  
  uint16_t blips = mesh.connectionCount() + 1;
  if ( blips > MAX_BLIPS )
    blips = MAX_BLIPS;
    
  for ( int i = 0; i < blips; i++) {
    String temp(i);
    float hue = control[temp];
    controllers[smoothIdx].hue[i] = hue;
  }
}

void sendWsControl( void ) {
  meshPrintDebug("sendWsControl():\n");
  
  String control = buildControl();
  broadcastWsMessage(control.c_str(), control.length(), OPCODE_TEXT);
}

String buildControl ( void ) {
  uint16_t blips = mesh.connectionCount() + 1;
  meshPrintDebug("buildControl(): blips=%d\n", blips);

  if ( blips > 3 ) {
    meshPrintDebug(" blips out of range =%d\n", blips);
    blips = 3;
  }

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& control = jsonBuffer.createObject();
  for (int i = 0; i < blips; i++ ) {
    control[String(i)] = String(controllers[smoothIdx].hue[i]);
  }

  String ret;
  control.printTo(ret);
  return ret;
}


