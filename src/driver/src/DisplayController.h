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
};

namespace DisplayController {

// Advances `st` in place given the outcome of the latest poll.
//   ok      : true if the poll returned a valid Reading.
//   r       : the Reading (ignored when ok == false).
//   now_ms  : monotonic timestamp at the moment the response was parsed.
//
// Implements the transition table in data-model.md §4 exactly.
void advance(DisplayState& st, bool ok, const Reading& r, uint32_t now_ms);

}  // namespace DisplayController
