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

## 10. Countdown step granularity (FR-016)

**Decision**: **Discrete per-LED on/off**, no per-LED brightness fade. At each "tick" the next LED to drop (the one currently at the trailing edge, i.e., the LED nearest the end opposite the fill end) is set to `CRGB::Black`; all other lit LEDs remain at `EXHAUSTED_QUOTA_COLOR`. The tick interval is `remaining_minutes_at_poll * 60_000 / NUM_OF_LED` ms, scheduled against the firmware's monotonic timer.

**Rationale**: Matches the spec's "the lit portion shrinks approximately linearly with elapsed time" wording and SC-007's "which quartile remains" testability. Discrete steps make it easy for the user to glance and count. Per-LED brightness fading would muddy that test and double the rendering complexity for no UX win — at 5-hour windows on a 30-LED strip, each LED is on for ~10 minutes, so a fade-out would itself last several minutes and be visually indistinguishable from "off".

**Alternatives considered**:
- Smooth brightness fade across the whole strip: visually elegant but breaks the "lit portion shrinks" framing.
- Tick on a fixed cadence (e.g., 1 minute) regardless of strip length: would not match strip-length to elapsed-time when remaining minutes < `NUM_OF_LED`.

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
