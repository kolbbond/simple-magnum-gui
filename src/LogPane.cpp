#include "LogPane.hh"

#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace smg {

namespace {

// Log::msg forwards here; the indent is applied as leading spaces like Log does
class PaneLog final: public Log {
public:
    explicit PaneLog(LogPane& p) : _pane(&p) {}

    using Log::msg;
    void msg(const char* fmt, ...) override {
        va_list ap;
        va_start(ap, fmt);
        forward(num_indent_, fmt, ap);
        va_end(ap);
    }
    void msg(const int incr, const char* fmt, ...) override {
        const int indent = incr != 0 ? num_indent_ : 0;
        va_list ap;
        va_start(ap, fmt);
        forward(indent, fmt, ap);
        va_end(ap);
        std::lock_guard<std::mutex> lk(mtx_);
        if(num_indent_ >= -incr) num_indent_ += incr;
    }

private:
    void forward(int indent, const char* fmt, va_list ap) {
        va_list cp;
        va_copy(cp, ap);
        const int n = std::vsnprintf(nullptr, 0, fmt, cp);
        va_end(cp);
        if(n < 0) return;
        std::string s(size_t(indent > 0 ? indent : 0), ' ');
        const size_t off = s.size();
        s.resize(off + size_t(n) + 1);
        std::vsnprintf(&s[off], size_t(n) + 1, fmt, ap);
        s.resize(off + size_t(n));
        _pane->add(LogPane::Level::Info, s);
    }
    LogPane* _pane;
};

ImVec4 level_color(LogPane::Level l) {
    switch(l) {
    case LogPane::Level::Warn: return { 1.f, 0.85f, 0.3f, 1.f };
    case LogPane::Level::Error: return { 1.f, 0.35f, 0.35f, 1.f };
    case LogPane::Level::Tool: return { 0.45f, 0.8f, 1.f, 1.f };
    default: return { 1.f, 1.f, 1.f, 1.f };
    }
}

} // namespace

LogPane::LogPane(size_t cap_bytes) : _cap(cap_bytes) {}

void LogPane::add(Level lvl, std::string_view text) {
    std::lock_guard<std::mutex> lk(_mtx);
    while(!text.empty()) {
        const size_t nl = text.find('\n');
        std::string_view ln = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        if(ln.size() > _cap) ln = ln.substr(0, _cap);
        _bytes += ln.size();
        _lines.push_back({ lvl, std::string(ln) });
        while(_bytes > _cap && _lines.size() > 1) {
            _bytes -= _lines.front().text.size();
            _lines.pop_front();
        }
    }
}

void LogPane::clear() {
    std::lock_guard<std::mutex> lk(_mtx);
    _lines.clear();
    _bytes = 0;
}

size_t LogPane::line_count() const {
    std::lock_guard<std::mutex> lk(_mtx);
    return _lines.size();
}

std::string_view LogPane::line(size_t i) const {
    std::lock_guard<std::mutex> lk(_mtx);
    if(i >= _lines.size()) throw std::out_of_range("LogPane::line");
    return _lines[i].text;
}

ShLogPr LogPane::as_log() { return std::make_shared<PaneLog>(*this); }

void LogPane::draw(const char* title) {
    if(title) {
        if(!ImGui::Begin(title)) {
            ImGui::End();
            return;
        }
        draw_body();
        ImGui::End();
    } else {
        draw_body();
    }
}

void LogPane::draw_body() {
    if(ImGui::Button("Clear")) clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &_auto_scroll);
    ImGui::SameLine();
    _filter.Draw("Filter", -100.f);
    ImGui::Separator();

    ImGui::BeginChild("##logpane_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    {
        // copy under lock: the draw must not hold the mutex while other threads log
        std::vector<Entry> snap;
        {
            std::lock_guard<std::mutex> lk(_mtx);
            snap.reserve(_lines.size());
            for(const auto& e : _lines)
                if(!_filter.IsActive() || _filter.PassFilter(e.text.c_str(), e.text.c_str() + e.text.size())) snap.push_back(e);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGuiListClipper clipper;
        clipper.Begin(int(snap.size()));
        while(clipper.Step())
            for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const Entry& e = snap[size_t(i)];
                ImGui::PushStyleColor(ImGuiCol_Text, level_color(e.level));
                ImGui::TextUnformatted(e.text.c_str(), e.text.c_str() + e.text.size());
                ImGui::PopStyleColor();
            }
        clipper.End();
        ImGui::PopStyleVar();
    }
    // stick to the bottom unless the user scrolled up
    const bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.f;
    if(ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel > 0.f)
        _auto_scroll = false;
    else if(at_bottom && ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel < 0.f)
        _auto_scroll = true;
    if(_auto_scroll) ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
}

} // namespace smg
