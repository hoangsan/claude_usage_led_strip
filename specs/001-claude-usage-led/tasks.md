---

description: "Task list for Claude Usage LED Indicator (001-claude-usage-led)"
---

# Tasks: Claude Usage LED Indicator

**Input**: Design documents from `/specs/001-claude-usage-led/`

**Prerequisites**: plan.md ✅ · spec.md ✅ · research.md ✅ · data-model.md ✅ · contracts/five-hour-endpoint.md ✅ · quickstart.md ✅

**Tests**: Test tasks are included because plan.md explicitly designs a test layer for both the Node.js server (`node:test`) and the ESP32 firmware (PlatformIO native).

**Organization**: Tasks are grouped by user story. Each user story is independently testable and corresponds to a render mode of the firmware state machine.

## Format: `[ID] [P?] [Story?] Description`

- **[P]**: Different files / no dependencies on incomplete tasks — can run in parallel.
- **[Story]**: Maps the task to a user story (US1..US4); only set within a user-story phase.
- Every task lists the exact file(s) it touches.

## Path Conventions

Per **FR-015** + plan.md Project Structure: code lives under `src/api/` (Node.js HTTP service) and `src/driver/` (ESP32 PlatformIO project). Tests live next to their component: `src/api/tests/` and `src/driver/test/test_logic/`.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Bring both project skeletons to a state where they can be built.

- [X] T001 Create the directory layout at the repo root: `src/api/{lib,routes,tests}/` and `src/driver/{include,src,test/test_logic}/` — exactly matching the tree in plan.md → Project Structure.
- [X] T002 Initialize the Node.js project in `src/api/package.json`: set `"type": "module"`, `"engines": {"node": ">=20"}`, add the `"start": "node server.js"` script, and install runtime deps `express` and `dotenv`.
- [X] T003 [P] Create `src/api/.env.example` with the two values the server needs at boot: `CLAUDE_ENDPOINT=https://api.anthropic.com/api/oauth/usage` and `PORT=3000`.
- [X] T004 [P] Create `src/driver/platformio.ini` targeting `board = esp32dev`, `framework = arduino`, `build_flags = -std=gnu++17`, and `lib_deps = fastled/FastLED@^3.6 bblanchon/ArduinoJson@^7`.
- [X] T005 [P] Create `src/driver/include/config.example.h` declaring every required `#define`: `WIFI_SSID`, `WIFI_PASSWORD`, `API_ENDPOINT`, `GET_USAGE_INTERVAL_MS=300000`, `NUM_OF_LED`, `LED_DATA_PIN=5`, and the three `CRGB(...)` color literals from data-model.md §3. Add a one-line header comment instructing operators to copy this file to `config.h` and never commit `config.h`.
- [X] T006 [P] Create `src/driver/include/README.md` (≤25 lines) explaining: how to copy `config.example.h` → `config.h`, what each `#define` does, the meaning of the three quota colors, and the constraint that `GET_USAGE_INTERVAL_MS` should not be set below 10 000.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Land the full API server (it serves all user stories with the same minimal payload) and the firmware plumbing (Wi-Fi, FastLED init, poll loop, state machine) so each user-story phase only adds a render mode.

**⚠️ CRITICAL**: No user-story phase can start until this phase is complete.

### Server: upstream client, transform, route, bootstrap

