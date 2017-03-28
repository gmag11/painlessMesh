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

painlessMesh  mesh;

SimpleList<String> expected;
SimpleList<String> received;

size_t noCBs = 0;

void newConnectionCallback(uint32_t nodeId) {
    mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
    auto msg = String("Hello boy");
    mesh.sendSingle(nodeId, msg);
    expected.push_back(msg);
}

void receivedCallback(uint32_t from, String &msg) {
    ++noCBs;
    received.push_back(msg);
    if (expected.size() == received.size())
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
