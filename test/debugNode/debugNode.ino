//************************************************************
// this is prints (and sends around) information used for debugging library
//
// 1. blinks led once for every node on the mesh
// 2. blink cycle repeats every BLINK_PERIOD and is in sync between nodes
//
//
//
//************************************************************

// Collect nodeInformation (on every change and 10 seconds)
// send it every 5 seconds
// Print on change/newConnection/receive from other node
#define UNITY
#include <painlessMesh.h>

// some gpio pin that is connected to an LED...
// on my rig, this is 5, change to the right number of your LED.
#define   LED             2       // GPIO number of connected LED, ON ESP-12 IS GPIO2

#define   BLINK_PERIOD    3000 // milliseconds until cycle repeat
#define   BLINK_DURATION  100  // milliseconds LED is on for

#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

#define   ISROOT          false

// Prototypes
void receivedCallback(uint32_t from, String & msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback(); 
//void nodeTimeAdjustedCallback(int32_t offset); 
//void delayReceivedCallback(uint32_t from, int32_t delay);

Scheduler     userScheduler; // to control your personal task
painlessMesh  mesh;

String state;
void collectData() {
    state = "";

#if ARDUINOJSON_VERSION_MAJOR==6
    DynamicJsonDocument jsonBuffer;
    JsonObject stateObj = jsonBuffer.to<JsonObject>();
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& stateObj = jsonBuffer.createObject();
#endif
    stateObj["nodeId"] = mesh.getNodeId();
#ifdef ESP32
    stateObj["hardware"] = "ESP32";
#else
    stateObj["hardware"] = "ESP8266";
#endif

    stateObj["isRoot"] = mesh.isRoot();
    stateObj["isRooted"] = mesh.isRooted();

    String subs = mesh.subConnectionJson();
#if ARDUINOJSON_VERSION_MAJOR==6
    DynamicJsonDocument subsBuffer;
    subsBuffer.nestingLimit = 255;
    DeserializationError error = deserializeJson(subsBuffer, subs);
    if (error) {
        return;
    }
    JsonArray subsArr = subsBuffer.as<JsonArray>();
#else
    DynamicJsonBuffer subsBuffer;
    JsonArray& subsArr = subsBuffer.parseArray(subs, 255);
    if (!subsArr.success())
        return;
#endif
    stateObj["subs"] = subsArr;
    stateObj["subsOrig"] = subs;
    stateObj["csize"] = mesh._connections.size();
#if ARDUINOJSON_VERSION_MAJOR==6
#else
    JsonArray& connections = stateObj.createNestedArray("connections");
    for(auto && conn : mesh._connections) {
        JsonObject& connection = connections.createNestedObject();
        connection["nodeId"] = conn->nodeId;
        connection["connected"] = conn->connected;
        connection["station"] = conn->station;
        connection["root"] = conn->root;
        connection["rooted"] = conn->rooted;
        connection["subs"] = conn->subConnections;
    }
#endif
#if ARDUINOJSON_VERSION_MAJOR==6
    serializeJsonPretty(stateObj, state);
#else
    stateObj.prettyPrintTo(state);
#endif
}

Task taskGatherState( TASK_SECOND * 30, TASK_FOREVER, &collectData); // start with a one second interval
Task taskPrintState(TASK_SECOND * 5, TASK_FOREVER, []() {
    Serial.println("Node state:");
    Serial.printf("%s\n", state.c_str());
    //stateObj.prettyPrintTo(Serial);
});

Task taskSendState(TASK_SECOND * 15, TASK_FOREVER, []() {
    //String str;
    //stateObj.prettyPrintTo(str);
    //mesh.sendBroadcast(str);
    mesh.sendBroadcast(state);
});

// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;

void setup() {
  Serial.begin(115200);

  pinMode(LED, OUTPUT);

  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  //mesh.setDebugMsgTypes(ERROR | DEBUG | CONNECTION | COMMUNICATION);  // set before init() so that you can see startup messages
  mesh.setDebugMsgTypes(ERROR);  // set before init() so that you can see startup messages

  mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);
  mesh.setRoot(ISROOT);
  mesh.setContainsRoot(true);

  userScheduler.addTask( taskGatherState );
  taskGatherState.enable();
  userScheduler.addTask( taskPrintState );
  taskPrintState.enable();
  userScheduler.addTask( taskSendState );
  taskSendState.enable();

  blinkNoNodes.set(BLINK_PERIOD, (mesh.getNodeList().size() + 1) * 2, []() {
      // If on, switch off, else switch on
      if (onFlag)
        onFlag = false;
      else
        onFlag = true;
      blinkNoNodes.delay(BLINK_DURATION);

      if (blinkNoNodes.isLastIteration()) {
        // Finished blinking. Reset task for next run 
        // blink number of nodes (including this node) times
        blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
        // Calculate delay based on current mesh time and BLINK_PERIOD
        // This results in blinks between nodes being synced
        blinkNoNodes.enableDelayed(BLINK_PERIOD - 
            (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
      }
  });
  userScheduler.addTask(blinkNoNodes);
  blinkNoNodes.enable();

  randomSeed(analogRead(A0));
}

void loop() {
  userScheduler.execute(); // it will run mesh scheduler as well
  mesh.update();
  digitalWrite(LED, !onFlag);
}

void receivedCallback(uint32_t from, String & msg) {
  Serial.printf("startHere: Received from %u msg = %s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
 
  Serial.printf("New Connection, nodeId = %u\n", nodeId);
  taskGatherState.forceNextIteration();
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
  taskGatherState.forceNextIteration();
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void delayReceivedCallback(uint32_t from, int32_t delay) {
  Serial.printf("Delay to node %u is %d us\n", from, delay);
}
