#include <arduinoUnity.h>

extern uint32_t timeAdjuster;
#include <painlessMeshSync.h>

#ifdef UNITY

void test_adjust_calc() {
    timeSync ts;

    uint32_t times[4];
    times[0] = 0; times[1] = 0;
    times[2] = 0; times[3] = 0;
    TEST_ASSERT_EQUAL(0x7FFFFFFF, ts.calcAdjustment(times));
    TEST_ASSERT_EQUAL(0, timeAdjuster);

    times[0] = 1; times[1] = 1;
    times[2] = 1; times[3] = 1;
    TEST_ASSERT_EQUAL(0, ts.calcAdjustment(times));
    TEST_ASSERT_EQUAL(0, timeAdjuster);

    times[0] = 1; times[1] = 3;
    times[2] = 1; times[3] = 3;
    TEST_ASSERT_EQUAL(0, ts.calcAdjustment(times));
    TEST_ASSERT_EQUAL(0, timeAdjuster);

    times[0] = 1; times[1] = 3;
    times[2] = 3; times[3] = 1;
    TEST_ASSERT_EQUAL(2, ts.calcAdjustment(times));
    TEST_ASSERT_EQUAL(2, timeAdjuster);

    times[0] = 1; times[1] = 3;
    times[2] = 5; times[3] = 1;
    TEST_ASSERT_EQUAL(3, ts.calcAdjustment(times));
    TEST_ASSERT_EQUAL(5, timeAdjuster);
    timeAdjuster = 0;

    times[0] = 3; times[1] = 1;
    times[2] = 1; times[3] = 3;
    TEST_ASSERT_EQUAL(-2, ts.calcAdjustment(times));
    TEST_ASSERT_EQUAL(0xFFFFFFFF-1, timeAdjuster);

    times[0] = 0xFFFFFFFF; times[1] = 1;
    times[2] = 1; times[3] = 0xFFFFFFFF;
    TEST_ASSERT_EQUAL(2, ts.calcAdjustment(times));
    TEST_ASSERT_EQUAL(0, timeAdjuster);
}

void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
}

void loop() {
    RUN_TEST(test_adjust_calc);
    UNITY_END(); // stop unit testing
}
#endif
