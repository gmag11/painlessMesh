#include <painlessMesh.h>
#include <unity.h>

#ifdef UNIT_TEST

void test_findConnection() {
    TEST_ASSERT(false);
}

void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
}

void loop() {
    RUN_TEST(test_findConnection);
    UNITY_END(); // stop unit testing
}
#endif
