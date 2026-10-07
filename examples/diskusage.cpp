// diskusage: where did the space go? Scans a folder or drive on a worker thread, then shows a
// size-sorted tree next to a squarified treemap (nested a few levels) and the biggest file types.
// Click a folder in the map to zoom in; right-click or Backspace goes up.
//   diskusage [folder]

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "GuiBase.hh"
#include "imgui.h"

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#    include <shellapi.h>
#else
#    include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

struct Node {
    std::string name; // utf-8; the root holds its full path
    std::uint64_t size = 0;
    std::uint64_t files = 0;
    int parent = -1;
    std::vector<int> children; // biggest first once the scan is done
    bool dir = false;
};

std::string formatSize(std::uint64_t bytes) {
    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    double v = static_cast<double>(bytes);
    int u = 0;
    while(v >= 1024.0 && u < 4) {
        v /= 1024.0;
        ++u;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), u == 0 ? "%.0f %s" : "%.1f %s", v, units[u]);
    return buf;
}

// links and junctions would count space twice or loop forever
bool isLink(const fs::directory_entry& e) {
    std::error_code ec;
    if(e.is_symlink(ec)) return true;
#if defined(_WIN32)
    const DWORD attr = GetFileAttributesW(e.path().c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    return false;
#endif
}

class Scanner {
public:
    ~Scanner() { cancel(); }

    void start(const fs::path& root) {
        cancel();
        _stop = false;
        _done = false;
        _files = 0;
        _bytes = 0;
        _worker = std::thread([this, root]() { run(root); });
    }

    void cancel() {
        _stop = true;
        if(_worker.joinable()) _worker.join();
    }

    bool done() const { return _done; }
    std::uint64_t files() const { return _files; }
    std::uint64_t bytes() const { return _bytes; }
    std::string current() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _current;
    }
    std::vector<Node> take() {
        std::lock_guard<std::mutex> lock(_mutex);
        return std::move(_result);
    }

private:
    std::thread _worker;
    std::mutex _mutex;
    std::vector<Node> _result;
    std::string _current;
    std::atomic<bool> _stop{ false };
    std::atomic<bool> _done{ true };
    std::atomic<std::uint64_t> _files{ 0 };
    std::atomic<std::uint64_t> _bytes{ 0 };

    void run(const fs::path& root) {
        std::vector<Node> nodes;
        nodes.push_back({ root.u8string(), 0, 0, -1, {}, true });
        std::vector<std::pair<int, fs::path>> stack{ { 0, root } };
        while(!stack.empty() && !_stop) {
            const std::pair<int, fs::path> top = stack.back();
            stack.pop_back();
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _current = top.second.u8string();
            }
            std::error_code ec;
            fs::directory_iterator it(top.second, fs::directory_options::skip_permission_denied, ec);
            for(const fs::directory_iterator end; !ec && it != end && !_stop; it.increment(ec)) {
                if(isLink(*it)) continue;
                Node n;
                n.name = it->path().filename().u8string();
                n.parent = top.first;
                std::error_code e2;
                n.dir = it->is_directory(e2);
                if(!n.dir) {
                    const std::uintmax_t s = it->file_size(e2);
                    n.size = e2 ? 0 : s;
                    n.files = 1;
                    ++_files;
                    _bytes += n.size;
                }
                const int index = static_cast<int>(nodes.size());
                nodes[top.first].children.push_back(index);
                if(n.dir) stack.push_back({ index, it->path() });
                nodes.push_back(std::move(n));
            }
        }
        // children always come after their parent, so one reverse pass totals every folder
        for(std::size_t i = nodes.size(); i-- > 1;) {
            nodes[nodes[i].parent].size += nodes[i].size;
            nodes[nodes[i].parent].files += nodes[i].files;
        }
        for(Node& n : nodes)
            std::sort(n.children.begin(), n.children.end(), [&nodes](int a, int b) { return nodes[a].size > nodes[b].size; });
        std::lock_guard<std::mutex> lock(_mutex);
        _result = std::move(nodes);
        _done = true;
    }
};

struct Rect {
    float x, y, w, h;
};

