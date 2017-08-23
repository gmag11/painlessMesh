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
    TEST_ASSERT((rb.jsonStrings.end() - 1)->equals("abcdef"));
    rb.push(c1, 7);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 2);
    TEST_ASSERT_EQUAL(rb.buffer.length(), 0);

    const char* c2 = "\0abcdef";
    rb.push(c2, 7);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 2);
    TEST_ASSERT_EQUAL(rb.buffer.length(), 6);

    const char* c3 = "ghi\0abcdef";
    rb.push(c3, 10);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 3);
    TEST_ASSERT((rb.jsonStrings.end() - 1)->equals("abcdefghi"));
    TEST_ASSERT_EQUAL((rb.jsonStrings.end() - 1)->length(), 9);
    TEST_ASSERT_EQUAL(rb.buffer.length(), 6);

    rb.push(c2, 7);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 4);
    TEST_ASSERT((rb.jsonStrings.end() - 1)->equals("abcdef"));
    TEST_ASSERT_EQUAL(rb.buffer.length(), 6);


    rb.clear();
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 0);
    TEST_ASSERT_EQUAL(rb.buffer.length(), 0);

    // Skip empty
    const char* c4 = "abc\0\0def";
    rb.push(c4, 8);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 1);
    TEST_ASSERT_EQUAL(rb.buffer.length(), 3);
}

// Test freeing pbuf afterwards
void test_pbuf_parse() {
    pbuf *p1 = pbuf_alloc(PBUF_RAW, 7, PBUF_POOL);
    pbuf *p2 = pbuf_alloc(PBUF_RAW, 17, PBUF_POOL);
    pbuf *p3 = pbuf_alloc(PBUF_RAW, 7, PBUF_POOL);

    p1->len = 7;
    p1->payload = (void*)"abcdef\0";
    p1->tot_len = 7;

    p2->len = 10;
    p2->payload = (void*)"ghi\0jklmno";
    p2->tot_len = 17;

    p3->len = 7;
    p3->payload = (void*)"pqr\0stu";
    p3->tot_len = 7;

    pbuf_cat(p2, p3);
    TEST_ASSERT(p2->next != NULL);

    auto rb = ReceiveBuffer();
    rb.push(p1);
    TEST_ASSERT_EQUAL(1, rb.jsonStrings.size());
    TEST_ASSERT_EQUAL(0, rb.buffer.length());

    rb.push(p2);
    TEST_ASSERT_EQUAL(3, rb.jsonStrings.size());
    TEST_ASSERT_EQUAL(3, rb.buffer.length());

    //pbuf_free(p1); //This seems to crash always.
    //pbuf_free(p2);

    TEST_ASSERT(rb.front().equals("abcdef"));
    rb.pop_front();
    TEST_ASSERT(rb.front().equals("ghi"));
    rb.pop_front();
    TEST_ASSERT(rb.front().equals("jklmnopqr"));
    rb.pop_front();
    TEST_ASSERT(rb.empty());
}

void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
}

void loop() {
    RUN_TEST(test_string_split);
    RUN_TEST(test_string_parse);
    RUN_TEST(test_pbuf_parse);
    UNITY_END(); // stop unit testing
}
#endif
