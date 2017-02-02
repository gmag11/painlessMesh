#include <arduinoUnity.h>

#include <painlessMeshSync.h>

#ifdef UNITY

void test_adjust_calc() {
    timeSync ts;

    // ts.times are invalid, should lead to max value
    TEST_ASSERT_EQUAL(0, ts.times[0]);
    TEST_ASSERT_EQUAL(0, ts.times[1]);
    TEST_ASSERT_EQUAL(0, ts.times[2]);
    TEST_ASSERT_EQUAL(0, ts.times[3]);
    TEST_ASSERT_EQUAL(0x7FFFFFFF, ts.calcAdjustment());
}

void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
}

void loop() {
    RUN_TEST(test_adjust_calc);
    UNITY_END(); // stop unit testing
}
#endif
