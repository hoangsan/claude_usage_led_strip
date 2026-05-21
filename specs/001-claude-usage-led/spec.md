# Feature Specification: Claude Usage LED Indicator

**Feature Branch**: `001-claude-usage-led`

**Created**: 2026-05-21

**Status**: Draft

**Input**: User description: "Display Claude usage on a WS2812B LED strip. Node.js API server exposes the user's Claude usage data; an ESP32 polls the server and reflects the five-hour usage window on an addressable LED strip with color-coded status, plus startup and error animations."

## Clarifications

### Session 2026-05-21

- Q: What thresholds separate the normal / warning / exhausted color bands of the five-hour utilization? → A: Defaults are `<70%` normal · `70–95%` warning · `>95%` exhausted. As of the same iteration that introduced configurable `ERROR_COLOR` / `STARTUP_COLOR`, the thresholds themselves are operator-configurable via `WARN_THRESHOLD_PERCENT` and `EXHAUSTED_THRESHOLD_PERCENT` (see FR-013); the three-band structure stays fixed.
- Q: What is the shape of the API server's response to the ESP32? → A: Minimal payload — the server returns exactly two fields: current five-hour utilization (percentage) and remaining minutes until the five-hour window resets (server pre-computes this delta so the firmware does not need a wall clock). The upstream Claude payload is not exposed.
- Q: Which visual colors are operator-configurable? → A: Five colors total — the three band colors (`NORMAL_QUOTA_COLOR`, `WARN_QUOTA_COLOR`, `EXHAUSTED_QUOTA_COLOR`) plus `ERROR_COLOR` (used by the blink) and `STARTUP_COLOR` (used by the chase). Defaults are green / amber / red / red / white respectively; the utilization thresholds that select between band colors remain fixed by the specification.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - At-a-glance five-hour Claude usage from the LED strip (Priority: P1)

A Claude Code user wants an ambient, always-visible indicator of how much of their current five-hour usage window they have consumed. They keep the LED strip on their desk and glance at it while working: a green strip means they have plenty of headroom, a yellow strip warns them they are running low, and a red strip signals the window is exhausted. The amount of strip that is lit corresponds to the percentage of the five-hour window already used, so they can see both how close to the limit they are and the trend over time, without opening a terminal.

**Why this priority**: This is the entire purpose of the device. Without this core behavior the system delivers no value; every other behavior is supportive.

**Independent Test**: With the API server running against an account whose five-hour utilization is known (e.g., 40%), power on the ESP32 and confirm the strip lights the corresponding portion of LEDs in the normal (green) color within one polling interval. Repeat with utilization values in the warning and exhausted bands and confirm the color changes accordingly.

**Acceptance Scenarios**:

1. **Given** the API server is reachable and reports a five-hour utilization of 30%, **When** the ESP32 completes a poll cycle, **Then** approximately 30% of the LEDs (starting from one end) light up in the normal/green color and the remaining LEDs are off.
2. **Given** the API server reports a five-hour utilization of 80%, **When** the ESP32 completes a poll cycle, **Then** approximately 80% of the LEDs light up in the warning/yellow color.
3. **Given** the API server reports a five-hour utilization of 100% (window exhausted), **When** the ESP32 completes a poll cycle, **Then** the full strip lights up in the exhausted/red color.
4. **Given** the LED strip is showing a usage level, **When** the configured polling interval elapses and a new poll returns updated utilization, **Then** the strip updates to reflect the new value (lit length and color band) without requiring a restart.

---

### User Story 2 - Clear feedback during startup before data is available (Priority: P2)

When the user first powers on the device (or after a reboot) the ESP32 has not yet received any usage data. Instead of a dark or frozen strip that looks broken, the user sees a recognizable "warming up" animation — a white chasing light running along the strip — so they know the device is alive and waiting for its first reading.

**Why this priority**: A user who cannot distinguish "device is off / broken" from "device is just waiting" will lose trust quickly. The animation makes the boot phase legible. It is P2 because the device is still functional without it (the first successful poll would eventually replace any default state), but the experience would be confusing.

**Independent Test**: Power-cycle the ESP32 with the API server unreachable or paused on first response. Observe the LED strip from the moment of power-on until the first successful data arrives, and confirm the white chasing animation runs continuously during that interval and then yields to the usage display once data is received.

**Acceptance Scenarios**:

1. **Given** the ESP32 has just powered on and no usage data has been received yet, **When** the user looks at the strip, **Then** a white chasing animation runs end-to-end and repeats until usage data arrives.
2. **Given** the chasing animation is running, **When** the first successful usage data is received from the API server, **Then** the animation stops and the strip transitions to the usage display from User Story 1.

---

### User Story 3 - Clear feedback when the API server is unreachable (Priority: P2)

The user needs to know when the displayed value is no longer trustworthy because the device cannot reach the API server (server down, network issue, wrong endpoint configured, etc.). In that case the strip blinks red so the user can distinguish a real exhausted-quota state (solid red) from a "we cannot tell you the quota" state (blinking red).

**Why this priority**: A silent failure that keeps showing the last known reading would mislead the user about their actual quota. Visible distinct error feedback is required for trust. P2 because it is a degraded-mode behavior rather than the main happy path.

**Independent Test**: With the ESP32 already running and showing usage normally, take the API server offline (or break the network path). Within one polling interval plus a short error window, confirm the strip switches to a blinking red pattern that is visually distinct from any normal usage display. Restore the server and confirm the strip returns to the usage display.

**Acceptance Scenarios**:

1. **Given** the ESP32 is configured with an `API_ENDPOINT` that is unreachable, **When** a poll attempt fails, **Then** the entire strip blinks red at a steady, perceptible rate.
2. **Given** the API server returns a non-success response or malformed data, **When** the ESP32 processes the response, **Then** the strip enters the same blinking red error state rather than displaying a stale or incorrect usage value.
3. **Given** the strip is in the blinking red error state, **When** a subsequent poll succeeds with valid five-hour data, **Then** the strip exits the error state and resumes the usage display.

---

### User Story 4 - Countdown to reset once exhausted (Priority: P3)

Once the five-hour window is exhausted (utilization at 100% or above), the user still wants to know roughly how long until the window reopens. Starting from the fully lit red strip established by User Story 1, the strip turns LEDs off progressively over the remaining minutes until the window resets — proceeding from the end opposite the fill direction — so by a glance at how much of the strip is still lit the user can estimate "reset is soon" vs. "reset is still far away" without opening a terminal.

**Why this priority**: The core "you are out of quota" message is already conveyed by the solid red strip from User Story 1. This countdown is a value-add that tells the user *when* relief comes. It is P3 because the device is fully usable for its primary purpose without it.

**Independent Test**: Configure the API server to return five-hour utilization at 100% with a remaining-minutes-until-reset value of a known interval (e.g., 30). Power on the ESP32 and confirm the strip starts fully lit in the exhausted color and progressively turns off LEDs over the next 30 minutes, with the turn-off proceeding from the end opposite the fill end, finishing fully off at the end of the interval.

**Acceptance Scenarios**:

1. **Given** the API server reports five-hour utilization of 100% with a remaining-minutes-until-reset value of 60, **When** the ESP32 enters the exhausted state, **Then** the strip begins fully lit in the exhausted/red color and over the next 60 minutes LEDs are progressively turned off starting from the end of the strip opposite the fill end, such that the lit portion shrinks roughly linearly with elapsed time.
2. **Given** the strip is mid-countdown (utilization still ≥ 100%), **When** a new poll arrives reporting utilization back below 100% (e.g., the window has just reset), **Then** the strip exits the countdown and resumes the normal proportional-fill display from User Story 1 in the appropriate color band.
3. **Given** the strip is mid-countdown, **When** the locally tracked remaining minutes reach zero and no subsequent poll has yet returned fresh data, **Then** the strip ends the countdown with all LEDs off and remains off until the next successful poll updates the display.

---

### Edge Cases