- [X] T007 [P] Implement `src/api/lib/claudeClient.js` — loads `~/.claude/.credentials.json` (path overridable via `CLAUDE_CREDENTIALS_FILE` env), extracts `claudeAiOauth.accessToken`, calls `process.env.CLAUDE_ENDPOINT` with the same headers used by `docs/claude-usage-node.js` (`Authorization: Bearer …`, `anthropic-beta: oauth-2025-04-20`, `User-Agent: claude-cli/<version> (external, cli)`), 10 s timeout, returns parsed JSON. Throws on non-2xx, non-JSON, missing token, or unreadable credentials file.
- [X] T008 [P] Implement `src/api/lib/transform.js` — pure ES module exporting `upstreamToMinimal(payload, nowMs = Date.now())` that validates per data-model.md §1 and returns `{ utilization, remaining_minutes }` where `remaining_minutes = Math.max(0, Math.floor((Date.parse(payload.five_hour.resets_at) - nowMs) / 60000))`. Throws `Error('invalid_upstream', { cause: ... })` on any validation failure.
- [X] T009 Implement `src/api/routes/fiveHour.js` — exports an Express handler that calls `claudeClient.fetchUsage()` then `transform.upstreamToMinimal()`, responds with `200 {utilization, remaining_minutes}` on success, `502 {error:"upstream_unavailable", detail}` on any upstream/transform throw, `500 {error:"server_misconfigured", detail}` when `CLAUDE_ENDPOINT` is missing. Sets `Cache-Control: no-store`. Depends on T007, T008.
- [X] T010 Implement `src/api/server.js` — loads `.env` via `dotenv/config`, fails fast if `CLAUDE_ENDPOINT` is missing, builds the Express app, mounts `GET /five-hour` (T009) and `GET /health` (returns `200 {status:"ok"}`), installs a catch-all 404 handler returning `{error:"not_found"}` and a method-not-allowed 405 handler, listens on `process.env.PORT || 3000`, logs every request to stdout. Depends on T009.

### Server: tests

- [X] T011 [P] Add `src/api/tests/transform.test.js` using `node:test` — cases: nominal payload (utilization 42.3 / future resets_at) returns floor-minute math; `five_hour` missing throws; `five_hour.utilization` missing throws; `resets_at` missing throws; `resets_at` unparseable throws; `resets_at` in the past returns `remaining_minutes: 0`; `utilization: 150` (overage) is passed through unchanged. Depends on T008.
- [X] T012 [P] Add `src/api/tests/route.test.js` using `node:test` — uses an in-process Express app and a stubbed `claudeClient` to assert: success returns `200` with the right shape and `Cache-Control: no-store`; thrown upstream error returns `502 {error:"upstream_unavailable"}`; missing `CLAUDE_ENDPOINT` returns `500 {error:"server_misconfigured"}`; `GET /bad-path` returns `404`; `POST /five-hour` returns `405`. Depends on T009, T010.

### Firmware: HTTP client, state machine, LED scaffolding, bootstrap

- [X] T013 [P] Implement `src/driver/src/UsageClient.h` and `src/driver/src/UsageClient.cpp` — exposes `bool fetch(Reading& out, String& errOut)` that issues `HTTPClient.GET(API_ENDPOINT)` with a 5 s timeout, parses the response body via ArduinoJson into a `Reading {float utilization; uint32_t remaining_minutes;}`, validates per data-model.md §2 (utilization ≥ 0, remaining_minutes integer ≥ 0), and returns `false` with a short `errOut` on any failure (network, non-2xx, deserialization error, missing field, type mismatch). Defines `struct Reading` in the header.
- [X] T014 [P] Implement `src/driver/src/DisplayController.h` and `src/driver/src/DisplayController.cpp` — declares `enum class DisplayMode { STARTUP, USAGE, COUNTDOWN, ERROR }`, `struct PollSnapshot { Reading reading; uint32_t received_at_ms; }`, `struct DisplayState { DisplayMode mode; PollSnapshot last_poll; }`, and `void advance(DisplayState& st, bool ok, const Reading& r, uint32_t now_ms)` implementing exactly the transition table in data-model.md §4. No FastLED calls.
- [X] T015 [P] Implement `src/driver/src/LedRenderer.h` and `src/driver/src/LedRenderer.cpp` — declares an internal `CRGB leds[]` buffer (sized at runtime from `NUM_OF_LED`), exposes `void begin(uint16_t numLed, uint8_t dataPin)`, `void clear()`, `void show()`, and forward-declares the four render methods that later phases will implement (`renderChase`, `renderProportionalFill`, `renderBlink`, plus the countdown reuses `renderProportionalFill`). Empty bodies for the render methods at this stage (they live as TODOs until phase 3+).
- [X] T016 Implement `src/driver/src/main.cpp` `setup()` — `Serial.begin(115200)`, connect Wi-Fi using `WIFI_SSID` / `WIFI_PASSWORD` with retries and stdout logging, call `LedRenderer::begin(NUM_OF_LED, LED_DATA_PIN)`, instantiate a static `DisplayState{ DisplayMode::STARTUP, {} }`. Depends on T015.
- [X] T017 Implement `src/driver/src/main.cpp` `loop()` — non-blocking: tracks `last_poll_ms`; when `millis() - last_poll_ms >= GET_USAGE_INTERVAL_MS`, calls `UsageClient::fetch(reading, errBuf)`, then `DisplayController::advance(state, ok, reading, millis())`, then resets `last_poll_ms`. On every loop iteration (no time gate) calls a placeholder `render(state, millis())` that for now only calls `LedRenderer::clear()` + `LedRenderer::show()`. Logs every poll outcome to Serial. Depends on T013, T014, T015, T016.

