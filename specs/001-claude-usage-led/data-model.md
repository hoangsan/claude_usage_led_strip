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

If any rule fails, or if the upstream HTTP call itself fails (network error, non-2xx, malformed JSON, expired token), the server returns a 5xx response. This drives the firmware into the error state (FR-011).

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
- `remaining_minutes` must be an integer ≥ 0. A value of 0 with `utilization ≥ 100` means the strip immediately enters the all-off terminal state of the countdown.
- Any field missing, wrong type, or out-of-range triggers the error state (FR-011) — same outcome as a network failure.

**Lifecycle**: every poll cycle constructs a fresh payload server-side; the firmware never caches across polls except via the `last_poll` snapshot in §4 below.

---

## 3. ESP32 configuration (compile-time, `include/config.h`)

The full set of operator-set values. Per FR-013 the six listed values are read from a config file; `WIFI_SSID` / `WIFI_PASSWORD` are plan-level additions (the firmware cannot reach the network without them and the spec is silent on provisioning).

| Field | Type | Required | Default | Notes |
|---|---|---|---|---|
| `WIFI_SSID` | C string | yes | — | Local-network SSID. |
| `WIFI_PASSWORD` | C string | yes | — | WPA2 passphrase. |
| `API_ENDPOINT` | C string (full URL) | yes | — | e.g. `"http://192.168.1.42:3000/five-hour"`. |
| `GET_USAGE_INTERVAL_MS` | `uint32_t` | yes | `300000` (5 min) | Minimum 10 s in code to prevent abusive polling. |
| `NUM_OF_LED` | `uint16_t` | yes | — | Number of WS2812B pixels on the strip. |
| `WARN_THRESHOLD_PERCENT` | `uint8_t` (0–100) | yes | `70` | Utilization at which the band moves from NORMAL into WARN. |
| `EXHAUSTED_THRESHOLD_PERCENT` | `uint8_t` (0–100) | yes | `95` | Utilization above which the band becomes EXHAUSTED. Must satisfy `WARN_THRESHOLD_PERCENT ≤ EXHAUSTED_THRESHOLD_PERCENT ≤ 100`. |
| `NORMAL_QUOTA_COLOR` | `CRGB` literal | yes | `CRGB(0x00,0xCC,0x00)` (green) | Color for utilization `< WARN_THRESHOLD_PERCENT`. |
| `WARN_QUOTA_COLOR` | `CRGB` literal | yes | `CRGB(0xFF,0xA5,0x00)` (yellow/amber) | Color for utilization inside the warn band (inclusive on both ends). |
| `EXHAUSTED_QUOTA_COLOR` | `CRGB` literal | yes | `CRGB(0xCC,0x00,0x00)` (red) | Color for utilization `> EXHAUSTED_THRESHOLD_PERCENT`. Also reused by the exhausted-state countdown. |
| `ERROR_COLOR` | `CRGB` literal | yes | `CRGB(0xCC,0x00,0x00)` (red) | Color used by the failure blink (FR-011). Operator may pick something distinct from the exhausted band to differentiate "we cannot tell you" from "you are at 100%". |
| `STARTUP_COLOR` | `CRGB` literal | yes | `CRGB(0xFF,0xFF,0xFF)` (white) | Color of the chasing-comet animation shown before the first successful poll (FR-010). The 3-LED comet's brightness ramp (255 / 128 / 64) is applied on top of this color. |

Config is read at compile time only. Changes require re-flash, which the spec accepts.

---

## 4. Firmware runtime state (in-memory only)

The firmware's state machine tracks exactly one display mode plus the snapshot from the last successful poll.

```cpp
enum class DisplayMode {
    STARTUP,    // White chase; entered at boot; exits on first successful Reading.
    USAGE,      // Proportional fill in band color; utilization < 100 %.
    COUNTDOWN,  // Drain animation; entered when a Reading reports utilization >= 100 %.
    ERROR       // Blinking red; entered on any failed poll (network or validation).
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
    PollSnapshot last_poll;       // only valid in USAGE or COUNTDOWN
};
```

### State transitions

| From | Event | To | Notes |
|---|---|---|---|
| `STARTUP` | first successful poll, `utilization < 100` | `USAGE` | Strip renders proportional fill. |
| `STARTUP` | first successful poll, `utilization ≥ 100` | `COUNTDOWN` | Strip starts fully lit; countdown begins. |
| `STARTUP` | first poll fails | `ERROR` | Per edge case "First-ever poll fails". |
| `USAGE` | successful poll, `utilization < 100` | `USAGE` | New snapshot; redraw fill/color. |
| `USAGE` | successful poll, `utilization ≥ 100` | `COUNTDOWN` | New snapshot; countdown begins from `remaining_minutes`. |
| `USAGE` | failed poll | `ERROR` | Last snapshot is discarded for display purposes. |
| `COUNTDOWN` | successful poll, `utilization < 100` | `USAGE` | Countdown abandoned; redraw normal fill. |
| `COUNTDOWN` | successful poll, `utilization ≥ 100` | `COUNTDOWN` | Resync local timer to new `remaining_minutes`. |
| `COUNTDOWN` | locally tracked minutes reach 0, no new poll | `COUNTDOWN` | Strip stays all-off until next poll updates state. |
| `COUNTDOWN` | failed poll | `ERROR` | Countdown abandoned. |
| `ERROR` | successful poll | `USAGE` or `COUNTDOWN` | Per FR-012; resumes immediately in the same cycle. |
| `ERROR` | failed poll | `ERROR` | Continues blinking. |

### Locally tracked remaining minutes (COUNTDOWN only)

At any moment the *effective* remaining minutes is:

```text
elapsed_ms        = millis() - last_poll.received_at_ms
elapsed_minutes   = elapsed_ms / 60000
effective_minutes = max(0, last_poll.reading.remaining_minutes - elapsed_minutes)
```

The lit-LED count during the countdown is `round(effective_minutes / minutes_per_led * NUM_OF_LED)` clamped to `[0, NUM_OF_LED]`, where `minutes_per_led = last_poll.reading.remaining_minutes / NUM_OF_LED`. Equivalently, the strip starts at `NUM_OF_LED` lit and decrements one LED per `(remaining_minutes_at_poll * 60_000) / NUM_OF_LED` ms.

---

## 5. Color-band selection (deterministic, no state)

Given `utilization` (float, percent), the band color is:

| Range | Color used |
|---|---|
| `utilization < WARN_THRESHOLD_PERCENT` | `NORMAL_QUOTA_COLOR` |
| `WARN_THRESHOLD_PERCENT ≤ utilization ≤ EXHAUSTED_THRESHOLD_PERCENT` | `WARN_QUOTA_COLOR` |
| `utilization > EXHAUSTED_THRESHOLD_PERCENT` | `EXHAUSTED_QUOTA_COLOR` |

Both threshold boundaries are inclusive into the warning band. Defaults are `70` and `95` so out-of-the-box behavior matches the spec's canonical split. Operators may tighten or relax the bands by editing `config.h`; the FR-016 countdown trigger at `utilization ≥ 100` is independent of these thresholds.

---

## 6. What is NOT stored

- No persistence across reboots (no NVS, no SPIFFS).
- No multi-poll history, no rolling average.
- No upstream usage fields other than the two listed in §1.
- No user identity, no auth tokens, no PII anywhere on the firmware side. The upstream OAuth token never leaves the host running the API server.