// squarified treemap (Bruls, Huizing, van Wijk): rows of items along the short side, adding to a
// row while that keeps the worst aspect ratio from getting worse. `areas` sum to r.w * r.h
std::vector<Rect> squarify(const std::vector<double>& areas, Rect r) {
    std::vector<Rect> out(areas.size());
    std::size_t start = 0;
    while(start < areas.size()) {
        const double side = std::min(r.w, r.h);
        if(side <= 0.0) break;
        std::size_t end = start;
        double sum = 0.0, best = 1e300;
        while(end < areas.size()) {
            const double s = sum + areas[end];
            const double lo = areas[end]; // areas are sorted descending: the newest is the smallest
            const double hi = areas[start];
            const double worst = std::max(side * side * hi / (s * s), s * s / (side * side * lo));
            if(worst > best) break;
            best = worst;
            sum = s;
            ++end;
        }
        const float thick = static_cast<float>(sum / side);
        float offset = 0.0f;
        for(std::size_t i = start; i < end; ++i) {
            const float len = static_cast<float>(areas[i] / thick);
            out[i] = r.w >= r.h ? Rect{ r.x, r.y + offset, thick, len } : Rect{ r.x + offset, r.y, len, thick };
            offset += len;
        }
        if(r.w >= r.h) {
            r.x += thick;
            r.w -= thick;
        } else {
            r.y += thick;
            r.h -= thick;
        }
        start = end;
    }
    return out;
}

ImU32 hueColor(float hue, float value) {
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(hue - std::floor(hue), 0.55f, value, r, g, b);
    return ImGui::GetColorU32(ImVec4(r, g, b, 1.0f));
}

