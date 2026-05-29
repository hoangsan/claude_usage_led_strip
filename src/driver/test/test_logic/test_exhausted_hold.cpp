// Unit tests for the one-poll "100% confirmation" hold in
// DisplayController::advance. On the first successful poll where utilization
// reaches 100%, the controller must hold USAGE so the operator sees the full
// red bar; only the next exhausted poll flips to COUNTDOWN. If utilization
// drops back below 100%, the gate is rearmed for the next episode.

#include "../../src/DisplayController.h"

#include <unity.h>

namespace {

Reading makeReading(float util, uint32_t remaining_minutes) {
    Reading r;
    r.utilization       = util;
    r.remaining_minutes = remaining_minutes;
    return r;
}

DisplayState freshState() {
    DisplayState st{};
    st.mode = DisplayMode::STARTUP;
    return st;
}

}  // namespace

// First exhausted poll holds USAGE (full red bar); second flips to COUNTDOWN.
void test_first_exhausted_poll_holds_usage(void) {
    DisplayState st = freshState();

    DisplayController::advance(st, true, makeReading(100.0f, 200), 1000);
    TEST_ASSERT_EQUAL(DisplayMode::USAGE, st.mode);
    TEST_ASSERT_TRUE(st.exhausted_shown);

    DisplayController::advance(st, true, makeReading(100.0f, 195), 2000);
    TEST_ASSERT_EQUAL(DisplayMode::COUNTDOWN, st.mode);

    DisplayController::advance(st, true, makeReading(100.0f, 190), 3000);
    TEST_ASSERT_EQUAL(DisplayMode::COUNTDOWN, st.mode);
}

// Dropping below 100% clears the gate so a later 100% poll shows the red bar
// once more.
void test_drop_below_rearms_exhausted_hold(void) {
    DisplayState st = freshState();

    DisplayController::advance(st, true, makeReading(100.0f, 200), 1000);
    DisplayController::advance(st, true, makeReading(100.0f, 195), 2000);
    TEST_ASSERT_EQUAL(DisplayMode::COUNTDOWN, st.mode);

    // Quota window rolled over; back to normal usage.
    DisplayController::advance(st, true, makeReading(40.0f, 180), 3000);
    TEST_ASSERT_EQUAL(DisplayMode::USAGE, st.mode);
    TEST_ASSERT_FALSE(st.exhausted_shown);

    // Hits 100% again: must hold USAGE for one poll, then COUNTDOWN.
    DisplayController::advance(st, true, makeReading(100.0f, 300), 4000);
    TEST_ASSERT_EQUAL(DisplayMode::USAGE, st.mode);
    DisplayController::advance(st, true, makeReading(100.0f, 295), 5000);
    TEST_ASSERT_EQUAL(DisplayMode::COUNTDOWN, st.mode);
}
