// fview: look at a folder of results. Images and CSV data show as a thumbnail grid; the lightbox steps
// through them with zoom/pan and a pixel readout, or a full plot for data. The folder is re-scanned
// every second, so pointing it at a run's output directory shows plots as they are written.
//   fview [folder | file] [-r] [-i]
// A file opens its folder with that file in the lightbox.
// -r includes subfolders, -i shows images only.
// Keys: Enter or click open, arrows step, Esc close, Backspace up, F5 rescan, Ctrl+wheel tile size,
// Ctrl+C copy image (+ file on Windows), Ctrl+Shift+C copy path (right-click a tile for both), Ctrl+V paste
// image/files into the folder (Windows), Ctrl+L path. On Windows a Linux path (~/run, /home/me/run) opens in WSL.

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/PluginManager/Manager.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

#include "GuiBase.hh"
#include "imgui.h"
#include "implot.h"

#include <SDL_clipboard.h>

#include "Capture.hh"

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#    include <shellapi.h>
#    include <shlobj.h> // DROPFILES
#endif

#if defined(__linux__)
#    include <csignal>
#    include <fcntl.h>
#    include <poll.h>
#    include <sys/wait.h>
#    include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr double Nan = std::numeric_limits<double>::quiet_NaN();
constexpr int ThumbSide = 320; // decoded thumbnail size; tiles scale it
constexpr int FullSide = 8192; // lightbox cap, inside every GL's max texture size
constexpr std::uintmax_t MaxFileBytes = std::uintmax_t{ 256 } * 1024 * 1024;
constexpr int ThumbSeries = 4; // data tiles plot the first few columns
constexpr int ThumbPoints = 400;

enum class Kind { Image, Data };

struct Item {
    fs::path path;
    std::string key; // utf-8 path, the cache key
    std::string name; // relative to the root
    std::string lower;
    std::string ext; // lowercase, no dot
    Kind kind = Kind::Image;
    std::uintmax_t size = 0;
    std::time_t modified = 0;
    fs::file_time_type stamp{};
};

struct Folder {
    fs::path path;
    std::string name;
    bool link = false;
    bool broken = false; // dangling link: listed, not enterable
};