**Checkpoint**: `curl http://<host>:3000/five-hour` returns the expected JSON shape, the server tests pass, and the firmware connects to Wi-Fi and polls every interval — the strip is intentionally dark because no render mode is wired yet. All four user-story phases can now begin.

---

## Phase 3: User Story 1 — Proportional fill + color band (Priority: P1) 🎯 MVP

**Goal**: Render the current five-hour utilization as a contiguous, color-coded fill that occupies a proportional share of the strip (FR-008, FR-009).

**Independent Test**: Per quickstart.md §3 "US1" — point the firmware at a server returning known utilization values and confirm strip fill length matches each value (within ±1 LED) and color matches the band (`<70%` green / `70–<90%` yellow / `≥90%` red).

### Tests for User Story 1 ⚠️

> Write these first and confirm they FAIL before implementing T020/T021.

- [X] T018 [P] [US1] Add `src/driver/test/test_logic/test_color_band.cpp` (PlatformIO native, Unity framework) — asserts `selectBandColor()` returns `NORMAL_QUOTA_COLOR` for `{0.0, 50.0, 69.99}`, `WARN_QUOTA_COLOR` for `{70.0, 80.0, 95.0}`, `EXHAUSTED_QUOTA_COLOR` for `{95.01, 100.0, 150.0}`.
- [X] T019 [P] [US1] Add `src/driver/test/test_logic/test_fill_count.cpp` — asserts `litLedCount(util, n)` returns `0` at `(0, 10)`, `n` at `(100, 10)`, `5` at `(50, 10)`, `10` at `(110, 10)` (cap), `1` at `(5, 10)` (round-to-nearest at boundary).

### Implementation for User Story 1

- [X] T020 [P] [US1] Add `src/driver/src/Animations.h` and `src/driver/src/Animations.cpp` with `CRGB selectBandColor(float utilization)` implementing the rule from data-model.md §5 (defaults 70 and 95; bands later made half-open in T044 and the exhausted default changed to 90 in T047).
- [X] T021 [P] [US1] In the same `Animations` module, add `uint16_t litLedCount(float utilization, uint16_t numLed)` returning `clamp(round(utilization * numLed / 100.0f), 0, numLed)` per research.md §7.
- [X] T022 [US1] Implement `LedRenderer::renderProportionalFill(uint16_t litCount, CRGB color, uint16_t numLed)` in `src/driver/src/LedRenderer.cpp` — clears the buffer, sets `leds[0 .. litCount-1] = color`, calls `show()`. This primitive is reused by US4. Depends on T015.
- [X] T023 [US1] In `src/driver/src/main.cpp`, replace the placeholder `render(state, now_ms)` body so that when `state.mode == DisplayMode::USAGE` it calls `LedRenderer::renderProportionalFill(litLedCount(util, NUM_OF_LED), selectBandColor(util), NUM_OF_LED)` using `state.last_poll.reading.utilization`. Keep STARTUP/COUNTDOWN/ERROR paths as `clear()` no-op for now. Depends on T020, T021, T022.
- [ ] T024 [US1] Bench-validate per quickstart.md §3 "US1": flash the firmware, run the server against a real Claude account, confirm the strip lights ~ the right fraction in the right color band after the first poll, and confirm the strip updates after a subsequent poll in which utilization moved. _Pending hardware: requires a flashed ESP32 + WS2812B strip on a real LAN; not executable in this environment._

