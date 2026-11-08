// scrolling log widget (ImGui ExampleAppLog style): filter box, auto-scroll, per-level colors.
// Independent of goose / rat::Log.
#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>

#include "imgui.h"
#include "log.hh"

namespace smg {

class LogPane {
public:
    enum class Level { Info, Warn, Error, Tool };

    // cap_bytes bounds the summed size of stored line text (newlines/levels not counted);
    // oldest lines are dropped first, the newest is always kept (truncated if alone > cap).
    explicit LogPane(size_t cap_bytes = 1 << 20);

    // Thread-safe: add, clear, line_count, as_log()->msg.
    // Multi-line input is split on '\n'; a trailing '\n' adds no empty line; "" adds nothing.
    void add(Level lvl, std::string_view line);
    void clear();
    size_t line_count() const;
    // Owning (UI) thread only: the view dies when the line is dropped. Throws std::out_of_range.
    std::string_view line(size_t i) const;

    // title == nullptr draws inside the current window; otherwise in its own ImGui::Begin/End.
    void draw(const char* title = nullptr);

    // Adapter forwarding Log::msg(fmt, ...) to add(Info, ...). Holds a raw pointer:
    // the pane must outlive the returned Log (and every copy of the pointer).
    ShLogPr as_log();

private:
    struct Entry {
        Level level;
        std::string text;
    };
    void draw_body();

    size_t _cap;
    size_t _bytes = 0;
    std::deque<Entry> _lines; // pop_front is O(1)
    mutable std::mutex _mtx;
    ImGuiTextFilter _filter;
    bool _auto_scroll = true;
};

} // namespace smg
