I want to display Claude usage on a WS2812B LED strip.

- Create a simple Node.js API server to read usage data (refer to my `@docs/claude-usage-node.js`). I will deploy this server on a machine that has access to full Claude usage information.
- The LED strip is controlled by an ESP32. The ESP32 will periodically call the API server above to retrieve only the five-hour usage data.
- Necessary configuration can be defined in the Node.js `.env` file:
  - `CLAUDE_ENDPOINT`
- Necessary configuration can be defined in the ESP32 config file:
  - `API_ENDPOINT`
  - `GET_USAGE_INTERVAL` (default: 5 minutes)
  - `NUM_OF_LED`
  - `NORMAL_QUOTA_COLOR` (green)
  - `WARN_QUOTA_COLOR` (yellow)
  - `EXHAUSTED_QUOTA_COLOR` (red)
- When the ESP32 starts and the LED strip has not received any usage information yet, display a white chasing animation effect.
- When the ESP32 cannot retrieve data from the API server, make the LED strip blink red.
- Organize the code inside a `src` folder with the following subfolders:
  - `api`
  - `driver`
- When usage reaches 100%, use the `reset_at` information to gradually turn off the LEDs from left to right (opposite of the progress bar direction) to represent the remaining time until reset.