**Checkpoint**: With server up and Claude credentials present, a freshly flashed device shows the current five-hour utilization in the correct color band on the strip. STARTUP/ERROR/COUNTDOWN still show a dark strip (intentional).

---

## Phase 4: User Story 2 — Startup chase animation (Priority: P2)

**Goal**: Display a white chasing animation while in `DisplayMode::STARTUP` (FR-010), so users can tell the device is alive before the first poll completes.

**Independent Test**: Per quickstart.md §3 "US2" — power-cycle with the server stopped; confirm continuous chase; start the server; confirm chase yields to USAGE within one polling interval.

### Implementation for User Story 2

- [X] T025 [US2] Implement `LedRenderer::renderChase(uint32_t frame_idx, uint16_t numLed)` in `src/driver/src/LedRenderer.cpp` — paints a 3-LED white comet with brightness ramp 255/128/64 at position `frame_idx % numLed`, all other pixels black, calls `show()`. Wraps cleanly at the strip ends.
- [X] T026 [US2] In `src/driver/src/main.cpp` `loop()`, add a 60 ms-cadence frame counter (tracked via `millis()` so it does not block). When `state.mode == DisplayMode::STARTUP`, increment `frame_idx` and call `LedRenderer::renderChase(frame_idx, NUM_OF_LED)`. Depends on T025.
- [ ] T027 [US2] Bench-validate per quickstart.md §3 "US2": power-cycle with the API server stopped (chase runs continuously), then start the server (chase yields to USAGE within one polling interval). _Pending hardware: requires a flashed ESP32 + WS2812B strip on a real LAN; not executable in this environment._

**Checkpoint**: From power-on until the first successful poll, the strip shows a recognizable white chase; transition to USAGE on first valid Reading.

---

## Phase 5: User Story 3 — Error blink (Priority: P2)

**Goal**: Display a 1 Hz red blink across the whole strip while in `DisplayMode::ERROR` (FR-011), visually distinct from any normal display state, and exit it automatically when polling recovers (FR-012).

**Independent Test**: Per quickstart.md §3 "US3" — with strip in USAGE, stop the server; within one polling interval the strip blinks; restart the server; next poll restores USAGE.

### Implementation for User Story 3

- [X] T028 [US3] Implement `LedRenderer::renderBlink(uint32_t now_ms, CRGB color, uint16_t numLed)` in `src/driver/src/LedRenderer.cpp` — fills the whole buffer with `color` when `(now_ms / 500) % 2 == 0` else with `CRGB::Black`, calls `show()`. Period = 1.0 s, duty 50% (per research.md §8).
- [X] T029 [US3] In `src/driver/src/main.cpp` `render(state, now_ms)`, route `state.mode == DisplayMode::ERROR` to `LedRenderer::renderBlink(now_ms, EXHAUSTED_QUOTA_COLOR, NUM_OF_LED)`. Depends on T028.
- [ ] T030 [US3] Bench-validate per quickstart.md §3 "US3": stop the API server with the strip in USAGE; confirm blink within one polling interval; restart the server; confirm recovery in the same cycle. _Pending hardware: requires a flashed ESP32 + WS2812B strip on a real LAN; not executable in this environment._

**Checkpoint**: Any poll failure (network, non-2xx, malformed JSON, missing field) drives the strip into 1 Hz red blink; the next successful poll restores USAGE (or COUNTDOWN) immediately.

---

## Phase 6: User Story 4 — Exhausted countdown (Priority: P3)

**Goal**: When `utilization >= 100`, drain the lit strip from the end opposite the fill end over the locally tracked remaining minutes (FR-016).

