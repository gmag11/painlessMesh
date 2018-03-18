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

void use_buffer(temp_buffer_t &buf) {
    for(auto i = 0; i < buf.length; ++i) {
        buf.buffer[i] = (char)random(65, 90);
    }
}

void test_string_parse() {
    temp_buffer_t buf;

    const char* c1 = "abcdef\0";
    auto rb = ReceiveBuffer();
    rb.push(c1, 7, buf);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 1);
    TEST_ASSERT_EQUAL(rb.buffer.length(), 0);
    TEST_ASSERT((--rb.jsonStrings.end())->equals("abcdef"));
    rb.push(c1, 7, buf);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 2);
    TEST_ASSERT_EQUAL(rb.buffer.length(), 0);

    const char* c2 = "\0abcdef";
    rb.push(c2, 7, buf);
    use_buffer(buf);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 2);
    TEST_ASSERT_EQUAL(rb.buffer.length(), 6);

    const char* c3 = "ghi\0abcdef";
    rb.push(c3, 10, buf);
    use_buffer(buf);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 3);
    TEST_ASSERT((--rb.jsonStrings.end())->equals("abcdefghi"));
    TEST_ASSERT_EQUAL((--rb.jsonStrings.end())->length(), 9);
    TEST_ASSERT_EQUAL(rb.buffer.length(), 6);

    rb.push(c2, 7, buf);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 4);
    TEST_ASSERT((--rb.jsonStrings.end())->equals("abcdef"));
    TEST_ASSERT_EQUAL(rb.buffer.length(), 6);


    rb.clear();
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 0);
    TEST_ASSERT_EQUAL(rb.buffer.length(), 0);
    use_buffer(buf);

    // Skip empty
    const char* c4 = "abc\0\0def";
    rb.push(c4, 8, buf);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 1);
    TEST_ASSERT_EQUAL(rb.buffer.length(), 3);

    rb.clear();
    const char v1[4] = {'a', 'b', '\0', 'c'};
    rb.push(v1, 4, buf);
    TEST_ASSERT_EQUAL(rb.jsonStrings.size(), 1);
    TEST_ASSERT(rb.front().equals("ab"));
    TEST_ASSERT_EQUAL(rb.buffer.length(), 1);
}

// Test freeing pbuf afterwards
/*
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

    temp_buffer_t buf;
    auto rb = ReceiveBuffer();
    rb.push(p1, buf);
    TEST_ASSERT_EQUAL(1, rb.jsonStrings.size());
    TEST_ASSERT_EQUAL(0, rb.buffer.length());

    rb.push(p2, buf);
    TEST_ASSERT_EQUAL(3, rb.jsonStrings.size());
    TEST_ASSERT_EQUAL(3, rb.buffer.length());
    use_buffer(buf);

    //pbuf_free(p1); //This seems to crash always.
    //pbuf_free(p2);

    TEST_ASSERT(rb.front().equals("abcdef"));
    rb.pop_front();
    TEST_ASSERT(rb.front().equals("ghi"));
    rb.pop_front();
    TEST_ASSERT(rb.front().equals("jklmnopqr"));
    rb.pop_front();
    TEST_ASSERT(rb.empty());
}*/

void test_pushing_sent_buffer() {
    temp_buffer_t buf;
    SentBuffer sb;
    TEST_ASSERT_EQUAL(0, sb.requestLength(buf.length));
    TEST_ASSERT(sb.empty());

    // Make sure that we correctly add + 1 length for strings
    String s1 = "abc";
    sb.push(s1);
    TEST_ASSERT_EQUAL(4, sb.requestLength(buf.length));
    TEST_ASSERT_EQUAL(1, sb.jsonStrings.size());
    TEST_ASSERT(!sb.empty());

    String s2 = "def";
    sb.push(s2);

    String s3 = "ghi";
    sb.push(s3, true);
    TEST_ASSERT((*sb.jsonStrings.begin()).equals("ghi"))
    TEST_ASSERT((++sb.jsonStrings.begin())->equals("abc"))

    sb.clear();
    TEST_ASSERT(sb.empty());
    TEST_ASSERT_EQUAL(0, sb.jsonStrings.size());
}

