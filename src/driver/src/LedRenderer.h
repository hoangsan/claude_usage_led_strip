// LedRenderer: thin wrapper around FastLED with a dynamically allocated CRGB
// buffer sized at runtime from NUM_OF_LED. Render methods are stateless and
// take the strip-length explicitly so the kernel logic stays decoupled from
// the global config macro.
#pragma once

#include <stdint.h>

#ifdef UNIT_TEST
#include "../test/test_logic/mocks/CRGB.h"
#else
#include <FastLED.h>
#endif

namespace LedRenderer {

// Allocates the internal CRGB buffer (numLed pixels) and initialises FastLED
// against `dataPin`. Safe to call exactly once at boot. `dataPin` matches
// LED_DATA_PIN in config.h and is consumed at compile time by FastLED's
// addLeds<> template, so this argument is presently ignored — kept in the
// signature so a future refactor can swap chipsets at runtime.
void begin(uint16_t numLed, uint8_t dataPin);

// Blanks the internal buffer (does NOT push to the strip).
void clear();

// Pushes the current internal buffer to the strip.
void show();

// --- Render primitives -----------------------------------------------------

// A single threshold tick: paint `color` at pixel `index`, on top of the fill.
struct ThresholdMarker {
    uint16_t index;
    CRGB     color;
};

// Light leds[0 .. litCount-1] in `color`, blacking the rest, then show().
// Reused for both USAGE proportional fill (US1) and COUNTDOWN (US4).
void renderProportionalFill(uint16_t litCount, CRGB color, uint16_t numLed);

// As renderProportionalFill, but after filling, draws each marker pixel in its
// own color — only where the pixel is not yet filled (index >= litCount), i.e.
// on the unlit background — before a single show(). A marker the fill has
// already reached is left as fill color, so it disappears into the bar. Markers
// with index >= numLed are also skipped. Used by USAGE to mark the band
// boundaries (e.g. a yellow tick at the warn threshold, red at exhausted) as
// "upcoming threshold" ticks ahead of the fill.
void renderProportionalFillWithMarkers(uint16_t                litCount,
                                       CRGB                    fillColor,
                                       const ThresholdMarker*  markers,
                                       uint8_t                 markerCount,
                                       uint16_t                numLed);

// A 3-LED comet at position (frame_idx % numLed) using `color` as the base
// hue, with brightness ramp 255 / 128 / 64 applied on top; all other pixels
// black. Wraps at strip ends. Calls show(). Pass STARTUP_COLOR for the
// default white comet behavior.
void renderChase(uint32_t frame_idx, CRGB color, uint16_t numLed);

// Square-wave blink: every 500 ms, the whole strip toggles between `color`
// (on phase) and CRGB::Black (off phase). Calls show().
void renderBlink(uint32_t now_ms, CRGB color, uint16_t numLed);

}  // namespace LedRenderer