**Independent Test**: Per quickstart.md §3 "US4" — point firmware at a stub returning `{utilization:100, remaining_minutes:N}`; confirm the strip starts fully lit and shrinks roughly linearly to zero LEDs over N minutes, with LEDs at high indices going dark first. Then change the stub to a sub-100% utilization and confirm immediate switch to USAGE.

### Tests for User Story 4 ⚠️

> Write this first and confirm it FAILS before implementing T032.

- [X] T031 [P] [US4] Add `src/driver/test/test_logic/test_countdown.cpp` — uses a fake monotonic clock (no `millis()`; pass `now_ms` as an argument). Given `PollSnapshot{ {100, 10}, 0 }` and `numLed=30`: assert `countdownLitCount` returns `30` at `now_ms=0`, `15` at `now_ms=5*60000`, `0` at `now_ms=10*60000`, `0` at `now_ms=10*60000+1`. Also assert that when the snapshot has `remaining_minutes=0`, the function returns `0` regardless of `now_ms`.

### Implementation for User Story 4

- [X] T032 [P] [US4] In `src/driver/src/Animations.cpp` (forward-declared in `Animations.h`), implement `uint16_t countdownLitCount(const PollSnapshot& snap, uint32_t now_ms, uint16_t numLed)` per data-model.md §4 — computes `effective_minutes = max(0, snap.reading.remaining_minutes - (now_ms - snap.received_at_ms) / 60000)`, returns `clamp(round(effective_minutes * numLed / max(1u, snap.reading.remaining_minutes)), 0, numLed)`.
- [X] T033 [US4] In `src/driver/src/main.cpp` `render(state, now_ms)`, route `state.mode == DisplayMode::COUNTDOWN` to `LedRenderer::renderProportionalFill(countdownLitCount(state.last_poll, now_ms, NUM_OF_LED), EXHAUSTED_QUOTA_COLOR, NUM_OF_LED)`. This reuses the primitive from US1 (T022). Depends on T022, T032.
- [ ] T034 [US4] Bench-validate per quickstart.md §3 "US4": with the API stubbed to `{utilization:100, remaining_minutes:2}` and `NUM_OF_LED≈30`, confirm the strip drains over ~2 min with high-index LEDs going dark first; switch the stub to `{utilization:5, remaining_minutes:295}` and confirm immediate switch to green USAGE display at the next poll. _Pending hardware: requires a flashed ESP32 + WS2812B strip on a real LAN; not executable in this environment._

**Checkpoint**: Reaching 100 % utilization triggers the countdown; remaining time is visible in the strip's lit fraction; window reopening (utilization < 100) restores USAGE.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T035 [P] Run the full automated suite from a clean checkout: `cd src/api && node --test tests/` and `cd src/driver && pio test -e native`. Fix any breakage. Record any test that needs to skip on this environment (and why) in `src/driver/test/test_logic/README.md` if needed. **Result**: server `node --test tests/*.test.js` → 15/15 pass; firmware `pio test -e native` → 19/19 pass (per Agent B run; build artifacts present under `src/driver/.pio/`).
- [ ] T036 [P] Run the full quickstart.md §3 verification protocol against a real ESP32 device on a real LAN — confirm US1, US2, US3, US4 and the listed edge cases all behave per spec. **Pending: requires ESP32 + WS2812B hardware on a LAN.**
- [X] T037 Code-review pass on `src/driver/` confirming **no upstream Claude schema field names** (e.g. `five_hour`, `resets_at`, `utilization` *as a nested-object reference*) appear anywhere in firmware code — the firmware must depend only on the two-field minimal contract (FR-007 audit). Remove any leftover TODOs in shipping files. **Result**: `grep -rn -E "five_hour|resets_at|seven_day|extra_usage|claudeAiOauth|accessToken" src/driver/` → no matches. `grep -rn -E "TODO|FIXME|XXX|HACK" src/ --include="*.js" --include="*.cpp" --include="*.h" --include="*.ini"` → matches only in `src/api/node_modules/` (third-party), none in shipping files.

