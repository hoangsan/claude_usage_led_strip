// Unit tests for Animations::selectBandColor.
//
// Boundary semantics (data-model.md §5):
//   util  < 70.0   -> NORMAL
//   70.0 <= util <= 95.0  -> WARN
//   util  > 95.0   -> EXHAUSTED
//
// The three quota-color macros are supplied via platformio.ini build_flags
// in the [env:native] block so the assertions and the production code see
// identical values.

#include "mocks/CRGB.h"
#include "../../src/Animations.h"

#include <unity.h>

void test_band_normal_at_zero(void) {
    const CRGB expected = NORMAL_QUOTA_COLOR;
    TEST_ASSERT_TRUE(expected == Animations::selectBandColor(0.0f));
}
void test_band_normal_at_fifty(void) {
    const CRGB expected = NORMAL_QUOTA_COLOR;
    TEST_ASSERT_TRUE(expected == Animations::selectBandColor(50.0f));
}
void test_band_normal_just_below_seventy(void) {
    const CRGB expected = NORMAL_QUOTA_COLOR;
    TEST_ASSERT_TRUE(expected == Animations::selectBandColor(69.99f));
}

void test_band_warn_at_seventy(void) {
    const CRGB expected = WARN_QUOTA_COLOR;
    TEST_ASSERT_TRUE(expected == Animations::selectBandColor(70.0f));
}
void test_band_warn_at_eighty(void) {
    const CRGB expected = WARN_QUOTA_COLOR;
    TEST_ASSERT_TRUE(expected == Animations::selectBandColor(80.0f));
}
void test_band_warn_at_ninety_five(void) {
    const CRGB expected = WARN_QUOTA_COLOR;
    TEST_ASSERT_TRUE(expected == Animations::selectBandColor(95.0f));
}

void test_band_exhausted_just_above_ninety_five(void) {
    const CRGB expected = EXHAUSTED_QUOTA_COLOR;
    TEST_ASSERT_TRUE(expected == Animations::selectBandColor(95.01f));
}
void test_band_exhausted_at_hundred(void) {
    const CRGB expected = EXHAUSTED_QUOTA_COLOR;
    TEST_ASSERT_TRUE(expected == Animations::selectBandColor(100.0f));
}
void test_band_exhausted_overage(void) {
    const CRGB expected = EXHAUSTED_QUOTA_COLOR;
    TEST_ASSERT_TRUE(expected == Animations::selectBandColor(150.0f));
}
