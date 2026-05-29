# Data Model: Claude Usage LED Indicator

The system has no persistent storage. The data model below describes the in-flight shapes that cross the boundaries between the upstream Claude service, the API server, the firmware, and the LED strip.

---

## 1. Upstream usage payload (consumed by the API server only)

Source: `GET https://api.anthropic.com/api/oauth/usage` (the same endpoint the reference `claude-usage-node.js` script hits, with Bearer token from `~/.claude/.credentials.json`).

Shape (only the fields the server reads — others are ignored):

```jsonc
{
  "five_hour": {
    "utilization": 42.3,        // number, percent of the 5-hour window already consumed; may exceed 100 during overage
    "resets_at": "2026-05-21T22:15:00Z"  // ISO 8601 UTC instant when the 5-hour window rolls over
  }
  // ...other fields the server does not consume...
}
```

**Validation rules** (server-side, before responding to the firmware):
- `five_hour` must be present and an object.
- `five_hour.utilization` must be a finite number ≥ 0. The server clamps the value forwarded to the firmware to ≥ 0; values above 100 are passed through unchanged (the firmware caps display at 100% itself per FR-008 + edge case).
- `five_hour.resets_at` must parse to a valid `Date`. The server treats unparseable values as upstream failure (returns 5xx).

If any rule fails, or if the upstream HTTP call itself fails (network error, non-2xx, malformed JSON), the server returns a 5xx response. This drives the firmware into the error state (FR-011).