struct Listing {
    fs::path root;
    std::vector<Folder> folders;
    std::vector<Item> items;
    std::string error;
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
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

// formats StbImageImporter decodes
bool isImageExt(const std::string& e) {
    static const char* exts[] = { "png", "jpg", "jpeg", "bmp", "tga", "gif", "psd", "hdr", "pic", "pnm", "ppm", "pgm" };
    return std::find(std::begin(exts), std::end(exts), e) != std::end(exts);
}

bool isDataExt(const std::string& e) { return e == "csv" || e == "tsv" || e == "dat"; }

// Places: home folders and drives, plus WSL distributions on Windows. A WSL distro's files are the
// share \\wsl.localhost\<distro>\ (Windows starts the distro on first access), so the native viewer
// reads them like any folder; Linux paths typed or passed in ("/home/me/run", "~/run") map to it.
struct Place {
    std::string label;
    fs::path path;
};

struct Distro {
    std::string name;
    unsigned uid = 1000;
    bool isDefault = false;
};

#if defined(_WIN32)
std::string narrow(const std::wstring& w) { return fs::path(w).u8string(); }

std::wstring regString(HKEY key, const wchar_t* sub, const wchar_t* value) {
    wchar_t buf[512] = {};
    DWORD size = sizeof(buf);
    if(RegGetValueW(key, sub, value, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) return {};
    return buf;
}

// registered distributions, from the registry (no wsl.exe launch)
std::vector<Distro> wslDistros() {
    std::vector<Distro> out;
    const wchar_t* lxss = L"Software\\Microsoft\\Windows\\CurrentVersion\\Lxss";
    HKEY root = nullptr;
    if(RegOpenKeyExW(HKEY_CURRENT_USER, lxss, 0, KEY_READ, &root) != ERROR_SUCCESS) return out;
    const std::wstring defaultId = regString(HKEY_CURRENT_USER, lxss, L"DefaultDistribution");
    wchar_t id[128];
    for(DWORD i = 0;; ++i) {
        DWORD len = 128;
        if(RegEnumKeyExW(root, i, id, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        Distro d;
        d.name = narrow(regString(root, id, L"DistributionName"));
        DWORD uid = 0;
        DWORD size = sizeof(uid);
        if(RegGetValueW(root, id, L"DefaultUid", RRF_RT_REG_DWORD, nullptr, &uid, &size) == ERROR_SUCCESS) d.uid = uid;
        d.isDefault = defaultId == id;
        if(!d.name.empty()) out.push_back(d);
    }
    RegCloseKey(root);
    std::stable_sort(out.begin(), out.end(), [](const Distro& a, const Distro& b) { return a.isDefault > b.isDefault; });
    return out;
}

fs::path wslRoot(const std::string& distro) { return fs::u8path("\\\\wsl.localhost\\" + distro + "\\"); }

// the default user's home, from the distro's own /etc/passwd
fs::path wslHome(const Distro& d) {
    std::ifstream passwd(wslRoot(d.name) / "etc" / "passwd");
    const std::string uid = ":" + std::to_string(d.uid) + ":";
    for(std::string line; std::getline(passwd, line);) {
        // name:x:uid:gid:gecos:home:shell
        const std::size_t first = line.find(':');
        const std::size_t second = first == std::string::npos ? first : line.find(':', first + 1);
        if(second == std::string::npos || line.compare(second, uid.size(), uid) != 0) continue;
        std::size_t pos = 0;
        for(int field = 0; field < 5 && pos != std::string::npos; ++field) pos = line.find(':', pos + 1);
        if(pos == std::string::npos) break;
        const std::string home = line.substr(pos + 1, line.find(':', pos + 1) - pos - 1);
        const std::size_t rel = home.find_first_not_of('/');
        if(rel == std::string::npos) return wslRoot(d.name);
        return wslRoot(d.name) / fs::u8path(home.substr(rel)).make_preferred();
    }
    return wslRoot(d.name) / "home";
}
#else
std::vector<Distro> wslDistros() { return {}; }
#endif

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
        for(const char* sub : { "Desktop", "Documents", "Downloads", "Pictures" }) {
            std::error_code ec;
            if(fs::is_directory(h / sub, ec)) out.push_back({ sub, h / sub });
        }
    }
#if defined(_WIN32)
    wchar_t drives[256] = {};
    const DWORD n = GetLogicalDriveStringsW(255, drives);
    for(const wchar_t* d = drives; d < drives + n && *d; d += std::wcslen(d) + 1) out.push_back({ narrow(d), fs::path(d) });
#else
    out.push_back({ "/", fs::path("/") });
#endif
    return out;
}

// \\?\UNC\server\share and \\?\C:\ (what canonical() returns for network paths) back to plain form
fs::path cleanPath(const fs::path& p) {
    const std::string s = utf8(p);
    if(s.rfind("\\\\?\\UNC\\", 0) == 0) return fs::u8path("\\\\" + s.substr(8));
    if(s.rfind("\\\\?\\", 0) == 0) return fs::u8path(s.substr(4));
    return p;
}

bool isWslPath(const fs::path& p) {
    const std::string s = toLower(utf8(p));
    return s.rfind("\\\\wsl.localhost\\", 0) == 0 || s.rfind("\\\\wsl$\\", 0) == 0;
}

// a user-typed or command-line location; on Windows, Linux paths go to the default WSL distro
fs::path resolveInput(const std::string& in) {
#if defined(_WIN32)
    if(!in.empty() && (in[0] == '/' || in[0] == '~')) {
        const std::vector<Distro> distros = wslDistros();
        if(!distros.empty()) {
            const Distro& d = distros.front();
            if(in[0] == '~') {
                const std::string rest = in.size() > 2 ? in.substr(2) : std::string();
                return rest.empty() ? wslHome(d) : wslHome(d) / fs::u8path(rest).make_preferred();
            }
            const std::string rest = in.substr(in.find_first_not_of('/') == std::string::npos ? in.size() : in.find_first_not_of('/'));
            return rest.empty() ? wslRoot(d.name) : wslRoot(d.name) / fs::u8path(rest).make_preferred();
        }
    }
#endif
    return fs::u8path(in);
}

std::string extensionOf(const fs::path& p) {
    const std::string e = toLower(utf8(p.extension()));
    return e.empty() ? e : e.substr(1);
}

Listing scan(const fs::path& root, bool recursive, bool imagesOnly) {
    Listing out;
    out.root = root;
    std::error_code ec;
    // never follows directory links, so a link back up the tree can't loop
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    if(ec) {
        out.error = ec.message();
        return out;
    }
    const fs::recursive_directory_iterator end;
    while(it != end) {
        const fs::directory_entry& e = *it;
        const std::string name = utf8(e.path().filename());
        const bool top = it.depth() == 0;
        if(!recursive || name.empty() || name[0] == '.') it.disable_recursion_pending();
        if(!name.empty() && name[0] != '.') {
            std::error_code sec;
            const bool link = e.is_symlink(sec);
            const bool exists = e.exists(sec); // follows the link
            if(!exists || e.is_directory(sec)) {
                if(top && (exists || link)) out.folders.push_back({ e.path(), name, link, !exists });
            } else {
                std::string ext = extensionOf(e.path());
                // `latest -> plot.png`: classify by the target
                if(ext.empty() && link) ext = extensionOf(fs::canonical(e.path(), sec));
                const bool image = isImageExt(ext);
                if(image || (!imagesOnly && isDataExt(ext))) {
                    Item item;
                    item.path = e.path();
                    item.key = utf8(e.path());
                    // lexical: fs::relative resolves links, so `latest` would show as its target
                    item.name = utf8(e.path().lexically_relative(root));
                    item.lower = toLower(item.name);
                    item.ext = ext;
                    item.kind = image ? Kind::Image : Kind::Data;
                    item.size = e.file_size(sec);
                    if(sec) item.size = 0;
                    item.stamp = e.last_write_time(sec);
                    item.modified = sec ? 0 : toTimeT(item.stamp);
                    out.items.push_back(std::move(item));
                }
            }
        }
        it.increment(ec);
        if(ec) {
            // unreadable subtree: step out of it and carry on
            ec.clear();
            if(it == end || it.depth() == 0) break;
            it.pop(ec);
            if(ec) break;
        }
    }
    std::sort(out.folders.begin(), out.folders.end(), [](const Folder& a, const Folder& b) { return toLower(a.name) < toLower(b.name); });
    return out;
}

// one scan at a time off the UI thread; a recursive tree can take a while
class Scanner {
public:
    ~Scanner() {
        if(_thread.joinable()) _thread.join();
    }

    [[nodiscard]] bool busy() const { return _busy; }

    void start(const fs::path& root, bool recursive, bool imagesOnly) {
        if(_busy) return;
        if(_thread.joinable()) _thread.join();
        _busy = true;
        _thread = std::thread([this, root, recursive, imagesOnly]() {
            Listing listing = scan(root, recursive, imagesOnly);
            std::lock_guard<std::mutex> lock(_mutex);
            _result = std::move(listing);
            _ready = true;
            _busy = false;
        });
    }

    bool take(Listing& out) {
        std::lock_guard<std::mutex> lock(_mutex);
        if(!_ready) return false;
        out = std::move(_result);
        _ready = false;
        return true;
    }

private:
    std::thread _thread;
    std::mutex _mutex;
    Listing _result;
    bool _ready = false;
    std::atomic<bool> _busy{ false };
};

struct Column {
    std::string name;
    std::vector<double> values;
};

struct Table {
    std::vector<Column> columns;
    std::size_t rows = 0;
};

bool parseNumber(const std::string& s, double& out) {
    const char* b = s.c_str();
    while(*b == ' ' || *b == '\t') ++b;
    if(*b == '\0') return false;
    char* end = nullptr;
    out = std::strtod(b, &end);
    while(*end == ' ' || *end == '\t') ++end;
    return *end == '\0';
}

std::vector<std::string> splitLine(const std::string& line, char delim) {
    std::vector<std::string> fields;
    if(delim == ' ') {
        std::istringstream ss(line);
        for(std::string f; ss >> f;) fields.push_back(f);
        return fields;
    }
    std::string cur;
    bool quoted = false;
    for(std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if(quoted) {
            if(c == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                cur += '"';
                ++i;
            } else if(c == '"') {
                quoted = false;
            } else {
                cur += c;
            }
        } else if(c == '"') {
            quoted = true;
        } else if(c == delim) {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    fields.push_back(cur);
    return fields;
}

char detectDelimiter(const std::string& line) {
    const std::size_t comma = std::count(line.begin(), line.end(), ',');
    const std::size_t semi = std::count(line.begin(), line.end(), ';');
    const std::size_t tab = std::count(line.begin(), line.end(), '\t');
    if(comma == 0 && semi == 0 && tab == 0) return ' ';
    if(tab >= comma && tab >= semi) return '\t';
    return semi > comma ? ';' : ',';
}

std::string trim(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t\"");
    if(b == std::string::npos) return {};
    return s.substr(b, s.find_last_not_of(" \t\"") - b + 1);
}

// same rules as csvplot: delimiter and header detected, # and % comments, bad cells become gaps
bool loadTable(const fs::path& path, Table& t, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if(!in) {
        error = "cannot open file";
        return false;
    }
    char delim = 0;
    bool first = true;
    for(std::string line; std::getline(in, line);) {
        if(!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t start = line.find_first_not_of(" \t");
        if(start == std::string::npos || line[start] == '#' || line[start] == '%') continue;
        if(!delim) delim = detectDelimiter(line);
        const std::vector<std::string> fields = splitLine(line, delim);
        if(first) {
            first = false;
            bool header = false;
            double v = 0.0;
            for(const std::string& f : fields)
                if(!trim(f).empty() && !parseNumber(f, v)) header = true;
            if(header) {
                for(const std::string& f : fields) t.columns.push_back({ trim(f), {} });
                continue;
            }
        }
        while(t.columns.size() < fields.size()) {
            Column c;
            c.name = "col " + std::to_string(t.columns.size() + 1);
            c.values.assign(t.rows, Nan);
            t.columns.push_back(std::move(c));
        }
        for(std::size_t c = 0; c < t.columns.size(); ++c) {
            double v = Nan;
            if(c >= fields.size() || !parseNumber(fields[c], v)) v = Nan;
            t.columns[c].values.push_back(v);
        }
        ++t.rows;
    }
    for(Column& c : t.columns) c.values.resize(t.rows, Nan);
    if(t.rows == 0) error = "no data rows";
    return t.rows > 0;
}

// tightly packed RGBA8, rows bottom-up as Magnum delivers them
struct Pixels {
    Magnum::Vector2i size;
    std::vector<std::uint8_t> rgba;
};

bool readableChannel(Magnum::PixelFormat channel) {
    return channel == Magnum::PixelFormat::R8Unorm || channel == Magnum::PixelFormat::R8Srgb || channel == Magnum::PixelFormat::R16Unorm ||
        channel == Magnum::PixelFormat::R32F;
}

float component(const char* p, Magnum::PixelFormat channel) {
    switch(channel) {
    case Magnum::PixelFormat::R8Unorm:
    case Magnum::PixelFormat::R8Srgb: return static_cast<float>(static_cast<std::uint8_t>(*p)) / 255.0f;
    case Magnum::PixelFormat::R16Unorm: {
        std::uint16_t v = 0;
        std::memcpy(&v, p, sizeof(v));
        return static_cast<float>(v) / 65535.0f;
    }
    case Magnum::PixelFormat::R32F: {
        float v = 0.0f;
        std::memcpy(&v, p, sizeof(v));
        return std::clamp(v, 0.0f, 1.0f); // HDR clips
    }
    default: return 0.0f;
    }
}

// box-filtered down to fit maxSide; grey and grey+alpha expand to RGBA
bool toRgba8(const Magnum::Trade::ImageData2D& image, int maxSide, Pixels& out) {
    if(image.isCompressed()) return false;
    const Magnum::PixelFormat format = image.format();
    if(Magnum::isPixelFormatImplementationSpecific(format) || Magnum::isPixelFormatDepthOrStencil(format)) return false;
    const Magnum::PixelFormat channel = Magnum::pixelFormatChannelFormat(format);
    if(!readableChannel(channel)) return false;
    const unsigned channels = Magnum::pixelFormatChannelCount(format);
    const std::size_t channelBytes = Magnum::pixelFormatSize(channel);

    const Magnum::Vector2i src = image.size();
    if(src.x() <= 0 || src.y() <= 0) return false;
    const int step = std::max(1, (std::max(src.x(), src.y()) + maxSide - 1) / maxSide);
    out.size = { (src.x() + step - 1) / step, (src.y() + step - 1) / step };
    out.rgba.assign(static_cast<std::size_t>(out.size.product()) * 4, 0);
    const Corrade::Containers::StridedArrayView3D<const char> px = image.pixels();

    for(int y = 0; y < out.size.y(); ++y)
        for(int x = 0; x < out.size.x(); ++x) {
            float sum[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            int n = 0;
            for(int sy = y * step; sy < std::min(src.y(), (y + 1) * step); ++sy)
                for(int sx = x * step; sx < std::min(src.x(), (x + 1) * step); ++sx) {
                    const char* p = static_cast<const char*>(px[static_cast<std::size_t>(sy)][static_cast<std::size_t>(sx)].data());
                    for(unsigned c = 0; c < channels; ++c) sum[c] += component(p + c * channelBytes, channel);
                    ++n;
                }
            float rgba[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            if(channels <= 2) {
                rgba[0] = rgba[1] = rgba[2] = sum[0] / static_cast<float>(n);
                if(channels == 2) rgba[3] = sum[1] / static_cast<float>(n);
            } else {
                for(unsigned c = 0; c < channels; ++c) rgba[c] = sum[c] / static_cast<float>(n);
            }
            std::uint8_t* dst = out.rgba.data() + (static_cast<std::size_t>(y) * out.size.x() + x) * 4;
            for(int c = 0; c < 4; ++c) dst[c] = static_cast<std::uint8_t>(std::lround(rgba[c] * 255.0f));
        }
    return true;
}

Magnum::GL::Texture2D makeTexture(const Pixels& p, bool crispZoom) {
    Magnum::GL::Texture2D texture;
    const int levels = static_cast<int>(Magnum::Math::log2(static_cast<Magnum::UnsignedInt>(std::max(p.size.x(), p.size.y())))) + 1;
    const Corrade::Containers::ArrayView<const void> data{ p.rgba.data(), p.rgba.size() };
    texture.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
        .setMinificationFilter(Magnum::GL::SamplerFilter::Linear, Magnum::GL::SamplerMipmap::Linear)
        .setMagnificationFilter(crispZoom ? Magnum::GL::SamplerFilter::Nearest : Magnum::GL::SamplerFilter::Linear)
        .setStorage(levels, Magnum::GL::TextureFormat::RGBA8, p.size)
        .setSubImage(0, {}, Magnum::ImageView2D{ Magnum::PixelStorage{}.setAlignment(1), Magnum::PixelFormat::RGBA8Unorm, p.size, data })
        .generateMipmap();
    return texture;
}

// Clipboard. Windows: a copied image goes out as a bitmap (pastes into chat, slides, Paint) plus the
// file itself (pastes into Explorer); a pasted bitmap or file list comes back in. Elsewhere only the
// path is copied, as text.
#if defined(_WIN32)
HGLOBAL globalCopy(const void* data, std::size_t n) {
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n);
    if(!h) return nullptr;
    std::memcpy(GlobalLock(h), data, n);
    GlobalUnlock(h);
    return h;
}

// rgba rows run bottom-up (Magnum's convention), as in a DIB with positive height
bool copyToClipboard(const Pixels* image, const fs::path& file) {
    if(!OpenClipboard(nullptr)) return false;
    EmptyClipboard();
    if(image && !image->rgba.empty()) {
        BITMAPINFOHEADER bih = {};
        bih.biSize = sizeof(bih);
        bih.biWidth = image->size.x();
        bih.biHeight = image->size.y();
        bih.biPlanes = 1;
        bih.biBitCount = 32;
        bih.biCompression = BI_RGB;
        std::vector<std::uint8_t> dib(sizeof(bih) + image->rgba.size());
        std::memcpy(dib.data(), &bih, sizeof(bih));
        std::uint8_t* px = dib.data() + sizeof(bih);
        for(std::size_t i = 0; i < image->rgba.size(); i += 4) {
            px[i + 0] = image->rgba[i + 2];
            px[i + 1] = image->rgba[i + 1];
            px[i + 2] = image->rgba[i + 0];
            px[i + 3] = image->rgba[i + 3];
        }
        if(HGLOBAL h = globalCopy(dib.data(), dib.size())) SetClipboardData(CF_DIB, h);
    }
    const std::wstring w = file.wstring();
    std::vector<std::uint8_t> drop(sizeof(DROPFILES) + (w.size() + 2) * sizeof(wchar_t), 0); // double-NUL list
    DROPFILES* df = reinterpret_cast<DROPFILES*>(drop.data());
    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;
    std::memcpy(drop.data() + sizeof(DROPFILES), w.c_str(), w.size() * sizeof(wchar_t));
    if(HGLOBAL h = globalCopy(drop.data(), drop.size())) SetClipboardData(CF_HDROP, h);
    CloseClipboard();
    return true;
}

// a 24/32-bit clipboard bitmap (screenshots, "copy image") as bottom-up RGBA
bool clipboardImage(Pixels& out) {
    if(!IsClipboardFormatAvailable(CF_DIB) || !OpenClipboard(nullptr)) return false;
    bool ok = false;
    if(HANDLE h = GetClipboardData(CF_DIB)) {
        const std::uint8_t* base = static_cast<const std::uint8_t*>(GlobalLock(h));
        const BITMAPINFOHEADER* bih = reinterpret_cast<const BITMAPINFOHEADER*>(base);
        const int bpp = bih->biBitCount;
        if(base && (bpp == 24 || bpp == 32) && (bih->biCompression == BI_RGB || bih->biCompression == BI_BITFIELDS)) {
            const int w = bih->biWidth;
            const int hgt = std::abs(bih->biHeight);
            const bool topDown = bih->biHeight < 0;
            // BITFIELDS with the plain header carries three colour masks after it
            const std::size_t masks = bih->biCompression == BI_BITFIELDS && bih->biSize == sizeof(BITMAPINFOHEADER) ? 12 : 0;
            const std::uint8_t* bits = base + bih->biSize + masks;
            const std::size_t stride = ((static_cast<std::size_t>(w) * bpp + 31) / 32) * 4;
            out.size = { w, hgt };
            out.rgba.assign(static_cast<std::size_t>(w) * hgt * 4, 255);
            bool anyAlpha = false;
            for(int y = 0; y < hgt; ++y) {
                const std::uint8_t* row = bits + stride * static_cast<std::size_t>(topDown ? hgt - 1 - y : y);
                for(int x = 0; x < w; ++x) {
                    const std::uint8_t* s = row + static_cast<std::size_t>(x) * (bpp / 8);
                    std::uint8_t* d = &out.rgba[(static_cast<std::size_t>(y) * w + x) * 4];
                    d[0] = s[2];
                    d[1] = s[1];
                    d[2] = s[0];
                    if(bpp == 32) {
                        d[3] = s[3];
                        anyAlpha = anyAlpha || s[3] != 0;
                    }
                }
            }
            // most 32-bit clipboard bitmaps leave alpha at 0 and mean opaque
            if(bpp == 32 && !anyAlpha)
                for(std::size_t i = 3; i < out.rgba.size(); i += 4) out.rgba[i] = 255;
            ok = w > 0 && hgt > 0;
        }
        GlobalUnlock(h);
    }
    CloseClipboard();
    return ok;
}

std::vector<fs::path> clipboardFiles() {
    std::vector<fs::path> files;
    if(!IsClipboardFormatAvailable(CF_HDROP) || !OpenClipboard(nullptr)) return files;
    if(HANDLE h = GetClipboardData(CF_HDROP)) {
        HDROP drop = static_cast<HDROP>(GlobalLock(h));
        const UINT n = drop ? DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0) : 0;
        for(UINT i = 0; i < n; ++i) {
            std::wstring name(DragQueryFileW(drop, i, nullptr, 0) + 1, L'\0');
            DragQueryFileW(drop, i, name.data(), static_cast<UINT>(name.size()));
            name.resize(name.size() - 1);
            files.emplace_back(name);
        }
        GlobalUnlock(h);
    }
    CloseClipboard();
    return files;
}
#endif

// name.ext, or name (2).ext, ... so a paste never overwrites
fs::path freeName(const fs::path& dir, const std::string& stem, const std::string& ext) {
    std::error_code ec;
    fs::path p = dir / fs::u8path(stem + ext);
    for(int i = 2; fs::exists(p, ec); ++i) p = dir / fs::u8path(stem + " (" + std::to_string(i) + ")" + ext);
    return p;
}

struct Job {
    fs::path path;
    std::string key;
    Kind kind = Kind::Image;
    bool full = false; // lightbox resolution, not a thumbnail
    fs::file_time_type stamp{};
};

struct Result {
    Job job;
    Pixels pixels;
    Magnum::Vector2i sourceSize;
    std::shared_ptr<const Table> table;
    std::string error;
};

// decodes and parses off the UI thread. One thread: it owns the plugin manager, which isn't
// thread-safe. Newest request first, so whatever is on screen now loads before what scrolled past
class Worker {
public:
    Worker() : _thread([this]() { run(); }) {}

    ~Worker() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stop = true;
        }
        _wake.notify_one();
        _thread.join();
    }

    void request(Job job) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if(!_queued.insert(queueKey(job)).second) return;
            _jobs.push_back(std::move(job));
        }
        _wake.notify_one();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _jobs.clear();
        _queued.clear();
    }

