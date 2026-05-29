# Implementation Plan: Claude Usage LED Indicator

**Branch**: `001-claude-usage-led` | **Date**: 2026-05-21 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-claude-usage-led/spec.md`

## Summary

Two-component embedded system that surfaces the user's current five-hour Claude usage on a WS2812B LED strip:

1. A small **Node.js HTTP service** runs on a machine that already has the user's Claude credentials. It hits the upstream Claude OAuth usage endpoint, extracts the five-hour fields, computes the remaining-minutes-until-reset, and exposes a minimal JSON endpoint for the LED device.
2. **ESP32 firmware** polls that endpoint at a configurable interval, drives the WS2812B strip with a proportional fill colored by the locked utilization bands (`<70%` green / `70–<90%` yellow / `≥90%` red), and runs three special-purpose visualizations on top: a white chasing animation while waiting for the first reading, a blinking-red error pattern on poll failure, and an "exhausted countdown" that drains LEDs from the opposite end of the strip over the remaining-minutes window.

Technical approach: pin all decisions that need wall-clock arithmetic to the server side (so the firmware needs only a monotonic timer); keep the API contract tiny so the firmware does not depend on the upstream Claude schema; use the well-trodden Arduino-ESP32 / FastLED / ArduinoJson stack on the device and a dependency-light Node.js / Express service on the host.

## Technical Context

**Language/Version**:
- API server: **Node.js 20 LTS** (uses native `fetch`, native `node:test`, ES modules).
- ESP32 firmware: **C++17** under the **Arduino-ESP32 core** (PlatformIO-managed).

**Primary Dependencies**:
- API server: `express` (HTTP routing) and `dotenv` (config). Nothing else; uses Node's native `fetch` for upstream.
- ESP32: `FastLED` (WS2812B driving), `ArduinoJson` (response parsing), `HTTPClient` + `WiFi` (built into Arduino-ESP32).

**Storage**: N/A on both sides. API server is stateless (fetches live each request, no cache). Firmware keeps state in RAM only; configuration is compile-time via a `config.h` header.

**Testing**:
- API server: `node:test` (built-in runner) for the transform layer (`upstream payload → minimal payload`) and the route handler. No external test framework added.
- ESP32: PlatformIO **native** environment for headless logic tests (color-band selection, fill-count math, countdown decrement); on-device verification via the quickstart steps.

**Target Platform**:
- API server: Linux or macOS host (the same machine that holds `~/.claude/.credentials.json`).
- ESP32 firmware: Generic **ESP32-WROOM-32** baseline; should work on any ESP32 family chip that Arduino-ESP32 supports (S2, S3, C3) since the dependencies are platform-agnostic.

**Project Type**: Multi-component embedded system. Two siblings under `src/`: a Node.js service (`src/api`) and an ESP32 firmware project (`src/driver`). Not a fit for the template's web/mobile shapes.

**Performance Goals**:
- API server: must comfortably serve one client polling at the configured cadence (default once every 5 minutes). Upstream call latency dominates; a poll round-trip under 2 s end-to-end is the practical target.
- ESP32: animations render at ≥ 30 fps (≈ 33 ms per frame budget); HTTP poll completes within a few seconds; the state machine never blocks the animation loop.

**Constraints**:
- ESP32 SRAM ≈ 520 KB. ArduinoJson document is sized for the minimal payload (256 bytes is overkill but safe). LED buffer = `NUM_OF_LED * 3` bytes. Both are tiny against the budget.
- API server: single-process, single-tenant, plain HTTP, local network only — no auth, no TLS in v1 (per spec Assumptions).
- The firmware must not depend on the upstream Claude schema (per FR-007).

**Scale/Scope**: Single user, single API server, one or a few ESP32 devices on the same LAN. The codebase deliberately stays small: ~300 LOC for the server and ~700 LOC for the firmware are realistic upper bounds.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

The project's `.specify/memory/constitution.md` contains only template placeholders (no ratified principles or sections). No governance gates are therefore enforced for this feature. The plan still adheres to the implicit best-practice posture suggested by the constitution template:

- **Simplicity (YAGNI)**: no abstractions beyond what the two components need; no database, no broker, no auth layer in v1.
- **Test-first where it pays off**: deterministic logic (utilization → color band, utilization → lit-LED count, countdown decrement) is unit-tested; integration verification of LED hardware behavior is via the quickstart manual checklist.
- **Observability**: the server logs every request and every upstream failure to stdout; the firmware logs over USB serial when connected.

**Result: PASS** (no constitution-defined gates to fail).

Re-evaluation after Phase 1 design: still PASS — no new dependencies or violations introduced.

## Project Structure

### Documentation (this feature)

```text
specs/001-claude-usage-led/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/
│   └── five-hour-endpoint.md   # Server ↔ firmware contract
├── checklists/
│   └── requirements.md  # From /speckit-specify
└── tasks.md             # Created by /speckit-tasks (not this command)
```

### Source Code (repository root)

```text
src/
├── api/                            # Node.js HTTP service
│   ├── package.json
│   ├── .env.example
│   ├── server.js                   # Express bootstrap; wires routes
│   ├── lib/
│   │   ├── claudeClient.js         # Loads credentials, calls CLAUDE_ENDPOINT
│   │   └── transform.js            # upstream JSON → { utilization, remaining_minutes }
│   ├── routes/
│   │   └── fiveHour.js             # GET /five-hour handler
│   └── tests/
│       ├── transform.test.js       # Pure-function tests on the mapper
│       └── route.test.js           # In-process supertest of GET /five-hour
└── driver/                          # ESP32 firmware (PlatformIO project)
    ├── platformio.ini
    ├── include/
    │   ├── config.example.h        # Operator-edited template
    │   └── README.md               # How to fill in config.h
    ├── src/
    │   ├── main.cpp                # setup() + loop() — orchestrates state machine
    │   ├── UsageClient.{h,cpp}     # HTTP GET + ArduinoJson parse → struct Reading
    │   ├── LedRenderer.{h,cpp}     # FastLED wrapper: fill, chase, blink, drain
    │   ├── Animations.{h,cpp}      # Frame-by-frame computations for chase/blink/drain
    │   └── DisplayController.{h,cpp} # State machine: STARTUP → USAGE → COUNTDOWN / ERROR
    └── test/
        └── test_logic/             # PlatformIO native unit tests
            ├── test_color_band.cpp
            ├── test_fill_count.cpp
            └── test_countdown.cpp
