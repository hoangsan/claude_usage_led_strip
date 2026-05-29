#ifndef UNIT_TEST

#include "LedRenderer.h"

#include <FastLED.h>

#include "../include/config.h"

namespace LedRenderer {

namespace {
CRGB*    g_leds    = nullptr;
uint16_t g_numLed  = 0;
}  // namespace

void begin(uint16_t numLed, uint8_t /*dataPin*/) {
    g_numLed = numLed;
    g_leds   = new CRGB[numLed];
    for (uint16_t i = 0; i < numLed; ++i) g_leds[i] = CRGB::Black;

    // FastLED's pin is a template parameter, so we use the compile-time
    // LED_DATA_PIN macro from config.h. (The dataPin argument is reserved
    // for a future runtime-pin variant.)
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(g_leds, numLed);
    FastLED.setBrightness(LED_BRIGHTNESS);
    FastLED.show();
}

void clear() {
    if (!g_leds) return;
    for (uint16_t i = 0; i < g_numLed; ++i) g_leds[i] = CRGB::Black;
}

void show() {
    FastLED.show();
}

namespace {
// Fill the buffer (no show): leds[0 .. litCount-1] = color, rest Black.
void fillBuffer(uint16_t litCount, CRGB color, uint16_t numLed) {
    if (litCount > numLed) litCount = numLed;
    for (uint16_t i = 0; i < numLed; ++i) {
        g_leds[i] = (i < litCount) ? color : CRGB::Black;
    }
}
}  // namespace

void renderProportionalFill(uint16_t litCount, CRGB color, uint16_t numLed) {
    if (!g_leds) return;
    fillBuffer(litCount, color, numLed);
    FastLED.show();
}

void renderProportionalFillWithMarkers(uint16_t               litCount,
                                       CRGB                   fillColor,
                                       const ThresholdMarker* markers,
                                       uint8_t                markerCount,
                                       uint16_t               numLed) {
    if (!g_leds) return;
    if (litCount > numLed) litCount = numLed;
    fillBuffer(litCount, fillColor, numLed);
    // Draw each tick only where it sits on the not-yet-filled background; a
    // marker the fill has reached is left as fill color so it disappears into
    // the bar (e.g. no stray yellow warn pixel inside a red fill).
    for (uint8_t m = 0; m < markerCount; ++m) {
        if (markers[m].index >= litCount && markers[m].index < numLed)
            g_leds[markers[m].index] = markers[m].color;
    }
    FastLED.show();
}

void renderChase(uint32_t frame_idx, CRGB color, uint16_t numLed) {
    if (!g_leds || numLed == 0) return;
    for (uint16_t i = 0; i < numLed; ++i) g_leds[i] = CRGB::Black;

    // 3-LED comet: head is the brightest, tail decays 255 / 128 / 64.
    // The brightness ramp is applied on top of `color` so the comet glows in
    // the configured hue (white by default when STARTUP_COLOR is the default).
    const uint8_t ramp[3] = {255, 128, 64};
    const uint16_t head   = static_cast<uint16_t>(frame_idx % numLed);
    for (uint8_t k = 0; k < 3; ++k) {
        // Negative trail position with wraparound on the unsigned ring.
        const uint16_t pos =
            static_cast<uint16_t>((head + numLed - k) % numLed);
        CRGB c = color;
        c.nscale8_video(ramp[k]);
        g_leds[pos] = c;
    }
    FastLED.show();
}

void renderBlink(uint32_t now_ms, CRGB color, uint16_t numLed) {
    if (!g_leds) return;
    const bool on = ((now_ms / 500UL) % 2UL) == 0UL;
    const CRGB c  = on ? color : CRGB::Black;
    for (uint16_t i = 0; i < numLed; ++i) g_leds[i] = c;
    FastLED.show();
}

}  // namespace LedRenderer

#endif  // !UNIT_TEST
