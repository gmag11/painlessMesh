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
    conn3.subConnections = "[{\"nodeId\":887034362,\"subs\":[{\"nodeId\":43,\"subs\":[]}]}]";
    mesh._connections.push_back(conn3);
    TEST_ASSERT(mesh.findConnection(887034362));

    TEST_ASSERT_EQUAL(NULL, mesh.findConnection(70));
    TEST_ASSERT_EQUAL(NULL, mesh.findConnection(88));
    TEST_ASSERT_EQUAL(NULL, mesh.findConnection(87));
    TEST_ASSERT_EQUAL(NULL, mesh.findConnection(62));
    TEST_ASSERT_EQUAL(NULL, mesh.findConnection(36));
    // 43 should match again
    TEST_ASSERT(mesh.findConnection(43));
}

bool lesserThan(uint32_t lessr, uint32_t than) {
    return
        (int) lesser - (int) than < 0;// Cast to int in case of time rollover
}

void test_comparison() {
    uint32_t uint32_max = 4294967295;
    TEST_ASSERT(lesserThan(uint32_max-1,uint32_max)); 
    TEST_ASSERT(lesserThan(uint32_max/2-1,uint32_max/2)); 
    TEST_ASSERT(lesserThan(uint32_max/2,uint32_max/2+1)); 
    TEST_ASSERT(lesserThan(10,100)); 

    // Overflow
    TEST_ASSERT(lesserThan(uint32_max,0)); 
    TEST_ASSERT(lesserThan(uint32_max,100)); 
}

void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
}

void loop() {
    RUN_TEST(test_findConnection);
    RUN_TEST(test_comparison);
    UNITY_END(); // stop unit testing
}
#endif
