// Operator template: copy this file to `config.h` and fill in your local
// values. `config.h` is *not* committed to the repository — keep it that way.
#pragma once

#include <FastLED.h>

// --- Network ---------------------------------------------------------------

#define WIFI_SSID       "your-ssid"
#define WIFI_PASSWORD   "your-passphrase"

// Full URL to the local API server's /five-hour endpoint.
#define API_ENDPOINT    "http://192.168.1.42:3000/five-hour"

// --- Polling cadence -------------------------------------------------------

// How often to poll the API server, in milliseconds. Default 5 minutes.
// Do NOT set below 10000 (10 s) — see include/README.md.
#define GET_USAGE_INTERVAL_MS 300000

// --- LED strip --------------------------------------------------------------

// Number of WS2812B pixels on the strip.
#define NUM_OF_LED      160

// Data pin driving the strip's DIN line.
#define LED_DATA_PIN    5

// Global brightness scaler applied to every pixel (0 = off, 255 = full).
// Lower this to dim the whole strip; the band colors and animations above
// are scaled proportionally. 50 is a comfortable indoor level.
#define LED_BRIGHTNESS  50

// --- Quota band thresholds (percent of the five-hour window used) ----------
// Boundaries between the three color bands. Half-open intervals: the warning
// band includes its lower bound and excludes its upper, so EXHAUSTED begins
// exactly at EXHAUSTED_THRESHOLD_PERCENT (where the red threshold marker sits):
//   utilization  <  WARN_THRESHOLD_PERCENT                       -> NORMAL
//   WARN_THRESHOLD_PERCENT <= util <  EXHAUSTED_THRESHOLD_PERCENT -> WARN
//   utilization  >= EXHAUSTED_THRESHOLD_PERCENT                  -> EXHAUSTED
// Constraint: 0 < WARN_THRESHOLD_PERCENT <= EXHAUSTED_THRESHOLD_PERCENT <= 100.

#define WARN_THRESHOLD_PERCENT       70
#define EXHAUSTED_THRESHOLD_PERCENT  90

// Set to 1 to paint a persistent tick at each threshold during the usage
// display: a WARN_QUOTA_COLOR pixel at WARN_THRESHOLD_PERCENT and an
// EXHAUSTED_QUOTA_COLOR pixel at EXHAUSTED_THRESHOLD_PERCENT. Set to 0 to
// disable. Each tick shares its band's color and is shown only on the
// not-yet-filled part of the strip, disappearing once the fill reaches it.
// USAGE display only.
#define SHOW_THRESHOLD_MARKERS       1

// --- Quota band colors -----------------------------------------------------

#define NORMAL_QUOTA_COLOR     CRGB(0x00, 0xCC, 0x00)
#define WARN_QUOTA_COLOR       CRGB(0xFF, 0xA5, 0x00)
#define EXHAUSTED_QUOTA_COLOR  CRGB(0xCC, 0x00, 0x00)

// --- State colors ----------------------------------------------------------
// Standalone colors for the three non-usage display states. Defaults match the
// spec ("red" blink, "white" chase, "blue" reset countdown) but can be retuned
// without touching the band colors above.

#define ERROR_COLOR     CRGB(0xCC, 0x00, 0x00)   // failure blink (FR-011)
#define STARTUP_COLOR   CRGB(0xFF, 0xFF, 0xFF)   // chase comet base color (FR-010)
#define COUNTDOWN_COLOR CRGB(0x00, 0x00, 0xCC)   // exhausted reset countdown (FR-016)
