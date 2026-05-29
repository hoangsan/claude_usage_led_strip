# Phase 0 Research: Claude Usage LED Indicator

This document records the technology-selection and behavioral-default decisions made during planning. Each topic is recorded as Decision → Rationale → Alternatives.

---

## 1. Node.js HTTP framework for the API server

**Decision**: **Express 4.x**, ES modules, native `fetch` for the upstream call. No additional middleware beyond the one route + `dotenv`.

**Rationale**: The server has exactly one route and is single-tenant on a local network. Express is the lowest-friction option for any maintainer who later picks up the project: ubiquitous, no surprises, no async-flow quirks. Native `fetch` avoids `node-fetch` / `axios` as deps. ES modules let us use `import` consistently with modern Node.

**Alternatives considered**:
- *`node:http` directly*: smallest dependency footprint, but the routing/middleware overhead of writing it longhand exceeds Express's footprint at this scale.
- *Fastify*: faster and slightly nicer types, but its plugin model is overkill for one endpoint and adds onboarding friction.
- *Hono / itty-router*: cute but unfamiliar to most Node developers; no advantage here.

---

## 2. WS2812B driver library on Arduino-ESP32

**Decision**: **FastLED 3.6+**. Use the `WS2812B` chipset binding and a single `CRGB leds[NUM_OF_LED]` buffer.

**Rationale**: FastLED has the most mature WS2812B timing on ESP32 (RMT-based, jitter-free), exposes the `CRGB` type that maps cleanly to a static `#define` in `config.h`, and offers built-in brightness scaling we use for the chase animation. It is widely deployed and easy to debug.

**Alternatives considered**:
- *Adafruit_NeoPixel*: also fine, but FastLED's color types and brightness primitives make the per-LED math simpler.
- *NeoPixelBus*: highest performance, but its API is heavier and offers no benefit at our pixel counts (<300 LEDs).

---

## 3. JSON parsing on the device

**Decision**: **ArduinoJson v7**, with a `JsonDocument` sized to **256 bytes**.

**Rationale**: The minimal payload is two scalar fields. 256 bytes is comfortably above the worst-case (~50 bytes of payload + ArduinoJson overhead). v7 uses dynamic allocation under the hood but caps growth at the document budget. Familiar to most embedded developers.

**Alternatives considered**:
- Hand-rolled string parser: brittle, easy to get wrong, no benefit.
- `cJSON`: works but C-style API is awkward from C++ and offers no win on this payload size.

---

## 4. Firmware configuration delivery

**Decision**: **Compile-time `include/config.h`** (operator copies `config.example.h` and edits values). No NVS, no web-based provisioning UI in v1.

**Rationale**: Spec FR-013 only requires that the six values are read from "a config file", and the Assumptions section accepts a restart-to-apply model. Compile-time config is by far the simplest path: type-safe, no runtime parsing, no flash wear, no extra build steps. The operator already has PlatformIO open to flash firmware, so editing `config.h` adds no friction.

**Alternatives considered**:
- *NVS (Preferences)*: avoids re-flashing for config changes, but introduces a provisioning UX problem (how does the operator set values the first time?). Out of scope for v1.
- *SPIFFS/LittleFS .env file*: extra build step (file-system image), more failure modes, no real benefit over `config.h` at this size.
- *WiFiManager-style captive portal*: substantial code & UX surface for v1.

---

## 5. Color encoding in `config.h`

**Decision**: Use **FastLED `CRGB` literal hex** directly in `config.h`:

```cpp
#define NORMAL_QUOTA_COLOR     CRGB(0x00, 0xCC, 0x00)
#define WARN_QUOTA_COLOR       CRGB(0xFF, 0xA5, 0x00)
#define EXHAUSTED_QUOTA_COLOR  CRGB(0xCC, 0x00, 0x00)
```

**Rationale**: No string parsing on boot, type-checked at compile time, identical syntax to anything an operator would copy from a hex picker (just swap the three byte literals). Defaults satisfy the spec ("green", "yellow", "red") while picking values that are pleasant on a WS2812B at moderate brightness (full `0xFF` on red/green is harsh).

**Alternatives considered**:
- Hex string `"#00CC00"` parsed at boot: small parser, but offers no advantage and introduces an error mode.
- HSV / named-color enum: nicer UX but more code; not needed for three configurable colors.

---

