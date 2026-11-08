#include "LogPane.hh"

#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace smg {

namespace {

// Log::msg forwards here; see LogPane::as_log for semantics
class PaneLog final: public Log {
public:
    explicit PaneLog(LogPane& p) : _pane(&p) {}
    ~PaneLog() override {
        std::lock_guard<std::mutex> lk(mtx_);
        if(!_pending.empty()) emit();
    }

    using Log::msg;
    void msg(const char* fmt, ...) override {
        va_list ap;
        va_start(ap, fmt);
        forward(0, fmt, ap);
        va_end(ap);
    }
    void msg(const int incr, const char* fmt, ...) override {
        va_list ap;
        va_start(ap, fmt);
        forward(incr, fmt, ap);
        va_end(ap);
    }
    void newl() override {
        std::lock_guard<std::mutex> lk(mtx_);
        emit();
    }

private:
    // caller holds mtx_
    void emit() {
        _pane->add(_level, _pending.empty() ? std::string_view("\n") : std::string_view(_pending));
        _pending.clear();
        _level = LogPane::Level::Info;
    }
    void forward(int incr, const char* fmt, va_list ap) {
        va_list cp;
        va_copy(cp, ap);
        const int n = std::vsnprintf(nullptr, 0, fmt, cp);
        va_end(cp);
        if(n < 0) return;
        std::string text(size_t(n) + 1, '\0');
        std::vsnprintf(&text[0], text.size(), fmt, ap);
        text.resize(size_t(n));

        std::lock_guard<std::mutex> lk(mtx_);
        for(size_t i = 0; i < text.size(); ++i) {
            const char c = text[i];
            if(c == '\x1B' && i + 1 < text.size() && text[i + 1] == '[') {
                size_t j = i + 2;
                while(j < text.size() && ((text[j] >= '0' && text[j] <= '9') || text[j] == ';')) ++j;
                if(j < text.size() && text[j] == 'm') {
                    const std::string code = text.substr(i + 2, j - i - 2);
                    if(code == "31")
                        _level = LogPane::Level::Error;
                    else if(code == "33")
                        _level = LogPane::Level::Warn;
                    i = j;
                    continue;
                }
            }
            if(c == '\n') {
                if(_pending.empty()) _pending = ""; // blank line: emit() sends "\n"
                emit();
                continue;
            }
            if(_pending.empty() && num_indent_ > 0) _pending.assign(size_t(num_indent_), ' ');
            _pending += c;
        }
        if(num_indent_ >= -incr) num_indent_ += incr;
    }
    LogPane* _pane;
    std::string _pending;
    LogPane::Level _level = LogPane::Level::Info;
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

LogPane::LogPane(size_t cap_bytes) : _cap(cap_bytes ? cap_bytes : 1) {}

void LogPane::add(Level lvl, std::string_view text) {
    std::lock_guard<std::mutex> lk(_mtx);
    while(!text.empty()) {
        const size_t nl = text.find('\n');
        std::string_view ln = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        if(ln.size() > _cap - 1) {
            size_t n = _cap - 1;
            while(n > 0 && (static_cast<unsigned char>(ln[n]) & 0xC0) == 0x80) --n; // keep UTF-8 whole
            ln = ln.substr(0, n);
        }
        _bytes += ln.size() + 1;
        _lines.push_back({ lvl, std::string(ln) });
        while(_bytes > _cap && _lines.size() > 1) {
            _bytes -= _lines.front().text.size() + 1;
            _lines.pop_front();
        }
    }
}

void LogPane::clear_locked() {
    _lines.clear();
    _bytes = 0;
}

void LogPane::clear() {
    std::lock_guard<std::mutex> lk(_mtx);
    clear_locked();
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
    ImGui::PushID(this);
    const bool do_clear = ImGui::Button("Clear");
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &_auto_scroll);
    ImGui::SameLine();
    _filter.Draw("Filter", -100.f);
    ImGui::Separator();

    ImGui::BeginChild("##logpane_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    // follow only if the user was already at the bottom (ExampleAppLog rule)
    const bool was_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
    {
        std::lock_guard<std::mutex> lk(_mtx);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        auto show = [](const Entry& e) {
            ImGui::PushStyleColor(ImGuiCol_Text, level_color(e.level));
            ImGui::TextUnformatted(e.text.c_str(), e.text.c_str() + e.text.size());
            ImGui::PopStyleColor();
        };
        ImGuiListClipper clipper;
        if(_filter.IsActive()) {
            std::vector<size_t> idx;
            for(size_t i = 0; i < _lines.size(); ++i) {
                const auto& t = _lines[i].text;
                if(_filter.PassFilter(t.c_str(), t.c_str() + t.size())) idx.push_back(i);
            }
            clipper.Begin(int(idx.size()));
            while(clipper.Step())
                for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) show(_lines[idx[size_t(i)]]);
        } else {
            clipper.Begin(int(_lines.size()));
            while(clipper.Step())
                for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) show(_lines[size_t(i)]);
        }
        clipper.End();
        ImGui::PopStyleVar();
    }
    if(_auto_scroll && was_bottom) ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
    ImGui::PopID();
    if(do_clear) clear();
}

} // namespace smg