float extensionHue(const std::string& name) {
    const std::size_t dot = name.rfind('.');
    if(dot == std::string::npos) return 0.0f;
    std::string ext = name.substr(dot);
    for(char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return static_cast<float>(std::hash<std::string>{}(ext) % 1000) / 1000.0f;
}

std::string extensionOf(const std::string& name) {
    const std::size_t dot = name.rfind('.');
    if(dot == std::string::npos || dot == 0) return "(none)";
    std::string ext = name.substr(dot);
    for(char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

void openWithSystem(const fs::path& p) {
#if defined(_WIN32)
    ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    if(fork() == 0) {
        execlp("xdg-open", "xdg-open", p.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
#endif
}

class DiskUsage {
public:
    explicit DiskUsage(const fs::path& root) {
        std::snprintf(_pathBuf, sizeof(_pathBuf), "%s", root.u8string().c_str());
        scan();
    }

    void draw() {
        if(_scanning && _scanner.done()) {
            _nodes = _scanner.take();
            _scanning = false;
            _focus = 0;
            summarizeExtensions();
        }
        drawToolbar();
        if(_scanning) {
            ImGui::Text("scanning... %llu files, %s", static_cast<unsigned long long>(_scanner.files()), formatSize(_scanner.bytes()).c_str());
            ImGui::TextDisabled("%s", _scanner.current().c_str());
            return;
        }
        if(_nodes.empty()) return;
        drawBreadcrumbs();
        if(ImGui::BeginTable("##layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("tree", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("map", ImGuiTableColumnFlags_WidthStretch, 1.8f);
            ImGui::TableNextColumn();
            drawTree();
            ImGui::TableNextColumn();
            drawTreemap();
            ImGui::EndTable();
        }
        if(!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Backspace)) goUp();
    }

private:
    Scanner _scanner;
    std::vector<Node> _nodes;
    std::vector<std::pair<std::string, std::uint64_t>> _extensions; // biggest first
    char _pathBuf[1024] = {};
    bool _scanning = false;
    int _focus = 0;
    int _hovered = -1;

    void scan() {
        _nodes.clear();
        _scanning = true;
        _scanner.start(fs::u8path(_pathBuf));
    }

    void goUp() {
        if(_focus > 0) _focus = _nodes[_focus].parent;
    }

    fs::path pathOf(int i) const {
        std::vector<int> chain;
        for(int n = i; n >= 0; n = _nodes[n].parent) chain.push_back(n);
        fs::path p = fs::u8path(_nodes[chain.back()].name);
        for(std::size_t k = chain.size() - 1; k-- > 0;) p /= fs::u8path(_nodes[chain[k]].name);
        return p;
    }

    void summarizeExtensions() {
        std::map<std::string, std::uint64_t> sums;
        for(const Node& n : _nodes)
            if(!n.dir) sums[extensionOf(n.name)] += n.size;
        _extensions.assign(sums.begin(), sums.end());
        std::sort(_extensions.begin(), _extensions.end(), [](const std::pair<std::string, std::uint64_t>& a, const std::pair<std::string, std::uint64_t>& b) {
            return a.second > b.second;
        });
    }

    void drawToolbar() {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 200.0f);
        const bool enter = ImGui::InputText("##root", _pathBuf, sizeof(_pathBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if(enter || ImGui::Button(_scanning ? "Restart" : "Scan")) scan();
        if(_scanning) {
            ImGui::SameLine();
            if(ImGui::Button("Stop")) {
                _scanner.cancel(); // the worker still totals and hands over what it scanned
            }
        }
    }

    void drawBreadcrumbs() {
        std::vector<int> chain;
        for(int n = _focus; n >= 0; n = _nodes[n].parent) chain.push_back(n);
        for(std::size_t k = chain.size(); k-- > 0;) {
            ImGui::PushID(chain[k]);
            if(ImGui::SmallButton(_nodes[chain[k]].name.c_str())) _focus = chain[k];
            ImGui::PopID();
            if(k > 0) {
                ImGui::SameLine(0.0f, 2.0f);
                ImGui::TextDisabled(">");
                ImGui::SameLine(0.0f, 2.0f);
            }
        }
        const Node& f = _nodes[_focus];
        ImGui::SameLine();
        ImGui::TextDisabled("   %s in %llu files", formatSize(f.size).c_str(), static_cast<unsigned long long>(f.files));
    }

    void contextMenu(int i) {
        if(ImGui::BeginPopupContextItem()) {
            if(ImGui::MenuItem("Open")) openWithSystem(pathOf(i));
            if(ImGui::MenuItem("Open containing folder")) openWithSystem(pathOf(_nodes[i].parent >= 0 ? _nodes[i].parent : i));
            ImGui::EndPopup();
        }
    }

    void drawTreeRows(int parent, int depth) {
        const Node& p = _nodes[parent];
        const std::size_t shown = std::min<std::size_t>(p.children.size(), 300);
        for(std::size_t k = 0; k < shown; ++k) {
            const int i = p.children[k];
            const Node& n = _nodes[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(i);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_OpenOnArrow;
            if(!n.dir || n.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if(i == _hovered) flags |= ImGuiTreeNodeFlags_Selected;
            const bool open = ImGui::TreeNodeEx(n.name.c_str(), flags);
            if(ImGui::IsItemClicked() && n.dir) _focus = i;
            contextMenu(i);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(formatSize(n.size).c_str());
            ImGui::TableNextColumn();
            const float share = p.size ? static_cast<float>(static_cast<double>(n.size) / static_cast<double>(p.size)) : 0.0f;
            char pct[16];
            std::snprintf(pct, sizeof(pct), "%.1f%%", share * 100.0f);
            ImGui::ProgressBar(share, ImVec2(-1, 0), pct);
            if(open && n.dir && !n.children.empty()) {
                drawTreeRows(i, depth + 1);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if(p.children.size() > shown) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("... %zu smaller items", p.children.size() - shown);
        }
    }

    void drawTree() {
        const float extH = ImGui::GetTextLineHeightWithSpacing() * 9.0f;
        if(ImGui::BeginTable("##tree", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable,
               ImVec2(0, ImGui::GetContentRegionAvail().y - extH))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableHeadersRow();
            drawTreeRows(_focus, 0);
            ImGui::EndTable();
        }
        ImGui::SeparatorText("biggest file types (whole scan)");
        const std::uint64_t total = _nodes[0].size ? _nodes[0].size : 1;
        for(std::size_t k = 0; k < std::min<std::size_t>(_extensions.size(), 7); ++k) {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float h = ImGui::GetTextLineHeight();
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + h, p.y + h), hueColor(extensionHue(_extensions[k].first), 0.85f), 2.0f);
            ImGui::Dummy(ImVec2(h, h));
            ImGui::SameLine();
            ImGui::Text("%-10s %10s  %4.1f%%", _extensions[k].first.c_str(), formatSize(_extensions[k].second).c_str(),
                100.0 * static_cast<double>(_extensions[k].second) / static_cast<double>(total));
        }
    }

    // one level of the map; folders big enough get a title strip and their own children inside
    void drawLevel(ImDrawList* draw, int parent, Rect r, int depth) {
        const Node& p = _nodes[parent];
        std::vector<int> items;
        std::vector<double> areas;
        const double total = static_cast<double>(p.size);
        if(total <= 0.0) return;
        const double scale = static_cast<double>(r.w) * r.h / total;
        for(int c : p.children) {
            const double a = static_cast<double>(_nodes[c].size) * scale;
            if(a < 4.0) break; // sorted biggest first: the rest are sub-pixel
            items.push_back(c);
            areas.push_back(a);
        }
        // the dropped tail still owns its share, so the shown items fill r minus that
        double shown = 0.0;
        for(double a : areas) shown += a;
        if(shown <= 0.0) return;
        const double fill = static_cast<double>(r.w) * r.h;
        for(double& a : areas) a *= fill / shown;
        const std::vector<Rect> rects = squarify(areas, r);
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        for(std::size_t k = 0; k < items.size(); ++k) {
            const Node& n = _nodes[items[k]];
            const Rect q = rects[k];
            if(q.w < 1.0f || q.h < 1.0f) continue;
            const ImVec2 a(q.x, q.y), b(q.x + q.w, q.y + q.h);
            const bool inside = mouse.x >= a.x && mouse.x < b.x && mouse.y >= a.y && mouse.y < b.y;
            const bool nest = n.dir && depth < 3 && q.w > 60.0f && q.h > 40.0f && !n.children.empty();
            if(n.dir) {
                draw->AddRectFilled(a, b, hueColor(0.58f + 0.08f * depth, 0.25f + 0.07f * depth));
            } else {
                draw->AddRectFilled(a, b, hueColor(extensionHue(n.name), inside ? 0.95f : 0.75f));
            }
            draw->AddRect(a, b, IM_COL32(0, 0, 0, 160));
            if(inside) _hovered = items[k];
            const float title = ImGui::GetTextLineHeight() + 2.0f;
            if(q.w > 50.0f && q.h > title) {
                draw->PushClipRect(a, b, true);
                draw->AddText(ImVec2(a.x + 3.0f, a.y + 1.0f), IM_COL32(255, 255, 255, 220), n.name.c_str());
                draw->PopClipRect();
            }
            if(nest) drawLevel(draw, items[k], Rect{ q.x + 2.0f, q.y + title, q.w - 4.0f, q.h - title - 2.0f }, depth + 1);
        }
    }

    void drawTreemap() {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 size = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("##map", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool hoveredMap = ImGui::IsItemHovered();
        _hovered = -1;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
        drawLevel(draw, _focus, Rect{ origin.x, origin.y, size.x, size.y }, 0);
        draw->PopClipRect();
        if(!hoveredMap || _hovered < 0) return;

        // zoom into the outermost folder under the mouse (the one directly inside the focus)
        int target = _hovered;
        while(target >= 0 && _nodes[target].parent != _focus) target = _nodes[target].parent;
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && target >= 0 && _nodes[target].dir) _focus = target;
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)) goUp();
        if(_hovered >= 0) {
            const Node& n = _nodes[_hovered];
            ImGui::SetTooltip("%s\n%s%s", pathOf(_hovered).u8string().c_str(), formatSize(n.size).c_str(),
                n.dir ? ("  (" + std::to_string(n.files) + " files)").c_str() : "");
        }
    }
};

} // namespace

int main(int argc, char** argv) {
    fs::path root;
    for(int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if(a == "--smg-frames" || a == "--smg-screenshot" || a == "--smg-record") ++i; // skip their values
        else if(a[0] != '-') root = fs::u8path(a);
    }
    if(root.empty()) {
#if defined(_WIN32)
        const char* home = std::getenv("USERPROFILE");
#else
        const char* home = std::getenv("HOME");
#endif
        root = home ? fs::u8path(home) : fs::current_path();
    }

    smg::GuiConfig config;
    config.title = "smg diskusage";
    config.size = { 1400, 850 };
    smg::GuiBase gui({ argc, argv }, config);

    DiskUsage usage(root);
    gui.add_callback([&usage]() {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("##diskusage", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
        usage.draw();
        ImGui::End();
        return 0;
    });

    while(gui.mainLoopIteration()) {}
    return 0;
}
