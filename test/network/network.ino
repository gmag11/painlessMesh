#include <arduinoUnity.h>

#include <painlessMesh.h>

#ifdef UNITY
/**
Here we test the network behaviour of a node. These tests rely on a echoNode to be present
*/

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555
bool endTest = false;
bool sendingDone = false;

painlessMesh  mesh;

SimpleList<String> expected;
SimpleList<String> received;

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
        endTest = true;
}

void test_no_received() {
    TEST_ASSERT_EQUAL(expected.size(), noCBs);
}

void test_received_equals_expected() {
    auto r = received.begin();
    auto e = expected.begin(); 
    while(r != received.end()) {
        TEST_ASSERT(*r == *e);
        ++r;
        ++e;
    }
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


void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
    //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE | DEBUG ); // all types on
    mesh.setDebugMsgTypes( ERROR | CONNECTION | DEBUG ); // all types on
    mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT );
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onReceive(&receivedCallback);
}

void loop() {
    mesh.update();
    //UNITY_END(); // stop unit testing
    if (endTest) {
        RUN_TEST(test_no_received);
        RUN_TEST(test_received_equals_expected);
        //logMessages();
        UNITY_END();
    }
}
#endif
