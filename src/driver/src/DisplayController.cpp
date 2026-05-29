#include "DisplayController.h"

namespace DisplayController {

namespace {

inline bool exhausted(const Reading& r) {
    return r.utilization >= 100.0f;
}

}  // namespace

void advance(DisplayState& st, bool ok, const Reading& r, uint32_t now_ms) {
    if (!ok) {
        // Tolerate transient failures: only after kErrorFailureThreshold
        // consecutive failed polls do we drive to ERROR. Below the threshold
        // we hold the current mode, so one slow/transient poll no longer
        // paints the strip red for a whole poll interval. The last_poll
        // snapshot is left as-is (kept in memory for inspection).
        if (st.consecutive_failures < 0xFF) {
            ++st.consecutive_failures;
        }
        if (st.consecutive_failures >= kErrorFailureThreshold) {
            st.mode = DisplayMode::ERROR;
        }
        return;
    }

    // Successful poll: clear the failure streak, refresh the snapshot, then
    // pick the mode.
    st.consecutive_failures     = 0;
    st.last_poll.reading        = r;
    st.last_poll.received_at_ms = now_ms;

    if (exhausted(r)) {
        // Hold USAGE for one poll so the operator sees the full-red bar that
        // confirms "quota hit" before the display switches to the blue
        // countdown. The flag persists across polls while we remain exhausted
        // and is cleared the moment utilization drops below 100%.
        if (!st.exhausted_shown) {
            st.exhausted_shown = true;
            st.mode            = DisplayMode::USAGE;
        } else {
            st.mode = DisplayMode::COUNTDOWN;
        }
    } else {
        st.exhausted_shown = false;
        st.mode            = DisplayMode::USAGE;
    }
}

}  // namespace DisplayController
