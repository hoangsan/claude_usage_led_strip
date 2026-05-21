// Animations: pure computations used by the renderer.
//   - selectBandColor : utilization -> NORMAL / WARN / EXHAUSTED color
//   - litLedCount     : utilization -> lit-LED count (round-to-nearest, clamped)
//   - countdownLitCount : countdown formula from data-model.md §4
//
// Color macros (NORMAL_QUOTA_COLOR / WARN_QUOTA_COLOR / EXHAUSTED_QUOTA_COLOR)
// are sourced from include/config.h on device. For the native unit-test build
// the test TU is expected to provide them (or include a stub) before including
// this header — see test/test_logic/test_color_band.cpp.
#pragma once

#include <stdint.h>

#ifdef UNIT_TEST
#include "../test/test_logic/mocks/CRGB.h"
#else
#include <FastLED.h>
#endif

struct PollSnapshot;  // forward decl; full type in DisplayController.h

namespace Animations {

// Returns the band color for `utilization`, per data-model.md §5.
// Boundaries are operator-configurable via WARN_THRESHOLD_PERCENT and
// EXHAUSTED_THRESHOLD_PERCENT (defaults 70 / 95, both inclusive into WARN).
CRGB selectBandColor(float utilization);

// Returns the number of LEDs that should be lit for `utilization` (percent) on
// a strip of `numLed` pixels, using round-to-nearest and clamping to
// [0, numLed]. Values >= 100 saturate at numLed.
uint16_t litLedCount(float utilization, uint16_t numLed);

// Countdown formula (FR-016 + data-model.md §4).
//
// effective_minutes = max(0, snap.reading.remaining_minutes
//                         - (now_ms - snap.received_at_ms) / 60000)
// returns clamp(round(effective_minutes * numLed
//                     / max(1, snap.reading.remaining_minutes)),
//               0, numLed)
//
// Edge cases:
//   - snap.reading.remaining_minutes == 0 -> always returns 0.
//   - now_ms before snap.received_at_ms is treated as zero elapsed.
//   - Elapsed beyond the original window returns 0.
uint16_t countdownLitCount(const PollSnapshot& snap,
                           uint32_t            now_ms,
                           uint16_t            numLed);

}  // namespace Animations
