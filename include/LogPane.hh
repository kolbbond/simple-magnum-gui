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

    // cap_bytes bounds the sum of (line size + 1) over stored lines (the +1 charges the newline, so
    // blank-line floods are bounded too). Oldest lines drop first; the newest is always kept, cut
    // to at most cap-1 bytes on a UTF-8 boundary. cap 0 is clamped to 1 (newest line only, empty).
    explicit LogPane(size_t cap_bytes = 1 << 20);

    // add, clear, line_count and the as_log() adapter are mutex-guarded (log from any thread);
    // draw() must run on the UI thread and holds the mutex while drawing.
    // Multi-line input is split on '\n'; a trailing '\n' adds no empty line; "" adds nothing.
    void add(Level lvl, std::string_view line);
    void clear();
    size_t line_count() const;
    // Owning (UI) thread only: the view dies when the line is dropped. Throws std::out_of_range.
    std::string_view line(size_t i) const;

    // title == nullptr draws inside the current window; otherwise in its own ImGui::Begin/End.
    void draw(const char* title = nullptr);

    // Adapter forwarding Log::msg to add(). Holds a raw pointer: the pane must outlive EVERY
    // holder of the returned ShLogPr (e.g. a GuiBase _lg), and the adapter's destructor.
    // Text is buffered until '\n' (a partial line stays pending; newl() ends the line, an empty
    // one if nothing is pending; pending text is flushed on destruction). ANSI SGR codes are
    // stripped; red -> Error, yellow -> Warn, else Info. Indent = leading spaces on each line start.
    ShLogPr as_log();

private:
    struct Entry {
        Level level;
        std::string text;
    };
    void draw_body();
    void clear_locked();

    size_t _cap;
    size_t _bytes = 0;
    std::deque<Entry> _lines; // pop_front is O(1)
    mutable std::mutex _mtx;
    ImGuiTextFilter _filter;
    bool _auto_scroll = true;
};

} // namespace smg