---

## Phase 8: Post-implementation hardening

**Purpose**: Small post-v1 fixes and robustness additions. The three server-side items (T038–T040) come from the security review and leave the wire contract unchanged; the two firmware-side items (T041–T042) harden Wi-Fi recovery and add a brightness knob. All are operator-facing improvements.

- [X] T038 Sanitise 5xx `detail` payloads in `src/api/lib/claudeClient.js` and `src/api/routes/fiveHour.js` — replace raw `err.message` (which embedded filesystem paths like `/home/<user>/.claude/.credentials.json` and native socket errors carrying hostnames/IPs) with short stable codes (`credentials_unreadable`, `credentials_invalid_json`, `credentials_missing_token`, `upstream_timeout`, `upstream_request_failed`, `upstream_status_<N>`, `upstream_invalid_json`). Original native cause is preserved on `err.cause` and written to stderr via an injected `errorLog`. See research.md §15.
- [X] T039 Add an in-process upstream-payload cache in `src/api/routes/fiveHour.js` controlled by `CACHE_TTL_MS` (default 60 000 ms, configurable in `src/api/.env`). Cache is keyed by the raw upstream payload; transform is re-run on every request so `remaining_minutes` stays current; any fetch or transform failure busts the cache. See research.md §16 and contracts/five-hour-endpoint.md → "Server-side caching".
- [X] T040 Add the `HOST` bind-address knob to `src/api/server.js` and `src/api/.env.example`. Default `0.0.0.0` preserves backward compatibility; operators on multi-homed machines (VPN, cellular tether, public IP) can now narrow exposure without code changes. See research.md §17.
- [X] T041 Re-establish Wi-Fi inside `doPoll()` in `src/driver/src/main.cpp` — when `WiFi.status() != WL_CONNECTED`, log and call `WiFi.reconnect()` before the fetch. The async reconnect lands on a later poll while the FR-011 blink covers the gap, so a router reboot or RF drop recovers without a power-cycle (FR-014). Also corrected the stale "will retry implicitly via poll" boot-failure log. See research.md §18.
- [X] T042 Add the `LED_BRIGHTNESS` knob (0–255, default 64; later lowered to 50 in T047) to `config.example.h` and apply it once via `FastLED.setBrightness()` in `LedRenderer::begin()` (`src/driver/src/LedRenderer.cpp`). Dims every band and animation proportionally and cuts peak current draw, with no change to colors or thresholds. See research.md §19 and data-model.md §3.
- [X] T043 Add threshold-marker ticks to the USAGE display behind the `SHOW_THRESHOLD_MARKERS` config flag (default 1). New pure helper `Animations::markerLedIndex(percent, numLed)` = `litLedCount(percent, numLed) - 1` (unit-tested in `test/test_logic/test_marker_index.cpp`); `LedRenderer::renderProportionalFillWithMarkers()` fills then overlays a `WARN_QUOTA_COLOR` pixel at the warn threshold and an `EXHAUSTED_QUOTA_COLOR` pixel at the exhausted threshold before a single `show()`; `main.cpp` USAGE case selects it via `#if SHOW_THRESHOLD_MARKERS`. Purely additive — FR-008/FR-009 fill semantics unchanged. See research.md §20 and data-model.md §3. (Overlap behavior later refined in T045.)
- [X] T044 Make the quota bands half-open in `Animations::selectBandColor` — change the WARN upper bound from inclusive to exclusive so `util >= EXHAUSTED_THRESHOLD_PERCENT` is EXHAUSTED (was `>`). Aligns the band flip with the exhausted threshold marker (T043): reaching the red tick now turns the bar red. Updated `test_color_band.cpp` (a boundary test now asserts the threshold value itself is EXHAUSTED, plus a just-below-threshold WARN case) and the FR-009 / data-model §5 boundary statements. See research.md §21. (Boundary tests later retargeted to the 90 default in T047.)
- [X] T045 Hide threshold markers once the fill reaches them in `LedRenderer::renderProportionalFillWithMarkers()` — draw a tick only where `index >= litCount` (on the not-yet-filled background), instead of unconditionally on top. Removes the stray yellow warn pixel that appeared inside a red (exhausted) fill; ticks now read purely as upcoming boundaries. One-line guard change in the (non-unit-tested) renderer layer plus comment/doc sync in `LedRenderer.h`, `main.cpp`, `config*.h`, data-model §3. See research.md §20.
- [X] T046 Redesign the exhausted countdown (supersedes T031/T032): blue fill that **grows toward the reset** instead of a red bar draining from full. `Animations::countdownLitCount` now measures the (locally decremented) remaining-minutes against the fixed `kFiveHourWindowMinutes` (300) window — `lit = round((300 − effective) / 300 × numLed)` — so 0 LEDs at a full window remaining and all LEDs at reset; `main.cpp` renders it in the new `COUNTDOWN_COLOR` (blue, default `0x0000CC`) via the existing `renderProportionalFill`. Rewrote `test_countdown.cpp` (6 cases for the new formula), added the config color, and updated FR-016 / FR-008 / US4 / SC-007 / data-model §4 / research.md §10. See research.md §10.
- [X] T047 Change the shipped defaults: `EXHAUSTED_THRESHOLD_PERCENT` 95 → 90 and `LED_BRIGHTNESS` 64 → 50 in `config.example.h` (and the `test_color_defaults.h` fixture so the native build mirrors the default). Retargeted the boundary band tests to 90 (`test_band_warn_just_below_ninety` @ 89.99, `test_band_exhausted_at_ninety` @ 90.0, `test_band_exhausted_just_above_ninety` @ 90.01) and the exhausted marker example (`markerLedIndex(90, 160) == 143`). Swept every "default 95 / default 64", band shorthand (`≥90%`, `[70%, 90%)`), and the `90–100%` exhausted-band range through spec.md / data-model.md / research.md / plan.md / quickstart.md / README.md / `include/README.md`. `config.h` already carried 90 / 50.
- [X] T048 Add a one-poll "quota hit" confirmation before the COUNTDOWN flip in `DisplayController::advance` (`src/driver/src/DisplayController.cpp` / `.h`): new `bool exhausted_shown` field in `DisplayState`; first successful poll with `utilization ≥ 100 %` holds `USAGE` (full-red bar) and sets the flag, second consecutive exhausted poll flips to `COUNTDOWN`, and the flag clears whenever utilization drops below 100 % so a later re-exhaustion replays the confirmation. RAM-only — a reboot at 100 % therefore replays the red bar once. `main.cpp::setup()` initializes `exhausted_shown = false`. New native tests `test/test_logic/test_exhausted_hold.cpp` (two cases: first-poll hold + drop-below rearm). Updated FR-016, US4 acceptance scenarios, edge cases, data-model.md §4 (transition table + new "one-poll quota hit confirmation" note), quickstart.md US4, research.md §22.