```

**Structure Decision**: Mandated by **FR-015** — the codebase lives under a top-level `src/` with two siblings: `api` for the Node.js service and `driver` for the ESP32 firmware. Each sibling is independently buildable: `cd src/api && npm install && node server.js` for the server; `cd src/driver && pio run --target upload` for the firmware. Tests live alongside their component (`src/api/tests`, `src/driver/test`) rather than in a shared top-level `tests/` tree, since the two components have completely different test runners (`node:test` vs PlatformIO native).

## Phase 0 Research

See [research.md](./research.md). Resolved topics include:

- Server framework (Express vs Fastify vs `node:http`)
- WS2812B library on Arduino-ESP32 (FastLED vs Adafruit_NeoPixel vs NeoPixelBus)
- JSON parsing on the device (ArduinoJson sizing)
- Configuration delivery on the firmware (compile-time header vs NVS)
- Color encoding in `config.h` (raw CRGB literal vs hex string parsing)
- Wi-Fi credentials handling (FR-013 does not mandate them; added as plan-level config)
- LED rounding rule when mapping `utilization%` → lit count
- Blink cadence and chase animation tempo defaults
- Countdown step granularity (discrete LED-by-LED vs smoothed brightness)
- Polling interval cadence during countdown

No `NEEDS CLARIFICATION` markers remain.

## Phase 1 Design

- **Data shapes** are defined in [data-model.md](./data-model.md).
- **Server ↔ firmware interface** is defined in [contracts/five-hour-endpoint.md](./contracts/five-hour-endpoint.md).
- **Operator onboarding & manual verification steps** are defined in [quickstart.md](./quickstart.md).
- **Agent context** (`CLAUDE.md` SPECKIT block) is updated to point at this plan so downstream Claude Code sessions land on the right artifacts.

## Complexity Tracking

No constitution-defined gates were violated, so no complexity justifications are required. The design deliberately stays at one process per side of the wire — no broker, no cache, no auth.
