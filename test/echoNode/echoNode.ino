//************************************************************
// this is a simple example that uses the painlessMesh library and echos any
// messages it receives
//
//************************************************************
#include "painlessMesh.h"

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

painlessMesh  mesh;

size_t logServerId = 0;

// Send message to the logServer every 10 seconds 
Task myLoggingTask(10000, TASK_FOREVER, []() {
    if (logServerId != 0) { 
#if ARDUINOJSON_VERSION_MAJOR==6
        DynamicJsonDocument jsonBuffer;
        JsonObject msg = jsonBuffer.to<JsonObject>();
#else
        DynamicJsonBuffer jsonBuffer;
        JsonObject& msg = jsonBuffer.createObject();
#endif
        msg["topic"] = "nodeStatus";
        msg["nodeId"] = mesh.getNodeId();
        msg["subs"] = mesh.subConnectionJson();
        msg["logNode"] = logServerId;
        msg["stability"] = mesh.stability;
        msg["time"] = mesh.getNodeTime();

        String str;
        msg.printTo(str);
        mesh.sendSingle(logServerId, str);

        // log to serial
        msg.printTo(Serial);
        Serial.printf("\n");
    }
});

void setup() {
  Serial.begin(115200);
    
  mesh.setDebugMsgTypes( ERROR | STARTUP | CONNECTION );  // set before init() so that you can see startup messages

  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT );
  mesh.onReceive(&receivedCallback);

  mesh.scheduler.addTask(myLoggingTask);
  myLoggingTask.enable();
}

void loop() {
  mesh.update();
}

void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("echoNode: Received from %u msg=%s\n", from, msg.c_str());
  mesh.sendSingle(from, msg);
#if ARDUINOJSON_VERSION_MAJOR==6
  DynamicJsonDocument jsonBuffer;
  DeserializationError error = deserializeJson(jsonBuffer, msg);
  if (error) {
      return;
  }
  JsonObject root = jsonBuffer.as<JsonObject>();
#else
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(msg);
#endif
  if (root.containsKey("topic")) {
      if (String("logServer").equals(root["topic"].as<String>())) {
          // check for on: true or false
          logServerId = root["nodeId"];
          Serial.printf("logServer detected!!!\n");
      }
      Serial.printf("Handled from %u msg=%s\n", from, msg.c_str());
  }
}
