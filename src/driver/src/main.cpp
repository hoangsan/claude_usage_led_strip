// ESP32 firmware orchestrator for the Claude Usage LED Indicator.
//   setup() — Wi-Fi join + FastLED init + state-machine seed.
//   loop()  — non-blocking: polls the API every GET_USAGE_INTERVAL_MS and
//             ticks the renderer every frame against millis().
//
// All time-sensitive work uses millis() deltas; we never delay() longer than
// a few ms. The render branches map 1:1 to DisplayMode values.

#include <Arduino.h>
#include <WiFi.h>

#include "../include/config.h"

#include "Animations.h"
#include "DisplayController.h"
#include "LedRenderer.h"
#include "UsageClient.h"

namespace {

constexpr uint32_t kFrameIntervalMs = 33;   // ~30 fps render cadence
constexpr uint32_t kChaseStepMs     = 60;   // research.md §9
constexpr uint32_t kWifiRetryMs     = 500;
constexpr uint8_t  kWifiMaxRetries  = 40;   // ~20 s total
// Floor on the visible STARTUP chase. If Wi-Fi joins faster than this, the
// loop keeps rendering chase frames until millis() reaches this mark before
// firing the first poll — otherwise the chase would flash by in <100 ms.
constexpr uint32_t kStartupMinMs    = 1500;

DisplayState g_state = { DisplayMode::STARTUP, {} };

uint32_t g_lastPollMs   = 0;
bool     g_firstPollDue = true;  // poll immediately at boot
uint32_t g_lastFrameMs  = 0;
uint32_t g_chaseFrame   = 0;
uint32_t g_lastChaseMs  = 0;

void tickStartupFrame(uint32_t now_ms) {
    if (now_ms - g_lastChaseMs >= kChaseStepMs) {
        g_lastChaseMs = now_ms;
        ++g_chaseFrame;
    }
    if (now_ms - g_lastFrameMs >= kFrameIntervalMs) {
        g_lastFrameMs = now_ms;
        LedRenderer::renderChase(g_chaseFrame, STARTUP_COLOR, NUM_OF_LED);
    }
}

void connectWifi() {
    Serial.printf("[wifi] connecting to %s...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint8_t tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < kWifiMaxRetries) {
        // Render chase frames while waiting so the strip is alive during join.
        const uint32_t retryUntil = millis() + kWifiRetryMs;
        while (static_cast<int32_t>(retryUntil - millis()) > 0) {
            tickStartupFrame(millis());
            delay(1);
        }
        Serial.print(".");
        ++tries;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[wifi] connected, ip=%s\n",
                      WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[wifi] connect failed; will retry implicitly via poll");
    }
}

void doPoll(uint32_t now_ms) {
    Reading r{};
    String  err;
    const bool ok = UsageClient::fetch(r, err);
    if (ok) {
        Serial.printf("[http] GET %s -> 200 util=%.2f remaining=%lu\n",
                      API_ENDPOINT, r.utilization,
                      static_cast<unsigned long>(r.remaining_minutes));
    } else {
        Serial.printf("[http] poll failed: %s\n", err.c_str());
    }
    DisplayController::advance(g_state, ok, r, now_ms);

    switch (g_state.mode) {
        case DisplayMode::STARTUP:   Serial.println("[display] STARTUP");   break;
        case DisplayMode::USAGE:     Serial.println("[display] USAGE");     break;
        case DisplayMode::COUNTDOWN: Serial.println("[display] COUNTDOWN"); break;
        case DisplayMode::ERROR:     Serial.println("[display] ERROR");     break;
    }
}

void render(const DisplayState& st, uint32_t now_ms) {
    switch (st.mode) {
        case DisplayMode::STARTUP: {
            // Chase ticks at 60 ms/LED regardless of frame cadence.
            if (now_ms - g_lastChaseMs >= kChaseStepMs) {
                g_lastChaseMs = now_ms;
                ++g_chaseFrame;
            }
            LedRenderer::renderChase(g_chaseFrame, STARTUP_COLOR, NUM_OF_LED);
            break;
        }
        case DisplayMode::USAGE: {
            const float util = st.last_poll.reading.utilization;
            LedRenderer::renderProportionalFill(
                Animations::litLedCount(util, NUM_OF_LED),
                Animations::selectBandColor(util),
                NUM_OF_LED);
            break;
        }
        case DisplayMode::COUNTDOWN: {
            LedRenderer::renderProportionalFill(
                Animations::countdownLitCount(st.last_poll, now_ms, NUM_OF_LED),
                EXHAUSTED_QUOTA_COLOR,
                NUM_OF_LED);
            break;
        }
        case DisplayMode::ERROR: {
            LedRenderer::renderBlink(now_ms, ERROR_COLOR, NUM_OF_LED);
            break;
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("[boot] claude_usage_led");

    // Bring the strip up BEFORE Wi-Fi join so the chase animation is visible
    // throughout the join phase (which can take up to ~20 s) and the first
    // poll. Without this the strip is dark until the first successful poll
    // and the user never sees STARTUP.
    LedRenderer::begin(NUM_OF_LED, LED_DATA_PIN);
    g_state.mode      = DisplayMode::STARTUP;
    g_state.last_poll = PollSnapshot{};
    Serial.println("[display] STARTUP");

    connectWifi();

    g_lastPollMs   = millis();
    g_firstPollDue = true;
}

void loop() {
    const uint32_t now_ms = millis();

    // ---- Poll cadence ----------------------------------------------------
    // First poll is gated by kStartupMinMs so the chase animation is visible
    // for at least that long even when Wi-Fi joined instantly (cached AP).
    const bool firstPollReady =
        g_firstPollDue && now_ms >= kStartupMinMs;
    const bool nextPollDue =
        !g_firstPollDue &&
        (now_ms - g_lastPollMs) >= static_cast<uint32_t>(GET_USAGE_INTERVAL_MS);
    if (firstPollReady || nextPollDue) {
        g_firstPollDue = false;
        g_lastPollMs   = now_ms;
        doPoll(now_ms);
    }

    // ---- Frame cadence (~30 fps) ----------------------------------------
    if ((now_ms - g_lastFrameMs) >= kFrameIntervalMs) {
        g_lastFrameMs = now_ms;
        render(g_state, now_ms);
    }

    // Yield to FreeRTOS so Wi-Fi tasks can run; ~1 ms is plenty.
    delay(1);
}
