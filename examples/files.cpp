// files: a small, fast file browser in the spirit of File Pilot.
// Places/drives sidebar, sortable listing (clipped, so huge folders stay smooth), instant filter,
// recursive search on a worker thread, text and image preview, open with the OS default app.
//   files [start-folder]
// Keys: Enter open, Backspace up, Alt+Left/Right back/forward, Ctrl+L path, Ctrl+F search, F5 refresh.

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/PluginManager/Manager.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImageView.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

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
#    include <sys/wait.h>
#    include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr std::size_t MaxSearchResults = 50000;

struct Entry {
    fs::path path;
    std::string name; // utf-8; for search results, the path relative to the search root
    std::string lower;
    std::string ext;
    bool dir = false;
    std::uintmax_t size = 0;
    std::time_t modified = 0;
};

std::string utf8(const fs::path& p) { return p.u8string(); }

std::string toLower(std::string s) {
    for(char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::time_t toTimeT(fs::file_time_type t) {
    // C++17 has no file_clock -> system_clock cast; shift by the two clocks' "now"
    const std::chrono::system_clock::time_point sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        t - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sys);
}

std::string formatSize(std::uintmax_t bytes) {
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

std::string formatTime(std::time_t t) {
    if(t <= 0) return {};
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&t));
    return buf;
}

bool isHidden(const fs::directory_entry& e, const std::string& name) {
    if(!name.empty() && name[0] == '.') return true;
#if defined(_WIN32)
    const DWORD attr = GetFileAttributesW(e.path().c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
#else
    (void)e;
    return false;
#endif
}

Entry makeEntry(const fs::directory_entry& e, std::string name) {
    std::error_code ec;
    Entry out;
    out.path = e.path();
    out.name = std::move(name);
    out.lower = toLower(out.name);
    out.dir = e.is_directory(ec);
    if(!out.dir) {
        out.size = e.file_size(ec);
        if(ec) out.size = 0;
        out.ext = toLower(utf8(e.path().extension()));
    }
    const fs::file_time_type t = e.last_write_time(ec);
    out.modified = ec ? 0 : toTimeT(t);
    return out;
}

std::vector<Entry> listDirectory(const fs::path& dir, bool showHidden, std::string& error) {
    std::vector<Entry> entries;
    std::error_code ec;
    fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
    if(ec) {
        error = ec.message();
        return entries;
    }
    error.clear();
    for(const fs::directory_iterator end; it != end; it.increment(ec)) {
        if(ec) break;
        const std::string name = utf8(it->path().filename());
        if(!showHidden && isHidden(*it, name)) continue;
        entries.push_back(makeEntry(*it, name));
    }
    return entries;
}

// recursive name search on a worker thread; the UI copies new results each frame
class Search {
public:
    ~Search() { cancel(); }

    void start(const fs::path& root, const std::string& query) {
        cancel();
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _results.clear();
        }
        _scanned = 0;
        _stop = false;
        _running = true;
        _worker = std::thread([this, root, q = toLower(query)]() { run(root, q); });
    }

    void cancel() {
        _stop = true;
        if(_worker.joinable()) _worker.join();
        _running = false;
    }

    // append results past `have` to `out`
    void collect(std::vector<Entry>& out) {
        std::lock_guard<std::mutex> lock(_mutex);
        for(std::size_t i = out.size(); i < _results.size(); ++i) out.push_back(_results[i]);
    }

    bool running() const { return _running; }
    std::size_t scanned() const { return _scanned; }

private:
    std::thread _worker;
    std::mutex _mutex;
    std::vector<Entry> _results;
    std::atomic<bool> _stop{ false };
    std::atomic<bool> _running{ false };
    std::atomic<std::size_t> _scanned{ 0 };

    void run(const fs::path& root, const std::string& q) {
        std::error_code ec;
        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        while(!ec && it != end && !_stop) {
            const std::string name = utf8(it->path().filename());
            ++_scanned;
            if(toLower(name).find(q) != std::string::npos) {
                std::error_code rel;
                const fs::path relative = fs::relative(it->path(), root, rel);
                Entry e = makeEntry(*it, rel ? name : utf8(relative));
                std::lock_guard<std::mutex> lock(_mutex);
                _results.push_back(std::move(e));
                if(_results.size() >= MaxSearchResults) break;
            }
            it.increment(ec);
            if(ec) {
                // unreadable subtree: step out of it and carry on
                ec.clear();
                if(it == end || it.depth() == 0) break;
                it.pop(ec);
            }
        }
        _running = false;
    }
};

