#include <arduinoUnity.h>

extern uint32_t timeAdjuster;
#include <painlessMeshSync.h>

#ifdef UNITY

void test_adjust_calc() {
    timeSync ts;

    TEST_ASSERT_EQUAL(0, timeAdjuster);

    // ts.times are invalid, should lead to max value
    TEST_ASSERT_EQUAL(0, ts.times[0]);
    TEST_ASSERT_EQUAL(0, ts.times[1]);
    TEST_ASSERT_EQUAL(0, ts.times[2]);
    TEST_ASSERT_EQUAL(0, ts.times[3]);
    TEST_ASSERT_EQUAL(0x7FFFFFFF, ts.calcAdjustment());
    TEST_ASSERT_EQUAL(0, timeAdjuster);

    ts.times[0] = 1; ts.times[1] = 1;
    ts.times[2] = 1; ts.times[3] = 1;
    TEST_ASSERT_EQUAL(0, ts.calcAdjustment());
    TEST_ASSERT_EQUAL(0, timeAdjuster);

    ts.times[0] = 1; ts.times[1] = 3;
    ts.times[2] = 1; ts.times[3] = 3;
    TEST_ASSERT_EQUAL(0, ts.calcAdjustment());
    TEST_ASSERT_EQUAL(0, timeAdjuster);

    ts.times[0] = 1; ts.times[1] = 3;
    ts.times[2] = 3; ts.times[3] = 1;
    TEST_ASSERT_EQUAL(2, ts.calcAdjustment());
    TEST_ASSERT_EQUAL(2, timeAdjuster);

    ts.times[0] = 1; ts.times[1] = 3;
    ts.times[2] = 5; ts.times[3] = 1;
    TEST_ASSERT_EQUAL(3, ts.calcAdjustment());
    TEST_ASSERT_EQUAL(5, timeAdjuster);
    timeAdjuster = 0;

    ts.times[0] = 3; ts.times[1] = 1;
    ts.times[2] = 1; ts.times[3] = 3;
    TEST_ASSERT_EQUAL(-2, ts.calcAdjustment());
    TEST_ASSERT_EQUAL(0xFFFFFFFF-1, timeAdjuster);

    ts.times[0] = 0xFFFFFFFF; ts.times[1] = 1;
    ts.times[2] = 1; ts.times[3] = 0xFFFFFFFF;
    TEST_ASSERT_EQUAL(2, ts.calcAdjustment());
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
