//************************************************************
// this is a simple example that uses the easyMesh library
//
// 1. blinks led once for every node on the mesh
// 2. blink cycle repeats every BLINK_PERIOD
// 3. sends a silly message to every node on the mesh at a random time betweew 1 and 5 seconds
// 4. prints anything it recieves to Serial.print
// 
//
//************************************************************
#include <painlessMesh.h>

// some gpio pin that is connected to an LED... 
// on my rig, this is 5, change to the right number of your LED.
#define   LED             2       // GPIO number of connected LED, ON ESP-12 IS GPIO2

#define   BLINK_PERIOD    3000000 // microseconds until cycle repeat
#define   BLINK_DURATION  100000  // microseconds LED is on for

#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

painlessMesh  mesh;
bool calc_delay = false;
SimpleList<uint32_t> nodes;
uint32_t sendMessageTime = 0;

void setup() {
    Serial.begin(115200);

    pinMode(LED, OUTPUT);

    //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
    mesh.setDebugMsgTypes(ERROR | DEBUG);  // set before init() so that you can see startup messages

    mesh.init(MESH_SSID, MESH_PASSWORD, MESH_PORT);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
    mesh.onNodeDelayReceived(&delayReceivedCallback);

    randomSeed(analogRead(A0));
}

void loop() {
    bool error;

    mesh.update();

    // run the blinky
    bool  onFlag = true;
    uint32_t cycleTime = mesh.getNodeTime() % BLINK_PERIOD;
    for (uint8_t i = 0; i < (mesh.getNodeList().size() + 1); i++) {
        uint32_t onTime = BLINK_DURATION * i * 2;

        if (cycleTime > onTime && cycleTime < onTime + BLINK_DURATION)
            onFlag = false;
    }
    digitalWrite(LED, onFlag);

    // get next random time for send message
    if (sendMessageTime == 0) {
        sendMessageTime = mesh.getNodeTime() + random(1000000, 5000000);
    }

    // if the time is ripe, send everyone a message!
    if (sendMessageTime != 0 && 
            (int) sendMessageTime - (int) mesh.getNodeTime() < 0) { // Cast to int in case of time rollover
        String msg = "Hello from node ";
        msg += mesh.getNodeId();
        error = mesh.sendBroadcast(msg + " myFreeMemory: " + String(ESP.getFreeHeap()));
        sendMessageTime = 0;

        if (calc_delay) {
            SimpleList<uint32_t>::iterator node = nodes.begin();
            while (node != nodes.end()) {
                mesh.startDelayMeas(*node);
                node++;
            }
            calc_delay = false;
        }
    }
}

void receivedCallback(uint32_t from, String &msg) {
    Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
    Serial.printf("Changed connections %s\n", mesh.subConnectionJson().c_str());

    nodes = mesh.getNodeList();

    Serial.printf("Num nodes: %d\n", nodes.size());
    Serial.printf("Connection list:");

    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end()) {
        Serial.printf(" %u", *node);
        node++;
    }
    Serial.println();
    calc_delay = true;
}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void delayReceivedCallback(uint32_t from, int32_t delay) {
    Serial.printf("Delay to node %u is %d us\n", from, delay);
}
