// gui test helper: with SMG_TEST_FRAMES=N (set by ctest) the app exits after N
// frames; unset, the test stays open for manual inspection
#pragma once

#include <cstdlib>

#include "GuiBase.hh"

namespace smgtest {

inline void frame_limit(smg::GuiBase& gui) {
    const char* env = std::getenv("SMG_TEST_FRAMES");
    if(env == nullptr) return;
    const long limit = std::strtol(env, nullptr, 10);
    gui.add_callback([&gui, limit, frames = 0L]() mutable {
        if(++frames >= limit) gui.exit();
        return 0;
    });
}

} // namespace smgtest
