// Unit tests for Animations::litLedCount (round-to-nearest, clamped).
// Color macros come from platformio.ini build_flags in [env:native].

#include "mocks/CRGB.h"
#include "../../src/Animations.h"

#include <unity.h>

void test_fill_zero_percent(void) {
    TEST_ASSERT_EQUAL_UINT16(0u, Animations::litLedCount(0.0f, 10));
}
void test_fill_full_at_hundred(void) {
    TEST_ASSERT_EQUAL_UINT16(10u, Animations::litLedCount(100.0f, 10));
}
void test_fill_half(void) {
    TEST_ASSERT_EQUAL_UINT16(5u, Animations::litLedCount(50.0f, 10));
}
void test_fill_caps_at_strip_length(void) {
    TEST_ASSERT_EQUAL_UINT16(10u, Animations::litLedCount(110.0f, 10));
}
void test_fill_rounds_to_nearest_at_boundary(void) {
    // 5% of 10 LEDs == 0.5 -> round-to-nearest gives 1.
    TEST_ASSERT_EQUAL_UINT16(1u, Animations::litLedCount(5.0f, 10));
}
