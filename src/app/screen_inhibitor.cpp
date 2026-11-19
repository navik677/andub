#include "screen_inhibitor.hpp"

#include <windows.h>

namespace anime::app {

void ScreenInhibitor::set(bool on) {
    if (on == active_) return;
    active_ = on;
    // Execution state is per thread; set() is always called from the GUI thread.
    SetThreadExecutionState(on ? (ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED) : ES_CONTINUOUS);
}

} // namespace anime::app
