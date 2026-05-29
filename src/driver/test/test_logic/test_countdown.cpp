// Unit tests for Animations::countdownLitCount.
// Uses an injected `now_ms` argument; no real millis() is touched.
// Color macros come from platformio.ini build_flags in [env:native].

#include "mocks/CRGB.h"
#include "../../src/Animations.h"
#include "../../src/DisplayController.h"

#include <unity.h>

namespace {

PollSnapshot makeSnap(float util, uint32_t remaining_minutes,
                      uint32_t received_at_ms = 0) {
    PollSnapshot s;
    s.reading.utilization       = util;
    s.reading.remaining_minutes = remaining_minutes;
    s.received_at_ms            = received_at_ms;
    return s;
}

}  // namespace

// New semantics: the bar fills 0 -> all as remaining-minutes drops from a full
// five-hour window (300 min) to 0. Computed against the fixed 300-min window.

void test_countdown_empty_at_full_window(void) {
    // A full window remaining -> nothing lit yet.
    const PollSnapshot s = makeSnap(100.0f, 300, 0);
    TEST_ASSERT_EQUAL_UINT16(0u, Animations::countdownLitCount(s, 0, 30));
}

void test_countdown_full_at_reset_moment(void) {
    // remaining == 0 means reset is now -> whole strip lit.
    const PollSnapshot s = makeSnap(100.0f, 0, 0);
    TEST_ASSERT_EQUAL_UINT16(30u, Animations::countdownLitCount(s, 0, 30));
}

void test_countdown_half_at_half_window(void) {
    // 150 of 300 minutes remaining -> half filled.
    const PollSnapshot s = makeSnap(100.0f, 150, 0);
    TEST_ASSERT_EQUAL_UINT16(15u, Animations::countdownLitCount(s, 0, 30));
}

void test_countdown_fills_as_time_elapses(void) {
    // Full window at poll; 150 min later only 150 remain -> half filled.
    const PollSnapshot s = makeSnap(100.0f, 300, 0);
    TEST_ASSERT_EQUAL_UINT16(15u,
        Animations::countdownLitCount(s, 150UL * 60UL * 1000UL, 30));
}

void test_countdown_full_past_reset(void) {
    // 150 min remaining at poll, 200 min elapsed -> effective 0 -> all lit.
    const PollSnapshot s = makeSnap(100.0f, 150, 0);
    TEST_ASSERT_EQUAL_UINT16(30u,
        Animations::countdownLitCount(s, 200UL * 60UL * 1000UL, 30));
}

void test_countdown_empty_when_remaining_exceeds_window(void) {
    // remaining > 5 h is clamped to the window -> empty bar.
    const PollSnapshot s = makeSnap(100.0f, 600, 0);
    TEST_ASSERT_EQUAL_UINT16(0u, Animations::countdownLitCount(s, 0, 30));
}