    std::vector<Result> take() {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<Result> out;
        out.swap(_done);
        return out;
    }

    std::size_t pending() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _jobs.size() + (_busy ? 1 : 0);
    }

private:
    std::mutex _mutex;
    std::condition_variable _wake;
    std::deque<Job> _jobs;
    std::set<std::string> _queued;
    std::vector<Result> _done;
    bool _stop = false;
    bool _busy = false;
    std::thread _thread; // last: starts once everything above exists

    static std::string queueKey(const Job& j) { return (j.full ? "F" : "T") + j.key; }

    void run() {
        Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter> importers;
        for(;;) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _wake.wait(lock, [this]() { return _stop || !_jobs.empty(); });
                if(_stop) return;
                job = std::move(_jobs.back());
                _jobs.pop_back();
                _queued.erase(queueKey(job));
                _busy = true;
            }
            Result r = process(importers, std::move(job));
            std::lock_guard<std::mutex> lock(_mutex);
            _done.push_back(std::move(r));
            _busy = false;
        }
    }

    static Result process(Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter>& importers, Job job) {
        Result r;
        std::error_code ec;
        // the importer picks a format by extension, which a link like `latest` lacks
        fs::path source = fs::canonical(job.path, ec);
        if(ec) source = job.path;
        const std::uintmax_t bytes = fs::file_size(source, ec);
        if(ec || bytes > MaxFileBytes) {
            r.error = ec ? "unreadable" : "too large";
        } else if(job.kind == Kind::Data) {
            std::shared_ptr<Table> t = std::make_shared<Table>();
            if(loadTable(source, *t, r.error)) r.table = std::move(t);
        } else {
            Corrade::Containers::Pointer<Magnum::Trade::AbstractImporter> importer = importers.loadAndInstantiate("AnyImageImporter");
            Corrade::Containers::Optional<Magnum::Trade::ImageData2D> image;
            if(!importer)
                r.error = "no image importer plugins";
            else if(importer->openFile(utf8(source)))
                image = importer->image2D(0);
            if(image) {
                r.sourceSize = image->size();
                if(!toRgba8(*image, job.full ? FullSide : ThumbSide, r.pixels)) r.error = "unsupported pixel format";
            } else if(r.error.empty()) {
                r.error = "cannot decode";
            }
        }
        r.job = std::move(job);
        return r;
    }
};