## 6. Wi-Fi credentials

**Decision**: Add **`WIFI_SSID`** and **`WIFI_PASSWORD`** to `config.h` as plan-level configuration values. They are not listed in FR-013 but are an obvious prerequisite for the firmware to reach the API server at all.

**Rationale**: The spec assumes a local-network deployment but does not specify provisioning. Compile-time Wi-Fi credentials are the minimal-fuss option for a v1 hobbyist build: the operator types them once into `config.h` alongside the API endpoint, then never touches them again. WPS / captive portal / BLE provisioning are explicitly out of scope.

**Alternatives considered**:
- WiFiManager: nice UX but adds ~30 KB of flash and a dependency.
- Hardcoded open network: clearly unacceptable.

---

## 7. LED rounding rule: utilization% → lit-LED count

**Decision**: Lit-LED count is computed as `round(utilization * NUM_OF_LED / 100.0)`, clamped to `[0, NUM_OF_LED]`. Utilization above 100% is treated as 100% for fill purposes (full bar), but still engages the countdown (FR-016).

**Rationale**: `round` (nearest) gives the visually best match across the strip — `floor` would visibly "lag" at the 50% midpoint of a small strip (e.g., 51% of 10 LEDs shows 5 LEDs, not 5.1) and `ceil` would visibly "lead" (51% shows 6 LEDs). Round-half-to-even is the default and is fine here. Edge case: 0% utilization with `round` yields 0 LEDs — the spec already accepts this (Edge Cases section: "either zero LEDs lit or a single LED lit").

**Alternatives considered**:
- `floor`: easier to reason about for monotonic fill but visually worse at small strip lengths.
- "Sub-LED" partial brightness (e.g., dim the last LED proportionally): more complex; not required by the spec.

---

## 8. Blink cadence (error state)

**Decision**: Blink period = **1.0 s** (500 ms on, 500 ms off), at the full configured `EXHAUSTED_QUOTA_COLOR` brightness, all LEDs in lockstep.

**Rationale**: 1 Hz is the standard "I am an alarm, not a steady display" rhythm — fast enough to be obvious, slow enough not to look like an animation. Distinct from any state in normal operation (which has no time-varying behavior at the whole-strip level). Satisfies SC-003 ("user can distinguish error from exhausted in at least 9/10 trials").

**Alternatives considered**:
- 2 Hz: more urgent but starts to feel "fault-light" jittery.
- Slow fade in/out: less alarming but easier to confuse with a wakeful idle.

---

## 9. Chase animation (startup state)

**Decision**: A single white "comet" of **3 LEDs** with a trailing brightness ramp (100% / 50% / 25%), advancing **one LED per 60 ms**. Wraps from end to end of the strip.

**Rationale**: At 60 ms/LED, a 30-LED strip completes a sweep in ~1.8 s — fast enough that the user recognizes "this is alive" within SC-004's 3-second budget but slow enough to read individual LEDs. White is unambiguously "not a quota color" (which would confuse the user into thinking they have data).

**Alternatives considered**:
- Solid pulsing white strip: less iconic as a "loading" state — could be confused with a settled "I'm done" display.
- Multi-color rainbow: visually impressive but signals "complete" rather than "waiting" to most users.

---

## 10. Countdown rendering: blue fill toward reset (FR-016)

**Decision**: The exhausted countdown is a **proportional fill in `COUNTDOWN_COLOR` (blue) that grows toward the reset**, not a red bar draining from full. The lit count is `round((kFiveHourWindowMinutes − effective_minutes) / kFiveHourWindowMinutes × NUM_OF_LED)` against the fixed 300-minute window, clamped to `[0, NUM_OF_LED]`, where `effective_minutes` is the locally decremented remaining-minutes value (clamped to the window). Discrete per-LED on/off (no brightness fade); the strip fills from the same fill end as the usage display, reusing `LedRenderer::renderProportionalFill`. Empty with a full window remaining, fully lit at reset.

**Rationale**: *(supersedes the original "drain a full red bar from the end opposite the fill" design.)* Measuring against the fixed five-hour window — rather than the remaining-minutes value at poll time — makes the bar length mean the same thing no matter when the user hit 100%: "half full" always means ~2.5 h to go. The original denominator (`remaining_at_poll`) made every countdown start full and drain over whatever time happened to be left, so bar length was not comparable between sessions. Blue is a distinct, calm "you're out, waiting for reset" signal — clearly different from the red exhausted band (90–100%) and the red error blink. A growing bar reads as "access filling back up." Discrete per-LED steps stay glanceable; SC-007's quartile test still applies, now reading "how full" instead of "how much remains".

