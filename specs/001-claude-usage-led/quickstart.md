# Quickstart: Claude Usage LED Indicator

Steps to bring up the API server and the ESP32-driven LED strip from a clean checkout. This is also the manual verification protocol for the spec's acceptance scenarios.

## Prerequisites

**On the host machine** (the one with your Claude credentials):
- Node.js 20+ installed.
- `~/.claude/.credentials.json` present (run `claude` once and sign in if not).
- A free TCP port for the API server (default `3000`).
- The host's LAN IP address.

**For the ESP32 device**:
- An ESP32 board (WROOM-32 baseline; S2/S3/C3 also fine).
- A WS2812B LED strip (any reasonable length; defaults assume 160 LEDs).
- A 5 V power supply sized for the strip; common ground with the ESP32.
- A short data wire from the ESP32 data pin (default GPIO 5) to the strip's `DIN`.
- PlatformIO Core or the PlatformIO VSCode extension.

## 1. Stand up the API server

```bash
cd src/api
cp .env.example .env
# Edit .env — set CLAUDE_ENDPOINT to https://api.anthropic.com/api/oauth/usage
#           (or to a different upstream if needed)
npm install
node start.js
```

Expected: the server prints `listening on http://0.0.0.0:3000` and logs every request.

**Smoke test from the host**:

```bash
curl -i http://localhost:3000/five-hour
```

Expected: `HTTP/1.1 200 OK` with a body like `{"utilization":<n>,"remaining_minutes":<m>}`.

**Failure smoke test** — break the upstream by temporarily renaming the credentials file:

```bash
mv ~/.claude/.credentials.json ~/.claude/.credentials.json.bak
curl -i http://localhost:3000/five-hour
mv ~/.claude/.credentials.json.bak ~/.claude/.credentials.json
```

Expected: `HTTP/1.1 502 Bad Gateway` with `{"error":"upstream_unavailable","detail":"credentials_unreadable"}`. The full native error (including the credentials path) appears on the server's stderr, not in the response body — see contracts/five-hour-endpoint.md → "Failure responses" for the full code set.

**Optional hardening** — for multi-homed hosts or to dial the upstream-load amplification surface:

```bash
# In src/api/.env:
HOST=192.168.1.42       # bind only on the LAN interface (default 0.0.0.0)
CACHE_TTL_MS=60000      # ms to reuse the last successful upstream payload (default 60 000; set 0 to disable)
```

## 2. Wire and flash the ESP32

1. Wire the strip's `DIN` to the ESP32's GPIO 5 (or change `LED_DATA_PIN` in `config.h`), and ensure shared ground.
2. From `src/driver`:

   ```bash
   cp include/config.example.h include/config.h
   ```

3. Edit `include/config.h`:
   - Set `WIFI_SSID` and `WIFI_PASSWORD` to your local Wi-Fi.
   - Set `API_ENDPOINT` to `"http://<host-lan-ip>:3000/five-hour"`.
   - Set `NUM_OF_LED` to your strip length.
   - Leave `LED_BRIGHTNESS` at the default 50, or lower it (0–255) for a dimmer strip.
   - Leave `SHOW_THRESHOLD_MARKERS` at the default 1 for band-boundary ticks, or set 0 to hide them.
   - Leave the three quota colors at their defaults (or tweak the `CRGB(...)` literals).
   - Leave `GET_USAGE_INTERVAL_MS` at the default 300000 (5 min) unless you want faster updates during testing.
4. Flash:

   ```bash
   pio run --target upload
   pio device monitor   # optional, watch logs over USB
   ```

Expected serial output on boot:

```
[wifi] connecting to <SSID>...
[wifi] connected, ip=<device-ip>
[display] STARTUP
[http] GET http://<host-lan-ip>:3000/five-hour -> 200
[display] USAGE  utilization=42.3  remaining_minutes=187
```

## 3. Manual verification of the acceptance scenarios

These mirror the spec's User Stories and Edge Cases. Run them once after a clean flash.

### US1 — Proportional fill + color band

1. With normal Claude usage, observe the strip after the first poll cycle: the lit segment should be roughly proportional to your current five-hour utilization, in the corresponding band's color (`<70%` green / `70–<90%` yellow / `≥90%` red).
2. To force specific bands during dev, point `API_ENDPOINT` at a small mock server returning canned JSON.

### US2 — Startup chase

1. Power-cycle the ESP32. Disconnect Wi-Fi briefly (or leave the API server stopped). Confirm the white chasing animation runs continuously while no data has arrived.
2. Start the API server. Confirm that within one polling interval (default 5 min — use a shorter `GET_USAGE_INTERVAL_MS` for faster feedback during dev) the chase yields to the usage display.

### US3 — Error blink

1. With the strip showing usage, stop the API server. Within one polling interval the strip should switch to a 1 Hz red blink (all LEDs in lockstep).
2. Restart the server. The next poll should restore the usage display.

### US4 — Exhausted countdown

1. Stub the API server (or point `API_ENDPOINT` at a small mock) to return `{"utilization": 100, "remaining_minutes": 30}`.
2. On the **first** poll that observes `utilization = 100` the strip should hold the full-red proportional fill (all LEDs lit in `EXHAUSTED_QUOTA_COLOR`) for one poll cycle — this is the "quota hit" confirmation. On the **next** exhausted poll it switches to a blue bar lit to roughly `(300 − 30) / 300 ≈ 90%` of its length and grows toward the full strip over the next 30 minutes. (For a quick check of countdown math, after the red→blue transition, poll a few fixed `remaining_minutes` values and confirm the fill snaps to the matching level: `300` → empty, `150` → half, `0` → full.)
3. While the countdown is running, change the mock to return `{"utilization": 5, "remaining_minutes": 295}`. The strip should switch to the proportional-fill display in green at the next poll. If you then change the mock back to `{"utilization": 100, "remaining_minutes": 30}`, the one-poll full-red confirmation should replay before the blue countdown resumes.

### Edge cases worth poking

- Set `GET_USAGE_INTERVAL_MS = 10000` (10 s) and break the network briefly: the strip should fall into the blink within ~10 s and recover automatically when the network returns.
- Return `{"utilization": 0, "remaining_minutes": 300}`: the strip should show 0 (or 1, depending on rounding) LED lit in green.
- Return `{"utilization": 110, "remaining_minutes": 15}`: same as the 100% case — the strip first holds a full-red bar for one poll, then the blue countdown lights to ~`(300 − 15) / 300 ≈ 95%` and grows to full over 15 minutes (overage behaves exactly like 100%).

## 4. Run the automated tests

```bash
# Server tests (host)
cd src/api
node --test tests/

# Firmware logic tests (host — no ESP32 needed)
cd src/driver
pio test -e native
```

Expected: both suites green. The firmware logic tests cover color-band selection, fill-count rounding, and countdown decrement against a fake monotonic clock.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Strip stays in white chase forever | ESP32 cannot reach the API server | Verify `API_ENDPOINT` (IP, port, path); check that the server is bound to `0.0.0.0` not `127.0.0.1` |
| Strip blinks red indefinitely | Server is unreachable or returning non-2xx | `curl` the endpoint from the host; check upstream credentials |
| First LED flickers but the rest stay dark | Power supply sag or wrong data pin | Verify the data pin matches `LED_DATA_PIN` and your supply can handle peak current |
| Wrong colors | `CRGB` literal byte order tweak needed for some clones | Try swapping G/R in the `CRGB(...)` literal; FastLED defaults to GRB chipset |
| `node:test` says "fetch is not defined" | Node < 18 | Upgrade to Node 20 LTS |
