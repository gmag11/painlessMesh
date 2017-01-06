#include <painlessMesh.h>
#include <unity.h>

#ifdef UNIT_TEST

void test_findConnection() {
    painlessMesh mesh;
    TEST_ASSERT_EQUAL(NULL, mesh.findConnection(1));

    meshConnectionType conn;
    mesh._connections.push_back(conn);
    TEST_ASSERT_EQUAL(NULL, mesh.findConnection(1));

    meshConnectionType conn2;
    conn2.nodeId = 1;
    mesh._connections.push_back(conn2);
    TEST_ASSERT(mesh.findConnection(1));

    // Add test for meshConnection with indirect connection
    meshConnectionType conn3;
    conn3.nodeId = 2;
    conn3.subConnections = "[{\"nodeId\":887034362,\"subs\":[{\"nodeId\":37418,\"subs\":[]}]}]";
    mesh._connections.push_back(conn3);
    TEST_ASSERT(mesh.findConnection(887034362));

    TEST_ASSERT_EQUAL(NULL, mesh.findConnection(70));
}

void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
}

void loop() {
    RUN_TEST(test_findConnection);
    UNITY_END(); // stop unit testing
}
#endif