struct Thumb {
    fs::file_time_type stamp{}; // version on screen
    bool pending = false;
    Corrade::Containers::Optional<Magnum::GL::Texture2D> texture;
    Magnum::Vector2i sourceSize;
    std::shared_ptr<const Table> table;
    std::string error;
};

struct Full {
    std::string key;
    fs::file_time_type stamp{};
    bool pending = false;
    Corrade::Containers::Optional<Magnum::GL::Texture2D> texture;
    Pixels pixels; // CPU copy for the readout
    Magnum::Vector2i sourceSize;
    std::string error;
};

#if defined(__linux__)
// formats the clipboard tools (and Windows' Image.FromFile) take as-is
std::string clipboardMime(const std::string& ext) {
    if(ext == "png") return "image/png";
    if(ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if(ext == "bmp") return "image/bmp";
    if(ext == "gif") return "image/gif";
    return {};
}

std::string findInPath(const char* name) {
    const char* path = std::getenv("PATH");
    std::stringstream dirs(path ? path : "");
    for(std::string dir; std::getline(dirs, dir, ':');) {
        std::string candidate = dir + "/" + name;
        if(!dir.empty() && access(candidate.c_str(), X_OK) == 0) return candidate;
    }
    return {};
}

std::string base64(const std::string& in) {
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::uint32_t buffer = 0;
    int bits = 0;
    for(const char c : in) {
        buffer = (buffer << 8) | static_cast<std::uint8_t>(c);
        bits += 8;
        while(bits >= 6) {
            bits -= 6;
            out += table[(buffer >> bits) & 0x3F];
        }
    }
    if(bits > 0) out += table[(buffer << (6 - bits)) & 0x3F];
    while(out.size() % 4 != 0) out += '=';
    return out;
}

// child with stdin from `input` (or /dev/null), stdout into a pipe when `output` is set; returns its exit code
int run(const std::vector<std::string>& args, const char* input, std::string* output) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for(const std::string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    int out[2] = { -1, -1 };
    if(output && pipe(out) != 0) return -1;
    const int in = open(input ? input : "/dev/null", O_RDONLY | O_CLOEXEC);
    if(in < 0) return -1;
    const pid_t pid = fork();
    if(pid == 0) {
        // xclip and wl-copy stay behind to serve the selection: they mustn't hold our stdout/stderr
        const int null = open("/dev/null", O_WRONLY);
        dup2(in, STDIN_FILENO);
        dup2(output ? out[1] : null, STDOUT_FILENO);
        dup2(null, STDERR_FILENO);
        execv(argv[0], argv.data());
        _exit(127);
    }
    close(in);
    if(output) {
        close(out[1]);
        char buf[512];
        for(ssize_t n; (n = read(out[0], buf, sizeof(buf))) > 0;) output->append(buf, static_cast<std::size_t>(n));
        close(out[0]);
    }
    int status = 0;
    if(pid < 0 || waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// one PowerShell kept running: it takes seconds to start, so only the first copy pays for it
class PowerShell {
public:
    ~PowerShell() { stop(); }

    // runs one line, returns the reply ("smg-ok" or "smg-err ...") or an error
    std::string command(const std::string& line) {
        if(_pid < 0 && !start()) return "smg-err cannot start powershell.exe";
        const std::string text = line + "\n";
        if(write(_in, text.data(), text.size()) != static_cast<ssize_t>(text.size())) {
            stop();
            return "smg-err powershell exited";
        }
        // first reply waits for startup
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        for(;;) {
            const std::size_t eol = _buffer.find('\n');
            if(eol != std::string::npos) {
                std::string reply = _buffer.substr(0, eol);
                _buffer.erase(0, eol + 1);
                if(!reply.empty() && reply.back() == '\r') reply.pop_back();
                if(reply.rfind("smg-", 0) == 0) return reply;
                continue; // anything else PowerShell printed
            }
            const long left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            pollfd p{ _out, POLLIN, 0 };
            char buf[512];
            const ssize_t n = left > 0 && poll(&p, 1, static_cast<int>(left)) > 0 ? read(_out, buf, sizeof(buf)) : 0;
            if(n <= 0) {
                stop();
                return "smg-err powershell did not answer";
            }
            _buffer.append(buf, static_cast<std::size_t>(n));
        }
    }

private:
    pid_t _pid = -1;
    int _in = -1;
    int _out = -1;
    std::string _buffer;

    bool start() {
        std::string exe = findInPath("powershell.exe");
        // WSL's appendWindowsPath=false leaves it off PATH
        if(exe.empty()) exe = "/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe";
        if(access(exe.c_str(), X_OK) != 0) return false;
        int in[2];
        int out[2];
        if(pipe(in) != 0) return false;
        if(pipe(out) != 0) {
            close(in[0]);
            close(in[1]);
            return false;
        }
        const char* argv[] = { exe.c_str(), "-NoProfile", "-NonInteractive", "-STA", "-Command", "-", nullptr };
        _pid = fork();
        if(_pid == 0) {
            dup2(in[0], STDIN_FILENO);
            dup2(out[1], STDOUT_FILENO);
            close(in[1]);
            close(out[0]);
            execv(argv[0], const_cast<char* const*>(argv));
            _exit(127);
        }
        close(in[0]);
        close(out[1]);
        _in = in[1];
        _out = out[0];
        fcntl(_in, F_SETFD, FD_CLOEXEC);
        fcntl(_out, F_SETFD, FD_CLOEXEC);
        if(_pid < 0) {
            stop();
            return false;
        }
        return command("Add-Type -AssemblyName System.Windows.Forms,System.Drawing; 'smg-ok'") == "smg-ok";
    }

    void stop() {
        if(_in >= 0) close(_in); // PowerShell exits at end of input
        if(_out >= 0) close(_out);
        if(_pid > 0) waitpid(_pid, nullptr, 0);
        _in = _out = _pid = -1;
        _buffer.clear();
    }
};

// SDL2's clipboard is text-only, so images go through the platform's own tool: PowerShell on WSL
// (WSLg doesn't bridge images), wl-copy on Wayland, xclip on X11. Copies run off the UI thread
class ImageClipboard {
public:
    ~ImageClipboard() {
        if(_thread.joinable()) _thread.join();
    }

    void copy(const fs::path& file) {
        if(_busy) return;
        if(_thread.joinable()) _thread.join();
        _busy = true;
        report("copying " + utf8(file.filename()) + "...");
        _thread = std::thread([this, file]() {
            report(copyNow(file));
            _busy = false;
        });
    }

    void report(std::string message) {
        std::lock_guard<std::mutex> lock(_mutex);
        _status = std::move(message);
        _when = std::chrono::steady_clock::now();
    }

    // the last message while it's fresh
    std::string status() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _busy || std::chrono::steady_clock::now() - _when < std::chrono::seconds(5) ? _status : std::string();
    }

private:
    std::thread _thread;
    std::mutex _mutex;
    std::string _status;
    std::chrono::steady_clock::time_point _when{};
    std::atomic<bool> _busy{ false };
    PowerShell _powershell; // worker thread only

    std::string copyNow(const fs::path& link) {
        std::error_code ec;
        fs::path file = fs::canonical(link, ec);
        if(ec) file = link;
        const std::string mime = clipboardMime(extensionOf(file));
        if(mime.empty()) return "copying images supports png, jpg, bmp and gif";
        const std::string done = "copied " + utf8(link.filename());
        if(std::getenv("WSL_DISTRO_NAME")) {
            std::string windowsPath;
            const std::string wslpath = findInPath("wslpath");
            if(wslpath.empty() || run({ wslpath, "-w", file.string() }, nullptr, &windowsPath) != 0) return "wslpath failed";
            while(!windowsPath.empty() && (windowsPath.back() == '\n' || windowsPath.back() == '\r')) windowsPath.pop_back();
            // base64 so no path character can break out of the script
            const std::string reply = _powershell.command("try { $p = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('" +
                base64(windowsPath) +
                "')); $i = [System.Drawing.Image]::FromFile($p); [System.Windows.Forms.Clipboard]::SetImage($i); $i.Dispose(); 'smg-ok' } "
                "catch { 'smg-err ' + $_.Exception.Message }");
            return reply == "smg-ok" ? done : reply.substr(std::min<std::size_t>(reply.size(), 8));
        }
        const std::string wlcopy = findInPath("wl-copy");
        if(std::getenv("WAYLAND_DISPLAY") && !wlcopy.empty())
            return run({ wlcopy, "--type", mime }, file.c_str(), nullptr) == 0 ? done : "wl-copy failed";
        const std::string xclip = findInPath("xclip");
        if(std::getenv("DISPLAY") && !xclip.empty())
            return run({ xclip, "-selection", "clipboard", "-t", mime, "-i", file.string() }, nullptr, nullptr) == 0 ? done
                                                                                                                     : "xclip failed";
        return "install wl-clipboard or xclip to copy images";
    }
};
#endif

enum class SortKey { Name, Date, Size, Type };

class Viewer {
public:
    Viewer(const fs::path& start, bool recursive, bool imagesOnly) : _recursive(recursive), _imagesOnly(imagesOnly) {
        std::error_code ec;
        if(!fs::is_regular_file(start, ec)) {
            navigate(start);
            return;
        }
        // a file: its folder, with the file open once the scan lists it
        navigate(start.has_parent_path() ? start.parent_path() : fs::path("."));
        _pendingOpen = utf8(_root / start.filename());
    }

    void draw() {
        receive();
        refresh();
        rebuildView();
        handleKeys();
        if(_open.empty()) {
            drawToolbar();
            drawGrid();
        } else {
            drawLightbox();
        }
        drawStatus();
    }

private:
    fs::path _root;
    Listing _listing;
    std::vector<int> _view; // indices into _listing.items, filtered and sorted
    std::map<std::string, Thumb> _thumbs;
    Full _full;
    Scanner _scanner;
    Worker _worker;
#if defined(__linux__)
    ImageClipboard _clipboard;
#endif
    bool _recursive = false;
    bool _imagesOnly = false;
    bool _live = true;
    bool _viewDirty = true;
    bool _scanned = false;
    float _rescanClock = 0.0f;
    float _tile = 200.0f;
    int _cols = 1;
    SortKey _sort = SortKey::Name;
    bool _ascending = true;
    char _filter[256] = {};
    std::string _selected; // key; survives rescans
    bool _scrollToSelected = false;
    std::string _open; // lightbox item key, empty when closed
    std::string _pendingOpen;
    bool _fitOpen = true;
    int _dataX = 0; // lightbox x column, -1 = row index
    bool _logY = false;
    std::vector<Place> _places = places();
    std::vector<Distro> _distros = wslDistros();
    char _pathBuf[1024] = {};
    bool _focusPath = false;
    Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter> _clipImporters; // the worker's lives on its thread
    std::string _notice;
    float _noticeTime = 0.0f;

    void navigate(fs::path dir) {
        std::error_code ec;
        fs::path canon = cleanPath(fs::weakly_canonical(dir, ec));
        if(ec) canon = std::move(dir);
        if(!fs::is_directory(canon, ec)) return;
        _root = canon;
        std::snprintf(_pathBuf, sizeof(_pathBuf), "%s", utf8(_root).c_str());
        _listing = Listing{};
        _listing.root = _root;
        _view.clear();
        _thumbs.clear();
        _worker.clear();
        _selected.clear();
        _open.clear();
        _filter[0] = '\0';
        _scanned = false;
        rescan();
    }

    void rescan() {
        _rescanClock = 0.0f;
        _scanner.start(_root, _recursive, _imagesOnly);
    }

    void refresh() {
        Listing fresh;
        if(_scanner.take(fresh) && fresh.root == _root) {
            _listing = std::move(fresh);
            _scanned = true;
            _viewDirty = true;
        }
        _rescanClock += ImGui::GetIO().DeltaTime;
        // a flag change while a scan runs is picked up by the next one. Every listing of a WSL folder
        // crosses into the VM, so those rescan less often
        const float interval = isWslPath(_root) ? 3.0f : 1.0f;
        if((_live && _rescanClock >= interval) || !_scanned) {
            if(!_scanner.busy()) rescan();
        }
    }

    void receive() {
        for(Result& r : _worker.take()) {
            if(r.job.full) {
                if(r.job.key != _full.key) continue;
                _full.pending = false;
                _full.stamp = r.job.stamp;
                _full.error = r.error;
                // a half-written file fails to decode: keep the last good frame
                if(!r.error.empty()) continue;
                _full.texture = makeTexture(r.pixels, true);
                _full.pixels = std::move(r.pixels);
                _full.sourceSize = r.sourceSize;
                continue;
            }
            const std::map<std::string, Thumb>::iterator it = _thumbs.find(r.job.key);
            if(it == _thumbs.end()) continue; // from a folder we've left
            Thumb& t = it->second;
            t.pending = false;
            t.stamp = r.job.stamp;
            t.error = r.error;
            if(!r.error.empty()) continue;
            if(r.job.kind == Kind::Image) {
                t.texture = makeTexture(r.pixels, false);
                t.sourceSize = r.sourceSize;
            } else {
                t.table = std::move(r.table);
            }
        }
    }

    void rebuildView() {
        if(!_viewDirty) return;
        _viewDirty = false;
        const std::vector<Item>& items = _listing.items;
        const std::string q = toLower(_filter);
        _view.clear();
        for(std::size_t i = 0; i < items.size(); ++i)
            if(q.empty() || items[i].lower.find(q) != std::string::npos) _view.push_back(static_cast<int>(i));
        const SortKey key = _sort;
        const bool asc = _ascending;
        std::stable_sort(_view.begin(), _view.end(), [&items, key, asc](int ia, int ib) {
            const Item& a = items[ia];
            const Item& b = items[ib];
            int c = 0;
            switch(key) {
            case SortKey::Date: c = a.stamp < b.stamp ? -1 : (b.stamp < a.stamp ? 1 : 0); break;
            case SortKey::Size: c = a.size < b.size ? -1 : (a.size > b.size ? 1 : 0); break;
            case SortKey::Type: c = a.ext.compare(b.ext); break;
            case SortKey::Name: break;
            }
            if(c == 0) c = a.lower.compare(b.lower); // also the tiebreak
            return asc ? c < 0 : c > 0;
        });
        if(!_pendingOpen.empty() && _scanned) {
            const int at = indexOf(_pendingOpen);
            _pendingOpen.clear();
            if(at >= 0) openAt(at);
        }
        if(!_open.empty() && indexOf(_open) < 0) _open.clear(); // deleted under us
        if(indexOf(_selected) < 0) _selected = _view.empty() ? std::string() : item(0).key;
    }

    [[nodiscard]] const Item& item(int viewIndex) const {
        return _listing.items[static_cast<std::size_t>(_view[static_cast<std::size_t>(viewIndex)])];
    }

    [[nodiscard]] int indexOf(const std::string& key) const {
        if(key.empty()) return -1;
        for(std::size_t i = 0; i < _view.size(); ++i)
            if(item(static_cast<int>(i)).key == key) return static_cast<int>(i);
        return -1;
    }

    Thumb& thumbFor(const Item& it) {
        Thumb& t = _thumbs[it.key];
        if(!t.pending && t.stamp != it.stamp) {
            t.pending = true;
            _worker.request({ it.path, it.key, it.kind, false, it.stamp });
        }
        return t;
    }

    void openAt(int viewIndex) {
        const int n = static_cast<int>(_view.size());
        if(n == 0) return;
        const int i = ((viewIndex % n) + n) % n; // wraps both ways
        _open = item(i).key;
        _selected = _open;
        _scrollToSelected = true;
        _fitOpen = true;
        _dataX = 0;
    }

    void notify(std::string message) {
        _notice = std::move(message);
        _noticeTime = 4.0f;
    }

    // the lightbox item, else the selected tile
    const Item* current() {
        const int i = indexOf(_open.empty() ? _selected : _open);
        return i < 0 ? nullptr : &item(i);
    }

    bool decodeForCopy(const fs::path& path, Pixels& out) {
        std::error_code ec;
        fs::path source = fs::canonical(path, ec);
        if(ec) source = path;
        Corrade::Containers::Pointer<Magnum::Trade::AbstractImporter> importer = _clipImporters.loadAndInstantiate("AnyImageImporter");
        if(!importer || !importer->openFile(utf8(source))) return false;
        Corrade::Containers::Optional<Magnum::Trade::ImageData2D> image = importer->image2D(0);
        return image && toRgba8(*image, FullSide, out);
    }

    // Ctrl+C: the image (and the file); Ctrl+Shift+C: the path as text
    void copy(bool pathOnly) {
        const Item* it = current();
        if(!it) return;
        if(pathOnly) {
            SDL_SetClipboardText(utf8(it->path).c_str());
            notify("copied path");
            return;
        }
#if defined(_WIN32)
        Pixels decoded;
        const Pixels* image = nullptr;
        if(it->kind == Kind::Image) {
            // the lightbox already holds the decoded pixels
            if(_full.key == it->key && !_full.pixels.rgba.empty())
                image = &_full.pixels;
            else if(decodeForCopy(it->path, decoded))
                image = &decoded;
        }
        if(!copyToClipboard(image, it->path))
            notify("clipboard busy, try again");
        else if(image)
            notify("copied image " + std::to_string(image->size.x()) + " x " + std::to_string(image->size.y()));
        else
            notify("copied file");
#elif defined(__linux__)
        // async; progress shows through _clipboard.status()
        if(it->kind == Kind::Image) {
            _clipboard.copy(it->path);
            return;
        }
        SDL_SetClipboardText(utf8(it->path).c_str());
        notify("copied path");
#else
        SDL_SetClipboardText(utf8(it->path).c_str());
        notify("copied path");
#endif
    }

    // Ctrl+V: copied files are copied into this folder; a bitmap is saved here as PNG
    void paste() {
#if defined(_WIN32)
        const std::vector<fs::path> files = clipboardFiles();
        if(!files.empty()) {
            int copied = 0;
            for(const fs::path& f : files) {
                std::error_code ec;
                if(!fs::is_regular_file(f, ec)) continue;
                const fs::path dst = freeName(_root, utf8(f.stem()), utf8(f.extension()));
                if(fs::copy_file(f, dst, ec)) {
                    ++copied;
                    _selected = utf8(dst);
                }
            }
            notify("pasted " + std::to_string(copied) + " file(s)");
            rescan();
            return;
        }
        Pixels image;
        if(clipboardImage(image)) {
            const std::time_t now = std::time(nullptr);
            char stem[48];
            std::strftime(stem, sizeof(stem), "pasted %Y-%m-%d %H%M%S", std::localtime(&now));
            const fs::path dst = freeName(_root, stem, ".png");
            const Magnum::ImageView2D view{ Magnum::PixelFormat::RGBA8Unorm,
                image.size,
                Corrade::Containers::ArrayView<const void>{ image.rgba.data(), image.rgba.size() } };
            if(smg::save_image(view, utf8(dst))) {
                _selected = utf8(dst);
                _scrollToSelected = true;
                notify("pasted " + utf8(dst.filename()));
                rescan();
            } else {
                notify("could not save " + utf8(dst.filename()));
            }
            return;
        }
        notify("clipboard has no image or files");
#else
        notify("paste is Windows-only for now");
#endif
    }

    void handleKeys() {
        const ImGuiIO& io = ImGui::GetIO();
        if(io.WantTextInput) return;
        const int n = static_cast<int>(_view.size());
        if(ImGui::IsKeyPressed(ImGuiKey_F5)) rescan();
        if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) copy(io.KeyShift);
        if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) paste();
        if(!_open.empty()) {
            const int i = indexOf(_open);
            if(ImGui::IsKeyPressed(ImGuiKey_Escape))
                _open.clear();
            else if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))
                openAt(i - 1);
            else if(ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow) ||
                ImGui::IsKeyPressed(ImGuiKey_Space))
                openAt(i + 1);
            return;
        }
        if(ImGui::IsKeyPressed(ImGuiKey_Backspace) && _root.has_parent_path() && _root.parent_path() != _root) {
            navigate(_root.parent_path());
            return;
        }
        if(n == 0) return;
        int step = 0;
        if(ImGui::IsKeyPressed(ImGuiKey_RightArrow)) step = 1;
        if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) step = -1;
        if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)) step = _cols;
        if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)) step = -_cols;
        if(ImGui::IsKeyPressed(ImGuiKey_Home)) step = -n;
        if(ImGui::IsKeyPressed(ImGuiKey_End)) step = n;
        const int cur = std::max(0, indexOf(_selected));
        if(step != 0) {
            _selected = item(std::clamp(cur + step, 0, n - 1)).key;
            _scrollToSelected = true;
        }
        if(ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) openAt(cur);
    }

    void drawBreadcrumbs() {
        fs::path acc;
        std::string prev;
        // MSVC splits \\wsl.localhost\Ubuntu\home as "\\wsl.localhost", "\", "Ubuntu", "home":
        // show that as "Ubuntu: / home"
        int skip = isWslPath(_root) ? 2 : 0;
        for(const fs::path& part : _root) {
            acc /= part;
            std::string label = utf8(part);
            if(label.empty()) continue;
            if(skip > 0) {
                --skip;
                continue;
            }
            if(isWslPath(_root) && prev.empty()) label += ":";
            if(!prev.empty()) {
                ImGui::SameLine(0.0f, 2.0f);
                // the root button already reads "/"
                if(prev != "/" && prev != "\\") {
                    ImGui::TextDisabled("/");
                    ImGui::SameLine(0.0f, 2.0f);
                }
            }
            prev = label;
            ImGui::PushID(utf8(acc).c_str());
            if(ImGui::SmallButton(label.c_str())) {
                ImGui::PopID();
                navigate(acc);
                return; // the components we're walking just changed
            }
            ImGui::PopID();
        }
    }

    void drawPlaces() {
        if(ImGui::Button("Go to")) ImGui::OpenPopup("##places");
        if(!ImGui::BeginPopup("##places")) return;
        for(const Place& p : _places)
            if(ImGui::MenuItem(p.label.c_str())) navigate(p.path);
#if defined(_WIN32)
        if(!_distros.empty()) {
            ImGui::SeparatorText("WSL");
            for(const Distro& d : _distros) {
                ImGui::PushID(d.name.c_str());
                const std::string home = d.name + (d.isDefault ? "  ~  (default)" : "  ~");
                // reading the distro's passwd starts it if it is stopped, so only on click
                if(ImGui::MenuItem(home.c_str())) navigate(wslHome(d));
                if(ImGui::MenuItem((d.name + "  /").c_str())) navigate(wslRoot(d.name));
                ImGui::PopID();
            }
        }
#endif
        ImGui::EndPopup();
    }

    void drawPathBox() {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        if(_focusPath) {
            ImGui::SetKeyboardFocusHere();
            _focusPath = false;
        }
        const char* hint = _distros.empty() ? "path (Ctrl+L)" : "path, or a Linux path like ~/run for WSL (Ctrl+L)";
        if(ImGui::InputTextWithHint("##path", hint, _pathBuf, sizeof(_pathBuf), ImGuiInputTextFlags_EnterReturnsTrue))
            navigate(resolveInput(_pathBuf));
    }

    void drawToolbar() {
        if(ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L)) _focusPath = true;
        drawPlaces();
        drawPathBox();
        drawBreadcrumbs();

        const char* sorts[] = { "name", "date", "size", "type" };
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("sort");
        for(int s = 0; s < 4; ++s) {
            ImGui::SameLine();
            if(ImGui::RadioButton(sorts[s], _sort == static_cast<SortKey>(s))) {
                _sort = static_cast<SortKey>(s);
                _viewDirty = true;
            }
        }
        ImGui::SameLine();
        if(ImGui::ArrowButton("##dir", _ascending ? ImGuiDir_Up : ImGuiDir_Down)) {
            _ascending = !_ascending;
            _viewDirty = true;
        }
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::SetNextItemWidth(220.0f);
        if(ImGui::InputTextWithHint("##filter", "filter", _filter, sizeof(_filter))) _viewDirty = true;
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::SetNextItemWidth(140.0f);
        ImGui::SliderFloat("##tile", &_tile, 80.0f, 520.0f, "tile %.0f");
        ImGui::SameLine(0.0f, 20.0f);
        if(ImGui::Checkbox("subfolders", &_recursive)) rescan();
        ImGui::SameLine();
        if(ImGui::Checkbox("images only", &_imagesOnly)) rescan();
        ImGui::SameLine();
        ImGui::Checkbox("live", &_live);

        const ImVec4 folderColor(0.84f, 0.73f, 0.49f, 1.0f);
        bool any = false;
        if(_root.has_parent_path() && _root.parent_path() != _root) {
            any = true;
            if(ImGui::Button("..")) {
                navigate(_root.parent_path());
                return;
            }
        }
        for(const Folder& f : _listing.folders) {
            // flow onto the next line when the chip won't fit
            if(any) ImGui::SameLine();
            if(any && ImGui::GetContentRegionAvail().x < ImGui::CalcTextSize(f.name.c_str()).x + 30.0f) ImGui::NewLine();
            any = true;
            if(f.broken) {
                ImGui::BeginDisabled();
                ImGui::Button((f.name + " (missing)").c_str());
                ImGui::EndDisabled();
                continue;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, folderColor);
            const bool go = ImGui::Button((f.name + (f.link ? " ->" : "/")).c_str());
            ImGui::PopStyleColor();
            if(go) {
                navigate(f.path);
                return;
            }
        }
        ImGui::Separator();
    }

    void drawGrid() {
        const ImGuiIO& io = ImGui::GetIO();
        const float status = ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("##grid", ImVec2(0, -status), ImGuiChildFlags_None, io.KeyCtrl ? ImGuiWindowFlags_NoScrollWithMouse : 0);
        if(io.KeyCtrl && ImGui::IsWindowHovered() && io.MouseWheel != 0.0f)
            _tile = std::clamp(_tile * (1.0f + 0.1f * io.MouseWheel), 80.0f, 520.0f);

        if(!_listing.error.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", _listing.error.c_str());
        else if(!_scanned)
            ImGui::TextDisabled("scanning...");
        else if(_view.empty())
            ImGui::TextDisabled(_filter[0] ? "nothing matches the filter" : "no images or data here");

        const float pad = 6.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const ImVec2 cell(_tile + 2.0f * pad, _tile + 2.0f * pad + ImGui::GetTextLineHeightWithSpacing());
        _cols = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (cell.x + spacing)));
        const int n = static_cast<int>(_view.size());
        const int rows = (n + _cols - 1) / _cols;
        const int sel = indexOf(_selected);

        int clicked = -1;
        ImGuiListClipper clipper;
        clipper.Begin(rows, cell.y + ImGui::GetStyle().ItemSpacing.y);
        if(_scrollToSelected && sel >= 0) clipper.IncludeItemByIndex(sel / _cols);
        while(clipper.Step())
            for(int r = clipper.DisplayStart; r < clipper.DisplayEnd; ++r)
                for(int c = 0; c < _cols; ++c) {
                    const int i = r * _cols + c;
                    if(i >= n) break;
                    if(c > 0) ImGui::SameLine();
                    if(drawTile(i, cell, pad, i == sel)) clicked = i;
                    if(_scrollToSelected && i == sel) {
                        ImGui::SetScrollHereY(0.5f);
                        _scrollToSelected = false;
                    }
                }
        ImGui::EndChild();
        if(clicked >= 0) openAt(clicked);
    }

    // returns true when clicked
    bool drawTile(int viewIndex, const ImVec2& cell, float pad, bool selected) {
        const Item& it = item(viewIndex);
        Thumb& t = thumbFor(it);
        ImGui::PushID(it.key.c_str());
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1(p0.x + cell.x, p0.y + cell.y);
        // first, so it claims hover before the thumbnail plot can
        const bool pressed = ImGui::InvisibleButton("##tile", cell);
        const bool hovered = ImGui::IsItemHovered();
        const bool tooltip = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal);
        if(ImGui::IsItemClicked(ImGuiMouseButton_Right)) _selected = it.key;
        ImGui::OpenPopupOnItemClick("##menu", ImGuiPopupFlags_MouseButtonRight);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, p1, ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), 6.0f);
        if(selected) dl->AddRect(p0, p1, ImGui::GetColorU32(ImGuiCol_NavCursor), 6.0f, 0, 2.0f);

        const ImVec2 box(p0.x + pad, p0.y + pad);
        const float side = _tile;
        if(it.kind == Kind::Image && t.texture) {
            const Magnum::Vector2 sz{ t.sourceSize };
            const float scale = std::min(side / sz.x(), side / sz.y());
            const ImVec2 size(sz.x() * scale, sz.y() * scale);
            const ImVec2 a(box.x + 0.5f * (side - size.x), box.y + 0.5f * (side - size.y));
            // Magnum rows are bottom-up
            dl->AddImage(
                Magnum::ImGuiIntegration::textureId(*t.texture), a, ImVec2(a.x + size.x, a.y + size.y), ImVec2(0, 1), ImVec2(1, 0));
        } else if(it.kind == Kind::Data && t.table) {
            ImGui::SetCursorScreenPos(box);
            drawThumbPlot(*t.table, ImVec2(side, side));
        } else {
            const char* note = !t.error.empty() ? t.error.c_str() : "loading...";
            const ImVec2 ts = ImGui::CalcTextSize(note);
            dl->AddText(
                ImVec2(box.x + 0.5f * (side - ts.x), box.y + 0.5f * (side - ts.y)), ImGui::GetColorU32(ImGuiCol_TextDisabled), note);
        }
        if(it.kind == Kind::Data) {
            const ImVec2 ts = ImGui::CalcTextSize(it.ext.c_str());
            const ImVec2 b(box.x + side - ts.x - 6.0f, box.y + 2.0f);
            dl->AddRectFilled(ImVec2(b.x - 4.0f, b.y), ImVec2(b.x + ts.x + 4.0f, b.y + ts.y + 2.0f), IM_COL32(40, 60, 90, 220), 3.0f);
            dl->AddText(ImVec2(b.x, b.y + 1.0f), IM_COL32(200, 225, 255, 255), it.ext.c_str());
        }

        const ImVec2 caption(p0.x + pad, p0.y + pad + side + 2.0f);
        dl->PushClipRect(caption, ImVec2(p1.x - pad, p1.y), true);
        dl->AddText(caption, ImGui::GetColorU32(ImGuiCol_Text), it.name.c_str());
        dl->PopClipRect();

        // the plot moved the cursor; SameLine needs the tile as the last item
        ImGui::SetCursorScreenPos(p0);
        ImGui::Dummy(cell);
        if(tooltip) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(it.name.c_str());
            ImGui::TextDisabled("%s   %s", formatSize(it.size).c_str(), formatTime(it.modified).c_str());
            if(t.texture) ImGui::TextDisabled("%d x %d px", t.sourceSize.x(), t.sourceSize.y());
            if(t.table) ImGui::TextDisabled("%zu rows, %zu columns", t.table->rows, t.table->columns.size());
            ImGui::EndTooltip();
        }
        if(ImGui::BeginPopup("##menu")) {
            if(ImGui::MenuItem(it.kind == Kind::Image ? "Copy image" : "Copy file", "Ctrl+C")) copy(false);
            if(ImGui::MenuItem("Copy path", "Ctrl+Shift+C")) copy(true);
            ImGui::EndPopup();
        }
        ImGui::PopID();
        return pressed;
    }

    static void drawThumbPlot(const Table& t, const ImVec2& size) {
        const ImPlotFlags flags = ImPlotFlags_CanvasOnly | ImPlotFlags_NoInputs | ImPlotFlags_NoFrame;
        if(!ImPlot::BeginPlot("##thumb", size, flags)) return;
        ImPlot::SetupAxes(nullptr,
            nullptr,
            ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_AutoFit,
            ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_AutoFit);
        // first column is usually time
        const bool hasX = t.columns.size() > 1;
        const int stride = std::max<int>(1, static_cast<int>(t.rows) / ThumbPoints);
        const int count = static_cast<int>(t.rows) / stride;
        const int bytes = stride * static_cast<int>(sizeof(double));
        for(std::size_t c = hasX ? 1 : 0, shown = 0; c < t.columns.size() && shown < ThumbSeries; ++c, ++shown) {
            const std::string label = "##" + std::to_string(c);
            if(hasX)
                ImPlot::PlotLine(
                    label.c_str(), t.columns[0].values.data(), t.columns[c].values.data(), count, ImPlotLineFlags_SkipNaN, 0, bytes);
            else
                ImPlot::PlotLine(
                    label.c_str(), t.columns[c].values.data(), count, static_cast<double>(stride), 0.0, ImPlotLineFlags_SkipNaN, 0, bytes);
        }
        ImPlot::EndPlot();
    }

    void drawLightbox() {
        const int i = indexOf(_open);
        if(i < 0) {
            _open.clear();
            return;
        }
        const Item& it = item(i);
        Thumb& t = thumbFor(it);

        if(ImGui::ArrowButton("##prev", ImGuiDir_Left)) openAt(i - 1);
        ImGui::SameLine();
        if(ImGui::ArrowButton("##next", ImGuiDir_Right)) openAt(i + 1);
        ImGui::SameLine();
        if(ImGui::Button("close")) _open.clear();
        ImGui::SameLine();
        if(ImGui::Button("fit")) _fitOpen = true;
        ImGui::SameLine();
        if(ImGui::Button("copy")) copy(false);
        ImGui::SetItemTooltip("Ctrl+C image, Ctrl+Shift+C path");
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", it.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%d/%zu   %s   %s", i + 1, _view.size(), formatSize(it.size).c_str(), formatTime(it.modified).c_str());
        if(_open.empty()) return;

        const ImVec2 size(-1, ImGui::GetContentRegionAvail().y - 2.0f * ImGui::GetFrameHeightWithSpacing());
        if(it.kind == Kind::Image)
            drawImageView(it, t, size);
        else
            drawDataView(t, size);
        _fitOpen = false;
    }

    void drawImageView(const Item& it, Thumb& t, const ImVec2& size) {
        if(_full.key != it.key) {
            _full = Full{};
            _full.key = it.key;
        }
        if(!_full.pending && _full.stamp != it.stamp) {
            _full.pending = true;
            _worker.request({ it.path, it.key, it.kind, true, it.stamp });
        }
        // the thumbnail stands in until the full image arrives
        Magnum::GL::Texture2D* tex = _full.texture ? &*_full.texture : (t.texture ? &*t.texture : nullptr);
        const Magnum::Vector2i src = _full.texture ? _full.sourceSize : t.sourceSize;
        if(!tex) {
            ImGui::TextDisabled("%s", !_full.error.empty() ? _full.error.c_str() : (!t.error.empty() ? t.error.c_str() : "loading..."));
            return;
        }

        bool hovered = false;
        ImPlotPoint mouse;
        if(_fitOpen) ImPlot::SetNextAxesToFit();
        if(ImPlot::BeginPlot("##image", size, ImPlotFlags_Equal | ImPlotFlags_NoLegend | ImPlotFlags_NoTitle)) {
            // inverted y: pixel rows count down from the top, the image convention
            ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_None, ImPlotAxisFlags_Invert);
            ImPlot::PlotImage("##img", Magnum::ImGuiIntegration::textureId(*tex), ImPlotPoint(0, 0), ImPlotPoint(src.x(), src.y()));
            hovered = ImPlot::IsPlotHovered();
            mouse = ImPlot::GetPlotMousePos();
            ImPlot::EndPlot();
        }

        ImGui::TextDisabled("%d x %d px%s", src.x(), src.y(), _full.pending ? "   loading full resolution..." : "");
        if(!_full.error.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s", _full.error.c_str());
        }
        const int px = static_cast<int>(std::floor(mouse.x));
        const int py = static_cast<int>(std::floor(mouse.y));
        if(!hovered || px < 0 || py < 0 || px >= src.x() || py >= src.y()) return;
        ImGui::SameLine(0.0f, 30.0f);
        ImGui::Text("x %d  y %d", px, py);
        const Pixels& p = _full.pixels;
        if(!_full.texture || p.size.x() <= 0) return;
        // pixels may be box-filtered below the source size
        const int qx = std::min(p.size.x() - 1, px * p.size.x() / src.x());
        const int qy = p.size.y() - 1 - std::min(p.size.y() - 1, py * p.size.y() / src.y());
        const std::uint8_t* v = p.rgba.data() + (static_cast<std::size_t>(qy) * p.size.x() + qx) * 4;
        ImGui::SameLine(0.0f, 20.0f);
        ImGui::ColorButton("##px",
            ImVec4(static_cast<float>(v[0]) / 255.0f, static_cast<float>(v[1]) / 255.0f, static_cast<float>(v[2]) / 255.0f, 1.0f),
            ImGuiColorEditFlags_NoTooltip,
            ImVec2(14, 14));
        ImGui::SameLine();
        ImGui::Text("rgba %3d %3d %3d %3d", v[0], v[1], v[2], v[3]);
    }

    void drawDataView(const Thumb& t, const ImVec2& size) {
        if(!t.table) {
            ImGui::TextDisabled("%s", !t.error.empty() ? t.error.c_str() : "loading...");
            return;
        }
        const Table& tab = *t.table;
        if(_dataX >= static_cast<int>(tab.columns.size()) || tab.columns.size() < 2) _dataX = -1;
        if(_fitOpen) ImPlot::SetNextAxesToFit();
        if(ImPlot::BeginPlot("##data", size)) {
            const char* xLabel = _dataX >= 0 ? tab.columns[static_cast<std::size_t>(_dataX)].name.c_str() : "row";
            ImPlot::SetupAxes(xLabel, nullptr);
            if(_logY) ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
            ImPlot::SetupLegend(ImPlotLocation_NorthEast);
            int shown = 0;
            for(std::size_t c = 0; c < tab.columns.size(); ++c) {
                if(static_cast<int>(c) == _dataX) continue;
                const Column& col = tab.columns[c];
                // wide tables: start with a few on, the legend toggles the rest
                ImPlot::HideNextItem(shown++ >= 6, ImPlotCond_Once);
                const std::string label = col.name + "##" + std::to_string(c);
                const int n = static_cast<int>(tab.rows);
                if(_dataX >= 0)
                    ImPlot::PlotLine(label.c_str(),
                        tab.columns[static_cast<std::size_t>(_dataX)].values.data(),
                        col.values.data(),
                        n,
                        ImPlotLineFlags_SkipNaN);
                else
                    ImPlot::PlotLine(label.c_str(), col.values.data(), n, 1.0, 0.0, ImPlotLineFlags_SkipNaN);
            }
            ImPlot::EndPlot();
        }
        ImGui::SetNextItemWidth(220.0f);
        const char* current = _dataX >= 0 ? tab.columns[static_cast<std::size_t>(_dataX)].name.c_str() : "row index";
        if(ImGui::BeginCombo("x", current)) {
            if(ImGui::Selectable("row index", _dataX < 0)) _dataX = -1;
            for(std::size_t c = 0; c < tab.columns.size(); ++c)
                if(ImGui::Selectable(tab.columns[c].name.c_str(), _dataX == static_cast<int>(c))) _dataX = static_cast<int>(c);
            ImGui::EndCombo();
            _fitOpen = true;
        }
        ImGui::SameLine();
        if(ImGui::Checkbox("log y", &_logY)) _fitOpen = true;
        ImGui::SameLine();
        ImGui::TextDisabled("%zu rows, %zu columns", tab.rows, tab.columns.size());
    }

    void drawStatus() {
        int images = 0;
        int data = 0;
        for(const int i : _view) {
            if(_listing.items[static_cast<std::size_t>(i)].kind == Kind::Image)
                ++images;
            else
                ++data;
        }
        ImGui::Text("%d images, %d data files", images, data);
        const std::size_t pending = _worker.pending();
        if(pending > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("   loading %zu", pending);
        }
        if(_scanner.busy()) {
            ImGui::SameLine();
            ImGui::TextDisabled("   scanning");
        }
        if(_noticeTime > 0.0f) {
            _noticeTime -= ImGui::GetIO().DeltaTime;
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.55f, std::min(1.0f, _noticeTime)), "   %s", _notice.c_str());
        }
