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
void test_band_warn_at_ninety_five(void);
void test_band_exhausted_just_above_ninety_five(void);
void test_band_exhausted_at_hundred(void);
void test_band_exhausted_overage(void);

// fill-count tests
void test_fill_zero_percent(void);
void test_fill_full_at_hundred(void);
void test_fill_half(void);
void test_fill_caps_at_strip_length(void);
void test_fill_rounds_to_nearest_at_boundary(void);

// countdown tests
void test_countdown_full_strip_at_t_zero(void);
void test_countdown_half_strip_at_half_window(void);
void test_countdown_empty_at_window_end(void);
void test_countdown_empty_past_window(void);
void test_countdown_returns_zero_when_no_remaining_minutes(void);

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
    RUN_TEST(test_band_warn_at_ninety_five);
    RUN_TEST(test_band_exhausted_just_above_ninety_five);
    RUN_TEST(test_band_exhausted_at_hundred);
    RUN_TEST(test_band_exhausted_overage);

    // Fill count
    RUN_TEST(test_fill_zero_percent);
    RUN_TEST(test_fill_full_at_hundred);
    RUN_TEST(test_fill_half);
    RUN_TEST(test_fill_caps_at_strip_length);
    RUN_TEST(test_fill_rounds_to_nearest_at_boundary);

    // Countdown
    RUN_TEST(test_countdown_full_strip_at_t_zero);
    RUN_TEST(test_countdown_half_strip_at_half_window);
    RUN_TEST(test_countdown_empty_at_window_end);
    RUN_TEST(test_countdown_empty_past_window);
    RUN_TEST(test_countdown_returns_zero_when_no_remaining_minutes);

    return UNITY_END();
}
