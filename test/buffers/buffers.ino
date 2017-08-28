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

    char v1[7] = {'a', 'b', 'c', 'd', 'e', 'f', '\0'};
    s1 = String(v1);
    TEST_ASSERT_EQUAL(s1.length(), 6);

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

    rb.clear();
    const char v1[4] = {'a', 'b', '\0', 'c'};
    rb.push(v1, 4);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 1);
    TEST_ASSERT(rb.front().equals("ab"));
    TEST_ASSERT_EQUAL(rb.buffer.length(), 1);
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

void test_pushing_sent_buffer() {
    SentBuffer sb;
    TEST_ASSERT_EQUAL(0, sb.requestLength());
    TEST_ASSERT(sb.empty());

    // Make sure that we correctly add + 1 length for strings
    String s1 = "abc";
    sb.push(s1);
    TEST_ASSERT_EQUAL(4, sb.requestLength());
    TEST_ASSERT_EQUAL(1, sb.jsonStrings.size());
    TEST_ASSERT(!sb.empty());

    String s2 = "def";
    sb.push(s2);

    String s3 = "ghi";
    sb.push(s3, true);
    TEST_ASSERT((*sb.jsonStrings.begin()).equals("ghi"))
    TEST_ASSERT((sb.jsonStrings.begin()+1)->equals("abc"))

    sb.clear();
    TEST_ASSERT(sb.empty());
    TEST_ASSERT_EQUAL(0, sb.jsonStrings.size());
}

void test_sent_buffer_read() {
    SentBuffer sb(2);
    String s1 = "ab";
    sb.push(s1);
    s1 = "defghi";
    sb.push(s1);
    s1 = "jklxyz";
    sb.push(s1);
    s1 = "mnopqr";
    sb.push(s1);
    s1 = "wvu";
    sb.push(s1);

    TEST_ASSERT_EQUAL(3, sb.requestLength());
    TEST_ASSERT_EQUAL(0, sb.buffer_length);
    auto c1 = sb.read(sb.requestLength());
    TEST_ASSERT_EQUAL('a', c1[0]);
    TEST_ASSERT_EQUAL('\0', c1[3]);
    TEST_ASSERT(3 <= sb.buffer_length);
    TEST_ASSERT(sb.buffer_length <= sb.total_buffer_length);

    sb.freeRead();

    TEST_ASSERT_EQUAL(0, sb.buffer_length);
    auto c2 = sb.read(2);
    TEST_ASSERT_EQUAL('d', c2[0]);
    TEST_ASSERT_EQUAL('e', c2[1]);
    TEST_ASSERT_EQUAL(5, sb.buffer_length);
    TEST_ASSERT_EQUAL(1, sb.jsonStrings.begin()->length());
    sb.freeRead();
    TEST_ASSERT_EQUAL(3, sb.buffer_length);
    TEST_ASSERT_EQUAL(3, sb.requestLength());
    auto c3 = sb.read(3);
    TEST_ASSERT_EQUAL('f', c3[0]);
    TEST_ASSERT_EQUAL('g', c3[1]);
    TEST_ASSERT_EQUAL('h', c3[2]);
    sb.freeRead();
    TEST_ASSERT_EQUAL(0, sb.buffer_length);
    TEST_ASSERT_EQUAL(2, sb.requestLength());
    auto c4 = sb.read(sb.requestLength());
    TEST_ASSERT_EQUAL('i', c4[0]);
    TEST_ASSERT_EQUAL('\0', c4[1]);
    sb.freeRead();
 
    // Make sure buffer grows as needed.
    auto c5 = sb.read(sb.requestLength());
    TEST_ASSERT(sb.buffer_length <= sb.total_buffer_length);
    TEST_ASSERT_EQUAL(7, sb.buffer_length);
    TEST_ASSERT_EQUAL('j', c5[0]);
    TEST_ASSERT_EQUAL('k', c5[1]);
    TEST_ASSERT_EQUAL('\0', c5[6]);
}

// Make sure that empty only returns true if the void * buffer is empty too

void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
}

void loop() {
    RUN_TEST(test_string_split);
    RUN_TEST(test_string_parse);
    RUN_TEST(test_pbuf_parse);

    RUN_TEST(test_pushing_sent_buffer);
    RUN_TEST(test_sent_buffer_read);
    UNITY_END(); // stop unit testing
}
#endif