#if defined(__linux__)
        const std::string clip = _clipboard.status();
        if(!clip.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("   %s", clip.c_str());
        }
#endif
    }
};

} // namespace

int main(int argc, char** argv) {
#if defined(__linux__)
    // a PowerShell that died mid-copy must not take us down with SIGPIPE
    std::signal(SIGPIPE, SIG_IGN);
#endif
    fs::path start = fs::current_path();
    bool recursive = false;
    bool imagesOnly = false;
    for(int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if(a == "-r")
            recursive = true;
        else if(a == "-i")
            imagesOnly = true;
        else if(a == "--smg-hidden")
            continue;
        else if(a.rfind("--smg-", 0) == 0 || a.rfind("--magnum-", 0) == 0)
            ++i; // skip their values
        else if(a[0] != '-')
            start = resolveInput(a); // on Windows "/home/me/run" or "~/run" opens in WSL
    }

    smg::GuiConfig config;
    config.title = "smg fview";
    config.size = { 1400, 900 };
    smg::GuiBase gui({ argc, argv }, config);

    Viewer viewer(start, recursive, imagesOnly);
    gui.add_callback([&viewer]() {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("##fview",
            nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
        viewer.draw();
        ImGui::End();
        return 0;
    });

    while(gui.mainLoopIteration()) {}
    return 0;
}
