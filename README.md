# Claude Usage LED

An ambient WS2812B LED strip that shows your current **Claude usage** at a glance. No more switching to the terminal to check `/usage` — you can see how much of your five-hour window is left from across the room.

<!-- Drop your end-product photo here. -->
![Finished build on the desk](docs/images/hero.jpg)

## What it does

- A proportional fill of the strip tracks your five-hour utilization in real time.
- The fill is color-coded across three bands — by default **green / amber / red** at **&lt;70 % / 70–95 % / &gt;95 %**.
- While the device is booting and waiting for its first reading, the strip plays a **white chasing animation** so you can tell it's alive.
- If the device can't reach the API server, the strip **blinks red** so you don't trust a stale value.
- When you hit **100 %**, the lit LEDs **drain off** toward the opposite end of the strip over the remaining time until your five-hour reset — so you also see, at a glance, how long until you're back.

Every visible state is operator-configurable — the band thresholds, all five colors, the strip length, the polling interval, the API URL.

## How it works

```
┌────────────────────────────────┐                       ┌─────────────────────────┐
│ Host machine                   │   GET /five-hour      │  ESP32                  │
│  Node.js API server (PM2)      │ ◄──────────────────── │   polls every 5 min     │
│   loads ~/.claude credentials  │                       │   (default)             │
│   calls upstream Claude        │ ─────────────────────►│   parses JSON           │
│   returns minimal JSON:        │  { "utilization":42,  │   advances state mach.  │
│   { utilization,               │    "remaining_        │   renders WS2812B       │
│     remaining_minutes }        │      minutes": 187 }  │                         │
└────────────────────────────────┘                       └─────────────────────────┘
```

A small Node.js server runs on a machine that's already signed into Claude. It hits the live Claude OAuth usage endpoint, distills the response to a two-field JSON payload (current utilization + remaining minutes until reset), and exposes it on your LAN. The ESP32 polls that endpoint on a cadence you set and renders the strip — no wall clock required on the device because the server pre-computes the reset delta.

## What you'll need

### Hardware

- An **ESP32** dev board (WROOM-32, S2, S3, or C3 — any of them work)
- A **WS2812B LED strip** (anything from a handful up to a couple hundred LEDs; **160** is the default in `config.example.h`)
- A **5 V power supply** sized for your strip (≈ 60 mA peak per LED at full white — for the default 160-LED strip pick a 5 V supply rated **10 A or more** to keep margin; smaller strips need proportionally less)
- A few **jumper wires**, optionally a 470 Ω resistor on the data line and a 1000 µF capacitor across the strip's 5 V/GND for stability
- A USB cable to flash the ESP32

<!-- Drop your wiring photo or diagram here. -->
![Hardware wiring](docs/images/hardware.jpg)

### Software

- **Node.js 20+** on the host that has Claude installed (`claude` already signed in, so `~/.claude/.credentials.json` exists)
- **PM2** (`npm install -g pm2`) to keep the server alive across reboots
- **PlatformIO** (the CLI alone, or the VS Code extension) to build and flash the firmware

## Setup

### 1. Get the code

```bash
git clone <this-repo-url>
cd claude_usage_led
```

### 2. Bring up the API server with PM2

The server is a tiny Express app under `src/api/`. It needs one env var: the upstream Claude endpoint. The default in `.env.example` is already correct for Claude.ai, so usually you just copy it.

```bash
cd src/api
cp .env.example .env

# Install deps and PM2 (skip the second line if you already have pm2 globally)
npm install
npm install -g pm2

# Start the server and tell PM2 to remember it across reboots
pm2 start server.js --name claude-usage-api
pm2 save

# (Linux/macOS) wire PM2 into your init system so it comes back after a reboot.
# Run the command PM2 prints out, then re-run `pm2 save`.
pm2 startup
```

Smoke-test it from the host:

```bash
curl http://localhost:3000/five-hour
# -> {"utilization": 42.3, "remaining_minutes": 187}
```

Useful PM2 commands once it's running:

```bash
pm2 status              # see if it's up
pm2 logs claude-usage-api   # tail the access log
pm2 restart claude-usage-api
pm2 stop claude-usage-api
```

### 3. Wire the LED strip

- Connect the strip's **5 V** and **GND** to your 5 V supply.
- Connect a **common ground** between the supply and the ESP32.
- Connect the strip's **DIN** to a GPIO on the ESP32 — by default this is **GPIO 5**. Change `LED_DATA_PIN` in `config.h` if you wire it somewhere else.
- (Optional but recommended) place a 470 Ω resistor in series on the DIN line, and a 1000 µF capacitor between 5 V and GND near the strip.

### 4. Flash the ESP32 firmware

```bash
cd src/driver
cp include/config.example.h include/config.h
```

Open `include/config.h` and fill in:

| Setting | What to put |
|---|---|
| `WIFI_SSID` / `WIFI_PASSWORD` | Your local Wi-Fi |
| `API_ENDPOINT` | `http://<host-LAN-ip>:3000/five-hour` (the machine running PM2 in step 2) |
| `NUM_OF_LED` | The actual length of your strip |
| `LED_DATA_PIN` | The GPIO you used (default 5) |

You can also retune the color bands and any of the five colors here — see the **Customising the look** section below.

Then build and flash:

```bash
pio run --target upload
pio device monitor      # optional, watch boot logs over USB
```

Expected serial output:

