// Unity entry point for the native test environment. Aggregates every test
// declared across test_color_band.cpp, test_fill_count.cpp,
// test_countdown.cpp.

#include <unity.h>

// --- Forward declarations (defined in sibling .cpp files) -----------------

// color-band tests
void test_band_normal_at_zero(void);
void test_band_normal_at_fifty(void);
void test_band_normal_just_below_seventy(void);
void test_band_warn_at_seventy(void);
void test_band_warn_at_eighty(void);
void test_band_warn_just_below_ninety(void);
void test_band_exhausted_at_ninety(void);
void test_band_exhausted_just_above_ninety(void);
void test_band_exhausted_at_hundred(void);
void test_band_exhausted_overage(void);

// fill-count tests
void test_fill_zero_percent(void);
void test_fill_full_at_hundred(void);
void test_fill_half(void);
void test_fill_caps_at_strip_length(void);
void test_fill_rounds_to_nearest_at_boundary(void);

// marker-index tests
void test_marker_warn_default_on_160(void);
void test_marker_exhausted_default_on_160(void);
void test_marker_at_hundred_is_last_led(void);
void test_marker_half_on_small_strip(void);
void test_marker_always_in_range(void);
void test_marker_zero_strip_is_safe(void);

// countdown tests
void test_countdown_empty_at_full_window(void);
void test_countdown_full_at_reset_moment(void);
void test_countdown_half_at_half_window(void);
void test_countdown_fills_as_time_elapses(void);
void test_countdown_full_past_reset(void);
void test_countdown_empty_when_remaining_exceeds_window(void);

// error-threshold tests
void test_error_after_threshold_consecutive_failures(void);
void test_success_resets_failure_streak(void);
void test_recovers_from_error_on_success(void);

// exhausted-hold tests
void test_first_exhausted_poll_holds_usage(void);
void test_drop_below_rearms_exhausted_hold(void);

void setUp(void) {}
void tearDown(void) {}

int main(int, char**) {
    UNITY_BEGIN();

    // Color band
    RUN_TEST(test_band_normal_at_zero);
    RUN_TEST(test_band_normal_at_fifty);
    RUN_TEST(test_band_normal_just_below_seventy);
    RUN_TEST(test_band_warn_at_seventy);
    RUN_TEST(test_band_warn_at_eighty);
    RUN_TEST(test_band_warn_just_below_ninety);
    RUN_TEST(test_band_exhausted_at_ninety);
    RUN_TEST(test_band_exhausted_just_above_ninety);
    RUN_TEST(test_band_exhausted_at_hundred);
    RUN_TEST(test_band_exhausted_overage);

    // Fill count
    RUN_TEST(test_fill_zero_percent);
    RUN_TEST(test_fill_full_at_hundred);
    RUN_TEST(test_fill_half);
    RUN_TEST(test_fill_caps_at_strip_length);
    RUN_TEST(test_fill_rounds_to_nearest_at_boundary);

    // Marker index
    RUN_TEST(test_marker_warn_default_on_160);
    RUN_TEST(test_marker_exhausted_default_on_160);
    RUN_TEST(test_marker_at_hundred_is_last_led);
    RUN_TEST(test_marker_half_on_small_strip);
    RUN_TEST(test_marker_always_in_range);
    RUN_TEST(test_marker_zero_strip_is_safe);

    // Countdown
    RUN_TEST(test_countdown_empty_at_full_window);
    RUN_TEST(test_countdown_full_at_reset_moment);
    RUN_TEST(test_countdown_half_at_half_window);
    RUN_TEST(test_countdown_fills_as_time_elapses);
    RUN_TEST(test_countdown_full_past_reset);
    RUN_TEST(test_countdown_empty_when_remaining_exceeds_window);

    // Error threshold
    RUN_TEST(test_error_after_threshold_consecutive_failures);
    RUN_TEST(test_success_resets_failure_streak);
    RUN_TEST(test_recovers_from_error_on_success);

    // Exhausted hold
    RUN_TEST(test_first_exhausted_poll_holds_usage);
    RUN_TEST(test_drop_below_rearms_exhausted_hold);

    return UNITY_END();
}
