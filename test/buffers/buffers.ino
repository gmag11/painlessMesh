#include <arduinoUnity.h>

#include <painlessMesh.h>

#ifdef UNITY

void test_string_split() {
    const char* c1 = "abcdef";
    String s1 = String(c1);
    TEST_ASSERT_EQUAL(s1.length(), 6);

    const char* c2 = "abcdef\0";
    String s2 = String(c2);
    TEST_ASSERT_EQUAL(s2.length(), 6);

    const char* c3 = "abcdef\0ghi";
    String s3 = String(c3);
    TEST_ASSERT_EQUAL(s3.length(), 6);
    const char *c4 = c3 + 7;
    s3 = String(c4);
    TEST_ASSERT_EQUAL(s3.length(), 3);
    const char* c5 = "\0abcdef\0ghi";
    String s5 = String(c5);
    TEST_ASSERT_EQUAL(s5.length(), 0);
 
    s3.concat(c3);
    TEST_ASSERT_EQUAL(s3.length(), 9);
    TEST_ASSERT(s3.equals(String("ghiabcdef")));

    // Next step is make sure we can actually send '\0' over tcp
}

void test_string_parse() {
    const char* c1 = "abcdef\0";
    auto rb = ReceiveBuffer();
    rb.push(c1, 7);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 1);
    TEST_ASSERT_EQUAL(rb.buffer.length(), 0);
}

// Test that it correctly starts a new buffer if last part of payload is \0

// Test freeing pbuf afterwards

void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
}

void loop() {
    RUN_TEST(test_string_split);
    RUN_TEST(test_string_parse);
    UNITY_END(); // stop unit testing
}
#endif