void test_sent_buffer_read() {
    temp_buffer_t buf;
    SentBuffer sb;
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

    TEST_ASSERT_EQUAL(3, sb.requestLength(buf.length));
    sb.read(sb.requestLength(buf.length), buf);
    TEST_ASSERT_EQUAL('a', buf.buffer[0]);
    TEST_ASSERT_EQUAL('\0', buf.buffer[2]);

    sb.freeRead();
    TEST_ASSERT_EQUAL(6, sb.jsonStrings.begin()->length());
    TEST_ASSERT(sb.jsonStrings.begin()->equals("defghi"));

    sb.read(2, buf);
    TEST_ASSERT_EQUAL('d', buf.buffer[0]);
    TEST_ASSERT_EQUAL('e', buf.buffer[1]);

    sb.freeRead();
    TEST_ASSERT_EQUAL(4, sb.jsonStrings.begin()->length());
    TEST_ASSERT_EQUAL(5, sb.requestLength(buf.length));
    sb.read(3, buf);
    TEST_ASSERT_EQUAL('f', buf.buffer[0]);
    TEST_ASSERT_EQUAL('g', buf.buffer[1]);
    TEST_ASSERT_EQUAL('h', buf.buffer[2]);
    use_buffer(buf);
    sb.freeRead();

    TEST_ASSERT_EQUAL(2, sb.requestLength(buf.length));
    sb.read(sb.requestLength(buf.length), buf);
    TEST_ASSERT_EQUAL('i', buf.buffer[0]);
    TEST_ASSERT_EQUAL('\0', buf.buffer[1]);
    sb.freeRead();
    use_buffer(buf);
 
    sb.read(sb.requestLength(buf.length), buf);
    TEST_ASSERT_EQUAL('j', buf.buffer[0]);
    TEST_ASSERT_EQUAL('k', buf.buffer[1]);
    TEST_ASSERT_EQUAL('l', buf.buffer[2]);
    TEST_ASSERT_EQUAL('\0', buf.buffer[6]);
    use_buffer(buf);
}

String randomString(uint32_t length) {
    String str;
    for(auto i = 0; i < length; ++i) {
        char rnd = random(65, 90);
        str += String(rnd);
    }
    return str;
}

void test_random_sent_receive() {
    temp_buffer_t buf;
    ReceiveBuffer rb;
    SentBuffer sb;

    SimpleList<String> sent;
    size_t i = 0;

    #ifdef ESP32
    auto end_i = 30;
    #else
    auto end_i = 5;
    #endif

    while(i < end_i || !sb.empty())  {
        if (i < end_i) {
            auto str = randomString(random(100, 3000));
            sb.push(str);
            sent.push_back(str);
        }
        ++i;

        temp_buffer_t rb_buf;
        size_t rq_len = sb.requestLength(rb_buf.length);
        if (random(0,3) < 1) {
            rq_len = random(0, rq_len);
        }

        use_buffer(buf);
        sb.read(rq_len, buf);
        use_buffer(rb_buf);
        rb.push(buf.buffer, rq_len, rb_buf);
        use_buffer(buf);
        use_buffer(rb_buf);
        sb.freeRead();

        if (random(0,3) < 2) {
            while(!rb.empty()) {
                TEST_ASSERT(rb.front().equals((*sent.begin())));
                sent.pop_front();
                rb.pop_front();
            }
        }
    }
}

void test_priority_push_dirty() {
    temp_buffer_t buf;
    SentBuffer sb;
    auto str = String("Hello world");
    sb.push(str);
    sb.push(str);
    sb.read(6, buf);
    sb.freeRead();
    TEST_ASSERT(!sb.clean);

    // Because the buffer != clean this prior message should become 2nd
    auto str_pr = String("bla");
    sb.push(str_pr, true);
    auto len = sb.requestLength(buf.length);
    TEST_ASSERT_EQUAL(6, len);
    sb.read(len, buf);
    sb.freeRead();
    TEST_ASSERT_EQUAL('w', buf.buffer[0]);
    TEST_ASSERT(sb.clean);
    len = sb.requestLength(buf.length);
    TEST_ASSERT_EQUAL(str_pr.length() + 1, len);
    TEST_ASSERT_EQUAL(2, sb.jsonStrings.size());
}

// Make sure that empty only returns true if the void * buffer is empty too

void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
}

void loop() {
    RUN_TEST(test_string_split);
    RUN_TEST(test_string_parse);
    //RUN_TEST(test_pbuf_parse);

    RUN_TEST(test_pushing_sent_buffer);
    RUN_TEST(test_sent_buffer_read);

    RUN_TEST(test_random_sent_receive);
    RUN_TEST(test_priority_push_dirty);
    UNITY_END(); // stop unit testing
}
#endif