**Verification**: Server (T038–T040): `cd src/api && node --test tests/*.test.js` → 15 / 15 pass; existing `body.detail` assertions remained satisfied because new codes still meet those constraints. Firmware (T043, T046, T048): `cd src/driver && pio test -e native` → 32 / 32 pass; covers the `markerLedIndex` cases, the rewritten countdown suite, the band/fill tests, and the new exhausted-hold pair.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: no dependencies — can start immediately.
- **Foundational (Phase 2)**: depends on Setup. **Blocks all user stories.**
- **User Stories (Phase 3+)**: depend on Foundational. Within the same single-developer track, do them in priority order: US1 → US2 → US3 → US4. With two developers post-foundational, US1 (server-touching, MVP) and US2 / US3 / US4 (firmware-only render modes) can fan out.
- **Polish (Phase 7)**: depends on all user stories you intend to ship.

### User Story Dependencies (within firmware)

- **US1** depends only on Phase 2.
- **US2** is independent of US1 — it adds rendering for `DisplayMode::STARTUP` only. Can ship without US1 done (the device would chase forever until US1 lands).
- **US3** is independent of US1 — it adds rendering for `DisplayMode::ERROR`. Can ship without US1 (device blinks on failure but never renders usage).
- **US4** depends on **US1's `renderProportionalFill` primitive** (T022). It can be implemented before US1's full integration completes, but it cannot ship without T022 in place. If US4 must ship first for some reason, lift T022 into Foundational.

