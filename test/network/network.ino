#include <arduinoUnity.h>

#include <painlessMesh.h>

#ifdef UNITY
/**
Here we test the network behaviour of a node. These tests rely on a echoNode to be present
*/

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555
bool endEchoTest = false;
bool sendingDone = false;
bool endTest = false;

painlessMesh  mesh;

SimpleList<String> expected;
SimpleList<String> received;
String lastNodeStatus;
uint32_t lastNSReceived;

size_t noCBs = 0;

String randomString(uint32_t length) {
    String str;
    for(auto i = 0; i < length; ++i) {
        char rnd = random(33, 90);
        str += String(rnd);
    }
    return str;
}

void addMessage(painlessMesh &m, String msg, uint32_t nodeId) {
    if (m.sendSingle(nodeId, msg))
        expected.push_back(msg);
}

void newConnectionCallback(uint32_t nodeId) {
    auto msg = String("Hello boy");
    mesh.sendSingle(nodeId, msg);
    expected.push_back(msg);

    // Test 10 short msgs
    for(auto i = 0; i < 10; ++i)
        addMessage(mesh, randomString(random(10,50)), nodeId);

    // Test 10 long msgs
    for(auto i = 0; i < 10; ++i)
        addMessage(mesh, randomString(random(1000,1300)), nodeId);


    //addMessage(mesh, randomString(1800), nodeId);
    //addMessage(mesh, randomString(3600), nodeId);

    sendingDone = true;
}

void receivedCallback(uint32_t from, String &msg) {
    ++noCBs;
    received.push_back(msg);
    if (sendingDone && expected.size() == received.size())
        endEchoTest = true;
}

void nodeStatusReceivedCallback(uint32_t from, String &msg) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(msg);
  if (root.containsKey("topic")) {
      if (String("nodeStatus").equals(root["topic"].as<String>())) {
        lastNodeStatus = msg;
        lastNSReceived = mesh.getNodeTime();
      }
  }
}

void test_no_received() {
    TEST_ASSERT(noCBs > 1);
    TEST_ASSERT_EQUAL(expected.size(), noCBs);
}

void test_received_equals_expected() {
    auto r = received.begin();
    auto e = expected.begin(); 
    while(r != received.end()) {
        TEST_ASSERT((*r).equals(*e));
        ++r;
        ++e;
    }
}

void test_node_status() {
    Serial.printf("Status: %s\n", lastNodeStatus.c_str());
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(lastNodeStatus);
    uint32_t t = root["time"];
    // Time delay within 50ms
    if (t > lastNSReceived) {
        TEST_ASSERT(t - lastNSReceived < 50000);
    } else {
        TEST_ASSERT(lastNSReceived - t < 50000);
    }
    uint32_t otherNode = root["nodeId"];
    TEST_ASSERT(mesh.findConnection(otherNode));
    uint32_t logNode = root["logNode"];
    TEST_ASSERT_EQUAL(logNode, mesh.getNodeId());
}

void logMessages() {
    auto r = received.begin();
    auto e = expected.begin(); 
    while(r != received.end()) {
        Serial.printf("Received: %s\n", (*r).c_str());
        Serial.printf("Expected: %s\n", (*e).c_str());
        ++r;
        ++e;
    }
}

Task logServerTask(10000, TASK_FOREVER, []() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& msg = jsonBuffer.createObject();
    msg["topic"] = "logServer";
    msg["nodeId"] = mesh.getNodeId();

    String str;
    msg.printTo(str);
    mesh.sendBroadcast(str);

    // log to serial
    //msg.printTo(Serial);
    //Serial.printf("\n");
});

Task waitTask;

void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
    //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE | DEBUG ); // all types on
    mesh.setDebugMsgTypes(S_TIME | ERROR | CONNECTION | DEBUG);
    mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT );
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onReceive(&receivedCallback);
}

void loop() {
    mesh.update();
    //UNITY_END(); // stop unit testing
    if (endEchoTest) {
        RUN_TEST(test_no_received);
        RUN_TEST(test_received_equals_expected);

        //logMessages();
        waitTask.set(30000, TASK_ONCE, []() {
            endTest = true;
        });
        mesh.scheduler.addTask(waitTask);
        waitTask.enableDelayed();
        mesh.scheduler.addTask(logServerTask);
        logServerTask.enable();
        endEchoTest = false;
        mesh.onReceive(&nodeStatusReceivedCallback);
    }
    if (endTest) {
        RUN_TEST(test_node_status);
        UNITY_END();
    }
}
#endif
