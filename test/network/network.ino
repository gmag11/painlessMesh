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

size_t noCBs = 0;

void newConnectionCallback(uint32_t nodeId) {
    mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
    Serial.printf("--> New Connection, nodeId = %u\n", nodeId);
    Serial.printf("--> My nodeId = %u\n", mesh.getNodeId());
    auto msg = String("Hello boy");
    mesh.sendSingle(nodeId, msg);
}

void receivedCallback(uint32_t from, String &msg) {
    Serial.printf("Received from %u msg=%s\n", from, msg.c_str());
    ++noCBs;
    endTest = true;
}

void test_connected() {
    TEST_ASSERT_EQUAL(1, noCBs);
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
        RUN_TEST(test_connected);
        UNITY_END();
    }
}
#endif
