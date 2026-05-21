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
    if (utilization <= static_cast<float>(EXHAUSTED_THRESHOLD_PERCENT))
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

uint16_t countdownLitCount(const PollSnapshot& snap,
                           uint32_t            now_ms,
                           uint16_t            numLed) {
    const uint32_t orig = snap.reading.remaining_minutes;
    if (orig == 0) return 0;

    uint32_t elapsed_ms = 0;
    if (now_ms >= snap.received_at_ms) {
        elapsed_ms = now_ms - snap.received_at_ms;
    }
    const uint32_t elapsed_minutes = elapsed_ms / 60000UL;

    uint32_t effective_minutes = 0;
    if (elapsed_minutes < orig) {
        effective_minutes = orig - elapsed_minutes;
    } else {
        effective_minutes = 0;
    }

    if (effective_minutes == 0) return 0;

    const float raw = lroundf(static_cast<float>(effective_minutes)
                              * static_cast<float>(numLed)
                              / static_cast<float>(orig));
    if (raw <= 0.0f)                       return 0;
    if (raw >= static_cast<float>(numLed)) return numLed;
    return static_cast<uint16_t>(raw);
}

}  // namespace Animations