**Token auto-refresh.** The OAuth access token in `~/.claude/.credentials.json` lives only ~8 hours. Rather than letting an expired token surface as a 5xx (the "502 every morning after the device is powered off overnight" symptom), `claudeClient.js` refreshes it automatically: before each upstream call it checks `expiresAt`, and if the token is expired or within 5 minutes of expiry it exchanges the stored `refreshToken` at `https://console.anthropic.com/v1/oauth/token` (Claude Code's public OAuth client), then writes the rotated credentials back to the file atomically — preserving every other field. The upstream's refresh tokens are single-use/rotating, so the *new* `refreshToken` must be persisted (it is). A 401 from the usage endpoint also forces one refresh + retry to cover clock skew or early revocation. Only an unrecoverable auth failure (no refresh token, or the refresh grant itself rejected) still produces a 5xx.

---

## 2. Minimal usage payload (API server → firmware)

This is the *only* data shape the firmware is allowed to depend on (FR-007).

```jsonc
{
  "utilization": 42.3,        // number, percent; may be > 100 during overage
  "remaining_minutes": 187    // integer minutes, ≥ 0; floor((resets_at - now) / 60s), clamped at 0
}
```

**Validation rules** (firmware-side):
- `utilization` must be a finite number ≥ 0. Values > 100 are accepted and clamped to 100 for the proportional fill (FR-008); they still engage the countdown (FR-016).
- `remaining_minutes` must be an integer ≥ 0. A value of 0 with `utilization ≥ 100` means the strip is fully lit — the reset-reached terminal state of the countdown.
- Any field missing, wrong type, or out-of-range triggers the error state (FR-011) — same outcome as a network failure.

**Lifecycle**: every poll cycle constructs a fresh payload server-side; the firmware never caches across polls except via the `last_poll` snapshot in §4 below.

---

## 3. ESP32 configuration (compile-time, `include/config.h`)

The full set of operator-set values. The values named in FR-013 are read from a config file; `WIFI_SSID` / `WIFI_PASSWORD` (network credentials), `LED_BRIGHTNESS` (post-implementation addition, see research §19), and `SHOW_THRESHOLD_MARKERS` (post-implementation addition, see research §20) are plan-level additions not named in the spec.

| Field | Type | Required | Default | Notes |
|---|---|---|---|---|
| `WIFI_SSID` | C string | yes | — | Local-network SSID. |
| `WIFI_PASSWORD` | C string | yes | — | WPA2 passphrase. |
| `API_ENDPOINT` | C string (full URL) | yes | — | e.g. `"http://192.168.1.42:3000/five-hour"`. |
| `GET_USAGE_INTERVAL_MS` | `uint32_t` | yes | `300000` (5 min) | Minimum 10 s in code to prevent abusive polling. |
| `NUM_OF_LED` | `uint16_t` | yes | — | Number of WS2812B pixels on the strip. |
| `LED_BRIGHTNESS` | `uint8_t` (0–255) | yes | `50` | Global brightness scaler applied to every pixel via `FastLED.setBrightness()` at init. Dims all bands and animations proportionally; lower values also cut peak current draw. |
| `WARN_THRESHOLD_PERCENT` | `uint8_t` (0–100) | yes | `70` | Utilization at which the band moves from NORMAL into WARN. |
| `EXHAUSTED_THRESHOLD_PERCENT` | `uint8_t` (0–100) | yes | `90` | Utilization at or above which the band becomes EXHAUSTED. Must satisfy `WARN_THRESHOLD_PERCENT ≤ EXHAUSTED_THRESHOLD_PERCENT ≤ 100`. |
| `SHOW_THRESHOLD_MARKERS` | `0` / `1` | yes | `1` | When `1`, the USAGE display draws a `WARN_QUOTA_COLOR` tick at the warn-threshold position and an `EXHAUSTED_QUOTA_COLOR` tick at the exhausted-threshold position (LED index `litLedCount(threshold) - 1`), but only on the not-yet-filled portion of the strip — a tick disappears once the fill reaches it. USAGE mode only; `0` disables. See research §20. |
| `NORMAL_QUOTA_COLOR` | `CRGB` literal | yes | `CRGB(0x00,0xCC,0x00)` (green) | Color for utilization `< WARN_THRESHOLD_PERCENT`. |
| `WARN_QUOTA_COLOR` | `CRGB` literal | yes | `CRGB(0xFF,0xA5,0x00)` (yellow/amber) | Color for utilization in the warn band `[WARN_THRESHOLD_PERCENT, EXHAUSTED_THRESHOLD_PERCENT)` (lower bound inclusive, upper exclusive). |
| `EXHAUSTED_QUOTA_COLOR` | `CRGB` literal | yes | `CRGB(0xCC,0x00,0x00)` (red) | Color for utilization `> EXHAUSTED_THRESHOLD_PERCENT`. Also reused by the exhausted-state countdown. |
| `ERROR_COLOR` | `CRGB` literal | yes | `CRGB(0xCC,0x00,0x00)` (red) | Color used by the failure blink (FR-011). Operator may pick something distinct from the exhausted band to differentiate "we cannot tell you" from "you are at 100%". |
| `STARTUP_COLOR` | `CRGB` literal | yes | `CRGB(0xFF,0xFF,0xFF)` (white) | Color of the chasing-comet animation shown before the first successful poll (FR-010). The 3-LED comet's brightness ramp (255 / 128 / 64) is applied on top of this color. |
| `COUNTDOWN_COLOR` | `CRGB` literal | yes | `CRGB(0x00,0x00,0xCC)` (blue) | Color of the exhausted reset-countdown bar (FR-016). Distinct from the red exhausted band so "out of quota, waiting for reset" reads differently from the in-window red fill. |

Config is read at compile time only. Changes require re-flash, which the spec accepts.

---

## 4. Firmware runtime state (in-memory only)

The firmware's state machine tracks exactly one display mode plus the snapshot from the last successful poll.

```cpp
enum class DisplayMode {
    STARTUP,    // White chase; entered at boot; exits on first successful Reading.
    USAGE,      // Proportional fill in band color; also held for one poll at the first
                // exhausted Reading so the full-red bar confirms "quota hit" before the
                // mode flips to COUNTDOWN.
    COUNTDOWN,  // Reset-countdown fill; entered on the second consecutive Reading
                // with utilization >= 100 %.
    ERROR       // Blinking red; entered after kErrorFailureThreshold consecutive failed polls.
};

struct Reading {
    float    utilization;        // percent; may exceed 100
    uint32_t remaining_minutes;  // minutes
};

struct PollSnapshot {
    Reading  reading;
    uint32_t received_at_ms;     // millis() at the moment the response was parsed
};

struct DisplayState {
    DisplayMode  mode;
    PollSnapshot last_poll;            // only valid in USAGE or COUNTDOWN
    uint8_t      consecutive_failures; // failed polls since the last success; reset to 0 on any success
    bool         exhausted_shown;      // true once the full-red USAGE bar has been
                                       // shown for the current exhaustion episode;
                                       // cleared when utilization drops below 100 %
};
```

**Failure tolerance**: a single failed poll does **not** drive the strip to ERROR. `advance()` increments `consecutive_failures` on each failed poll and only switches to ERROR once it reaches `DisplayController::kErrorFailureThreshold` (default `2`). Below the threshold the previously displayed mode is held, so one slow upstream call (the API server's own timeout is 10 s; the firmware now allows 12 s) or a brief Wi-Fi/reconnect blip no longer flashes red for a whole poll interval. Any successful poll resets the counter to 0. This refines — rather than contradicts — FR-011: a *sustained* inability to reach the API still surfaces as ERROR; a lone transient does not.

**One-poll "quota hit" confirmation**: when a successful poll first reports `utilization ≥ 100 %`, the controller holds `USAGE` for that one poll so the full-red proportional bar is visible as the "you just hit 100%" confirmation before the display switches to the blue countdown. The next exhausted poll flips to `COUNTDOWN`; subsequent exhausted polls stay in `COUNTDOWN`. The `exhausted_shown` gate is rearmed (cleared) whenever utilization drops below 100 %, so a later re-exhaustion replays the one-poll red confirmation. The flag is RAM-only — a reboot while still at 100 % therefore also replays the red bar once, by design (see research §22).

### State transitions

| From | Event | To | Notes |
|---|---|---|---|
| `STARTUP` | first successful poll, `utilization < 100` | `USAGE` | Strip renders proportional fill. |
| `STARTUP` | first successful poll, `utilization ≥ 100`, `!exhausted_shown` | `USAGE` | Full-red bar shown for one poll; sets `exhausted_shown = true`. |
| `STARTUP` | first poll fails, streak `< kErrorFailureThreshold` | `STARTUP` | Hold the chase; the lone failure is tolerated. |
| `STARTUP` | poll fails, streak reaches `kErrorFailureThreshold` | `ERROR` | Per edge case "First-ever poll fails", once sustained. |
| `USAGE` | successful poll, `utilization < 100` | `USAGE` | New snapshot; redraw fill/color; streak reset to 0; clears `exhausted_shown`. |
| `USAGE` | successful poll, `utilization ≥ 100`, `!exhausted_shown` | `USAGE` | First exhausted poll: hold the full-red bar as quota-hit confirmation; sets `exhausted_shown = true`. |
| `USAGE` | successful poll, `utilization ≥ 100`, `exhausted_shown` | `COUNTDOWN` | Second consecutive exhausted poll: countdown begins from `remaining_minutes`. |
| `USAGE` | failed poll, streak `< kErrorFailureThreshold` | `USAGE` | Hold last fill; transient failure tolerated. |
| `USAGE` | failed poll, streak reaches `kErrorFailureThreshold` | `ERROR` | Last snapshot is discarded for display purposes. |
| `COUNTDOWN` | successful poll, `utilization < 100` | `USAGE` | Countdown abandoned; redraw normal fill; streak reset to 0; clears `exhausted_shown`. |
| `COUNTDOWN` | successful poll, `utilization ≥ 100` | `COUNTDOWN` | Resync local timer to new `remaining_minutes`. |
| `COUNTDOWN` | locally tracked minutes reach 0, no new poll | `COUNTDOWN` | Strip stays fully lit until next poll updates state. |
| `COUNTDOWN` | failed poll, streak `< kErrorFailureThreshold` | `COUNTDOWN` | Hold countdown; transient failure tolerated. |
| `COUNTDOWN` | failed poll, streak reaches `kErrorFailureThreshold` | `ERROR` | Countdown abandoned. |
| `ERROR` | successful poll | `USAGE` or `COUNTDOWN` | Per FR-012; resumes immediately in the same cycle; streak reset to 0. |
| `ERROR` | failed poll | `ERROR` | Continues blinking. |

### Locally tracked remaining minutes (COUNTDOWN only)

At any moment the *effective* remaining minutes is:

```text
elapsed_ms        = millis() - last_poll.received_at_ms
elapsed_minutes   = elapsed_ms / 60000
effective_minutes = clamp(max(0, last_poll.reading.remaining_minutes - elapsed_minutes), 0, 300)
```

The countdown bar **fills toward the reset**, measured against the fixed five-hour (300-minute) window:

```text
lit = round((300 - effective_minutes) / 300 * NUM_OF_LED)   clamped to [0, NUM_OF_LED]
```

So the strip is empty when a full window remains (`effective_minutes == 300`) and fully lit at reset (`effective_minutes == 0`), gaining one LED per `300 * 60_000 / NUM_OF_LED` ms. The 300-minute reference is the `Animations::kFiveHourWindowMinutes` constant — deliberately **not** the remaining-minutes value at poll time (which the earlier draining formula used as its denominator).

---

## 5. Color-band selection (deterministic, no state)

Given `utilization` (float, percent), the band color is:

| Range | Color used |
|---|---|
| `utilization < WARN_THRESHOLD_PERCENT` | `NORMAL_QUOTA_COLOR` |
| `WARN_THRESHOLD_PERCENT ≤ utilization < EXHAUSTED_THRESHOLD_PERCENT` | `WARN_QUOTA_COLOR` |
| `utilization ≥ EXHAUSTED_THRESHOLD_PERCENT` | `EXHAUSTED_QUOTA_COLOR` |

The bands are half-open: each includes its lower bound and excludes its upper, so the EXHAUSTED band begins exactly at `EXHAUSTED_THRESHOLD_PERCENT` — the same point as the red threshold marker (§3, research §20). Defaults are `70` and `90`. Operators may tighten or relax the bands by editing `config.h`; the FR-016 countdown trigger at `utilization ≥ 100` is independent of these thresholds.

---

## 6. What is NOT stored

- No persistence across reboots (no NVS, no SPIFFS).
- No multi-poll history, no rolling average.
- No upstream usage fields other than the two listed in §1.
- No user identity, no auth tokens, no PII anywhere on the firmware side. The upstream OAuth token never leaves the host running the API server.