**Alternatives considered**:
- *Original draining red bar from the opposite end (superseded)*: started full and emptied; bar length scaled to remaining-at-poll, so not comparable across sessions, and reused the red exhausted color (too close to the error blink).
- *Smooth brightness fade*: visually elegant but breaks the discrete "how full" glance test.
- *Keep the countdown red*: rejected — too easily confused with the exhausted band and the error blink; blue gives the state its own identity.

---

## 11. Polling interval during countdown

**Decision**: The poll interval **does not change** during the countdown — it stays at `GET_USAGE_INTERVAL` (default 5 min). The firmware keeps its locally tracked remaining minutes in sync with the latest poll value.

**Rationale**: Simpler state machine, predictable network load, no risk of hammering the upstream. If a poll lands while the countdown still says "10 minutes left" and the new value disagrees by ~1 minute, the firmware overwrites silently — drift is bounded by the poll interval and the user's quartile-level test in SC-007 tolerates this.

**Alternatives considered**:
- Tighten polling to 1 min once in countdown: marginal UX gain, more network traffic, more code paths.
- Stop polling once countdown has begun: leaves the firmware blind to early window reopening (the spec explicitly requires resuming the proportional fill if utilization drops below 100% mid-countdown).

---

## 12. Five-hour utilization extraction from upstream

**Decision**: The server reads the upstream Claude OAuth usage endpoint exactly as in the reference script (`docs/claude-usage-node.js`), accesses `body.five_hour.utilization` (number percent) and `body.five_hour.resets_at` (ISO 8601 timestamp), then computes `remaining_minutes = max(0, floor((Date.parse(resets_at) - Date.now()) / 60_000))`.

**Rationale**: This matches the existing /usage data shape and keeps the server fully decoupled from the upstream schema's other fields. `floor` (not `round`) is chosen for `remaining_minutes` to avoid ever overstating remaining time (always show "at most this much left"). If `resets_at` is missing/unparseable, the server returns 5xx so the firmware enters the error state.

**Alternatives considered**:
- `ceil`: would overstate remaining time at the boundary by up to a minute, contradicting the conservative posture the spec implies.
- Read `body.five_hour.minutes_until_reset` (if upstream provides one): the upstream schema in the reference script does not document such a field, so we derive it ourselves to avoid assuming.

---

## 13. Server endpoint path

**Decision**: **`GET /five-hour`** on the API server. No auth, no query params. Health check exposed as a side endpoint `GET /health` returning 200 with a static body (optional, not required by the firmware).

**Rationale**: Direct, self-documenting, leaves room for future siblings (`/seven-day`, etc.) without restructuring. The firmware's `API_ENDPOINT` config holds the full URL including the path, so this choice is not load-bearing — operators can override.

**Alternatives considered**:
- `GET /` (root): convenient but loses the descriptive name.
- `GET /v1/five-hour`: versioning is YAGNI at this scope.

---

## 14. Test scoping

**Decision**:
- *Server unit tests* (`node:test`): cover `transform.js` against fixture upstream payloads (normal, missing field, expired credentials shape, overage > 100%). Cover the route handler with an in-process Express app using `node:test` + the `fetch` global.
- *Firmware native tests* (PlatformIO `test_logic`): cover the pure logic — color-band selector, lit-LED count, countdown decrement against a fake monotonic clock. No hardware mocks; no I/O.
- *No on-device automated tests*. End-to-end LED behavior is verified manually via the quickstart steps.

**Rationale**: The valuable failure modes here are arithmetic/boundary (rounding, threshold edges, countdown reaching zero) and JSON shape — both can be exercised with pure-function tests on each side of the wire. The remaining behavior is visual and hard to assert from code without a camera.

**Alternatives considered**:
- Embedded HIL (hardware-in-the-loop) tests: high effort, not justified for a hobbyist-scale project.
- Snapshot-test the WS2812B frame buffer: feasible, but the value comes from visual confirmation, which the quickstart already provides.

---

## 15. Error-detail sanitisation in 5xx responses *(post-implementation hardening)*

