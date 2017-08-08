#include <Arduino.h>
#include <unity.h>

#ifdef UNIT_TEST

// void setUp(void) {
// // set stuff up here
// }

// void tearDown(void) {
// // clean stuff up here
// }

void test_myTest(void) {
    boolean test;
    test = true;
    TEST_ASSERT_EQUAL(test,true);
}


void setup() {
    UNITY_BEGIN();    // IMPORTANT LINE!
    RUN_TEST(test_myTest);
}


void loop() {
  //UNITY_END();
}

#endif
