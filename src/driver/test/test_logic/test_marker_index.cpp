// Unit tests for Animations::markerLedIndex — the threshold tick position.
// markerLedIndex(p, n) == litLedCount(p, n) - 1, i.e. the topmost LED lit at
// exactly utilization p. Color macros come from platformio.ini / defaults.

#include "mocks/CRGB.h"
#include "../../src/Animations.h"

#include <unity.h>

void test_marker_warn_default_on_160(void) {
    // 70% of 160 == 112 lit -> topmost index 111.
    TEST_ASSERT_EQUAL_UINT16(111u, Animations::markerLedIndex(70.0f, 160));
}
void test_marker_exhausted_default_on_160(void) {
    // 90% of 160 == 144 lit -> topmost index 143.
    TEST_ASSERT_EQUAL_UINT16(143u, Animations::markerLedIndex(90.0f, 160));
}
void test_marker_at_hundred_is_last_led(void) {
    // 100% lights all 160 -> topmost index is the final pixel, 159.
    TEST_ASSERT_EQUAL_UINT16(159u, Animations::markerLedIndex(100.0f, 160));
}
void test_marker_half_on_small_strip(void) {
    // 50% of 10 == 5 lit -> index 4.
    TEST_ASSERT_EQUAL_UINT16(4u, Animations::markerLedIndex(50.0f, 10));
}
void test_marker_always_in_range(void) {
    // Whatever the threshold, the index stays a valid pixel on the strip.
    TEST_ASSERT_TRUE(Animations::markerLedIndex(70.0f, 8) < 8u);
    TEST_ASSERT_TRUE(Animations::markerLedIndex(90.0f, 8) < 8u);
}
void test_marker_zero_strip_is_safe(void) {
    TEST_ASSERT_EQUAL_UINT16(0u, Animations::markerLedIndex(70.0f, 0));
}
