# Firmware configuration

This directory holds the operator-edited compile-time configuration for the
ESP32 firmware.

## Bring-up

1. Copy the template to the real config file (which is git-ignored):

   ```bash
   cp include/config.example.h include/config.h
   ```

2. Edit `include/config.h` and set the values for your deployment.

## Settings

| `#define` | Purpose |
|---|---|
| `WIFI_SSID` / `WIFI_PASSWORD` | Local Wi-Fi to join. |
| `API_ENDPOINT` | Full URL of the local server's `/five-hour` endpoint. |
| `GET_USAGE_INTERVAL_MS` | Poll cadence in ms. Default 300000 (5 min). Do **not** set below 10000. |
| `NUM_OF_LED` | Pixel count on the WS2812B strip. |
| `LED_DATA_PIN` | GPIO driving the strip's `DIN`. |
| `WARN_THRESHOLD_PERCENT` | Utilization at which the strip turns from NORMAL into WARN (default `70`). |
| `EXHAUSTED_THRESHOLD_PERCENT` | Utilization above which the strip enters EXHAUSTED (default `95`). Must satisfy `WARN_THRESHOLD_PERCENT ≤ EXHAUSTED_THRESHOLD_PERCENT ≤ 100`. |
| `NORMAL_QUOTA_COLOR` | Color for utilization below `WARN_THRESHOLD_PERCENT` (green by default). |
| `WARN_QUOTA_COLOR` | Color for utilization inside the warn band (amber by default). |
| `EXHAUSTED_QUOTA_COLOR` | Color for utilization above `EXHAUSTED_THRESHOLD_PERCENT` (red by default); also used for the exhausted-state countdown. |
| `ERROR_COLOR` | Color of the failure blink (red by default). Set to something distinct from `EXHAUSTED_QUOTA_COLOR` if you want "we cannot tell you" and "you are at 100%" to be visually distinguishable. |
| `STARTUP_COLOR` | Base color of the 3-LED chase comet shown before the first successful poll (white by default). |

Never commit `config.h` — it carries your Wi-Fi password.
