// DisplayController: pure state-machine kernel. No FastLED, no Wi-Fi, no I/O.
// The transition table is the canonical implementation of data-model.md §4.
#pragma once

#include <stdint.h>

#include "UsageClient.h"  // for struct Reading

enum class DisplayMode {
    STARTUP,
    USAGE,
    COUNTDOWN,
    ERROR
};

struct PollSnapshot {
    Reading  reading;
    uint32_t received_at_ms;
};

struct DisplayState {
    DisplayMode  mode;
    PollSnapshot last_poll;
    uint8_t      consecutive_failures;  // reset to 0 on every successful poll
    // True once the full-red USAGE bar has been displayed for the current
    // exhaustion episode. Gates the one-poll "100% confirmation" before the
    // mode flips to COUNTDOWN; cleared when utilization drops below 100%.
    bool         exhausted_shown;
};

namespace DisplayController {

// Number of consecutive failed polls tolerated before the strip switches to
// the red ERROR blink. Below this count, advance() holds the previously
// displayed mode so a single slow/transient poll (slow upstream, brief Wi-Fi
// blip, the async WiFi.reconnect() in doPoll) does not flash red. A successful
// poll resets the counter.
constexpr uint8_t kErrorFailureThreshold = 2;

// Advances `st` in place given the outcome of the latest poll.
//   ok      : true if the poll returned a valid Reading.
//   r       : the Reading (ignored when ok == false).
//   now_ms  : monotonic timestamp at the moment the response was parsed.
//
// Implements the transition table in data-model.md §4, with a
// consecutive-failure tolerance of kErrorFailureThreshold before ERROR.
void advance(DisplayState& st, bool ok, const Reading& r, uint32_t now_ms);

}  // namespace DisplayController