**Decision**: The API server's 5xx responses carry **short, stable identifier strings** in their `detail` field (e.g. `credentials_unreadable`, `upstream_timeout`, `upstream_status_401`). The original native error — including filesystem paths, hostnames, DNS errors, and the upstream response body — is kept on `err.cause` and written only to the operator's stderr via `console.error`. The route handler accepts an `errorLog` dependency for testability.

**Rationale**: Spec assumes the API server runs on a trusted LAN, but the threat model widens easily — a guest device, an IoT VLAN, a tethering laptop, a misconfigured firewall. Leaking the operator's home-directory path (`/home/<user>/.claude/...`) in a 502 body to any LAN client is gratuitous PII exposure with no upside. Short codes give the operator everything they need for triage via PM2 logs while making the wire surface flat.

**Alternatives considered**:
- *Leave messages unchanged*: simplest, but the LAN-leak surface keeps growing as more error paths are added.
- *Numeric error codes only (e.g., 5021)*: stable but harder for operators to grep; the string codes are equally machine-friendly while being human-readable in `pm2 logs`.
- *Hide all 5xx detail (return empty `detail`)*: too opaque — operators legitimately need to distinguish "you forgot to set CLAUDE_ENDPOINT" from "your Claude token expired" without digging into stderr.

---

## 16. Upstream-payload cache to bound LAN amplification *(post-implementation hardening)*

**Decision**: The `/five-hour` handler caches the most recent successful upstream payload for `CACHE_TTL_MS` (default 60 000 ms, configurable via `.env`). Within the TTL, requests return the cached upstream payload re-transformed against `Date.now()` — so `remaining_minutes` is always accurate even on a cache hit, while `fetchUsage()` is not called. Cache is **per-process** (no Redis, no shared memory) and busts on any failure.

**Rationale**: The endpoint is unauthenticated on the LAN. Any host that can reach the API server can call `/five-hour` arbitrarily often; absent caching, every such call is proxied straight to the upstream Claude endpoint, which could rate-limit the operator's account or quietly burn their quota. A 60 s cache caps the upstream call rate at ≤ 1 / minute regardless of LAN abuse, while introducing at most 60 s of staleness — well inside SC-002's one-polling-interval budget (default 5 min).

Caching the raw upstream payload (not the minimal payload) preserves accurate `remaining_minutes` because the transform always uses the current wall clock. Caching is opt-out (`CACHE_TTL_MS=0` disables it).

**Alternatives considered**:
- *Rate-limit by source IP (e.g., `express-rate-limit`)*: solves the abuse case but doesn't help the firmware's legitimate poll cadence or share results across clients. Adds a dependency.
- *Cache the transformed payload*: simpler, but `remaining_minutes` would be stale by up to the TTL. Caching upstream + re-transforming costs essentially nothing extra.
- *Shared cache (Redis / file)*: overkill for a single-process server. In-process is sufficient because the deployment is one-host.

---

## 17. Bind-address knob (`HOST`) *(post-implementation hardening)*

**Decision**: The server reads `process.env.HOST` (default `0.0.0.0`) and passes it to `app.listen(port, host)`. Documented in `.env.example` with guidance to set it to a specific LAN IP on multi-homed machines, or `127.0.0.1` for SSH-tunnel deployments.

**Rationale**: Binding to `0.0.0.0` is convenient when bringing the server up for the first time, but on a host that also has a VPN interface, a cellular tether, or a public IP, the endpoint suddenly straddles networks the operator didn't intend to expose to. The knob exists so the deployment can become as narrow as the operator wants without code changes.

**Alternatives considered**:
- *Hardcode `127.0.0.1` and require SSH tunneling*: safest, but breaks the documented "ESP32 reaches server over LAN" flow.
- *Auto-detect the LAN interface*: brittle (multiple interfaces, RFC1918 ambiguity, container networks); leaves the operator unsure which IP got picked.
- *Bind to all interfaces, document the risk in a footnote*: what we had before. The footnote was easy to miss.

---

## 18. Wi-Fi reconnect after the one-shot join *(post-implementation hardening)*

**Decision**: `connectWifi()` runs once in `setup()`. From then on, the top of `doPoll()` checks `WiFi.status()`; if the link is down it logs and calls `WiFi.reconnect()` before attempting the fetch. The reconnect is asynchronous, so the triggering poll still fails (driving the FR-011 error blink); recovery lands on a later poll once the STA re-associates. No new config or timer — recovery is paced by `GET_USAGE_INTERVAL_MS`.

