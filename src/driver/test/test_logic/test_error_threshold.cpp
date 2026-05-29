// Unit tests for DisplayController::advance failure tolerance.
// A single failed poll must NOT trip ERROR; only kErrorFailureThreshold
// consecutive failures may. A successful poll clears the streak.

#include "../../src/DisplayController.h"

#include <unity.h>

namespace {

Reading makeReading(float util, uint32_t remaining_minutes) {
    Reading r;
    r.utilization       = util;
    r.remaining_minutes = remaining_minutes;
    return r;
}

DisplayState usageState() {
    DisplayState st{};
    st.mode                 = DisplayMode::USAGE;
    st.consecutive_failures = 0;
    return st;
}

}  // namespace

// One failure from USAGE holds USAGE; the threshold-th consecutive failure
// flips to ERROR.
void test_error_after_threshold_consecutive_failures(void) {
    DisplayState  st = usageState();
    const Reading r  = makeReading(50.0f, 120);

    // First failure: below threshold (2), hold the current mode.
    DisplayController::advance(st, false, r, 1000);
    TEST_ASSERT_EQUAL(DisplayMode::USAGE, st.mode);
    TEST_ASSERT_EQUAL_UINT8(1, st.consecutive_failures);

    // Second consecutive failure reaches the threshold -> ERROR.
    DisplayController::advance(st, false, r, 2000);
    TEST_ASSERT_EQUAL(DisplayMode::ERROR, st.mode);
    TEST_ASSERT_EQUAL_UINT8(2, st.consecutive_failures);
}

// A single failure between two successes never shows ERROR, and the counter
// resets so a later lone failure is also tolerated.
void test_success_resets_failure_streak(void) {
    DisplayState  st = usageState();
    const Reading r  = makeReading(50.0f, 120);

    DisplayController::advance(st, false, r, 1000);  // 1 failure, hold USAGE
    TEST_ASSERT_EQUAL(DisplayMode::USAGE, st.mode);

    DisplayController::advance(st, true, r, 2000);   // success clears streak
    TEST_ASSERT_EQUAL(DisplayMode::USAGE, st.mode);
    TEST_ASSERT_EQUAL_UINT8(0, st.consecutive_failures);

    // A subsequent lone failure is tolerated again (not ERROR).
    DisplayController::advance(st, false, r, 3000);
    TEST_ASSERT_EQUAL(DisplayMode::USAGE, st.mode);
    TEST_ASSERT_EQUAL_UINT8(1, st.consecutive_failures);
}

// Recovery: once in ERROR, a successful poll returns to USAGE/COUNTDOWN.
void test_recovers_from_error_on_success(void) {
    DisplayState  st = usageState();
    const Reading r  = makeReading(50.0f, 120);

    DisplayController::advance(st, false, r, 1000);
    DisplayController::advance(st, false, r, 2000);
    TEST_ASSERT_EQUAL(DisplayMode::ERROR, st.mode);

    DisplayController::advance(st, true, makeReading(50.0f, 120), 3000);
    TEST_ASSERT_EQUAL(DisplayMode::USAGE, st.mode);
    TEST_ASSERT_EQUAL_UINT8(0, st.consecutive_failures);
}