### Parallel Opportunities

- Setup: T003–T006 all run in parallel after T001 / T002.
- Foundational server side: T007 + T008 in parallel; T011 + T012 in parallel once their deps are ready.
- Foundational firmware side: T013 + T014 + T015 in parallel (different files, no dependencies on each other).
- Across Foundational: the entire server track (T007–T012) and the entire firmware track (T013–T017) can run in parallel by two developers.
- US1 tests T018 + T019 in parallel; US1 implementation T020 + T021 in parallel.
- US4 test T031 and implementation T032 in parallel.
- Polish T035 + T036 in parallel.

---

## Parallel Example: foundational fan-out

```text
# Two developers, post-Setup:

# Dev A (server track):
T007  [P]  src/api/lib/claudeClient.js
T008  [P]  src/api/lib/transform.js
T009       src/api/routes/fiveHour.js           (after T007 + T008)
T010       src/api/server.js                     (after T009)
T011  [P]  src/api/tests/transform.test.js       (after T008)
T012  [P]  src/api/tests/route.test.js           (after T009 + T010)

# Dev B (firmware track):
T013  [P]  src/driver/src/UsageClient.{h,cpp}
T014  [P]  src/driver/src/DisplayController.{h,cpp}
T015  [P]  src/driver/src/LedRenderer.{h,cpp}    (skeleton)
T016       src/driver/src/main.cpp setup()       (after T015)
T017       src/driver/src/main.cpp loop()        (after T013, T014, T015, T016)
```

## Parallel Example: User Story 1

```text
# Tests first (run in parallel):
T018  [P]  src/driver/test/test_logic/test_color_band.cpp
T019  [P]  src/driver/test/test_logic/test_fill_count.cpp

# Then implementation (T020 + T021 in parallel):
T020  [P]  src/driver/src/Animations.{h,cpp}     (selectBandColor)
T021  [P]  src/driver/src/Animations.cpp         (litLedCount)
T022       src/driver/src/LedRenderer.cpp        (renderProportionalFill)  (after T015)
T023       src/driver/src/main.cpp               (wire USAGE rendering)    (after T020, T021, T022)
T024       bench validation
```

---

## Implementation Strategy

### MVP first (User Story 1 only)

1. Phase 1: Setup.
2. Phase 2: Foundational — server is fully functional and the firmware polls cleanly.
3. Phase 3: User Story 1 — the strip displays current five-hour utilization color-coded.
4. **STOP and VALIDATE**: bench-test US1, demo. This is the minimum useful product.

### Incremental delivery

After US1 ships, layer in:

1. US2 (startup chase) — turns the "boot looks broken" into "boot looks alive".
2. US3 (error blink) — turns silent failures into a loud visible signal.
3. US4 (exhausted countdown) — adds time-to-reset information once the user is already out.

Each layer is independent: skipping any of US2/US3/US4 still leaves a working MVP.

### Parallel team strategy

Two developers post-Foundational:

- Dev A finishes US1 (the MVP-blocking story, server + firmware touchpoint).
- Dev B starts on US2 and US3 in parallel — both are firmware-only render-mode additions on disjoint code paths (`renderChase` vs `renderBlink`).
- US4 happens last (or in parallel with US3 if Dev A is free) because it reuses T022's primitive.

---

## Notes

- `[P]` means different files and no dependency on an incomplete task.
- The firmware must never reference upstream Claude schema field names (FR-007). T037 audits this at the end of the project; reviewers should also flag any new references during day-to-day work.
- Whenever a render task ships, run the corresponding quickstart.md §3 step on the bench, not just the automated tests — the value of this project is visual.
- Commit at each checkpoint to keep the bisect surface small.