**Rationale**: An always-on ambient device will outlive router reboots and RF drops. Before this, a link loss left the strip blinking ERROR forever because nothing re-initiated the connection — the original "will retry implicitly via poll" comment was wrong, since the poll path only *reads* `WiFi.status()`. Re-initiating inside `doPoll()` reuses the existing poll cadence and serial logging, keeps recovery deterministic, and makes FR-014 (automatic recovery without a power-cycle) hold for link loss, not just API errors.

**Alternatives considered**:
- *`WiFi.setAutoReconnect(true)` only*: relies on the Arduino-ESP32 core's event-driven reconnect, whose behavior varies by core version and often gives up after `NO_AP_FOUND` (the router-reboot case we care about). Reconnect would also happen silently, with no serial visibility.
- *`WiFi.persistent(true)`*: pointless here — credentials are compile-time constants from `config.h`, so persisting them to NVS only adds flash wear.
- *A separate sub-poll-interval reconnect timer in `loop()`*: snappier recovery, but more state for a device where a multi-minute recovery is invisible to the user. Deferred unless faster recovery is wanted.

---

## 19. Global brightness scaler (`LED_BRIGHTNESS`) *(post-implementation addition)*

**Decision**: Add `LED_BRIGHTNESS` (`uint8_t`, 0–255, default `50`) to `config.h` and apply it once via `FastLED.setBrightness(LED_BRIGHTNESS)` in `LedRenderer::begin()`.

**Rationale**: At full output a WS2812B strip is harsh as an ambient indoor display, and full brightness draws the most current. A single global scaler dims every state — all three quota bands, the startup chase, and the error blink — proportionally, with no change to the configured colors or thresholds, and reduces peak current draw alongside it. `setBrightness` is FastLED's built-in scaler, so this is one call at init rather than per-pixel math.

**Alternatives considered**:
- *Per-color brightness*: more knobs, no real benefit; the bands already encode intent through hue.
- *Runtime brightness from the API response*: would avoid a re-flash to retune, but adds protocol surface and firmware state for a value operators set once. Deferred; noted as a possible follow-up.

---

## 20. Threshold marker ticks (`SHOW_THRESHOLD_MARKERS`) *(post-implementation addition)*

**Decision**: When `SHOW_THRESHOLD_MARKERS` is set (default `1`), the USAGE display draws two "upcoming threshold" ticks: a `WARN_QUOTA_COLOR` pixel at the warn-threshold position and an `EXHAUSTED_QUOTA_COLOR` pixel at the exhausted-threshold position, but **only where each tick sits on the not-yet-filled background** (its index `≥ litCount`). A tick the fill has reached is left as fill color, so it disappears into the bar. A tick's index is `Animations::markerLedIndex(percent, NUM_OF_LED)` = `litLedCount(percent, NUM_OF_LED) - 1` — the topmost LED that would be lit at exactly that utilization (e.g. 70% → index 111, 90% → 143 on a 160-LED strip). The renderer fills the buffer, draws the surviving marker pixels, then does a single `show()`. Markers appear only in the USAGE mode; STARTUP, ERROR, and COUNTDOWN are untouched. Set the macro to `0` to disable.

**Rationale**: Reusing `litLedCount` for the position guarantees the tick lands exactly where the band color would change, so the marker stays correct under any operator-chosen thresholds or strip length with no extra config. Suppressing a tick once the fill reaches it makes the ticks read purely as *upcoming* boundaries on the dark background — and avoids the one visible wart of the original on-top approach: a stray yellow warn pixel stranded inside a fully red (exhausted) bar. This is still a thin overlay on top of the fill: FR-008/FR-009 fill semantics are unchanged, and `markerLedIndex` is unit-tested alongside `litLedCount`.

**Alternatives considered**:
- *Draw markers on top of the fill (original implementation, now superseded)*: simpler guard (`index < numLed` only), but left a yellow warn tick visible inside the red fill once utilization passed the exhausted threshold — the wart this change removes.
- *Markers in a fixed contrasting color (e.g. white)*: always visible regardless of fill, but visually noisier and decouples the tick from the band it marks. Left as a one-line retune for operators who want it.
- *Render the markers in COUNTDOWN too*: rejected — the countdown is a blue reset-fill with its own semantics; band ticks there would mislead.