void openWithSystem(const fs::path& p) {
#if defined(_WIN32)
    ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, p.parent_path().c_str(), SW_SHOWNORMAL);
#else
    if(fork() == 0) {
        execlp("xdg-open", "xdg-open", p.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
#endif
}

struct Place {
    std::string label;
    fs::path path;
};

std::vector<Place> places() {
    std::vector<Place> out;
#if defined(_WIN32)
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    if(home) {
        const fs::path h = fs::u8path(home);
        out.push_back({ "Home", h });
        for(const char* sub : { "Desktop", "Documents", "Downloads", "Pictures", "Music" }) {
            std::error_code ec;
            if(fs::is_directory(h / sub, ec)) out.push_back({ sub, h / sub });
        }
    }
#if defined(_WIN32)
    wchar_t drives[256] = {};
    const DWORD n = GetLogicalDriveStringsW(255, drives);
    for(const wchar_t* d = drives; d < drives + n && *d; d += std::wcslen(d) + 1) out.push_back({ utf8(fs::path(d)), fs::path(d) });
#else
    out.push_back({ "/", fs::path("/") });
    out.push_back({ "/tmp", fs::path("/tmp") });
#endif
    return out;
}

// preview of the selected file: an image texture, the first lines of a text file, or nothing
class Preview {
public:
    void select(const Entry* e) {
        const fs::path p = e ? e->path : fs::path{};
        if(p == _path) return;
        _path = p;
        _texture = Corrade::Containers::NullOpt;
        _text.clear();
        _note.clear();
        if(!e || e->dir) return;
        static const char* images[] = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".psd", ".hdr" };
        if(std::find(std::begin(images), std::end(images), e->ext) != std::end(images)) loadImage(*e);
        else loadText(*e);
    }

    void draw(const Entry* e) {
        if(!e) {
            ImGui::TextDisabled("nothing selected");
            return;
        }
        ImGui::TextWrapped("%s", utf8(e->path.filename()).c_str());
        ImGui::TextDisabled("%s   %s", e->dir ? "folder" : formatSize(e->size).c_str(), formatTime(e->modified).c_str());
        ImGui::Separator();
        if(_texture) {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const float scale = std::min({ 1.0f, avail.x / _size.x(), avail.y / _size.y() });
            Magnum::ImGuiIntegration::image(*_texture, Magnum::Vector2{ _size } * scale);
            ImGui::TextDisabled("%d x %d", _size.x(), _size.y());
        } else if(!_text.empty()) {
            ImGui::BeginChild("##text", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(_text.c_str());
            ImGui::EndChild();
        } else if(!_note.empty()) {
            ImGui::TextDisabled("%s", _note.c_str());
        }
    }

private:
    fs::path _path;
    Corrade::Containers::Optional<Magnum::GL::Texture2D> _texture;
    Magnum::Vector2i _size;
    std::string _text;
    std::string _note;
    Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter> _importers;

    void loadImage(const Entry& e) {
        if(e.size > 64u * 1024u * 1024u) {
            _note = "image too large to preview";
            return;
        }
        Corrade::Containers::Pointer<Magnum::Trade::AbstractImporter> importer = _importers.loadAndInstantiate("AnyImageImporter");
        Corrade::Containers::Optional<Magnum::Trade::ImageData2D> image;
        if(importer && importer->openFile(utf8(e.path))) image = importer->image2D(0);
        if(!image || image->isCompressed()) {
            _note = "no preview (image importer plugins missing or unsupported format)";
            return;
        }
        _size = image->size();
        Magnum::GL::Texture2D texture;
        texture.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
            .setMinificationFilter(Magnum::GL::SamplerFilter::Linear)
            .setMagnificationFilter(Magnum::GL::SamplerFilter::Linear)
            .setStorage(1, Magnum::GL::textureFormat(image->format()), _size)
            .setSubImage(0, Magnum::Vector2i{}, Magnum::ImageView2D{ *image });
        _texture = std::move(texture);
    }

    void loadText(const Entry& e) {
        std::ifstream in(e.path, std::ios::binary);
        std::string head(std::min<std::uintmax_t>(e.size, 64 * 1024), '\0');
        in.read(head.data(), static_cast<std::streamsize>(head.size()));
        head.resize(static_cast<std::size_t>(in.gcount()));
        // a NUL in the first bytes means binary
        if(head.find('\0') != std::string::npos) {
            _note = "binary file";
            return;
        }
        _text = std::move(head);
        if(e.size > _text.size()) _text += "\n...";
    }
};

class Browser {
public:
    explicit Browser(fs::path start) : _places(places()) { navigate(std::move(start), true); }

    void draw() {
        handleKeys();
        drawToolbar();
        if(ImGui::BeginTable("##layout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0, -ImGui::GetFrameHeightWithSpacing()))) {
            ImGui::TableSetupColumn("places", ImGuiTableColumnFlags_WidthFixed, 170.0f);
            ImGui::TableSetupColumn("list", ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableSetupColumn("preview", ImGuiTableColumnFlags_WidthStretch, 1.4f);
            ImGui::TableNextColumn();
            drawPlaces();
            ImGui::TableNextColumn();
            drawListing();
            ImGui::TableNextColumn();
            ImGui::BeginChild("##preview");
            _preview.draw(selected());
            ImGui::EndChild();
            ImGui::EndTable();
        }
        drawStatus();
    }

private:
    fs::path _dir;
    std::vector<fs::path> _back, _forward;
    std::vector<Entry> _entries;
    std::vector<Entry> _found; // search results, filled from the worker
    std::vector<const Entry*> _view;
    std::string _error;
    std::vector<Place> _places;
    Search _search;
    Preview _preview;
    char _pathBuf[1024] = {};
    char _query[256] = {};
    bool _recursive = false;
    bool _showHidden = false;
    bool _viewDirty = true;
    int _selected = -1;
    int _sortColumn = 0;
    bool _sortAscending = true;
    bool _focusPath = false;
    bool _focusQuery = false;
    bool _scrollToSelected = false;
    fs::path _selectedPath; // survives view rebuilds; _view pointers do not
    int _pendingActivate = -1;

    bool searching() const { return _recursive && _query[0] != '\0'; }

    void select(int i) {
        _selected = i;
        _selectedPath = i >= 0 && i < static_cast<int>(_view.size()) ? _view[i]->path : fs::path{};
    }

    const Entry* selected() const { return _selected >= 0 && _selected < static_cast<int>(_view.size()) ? _view[_selected] : nullptr; }

    void navigate(fs::path dir, bool record) {
        std::error_code ec;
        fs::path canon = fs::weakly_canonical(dir, ec);
        if(ec) canon = dir;
        if(!fs::is_directory(canon, ec)) return;
        if(record && !_dir.empty() && canon != _dir) {
            _back.push_back(_dir);
            _forward.clear();
        }
        _dir = canon;
        std::snprintf(_pathBuf, sizeof(_pathBuf), "%s", utf8(_dir).c_str());
        _query[0] = '\0';
        _search.cancel();
        _found.clear();
        _selectedPath.clear();
        reload();
    }

    void reload() {
        _view.clear(); // points into _entries
        _entries = listDirectory(_dir, _showHidden, _error);
        _viewDirty = true;
    }

    void goBack() {
        if(_back.empty()) return;
        _forward.push_back(_dir);
        const fs::path p = _back.back();
        _back.pop_back();
        navigate(p, false);
    }

    void goForward() {
        if(_forward.empty()) return;
        _back.push_back(_dir);
        const fs::path p = _forward.back();
        _forward.pop_back();
        navigate(p, false);
    }

    void goUp() {
        if(_dir.has_parent_path() && _dir.parent_path() != _dir) navigate(_dir.parent_path(), true);
    }

    void activate(const Entry& e) {
        if(e.dir) navigate(e.path, true);
        else openWithSystem(e.path);
    }

    void startSearch() {
        _view.clear(); // points into _found
        _found.clear();
        if(searching()) _search.start(_dir, _query);
        else _search.cancel();
        _viewDirty = true;
    }

    void rebuildView() {
        if(searching()) {
            const std::size_t before = _found.size();
            _search.collect(_found); // may reallocate: rebuild before touching _view
            if(_found.size() != before) _viewDirty = true;
        }
        if(!_viewDirty) return;
        _viewDirty = false;

        _view.clear();
        if(searching()) {
            for(const Entry& e : _found) _view.push_back(&e);
        } else {
            const std::string q = toLower(_query);
            for(const Entry& e : _entries)
                if(q.empty() || e.lower.find(q) != std::string::npos) _view.push_back(&e);
        }
        const int column = _sortColumn;
        const bool asc = _sortAscending;
        std::stable_sort(_view.begin(), _view.end(), [column, asc](const Entry* a, const Entry* b) {
            if(a->dir != b->dir) return a->dir; // folders first, either direction
            int c = 0;
            switch(column) {
                case 1: c = a->size < b->size ? -1 : (a->size > b->size ? 1 : 0); break;
                case 2: c = a->modified < b->modified ? -1 : (a->modified > b->modified ? 1 : 0); break;
                case 3: c = a->ext.compare(b->ext); break;
                default: c = a->lower.compare(b->lower); break;
            }
            return asc ? c < 0 : c > 0;
        });
        _selected = -1;
        for(std::size_t i = 0; i < _view.size(); ++i)
            if(_view[i]->path == _selectedPath) _selected = static_cast<int>(i);
        if(_selected < 0 && !_view.empty()) select(0);
    }

    void handleKeys() {
        const ImGuiIO& io = ImGui::GetIO();
        if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L)) _focusPath = true;
        if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) _focusQuery = true;
        if(ImGui::IsKeyPressed(ImGuiKey_F5)) {
            reload();
            if(searching()) startSearch();
        }
        if(io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) goBack();
        if(io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_RightArrow)) goForward();
        if(io.WantTextInput) return; // typing in the path or search box
        const int n = static_cast<int>(_view.size());
        if(ImGui::IsKeyPressed(ImGuiKey_Backspace)) goUp();
        if(n == 0) return;
        int step = 0;
        if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)) step = 1;
        if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)) step = -1;
        if(ImGui::IsKeyPressed(ImGuiKey_PageDown)) step = 20;
        if(ImGui::IsKeyPressed(ImGuiKey_PageUp)) step = -20;
        if(ImGui::IsKeyPressed(ImGuiKey_Home)) step = -n;
        if(ImGui::IsKeyPressed(ImGuiKey_End)) step = n;
        if(step != 0) {
            select(std::clamp(_selected + step, 0, n - 1));
            _scrollToSelected = true;
        }
        if(ImGui::IsKeyPressed(ImGuiKey_Enter) && selected()) activate(*selected());
    }

    void drawToolbar() {
        ImGui::BeginDisabled(_back.empty());
        if(ImGui::ArrowButton("##back", ImGuiDir_Left)) goBack();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(_forward.empty());
        if(ImGui::ArrowButton("##fwd", ImGuiDir_Right)) goForward();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::ArrowButton("##up", ImGuiDir_Up)) goUp();
        ImGui::SameLine();

        const float searchWidth = 300.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - searchWidth - 140.0f);
        if(_focusPath) {
            ImGui::SetKeyboardFocusHere();
            _focusPath = false;
        }
        if(ImGui::InputText("##path", _pathBuf, sizeof(_pathBuf), ImGuiInputTextFlags_EnterReturnsTrue)) navigate(fs::u8path(_pathBuf), true);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(searchWidth);
        if(_focusQuery) {
            ImGui::SetKeyboardFocusHere();
            _focusQuery = false;
        }
        if(ImGui::InputTextWithHint("##query", "filter (Ctrl+F)", _query, sizeof(_query))) {
            if(_recursive) startSearch();
            else _viewDirty = true;
        }
        ImGui::SameLine();
        if(ImGui::Checkbox("subfolders", &_recursive)) startSearch();
        ImGui::SameLine();
        if(ImGui::Checkbox("hidden", &_showHidden)) reload();
    }

    void drawPlaces() {
        ImGui::BeginChild("##places");
        for(const Place& p : _places)
            if(ImGui::Selectable(p.label.c_str(), p.path == _dir)) navigate(p.path, true);
        ImGui::EndChild();
    }

    void drawListing() {
        rebuildView();
        const ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
            | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersInnerV;
        if(!ImGui::BeginTable("##list", 4, flags)) return;
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(searching() ? "Path" : "Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableHeadersRow();

        ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
        if(specs && specs->SpecsDirty && specs->SpecsCount > 0) {
            _sortColumn = specs->Specs[0].ColumnIndex;
            _sortAscending = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
            specs->SpecsDirty = false;
            _viewDirty = true;
            rebuildView();
        }

        const ImVec4 dirColor(0.95f, 0.80f, 0.35f, 1.0f);
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(_view.size()));
        if(_scrollToSelected && _selected >= 0) clipper.IncludeItemByIndex(_selected);
        while(clipper.Step())
            for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const Entry& e = *_view[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                if(e.dir) ImGui::PushStyleColor(ImGuiCol_Text, dirColor);
                if(ImGui::Selectable(e.name.c_str(), i == _selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    select(i);
                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) _pendingActivate = i;
                }
                if(e.dir) ImGui::PopStyleColor();
                if(_scrollToSelected && i == _selected) {
                    ImGui::SetScrollHereY();
                    _scrollToSelected = false;
                }
                ImGui::PopID();
                ImGui::TableNextColumn();
                if(!e.dir) ImGui::TextUnformatted(formatSize(e.size).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(formatTime(e.modified).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(e.dir ? "folder" : (e.ext.empty() ? "" : e.ext.c_str() + 1));
            }
        ImGui::EndTable();
        // deferred: navigating frees the entries the rows above point into
        if(_pendingActivate >= 0 && _pendingActivate < static_cast<int>(_view.size())) {
            const Entry e = *_view[_pendingActivate];
            _pendingActivate = -1;
            activate(e);
        }
        _preview.select(selected());
    }

    void drawStatus() {
        std::uintmax_t bytes = 0;
        int files = 0;
        for(const Entry* e : _view)
            if(!e->dir) {
                bytes += e->size;
                ++files;
            }
        if(!_error.empty()) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", _error.c_str());
        else ImGui::Text("%zu items, %d files, %s", _view.size(), files, formatSize(bytes).c_str());
        if(searching()) {
            ImGui::SameLine();
            ImGui::TextDisabled("   %s: scanned %zu, found %zu%s", _search.running() ? "searching" : "done", _search.scanned(), _found.size(),
                _found.size() >= MaxSearchResults ? " (limit)" : "");
        }
    }
};

} // namespace

int main(int argc, char** argv) {
    fs::path start = fs::current_path();
    for(int i = 1; i < argc; ++i)
        if(argv[i][0] != '-') start = fs::u8path(argv[i]);

    smg::GuiConfig config;
    config.title = "smg files";
    config.size = { 1280, 800 };
    smg::GuiBase gui({ argc, argv }, config);

    Browser browser(start);
    gui.add_callback([&browser]() {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("##files", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
        browser.draw();
        ImGui::End();
        return 0;
    });

    while(gui.mainLoopIteration()) {}
    return 0;
}
