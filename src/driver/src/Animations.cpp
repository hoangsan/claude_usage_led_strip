#include "Animations.h"

#include <math.h>

#include "DisplayController.h"  // PollSnapshot, Reading

#ifndef UNIT_TEST
#include "../include/config.h"
#endif

namespace Animations {

CRGB selectBandColor(float utilization) {
    // Thresholds come from include/config.h on device, or from the
    // force-included test_color_defaults.h in the native test build.
    if (utilization < static_cast<float>(WARN_THRESHOLD_PERCENT))
        return NORMAL_QUOTA_COLOR;
    if (utilization < static_cast<float>(EXHAUSTED_THRESHOLD_PERCENT))
        return WARN_QUOTA_COLOR;
    return EXHAUSTED_QUOTA_COLOR;
}

uint16_t litLedCount(float utilization, uint16_t numLed) {
    if (utilization <= 0.0f) return 0;
    const float raw = lroundf(utilization * static_cast<float>(numLed) / 100.0f);
    if (raw <= 0.0f)              return 0;
    if (raw >= static_cast<float>(numLed)) return numLed;
    return static_cast<uint16_t>(raw);
}

uint16_t markerLedIndex(float thresholdPercent, uint16_t numLed) {
    if (numLed == 0) return 0;
    const uint16_t lit = litLedCount(thresholdPercent, numLed);  // [0, numLed]
    if (lit == 0) return 0;  // degenerate (threshold <= 0): mark the first LED
    return static_cast<uint16_t>(lit - 1);
}

uint16_t countdownLitCount(const PollSnapshot& snap,
                           uint32_t            now_ms,
                           uint16_t            numLed) {
    const uint32_t window = kFiveHourWindowMinutes;  // 300 (5 h), never 0

    uint32_t elapsed_ms = 0;
    if (now_ms >= snap.received_at_ms) {
        elapsed_ms = now_ms - snap.received_at_ms;
    }
    const uint32_t elapsed_minutes = elapsed_ms / 60000UL;

    const uint32_t orig = snap.reading.remaining_minutes;
    uint32_t effective_minutes =
        (elapsed_minutes < orig) ? (orig - elapsed_minutes) : 0;

    // A remaining value larger than the window would otherwise yield a
    // negative fill; clamp so it just reads as an empty bar.
    if (effective_minutes > window) effective_minutes = window;

    // Fill grows as the reset approaches: 0 LEDs with a full window left,
    // all LEDs at reset (effective_minutes == 0).
    const uint32_t elapsed_of_window = window - effective_minutes;  // [0, window]
    const float raw = lroundf(static_cast<float>(elapsed_of_window)
                              * static_cast<float>(numLed)
                              / static_cast<float>(window));
    if (raw <= 0.0f)                       return 0;
    if (raw >= static_cast<float>(numLed)) return numLed;
    return static_cast<uint16_t>(raw);
}

}  // namespace Animations
