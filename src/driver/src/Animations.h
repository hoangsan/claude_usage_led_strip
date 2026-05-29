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
// Half-open bands (inclusive lower bound, exclusive upper):
//   util < WARN -> NORMAL; WARN <= util < EXHAUSTED -> WARN; util >= EXHAUSTED
//   -> EXHAUSTED. Boundaries are operator-configurable via WARN_THRESHOLD_PERCENT
// and EXHAUSTED_THRESHOLD_PERCENT (defaults 70 / 95).
CRGB selectBandColor(float utilization);

// Returns the number of LEDs that should be lit for `utilization` (percent) on
// a strip of `numLed` pixels, using round-to-nearest and clamping to
// [0, numLed]. Values >= 100 saturate at numLed.
uint16_t litLedCount(float utilization, uint16_t numLed);

// Returns the LED index of the threshold marker for `thresholdPercent` on a
// strip of `numLed` pixels: the topmost LED that would be lit at exactly that
// utilization, i.e. litLedCount(thresholdPercent, numLed) - 1. This places a
// tick at the band boundary (e.g. 70% -> index 111 on a 160-LED strip). The
// result is always a valid index in [0, numLed-1] for numLed > 0.
uint16_t markerLedIndex(float thresholdPercent, uint16_t numLed);

// The five-hour quota window, in minutes. The countdown fill is measured
// against this fixed window, NOT the remaining-minutes value at poll time.
constexpr uint32_t kFiveHourWindowMinutes = 5 * 60;  // 300

// Countdown fill (FR-016 + data-model.md §4). The bar GROWS toward the reset:
// empty with a full window remaining, completely full at reset.
//
// effective_minutes = clamp(max(0, remaining_minutes - elapsed_minutes),
//                           0, kFiveHourWindowMinutes)
// returns clamp(round((kFiveHourWindowMinutes - effective_minutes) * numLed
//                     / kFiveHourWindowMinutes), 0, numLed)
//
// where elapsed_minutes = (now_ms - snap.received_at_ms) / 60000.
//
// Edge cases:
//   - remaining_minutes == 0 (reset is now) -> returns numLed (all lit).
//   - now_ms before snap.received_at_ms is treated as zero elapsed.
//   - remaining_minutes >= kFiveHourWindowMinutes -> returns 0 (empty bar).
uint16_t countdownLitCount(const PollSnapshot& snap,
                           uint32_t            now_ms,
                           uint16_t            numLed);

}  // namespace Animations
