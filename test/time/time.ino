#include <arduinoUnity.h>

#include <painlessMesh.h>

#ifdef UNITY

void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
}

void loop() {
    //RUN_TEST(test_comparison);
    UNITY_END(); // stop unit testing
}
#endif
