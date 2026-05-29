// Minimal CRGB shim used by the PlatformIO `native` test environment so the
// firmware logic modules can compile on the host without pulling in the real
// FastLED library. Only the subset of the FastLED CRGB API consumed by the
// Animations module is provided.
#pragma once

#include <stdint.h>

struct CRGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    CRGB() : r(0), g(0), b(0) {}
    CRGB(uint8_t rr, uint8_t gg, uint8_t bb) : r(rr), g(gg), b(bb) {}

    bool operator==(const CRGB& o) const {
        return r == o.r && g == o.g && b == o.b;
    }
    bool operator!=(const CRGB& o) const { return !(*this == o); }

    static const CRGB Black;
};