---

## 21. Half-open band intervals *(post-implementation refinement)*

**Decision**: `selectBandColor` changed the upper edge of the WARN band from inclusive to exclusive, making both bands half-open `[lower, upper)`:
- `util < WARN_THRESHOLD_PERCENT` → NORMAL,
- `WARN_THRESHOLD_PERCENT ≤ util < EXHAUSTED_THRESHOLD_PERCENT` → WARN,
- `util ≥ EXHAUSTED_THRESHOLD_PERCENT` → EXHAUSTED.

Previously the WARN band was closed on both ends (`WARN ≤ util ≤ EXHAUSTED`), so EXHAUSTED only began *above* the threshold. The only behavioral change is at `util == EXHAUSTED_THRESHOLD_PERCENT` exactly: WARN → EXHAUSTED.

**Rationale**: The exhausted threshold marker (research §20) sits at the topmost LED lit at exactly `EXHAUSTED_THRESHOLD_PERCENT`. Under the old closed-interval rule the fill at that point was still WARN-colored, leaving a lone red marker pixel atop a yellow bar — the band flip lagged the marker by one threshold step. Half-open intervals make the EXHAUSTED band start *at* the threshold, so reaching the red tick turns the bar red and the tick blends into its band. The WARN lower bound stays inclusive, so the warn tick was already consistent. The `0 < WARN ≤ EXHAUSTED ≤ 100` constraint is unchanged; when `WARN == EXHAUSTED` the WARN band is simply empty (a two-band normal/exhausted scheme). Boundary cases are unit-tested (`test_band_warn_just_below_ninety_five`, `test_band_exhausted_at_ninety_five`).

**Alternatives considered**:
- *Move the marker instead of changing the band* (place the exhausted tick one LED higher, at the first EXHAUSTED pixel): keeps the old closed band but makes the tick no longer mark the threshold's own position; more surprising than fixing the interval.
- *Leave it closed*: the one-pixel mismatch is cosmetic, but the inconsistency was exactly what the marker made visible.

---

## 22. One-poll "quota hit" confirmation before COUNTDOWN *(post-implementation addition)*

**Decision**: When a successful poll first reports `utilization ≥ 100 %`, `DisplayController::advance` holds `USAGE` for that single poll so the operator sees the full-red proportional bar (every LED in `EXHAUSTED_QUOTA_COLOR`, with the threshold ticks blended in) as the "you just hit 100%" confirmation. The next exhausted poll flips to `COUNTDOWN`. A new `bool exhausted_shown` field in `DisplayState` is the gate: set on the first exhausted success, cleared whenever utilization drops below 100 %. The flag lives in RAM only — a reboot while still exhausted replays the red confirmation once, by design.

**Rationale**: Previously the controller jumped straight from `USAGE` to the blue `COUNTDOWN` at the moment of exhaustion, which skipped the most informative single frame in the device's life: "the bar just filled up". The user lost the visual evidence that the strip actually *reached* full before the meaning changed. Holding the red bar for one poll cycle (one `GET_USAGE_INTERVAL_MS`, default 5 min) is enough to register as a "quota hit" moment without significantly delaying the blue countdown, and it tightly couples the change in color to the user-meaningful event (hitting 100%) rather than to an arbitrary state-machine edge. Rearming on drop-below-100% means a window that climbs back to 100% later (e.g. after a brief reset window or upstream correction) replays the same confirmation, so the behavior is consistent across episodes rather than one-shot for the device's lifetime.

**Alternatives considered**:
- *Fixed-duration hold (3 s / 5 s) via a millis() timer*: independent of poll cadence and would also work, but adds a second timer to the controller and decouples the red bar from the poll that caused it. Tying the hold to "one poll cycle" keeps the state machine purely event-driven on poll completions and matches the existing tolerance-counter style.
- *Persist `exhausted_shown` in NVS so reboots don't re-flash red*: rejected as gratuitous flash wear for one frame of behavior. A fresh boot at 100% replaying the red bar is also arguably *better* (the user just power-cycled; the confirmation is now informative again rather than redundant). Keep state RAM-only, matching the rest of `DisplayState`.
- *Skip the confirmation entirely (original behavior)*: the change exists precisely because the instant red→blue transition hid the moment of exhaustion.