```
[boot] claude_usage_led
[wifi] connecting to <SSID>...
[wifi] connected, ip=192.168.1.57
[display] STARTUP
[http] GET http://192.168.1.42:3000/five-hour -> 200 util=42.30 remaining=187
[display] USAGE
```

The strip should chase white for a second or two, then settle into the proportional fill in the correct color.

## Customising the look

Everything visible is in `src/driver/include/config.h`. The most-used knobs:

| `#define` | Default | What it does |
|---|---|---|
| `WARN_THRESHOLD_PERCENT` | `70` | Where the strip turns from green into amber. |
| `EXHAUSTED_THRESHOLD_PERCENT` | `95` | Where the strip turns from amber into red. |
| `NORMAL_QUOTA_COLOR` | `CRGB(0x00, 0xCC, 0x00)` (green) | Color below the warn threshold. |
| `WARN_QUOTA_COLOR` | `CRGB(0xFF, 0xA5, 0x00)` (amber) | Color in the warn band (inclusive on both ends). |
| `EXHAUSTED_QUOTA_COLOR` | `CRGB(0xCC, 0x00, 0x00)` (red) | Color above the exhausted threshold. Also the countdown color. |
| `ERROR_COLOR` | `CRGB(0xCC, 0x00, 0x00)` (red) | Color of the failure blink. Set this to something distinct (e.g. magenta) if you want "I can't reach the server" and "you're at 100 %" to look different. |
| `STARTUP_COLOR` | `CRGB(0xFF, 0xFF, 0xFF)` (white) | Base color of the boot chase. |
| `GET_USAGE_INTERVAL_MS` | `300000` (5 min) | Poll cadence. Don't go below 10 000 (10 s). |

Re-flash after editing and the new look kicks in immediately.

Full reference: [`src/driver/include/README.md`](src/driver/include/README.md).

## Verifying everything works

<!-- Drop a picture of the strip in each state (green / amber / red / blink / chase / countdown) here. -->
![Strip running through its states](docs/images/states.jpg)

Quick checks:

- **Strip lights green / amber / red proportionally** — point it at the live server and watch your strip after each five-hour interval.
- **Blink on failure** — `pm2 stop claude-usage-api`. Within one polling interval the strip should switch to a 1 Hz red blink. `pm2 start claude-usage-api` and the next poll restores the normal display.
- **Chase on boot** — power-cycle the ESP32 with the server stopped. The white comet should run continuously.
- **Countdown** — point `API_ENDPOINT` at a small mock returning `{"utilization":100,"remaining_minutes":2}` and watch the strip drain from the opposite end over ~2 minutes.

Full bench-test protocol is in [`specs/001-claude-usage-led/quickstart.md`](specs/001-claude-usage-led/quickstart.md).

## Troubleshooting

| Symptom | Most likely cause | Fix |
|---|---|---|
| Strip never leaves the white chase | ESP32 can't reach the server | Check `API_ENDPOINT` (IP, port, path), make sure PM2 is bound to `0.0.0.0:3000`, try `curl` from another machine on the LAN |
| Strip blinks red forever | Server is reachable but upstream is failing | `pm2 logs claude-usage-api` — usually means your Claude token has expired (`claude auth login` again on the host) |
| Wrong colors (red/green swapped) | WS2812B clone with a different chipset order | In `LedRenderer.cpp`, swap `GRB` for `RGB` in `FastLED.addLeds<WS2812B, …>(…)` |
| First LED flickers, rest stay dark | Power supply sag or wrong data pin | Verify the data pin matches `LED_DATA_PIN`, confirm the supply can handle peak current, add the capacitor |
| Server tests fail with "fetch is not defined" | Node < 18 | Upgrade to Node 20 LTS |

## Project structure

```
src/
├── api/        Node.js HTTP service (Express + native fetch + dotenv)
└── driver/     ESP32 firmware (Arduino-ESP32 + FastLED + ArduinoJson)

specs/001-claude-usage-led/   Full spec, plan, contract, and data model
docs/images/                  Photos (you supply these)
```

If you want to read why the project does what it does, [`specs/001-claude-usage-led/spec.md`](specs/001-claude-usage-led/spec.md) is the user-facing spec; [`specs/001-claude-usage-led/plan.md`](specs/001-claude-usage-led/plan.md) is the implementation plan.

## Security & privacy

- The server runs on your local network with **no authentication and no TLS** — it's intended for a trusted LAN. Don't expose it to the public internet without putting a reverse proxy and auth in front of it.
- The ESP32 never sees your Claude OAuth token. The server is the only piece with credentials; it returns nothing but the two numeric fields the firmware needs.
- `include/config.h` (with your Wi-Fi password) and `src/api/.env` are git-ignored by default — keep them that way.
- 5xx responses use short error codes (e.g. `upstream_unavailable`, `credentials_unreadable`) rather than raw error messages so the LAN-facing body never carries filesystem paths or upstream hostnames. The full error (with stack and cause) is logged to stderr — `pm2 logs claude-usage-api` to see it.
- `HOST` (default `0.0.0.0`) and `CACHE_TTL_MS` (default `60000`) in `src/api/.env` let you tighten the deployment. On a multi-homed machine set `HOST` to your LAN IP to avoid exposing the endpoint on a VPN / cellular interface. The cache caps how often a LAN client can force a real upstream Claude call.

## License

Released under the [MIT License](LICENSE).