- **First-ever poll fails**: device boots, the startup chasing animation runs, the first poll then fails — the strip transitions from chasing animation directly into the blinking red error state (not into a stale usage display).
- **Five-hour window has just reset**: server reports very low utilization (e.g., 0–1%) — the strip shows either zero LEDs lit or a single LED lit in the normal color; it does not show a blank/off strip indistinguishable from a powered-off device.
- **Server returns utilization > 100%** (overage): the strip caps the visual fill at the full strip length, uses the exhausted (red) color, and engages the countdown-to-reset visualization (User Story 4 / FR-016) just as it would for exactly 100%.
- **Remaining-minutes value is zero or negative at receipt** (the server's view says the window already rolled over but the upstream still reports ≥ 100% utilization): the countdown has zero remaining time, so the strip immediately shows all LEDs off until the next poll updates the display.
- **Successful poll mid-countdown shows utilization below 100%**: the countdown is abandoned and the strip switches to the normal proportional-fill display for the new value in the same poll cycle.
- **Server returns no five-hour data** (field missing or null) while the response itself is HTTP-success: treated as a data error, strip enters the blinking red error state rather than displaying a misleading value.
- **Number of LEDs is small** (e.g., a handful): the per-LED step in utilization is coarse — the strip still maps utilization proportionally and color thresholds still apply; users tolerate coarse granularity on short strips.
- **Polling interval is changed in config**: after restart, the new interval is honored; while waiting for the next poll, the previously shown state remains on the strip.
- **Server is reachable but very slow**: a poll that takes longer than usual but still succeeds should not trigger the error state; the error state is reserved for failed/invalid responses, not slow ones.
- **Credentials on the server side are missing or expired** (the upstream Claude usage source rejects the server): the API server cannot produce a five-hour value, so the ESP32 sees a data error and shows the blinking red error state.

## Requirements *(mandatory)*

### Functional Requirements

#### API server (deployed on a machine with access to the user's full Claude usage)

- **FR-001**: The API server MUST expose an HTTP endpoint that returns the current five-hour Claude usage data for the account it is configured to read.
- **FR-002**: The API server MUST source its data from the Claude usage endpoint configured by the operator (`CLAUDE_ENDPOINT` in its `.env` file) and MUST NOT require any other runtime configuration to operate.
- **FR-003**: The API server's response to the ESP32 endpoint MUST be a minimal payload containing exactly two pieces of information: (a) the current five-hour utilization value (as a percentage of the window used), and (b) the remaining time until the five-hour window resets, expressed in whole minutes (the server is responsible for computing this delta from the upstream reset moment; the firmware does not need a wall clock). The server MUST NOT expose other upstream usage fields (seven-day, Opus, Sonnet, extra credits) to the ESP32, so that the firmware is decoupled from the upstream Claude usage schema.
- **FR-004**: The API server MUST return a non-success response (or otherwise signal failure) when it cannot retrieve usage data from the upstream Claude source, rather than returning a fabricated or stale value.
- **FR-005**: The API server MUST be deployable on a single machine that has access to the user's Claude credentials and reachable on the local network by the ESP32.

#### ESP32 LED controller (drives the WS2812B strip)

- **FR-006**: The ESP32 MUST periodically poll the API server's usage endpoint at an interval defined by `GET_USAGE_INTERVAL` (default: 5 minutes).
- **FR-007**: The ESP32 MUST consume the five-hour utilization value and the remaining-minutes-until-reset value from the API server's minimal response when computing the LED display. The utilization MUST drive the proportional fill and color band (FR-008, FR-009); the remaining-minutes value MUST drive the exhausted-state countdown (FR-016). Between polls the firmware MUST decrement the remaining-minutes value locally using its monotonic timer (elapsed time since the last successful poll). The firmware MUST NOT depend on any upstream Claude usage field shape; the server is the firmware's only schema authority.
- **FR-008**: The ESP32 MUST drive a WS2812B LED strip of length `NUM_OF_LED`, lighting a contiguous segment from one end of the strip (the "fill end") proportional to the current five-hour utilization (e.g., 50% utilization lights approximately half of the LEDs). The same fill end MUST be used consistently across reboots; the opposite end is the starting point of the countdown defined in FR-016.
- **FR-009**: The ESP32 MUST color the lit segment based on the current five-hour utilization, using a three-band scheme whose two boundaries are operator-configurable:
  - utilization `<` `WARN_THRESHOLD_PERCENT` (default `70`) → `NORMAL_QUOTA_COLOR` (green by default),
  - `WARN_THRESHOLD_PERCENT` ≤ utilization ≤ `EXHAUSTED_THRESHOLD_PERCENT` (default `95`) → `WARN_QUOTA_COLOR` (yellow by default),
  - utilization `>` `EXHAUSTED_THRESHOLD_PERCENT` → `EXHAUSTED_QUOTA_COLOR` (red by default).
  The thresholds MUST satisfy `0 < WARN_THRESHOLD_PERCENT ≤ EXHAUSTED_THRESHOLD_PERCENT ≤ 100`. The exhausted countdown (FR-016) still triggers strictly at utilization `≥ 100` regardless of `EXHAUSTED_THRESHOLD_PERCENT`.
- **FR-010**: Until the ESP32 has received its first valid usage response, the strip MUST display a chasing animation effect in `STARTUP_COLOR` (white by default) and MUST NOT display any usage-derived state.
- **FR-011**: When a poll attempt fails (network failure, non-success response, missing/invalid five-hour data), the ESP32 MUST drive the entire strip into a blinking pattern in `ERROR_COLOR` (red by default) that is visually distinct from any normal usage display, and MUST keep that pattern until a subsequent poll succeeds.
- **FR-012**: When a subsequent poll succeeds after an error state, the ESP32 MUST leave the error pattern and resume the usage display in the same cycle.
- **FR-013**: The ESP32 MUST read all of `API_ENDPOINT`, `GET_USAGE_INTERVAL`, `NUM_OF_LED`, `WARN_THRESHOLD_PERCENT`, `EXHAUSTED_THRESHOLD_PERCENT`, `NORMAL_QUOTA_COLOR`, `WARN_QUOTA_COLOR`, `EXHAUSTED_QUOTA_COLOR`, `ERROR_COLOR`, and `STARTUP_COLOR` from its config file (no hard-coded values for these). `ERROR_COLOR` drives the failure blink (FR-011); `STARTUP_COLOR` drives the pre-data chasing animation (FR-010); `WARN_THRESHOLD_PERCENT` and `EXHAUSTED_THRESHOLD_PERCENT` drive the band-selection rule in FR-009.
- **FR-014**: The ESP32 MUST tolerate transient API failures: an error state is left behind automatically when polling recovers; the user is not required to power-cycle the device.
- **FR-016**: When the current five-hour utilization is at or above 100%, the ESP32 MUST display a *countdown-to-reset* visualization in place of the static exhausted full-bar:
  - the strip MUST begin fully lit in the exhausted color (FR-009),
  - the countdown duration MUST be sourced from the remaining-minutes-until-reset value of the most recent successful poll, decremented locally between polls via the firmware's monotonic timer,
  - LEDs MUST be turned off progressively over that remaining time, such that the lit portion shrinks approximately linearly with elapsed time,
  - the turn-off order MUST proceed from the end opposite the fill end (FR-008), producing a visual reversal of how the strip filled,
  - the countdown MUST end with the strip fully off when the locally tracked remaining minutes reach zero, and the strip MUST remain off until a subsequent successful poll updates the display,
  - if a subsequent poll arrives mid-countdown reporting utilization below 100%, the countdown MUST be abandoned and the proportional-fill display resumed for the new value.

#### Project / packaging

- **FR-015**: The codebase MUST be organized inside a top-level `src` folder with at least two subfolders: `api` (the Node.js server) and `driver` (the ESP32 LED firmware).

### Key Entities *(include if feature involves data)*

- **Five-hour usage reading**: A single snapshot of the user's current five-hour Claude usage window. Conceptually carries (a) a current utilization value expressed as a percentage of the window, and (b) the remaining time (in whole minutes) until the window resets. This is the only piece of data the LED strip needs to render its state; the firmware does not need a wall clock to interpret it.
- **LED display state**: The current visual mode of the strip — one of: *startup (white chasing animation)*, *usage display (proportional fill colored by quota band)*, *exhausted countdown (fully lit exhausted color draining off from the end opposite the fill end over the remaining time until reset)*, or *error (blinking red)*. At any moment exactly one state is active.
- **ESP32 configuration**: The set of parameters that govern device behavior — `API_ENDPOINT`, `GET_USAGE_INTERVAL`, `NUM_OF_LED`, `WARN_THRESHOLD_PERCENT`, `EXHAUSTED_THRESHOLD_PERCENT`, `NORMAL_QUOTA_COLOR`, `WARN_QUOTA_COLOR`, `EXHAUSTED_QUOTA_COLOR`, `ERROR_COLOR`, `STARTUP_COLOR`. Read at startup; changes take effect after restart.
- **API server configuration**: The set of parameters that govern the server — `CLAUDE_ENDPOINT` (and credentials sourced from the host machine as needed). Read at startup.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can look at the LED strip and identify whether they are in the normal, warning, or exhausted band of their five-hour Claude usage in under 2 seconds, without any other tool.
- **SC-002**: When five-hour utilization changes on the Claude side, the LED strip reflects the new value within one polling interval (default: within 5 minutes) of that change.
- **SC-003**: When the API server becomes unreachable or returns invalid data, the LED strip enters the distinct error state within one polling interval, and a user can correctly distinguish "error" from "exhausted quota" on first glance in at least 9 out of 10 trials.
- **SC-004**: When the device is first powered on, the user can tell within 3 seconds that the device is alive and waiting for data (i.e., the startup animation is recognizable as activity, not as a frozen strip).
- **SC-005**: After initial deployment, the device runs unattended for at least 7 consecutive days, recovering automatically from transient API/network failures without requiring a power-cycle or reconfiguration.
- **SC-006**: An operator can change any one of the ESP32 configuration parameters (endpoint, interval, LED count, the three colors) and have the device pick up the new value after a single restart — no firmware rebuild required for any value not in this list is in scope of this success criterion.
- **SC-007**: When the device is in the exhausted countdown, a user can correctly identify which quartile of the time-to-reset remains (first / second / third / fourth quarter) on first glance, in at least 8 out of 10 trials.

## Assumptions

- The ESP32 and the machine running the Node.js API server are on the same local network, and the ESP32 can reach the server over plain HTTP at the configured `API_ENDPOINT`. Encryption, authentication, and exposure to the public internet are out of scope for v1.
- The machine running the API server has the user's Claude credentials already installed locally (e.g., in the standard Claude credentials file) and the server is run by that same user; the server inherits this access rather than receiving credentials over the wire.
- The LED strip is a standard WS2812B addressable strip wired to a data pin on the ESP32 with an appropriately sized power supply for `NUM_OF_LED`. Hardware selection, wiring, and power sizing are the operator's responsibility.
- "Five-hour usage data" refers to the five-hour rolling window already exposed by the user's Claude usage source (the same window surfaced by the Claude `/usage` command). The API server passes that value through; it does not invent or recompute it.
- All five visual colors are user-configurable: the three band colors (`NORMAL_QUOTA_COLOR`, `WARN_QUOTA_COLOR`, `EXHAUSTED_QUOTA_COLOR`), the failure-blink color (`ERROR_COLOR`), and the startup-chase color (`STARTUP_COLOR`). The two utilization thresholds that select between the bands (`WARN_THRESHOLD_PERCENT`, `EXHAUSTED_THRESHOLD_PERCENT`) are also user-configurable, with defaults `70` and `95` respectively. The three-band structure is itself fixed (no fewer or greater number of color regions in v1).
- A single device serves a single user / single Claude account at a time. Multi-user dashboards, per-project breakdowns, or simultaneous monitoring of multiple accounts are out of scope for v1.
- The device displays only the five-hour window. Other usage signals (seven-day, Opus, Sonnet, extra credits) may be present in the API response but are intentionally not visualized in v1.
- A single restart is acceptable for picking up configuration changes; live/over-the-air reconfiguration is out of scope for v1.
- The remaining-minutes-until-reset value in the API response is computed by the server (which has access to the upstream reset moment) and delivered as a whole-minute count valid at the moment the response was sent. The firmware uses its native monotonic timer to track elapsed time since the last successful poll and decrements the value locally; no wall-clock or network-time synchronization is required on the ESP32.
- The API server applies a short in-process cache (default 60 s, configurable via `CACHE_TTL_MS`) on upstream Claude responses to prevent any LAN client from amplifying load onto the operator's upstream quota. Cached responses still re-run the server's `remaining_minutes` derivation against the current wall clock on every request, so the firmware never sees a stale countdown value. The cache TTL is bounded well inside SC-002's one-polling-interval budget.
- 5xx error responses from the API server carry short stable codes in their `detail` field (for example `upstream_unavailable` + `credentials_unreadable`) rather than raw error messages, so the LAN-facing body never leaks operator-side filesystem paths or upstream hostnames. Full diagnostics are written to the operator's stderr for `pm2 logs`-style inspection.
- The API server binds to the interface named by `HOST` (default `0.0.0.0`). On multi-homed hosts (a VPN, a cellular tether, a machine with a public IP) the operator should set `HOST` to a specific LAN address to avoid exposing the endpoint beyond the intended network. This is a deployment-time choice; the firmware contract is unaffected.
