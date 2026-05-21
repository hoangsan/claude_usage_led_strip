#include "DisplayController.h"

namespace DisplayController {

namespace {

inline bool exhausted(const Reading& r) {
    return r.utilization >= 100.0f;
}

}  // namespace

void advance(DisplayState& st, bool ok, const Reading& r, uint32_t now_ms) {
    if (!ok) {
        // Any failed poll, from any state, drives to ERROR. The last_poll
        // snapshot is left as-is (it is "discarded for display purposes"
        // but kept in memory so a future regression can inspect it).
        st.mode = DisplayMode::ERROR;
        return;
    }

    // Successful poll: refresh the snapshot, then pick the mode.
    st.last_poll.reading        = r;
    st.last_poll.received_at_ms = now_ms;

    if (exhausted(r)) {
        st.mode = DisplayMode::COUNTDOWN;
    } else {
        st.mode = DisplayMode::USAGE;
    }
}

}  // namespace DisplayController
