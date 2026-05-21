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

void test_countdown_full_strip_at_t_zero(void) {
    const PollSnapshot s = makeSnap(100.0f, 10, 0);
    TEST_ASSERT_EQUAL_UINT16(30u, Animations::countdownLitCount(s, 0, 30));
}

void test_countdown_half_strip_at_half_window(void) {
    const PollSnapshot s = makeSnap(100.0f, 10, 0);
    // 5 minutes elapsed in a 10-minute window -> half lit.
    TEST_ASSERT_EQUAL_UINT16(15u,
        Animations::countdownLitCount(s, 5UL * 60UL * 1000UL, 30));
}

void test_countdown_empty_at_window_end(void) {
    const PollSnapshot s = makeSnap(100.0f, 10, 0);
    TEST_ASSERT_EQUAL_UINT16(0u,
        Animations::countdownLitCount(s, 10UL * 60UL * 1000UL, 30));
}

void test_countdown_empty_past_window(void) {
    const PollSnapshot s = makeSnap(100.0f, 10, 0);
    TEST_ASSERT_EQUAL_UINT16(0u,
        Animations::countdownLitCount(s, 10UL * 60UL * 1000UL + 1UL, 30));
}

void test_countdown_returns_zero_when_no_remaining_minutes(void) {
    const PollSnapshot s = makeSnap(100.0f, 0, 0);
    TEST_ASSERT_EQUAL_UINT16(0u, Animations::countdownLitCount(s, 0, 30));
    TEST_ASSERT_EQUAL_UINT16(0u, Animations::countdownLitCount(s, 1234, 30));
    TEST_ASSERT_EQUAL_UINT16(0u,
        Animations::countdownLitCount(s, 999UL * 60UL * 1000UL, 30));
}
