// csvplot: drop CSV files on the window (or pass them as arguments) and plot their columns.
// Delimiter (, ; tab or spaces) and header row are detected; # and % lines are comments; empty or
// non-numeric cells become gaps. "follow" re-reads a file whenever it changes, for watching a
// simulation write its output. A draggable cursor reads off every plotted series.
//   csvplot [file.csv ...]

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <SDL_events.h>

#include "FileDialog.hh"
#include "GuiBase.hh"
#include "imgui.h"
#include "implot.h"

namespace fs = std::filesystem;

namespace {

constexpr double Nan = std::numeric_limits<double>::quiet_NaN();

struct Column {
    std::string name;
    std::vector<double> values;
    double min = Nan, max = Nan, mean = Nan;
};

struct Dataset {
    std::string path;
    std::string name;
    std::vector<Column> columns;
    std::size_t rows = 0;
    std::string error;
    fs::file_time_type stamp{};
    // UI state, kept across reloads
    int id = 0;
    int x = 0; // -1 = row index
    std::vector<bool> plotted;
    bool follow = false;
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

void loadCsv(Dataset& d) {
    d.columns.clear();
    d.rows = 0;
    d.error.clear();
    std::error_code ec;
    d.stamp = fs::last_write_time(fs::u8path(d.path), ec);
    std::ifstream in(fs::u8path(d.path), std::ios::binary);
    if(!in) {
        d.error = "cannot open file";
        return;
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
                for(const std::string& f : fields) d.columns.push_back({ trim(f) });
                continue;
            }
        }
        // ragged rows: grow columns, padding earlier rows with gaps
        while(d.columns.size() < fields.size()) {
            Column c;
            c.name = "col " + std::to_string(d.columns.size() + 1);
            c.values.assign(d.rows, Nan);
            d.columns.push_back(std::move(c));
        }
        for(std::size_t c = 0; c < d.columns.size(); ++c) {
            double v = Nan;
            if(c >= fields.size() || !parseNumber(fields[c], v)) v = Nan;
            d.columns[c].values.push_back(v);
        }
        ++d.rows;
    }
    for(Column& c : d.columns) {
        c.values.resize(d.rows, Nan);
        double sum = 0.0;
        std::size_t n = 0;
        for(double v : c.values) {
            if(std::isnan(v)) continue;
            c.min = std::isnan(c.min) ? v : std::min(c.min, v);
            c.max = std::isnan(c.max) ? v : std::max(c.max, v);
            sum += v;
            ++n;
        }
        c.mean = n ? sum / static_cast<double>(n) : Nan;
    }
    if(d.rows == 0) d.error = "no data rows";
}

// parses on worker threads so a large file doesn't freeze the window
class Loader {
public:
    ~Loader() {
        for(std::thread& t : _threads) t.join();
    }

    void load(Dataset d) {
        ++_pending;
        _threads.emplace_back([this, d]() mutable {
            loadCsv(d);
            std::lock_guard<std::mutex> lock(_mutex);
            _done.push_back(std::move(d));
            --_pending;
        });
    }

    std::vector<Dataset> take() {
        if(_pending == 0) {
            for(std::thread& t : _threads) t.join();
            _threads.clear();
        }
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<Dataset> out;
        out.swap(_done);
        return out;
    }

    int pending() const { return _pending; }

private:
    std::vector<std::thread> _threads;
    std::mutex _mutex;
    std::vector<Dataset> _done;
    std::atomic<int> _pending{ 0 };
};

// value of a series at x: linear interpolation when x is sorted, else the nearest sample
double valueAt(const std::vector<double>& xs, const std::vector<double>& ys, double x) {
    const std::size_t n = std::min(xs.size(), ys.size());
    if(n == 0) return Nan;
    if(std::is_sorted(xs.begin(), xs.begin() + static_cast<std::ptrdiff_t>(n))) {
        const std::size_t i = static_cast<std::size_t>(std::lower_bound(xs.begin(), xs.begin() + static_cast<std::ptrdiff_t>(n), x) - xs.begin());
        if(x < xs[0] || x > xs[n - 1]) return Nan; // outside this series
        if(i == 0) return ys[0];
        const double t = (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
        return ys[i - 1] + t * (ys[i] - ys[i - 1]);
    }
    std::size_t best = 0;
    for(std::size_t i = 1; i < n; ++i)
        if(std::abs(xs[i] - x) < std::abs(xs[best] - x)) best = i;
    return ys[best];
}

class Plotter {
public:
    void add(const std::string& path) {
        Dataset d;
        d.path = path;
        d.name = fs::u8path(path).filename().u8string();
        d.id = _nextId++;
        _loader.load(std::move(d));
    }

    void draw() {
        receive();
        followFiles();

        if(ImGui::BeginTable("##layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("files", ImGuiTableColumnFlags_WidthFixed, 300.0f);
            ImGui::TableSetupColumn("plot", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextColumn();
            drawSidebar();
            ImGui::TableNextColumn();
            drawPlot();
            ImGui::EndTable();
        }
        if(_dialog.draw() && _dialog.valid()) add(_dialog.path());
    }

private:
    std::vector<Dataset> _sets;
    std::vector<std::vector<double>> _index; // row-index x values, per dataset
    Loader _loader;
    smg::FileDialog _dialog{ "csvOpen" };
    int _nextId = 0;
    int _style = 0; // 0 lines, 1 lines + markers, 2 scatter
    bool _stacked = false;
    bool _logY = false;
    bool _cursorOn = true;
    double _cursor = 0.0;
    bool _fit = true;
    float _followClock = 0.0f;

    void receive() {
        for(Dataset& d : _loader.take()) {
            std::vector<Dataset>::iterator existing =
                std::find_if(_sets.begin(), _sets.end(), [&d](const Dataset& s) { return s.id == d.id; });
            if(existing != _sets.end()) {
                // reload: keep the user's choices when the columns still line up
                d.x = existing->x;
                d.follow = existing->follow;
                d.plotted = existing->plotted;
                d.plotted.resize(d.columns.size(), false);
                *existing = std::move(d);
                continue;
            }
            d.plotted.assign(d.columns.size(), false);
            // first column is usually time; plot the next few against it
            d.x = d.columns.size() > 1 ? 0 : -1;
            for(std::size_t c = d.x < 0 ? 0 : 1, shown = 0; c < d.columns.size() && shown < 4; ++c, ++shown) d.plotted[c] = true;
            // keep the order files were added in, whichever finished parsing first
            const std::vector<Dataset>::iterator at =
                std::find_if(_sets.begin(), _sets.end(), [&d](const Dataset& s) { return s.id > d.id; });
            _sets.insert(at, std::move(d));
            _index.clear();
            _fit = true;
        }
    }

    void followFiles() {
        _followClock += ImGui::GetIO().DeltaTime;
        if(_followClock < 1.0f) return;
        _followClock = 0.0f;
        for(const Dataset& d : _sets) {
            if(!d.follow) continue;
            std::error_code ec;
            const fs::file_time_type t = fs::last_write_time(fs::u8path(d.path), ec);
            if(!ec && t != d.stamp) {
                Dataset again;
                again.path = d.path;
                again.name = d.name;
                again.id = d.id;
                _loader.load(std::move(again));
            }
        }
    }

    const std::vector<double>& xValues(std::size_t setIndex) {
        const Dataset& d = _sets[setIndex];
        if(d.x >= 0 && d.x < static_cast<int>(d.columns.size())) return d.columns[d.x].values;
        if(_index.size() <= setIndex) _index.resize(setIndex + 1);
        std::vector<double>& idx = _index[setIndex];
        if(idx.size() != d.rows) {
            idx.resize(d.rows);
            for(std::size_t i = 0; i < d.rows; ++i) idx[i] = static_cast<double>(i);
        }
        return idx;
    }

    void drawSidebar() {
        ImGui::BeginChild("##side");
        if(ImGui::Button("Open...")) _dialog.open("Open data file", ".csv,.tsv,.txt,.dat,.*");
        ImGui::SameLine();
        ImGui::TextDisabled("or drop files here");
        if(_loader.pending() > 0) ImGui::TextDisabled("loading %d file(s)...", _loader.pending());
        ImGui::Separator();

        int remove = -1;
        for(std::size_t s = 0; s < _sets.size(); ++s) {
            Dataset& d = _sets[s];
            ImGui::PushID(d.id);
            const bool open = ImGui::CollapsingHeader(d.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 10.0f);
            if(ImGui::SmallButton("x")) remove = static_cast<int>(s);
            if(open) {
                if(!d.error.empty()) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", d.error.c_str());
                ImGui::TextDisabled("%zu rows, %zu columns", d.rows, d.columns.size());
                ImGui::Checkbox("follow file", &d.follow);
                ImGui::SetNextItemWidth(-1);
                const char* xName = d.x >= 0 && d.x < static_cast<int>(d.columns.size()) ? d.columns[d.x].name.c_str() : "row index";
                if(ImGui::BeginCombo("##x", (std::string("x: ") + xName).c_str())) {
                    if(ImGui::Selectable("row index", d.x < 0)) d.x = -1;
                    for(std::size_t c = 0; c < d.columns.size(); ++c)
                        if(ImGui::Selectable(d.columns[c].name.c_str(), d.x == static_cast<int>(c))) d.x = static_cast<int>(c);
                    ImGui::EndCombo();
                    _fit = true;
                }
                for(std::size_t c = 0; c < d.columns.size(); ++c) {
                    if(static_cast<int>(c) == d.x) continue;
                    bool on = d.plotted[c];
                    if(ImGui::Checkbox(d.columns[c].name.c_str(), &on)) {
                        d.plotted[c] = on;
                        _fit = true;
                    }
                    if(ImGui::IsItemHovered())
                        ImGui::SetTooltip("min %g\nmax %g\nmean %g", d.columns[c].min, d.columns[c].max, d.columns[c].mean);
                }
            }
            ImGui::PopID();
        }
        if(remove >= 0) {
            _sets.erase(_sets.begin() + remove);
            _index.clear();
        }

        ImGui::Separator();
        ImGui::RadioButton("lines", &_style, 0);
        ImGui::SameLine();
        ImGui::RadioButton("markers", &_style, 1);
        ImGui::SameLine();
        ImGui::RadioButton("scatter", &_style, 2);
        if(ImGui::Checkbox("stacked", &_stacked)) _fit = true;
        ImGui::SameLine();
        if(ImGui::Checkbox("log y", &_logY)) _fit = true;
        ImGui::SameLine();
        ImGui::Checkbox("cursor", &_cursorOn);
        if(ImGui::Button("fit")) _fit = true;
        ImGui::EndChild();
    }

    struct Series {
        std::size_t set;
        std::size_t column;
    };

    std::vector<Series> plottedSeries() const {
        std::vector<Series> out;
        for(std::size_t s = 0; s < _sets.size(); ++s)
            for(std::size_t c = 0; c < _sets[s].columns.size(); ++c)
                if(_sets[s].plotted[c] && static_cast<int>(c) != _sets[s].x) out.push_back({ s, c });
        return out;
    }

    void plotSeries(const Series& s) {
        const std::vector<double>& xs = xValues(s.set);
        const Column& col = _sets[s.set].columns[s.column];
        const std::string label = (_sets.size() > 1 ? _sets[s.set].name + ": " : std::string()) + col.name + "##" +
                                  std::to_string(_sets[s.set].id) + "." + std::to_string(s.column);
        const int n = static_cast<int>(std::min(xs.size(), col.values.size()));
        if(_style == 2) {
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 2.0f);
            ImPlot::PlotScatter(label.c_str(), xs.data(), col.values.data(), n);
            return;
        }
        if(_style == 1) ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 2.5f);
        ImPlot::PlotLine(label.c_str(), xs.data(), col.values.data(), n, ImPlotLineFlags_SkipNaN);
    }

    void setupAxes(const char* xLabel) {
        ImPlot::SetupAxes(xLabel, nullptr);
        if(_logY) ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
    }

    void drawCursor(bool readout) {
        if(!_cursorOn) return;
        ImPlot::DragLineX(0, &_cursor, ImVec4(1.0f, 0.85f, 0.3f, 0.9f), 1.5f);
        if(readout) ImPlot::TagX(_cursor, ImVec4(1.0f, 0.85f, 0.3f, 0.9f), "%g", _cursor);
    }

    void drawPlot() {
        const std::vector<Series> series = plottedSeries();
        if(_sets.empty()) {
            ImGui::TextDisabled("Drop .csv files on the window, or use Open...");
            return;
        }
        const char* xLabel = _sets.size() == 1 && _sets[0].x >= 0 && _sets[0].x < static_cast<int>(_sets[0].columns.size())
                                 ? _sets[0].columns[_sets[0].x].name.c_str()
                                 : nullptr;
        const float readoutHeight = _cursorOn && !series.empty() ? ImGui::GetTextLineHeightWithSpacing() * (1.5f + std::min<float>(series.size(), 6.0f)) : 0.0f;
        const ImVec2 size(-1, ImGui::GetContentRegionAvail().y - readoutHeight);
        if(_fit) {
            // the cursor starts mid-range so it is visible
            double lo = std::numeric_limits<double>::max(), hi = -lo;
            for(const Series& s : series) {
                const std::vector<double>& xs = xValues(s.set);
                for(double v : xs)
                    if(!std::isnan(v)) {
                        lo = std::min(lo, v);
                        hi = std::max(hi, v);
                    }
            }
            if(lo <= hi) _cursor = 0.5 * (lo + hi);
        }

        if(_stacked && series.size() > 1) {
            const int rows = static_cast<int>(series.size());
            if(_fit) ImPlot::SetNextAxesToFit();
            if(ImPlot::BeginSubplots("##stack", rows, 1, size, ImPlotSubplotFlags_LinkAllX)) {
                for(const Series& s : series) {
                    if(_fit) ImPlot::SetNextAxesToFit();
                    if(ImPlot::BeginPlot(("##p" + std::to_string(s.set) + "." + std::to_string(s.column)).c_str())) {
                        setupAxes(nullptr);
                        plotSeries(s);
                        drawCursor(false);
                        ImPlot::EndPlot();
                    }
                }
                ImPlot::EndSubplots();
            }
        } else {
            if(_fit) ImPlot::SetNextAxesToFit();
            if(ImPlot::BeginPlot("##plot", size)) {
                setupAxes(xLabel);
                for(const Series& s : series) plotSeries(s);
                drawCursor(true);
                ImPlot::EndPlot();
            }
        }
        _fit = false;

        if(_cursorOn && !series.empty() && ImGui::BeginTable("##readout", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, readoutHeight))) {
            ImGui::TableSetupColumn("series", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 160.0f);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("at x = %g", _cursor);
            for(const Series& s : series) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(_sets[s.set].columns[s.column].name.c_str());
                ImGui::TableNextColumn();
                const double v = valueAt(xValues(s.set), _sets[s.set].columns[s.column].values, _cursor);
                if(std::isnan(v)) ImGui::TextDisabled("-");
                else ImGui::Text("%g", v);
            }
            ImGui::EndTable();
        }
    }
};

// GuiBase plus file drops (SDL hands unhandled events to anyEvent)
class App: public smg::GuiBase {
public:
    App(const Arguments& args, const smg::GuiConfig& config) : smg::GuiBase(args, config) {}
    std::vector<std::string> dropped;

private:
    void anyEvent(SDL_Event& event) override {
        if(event.type == SDL_DROPFILE) {
            dropped.emplace_back(event.drop.file);
            SDL_free(event.drop.file);
        }
    }
};

} // namespace

int main(int argc, char** argv) {
    smg::GuiConfig config;
    config.title = "smg csvplot";
    config.size = { 1400, 850 };
    App app({ argc, argv }, config);

    Plotter plotter;
    for(int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if(a == "--smg-frames" || a == "--smg-screenshot" || a == "--smg-record") ++i; // skip their values
        else if(a[0] != '-') plotter.add(a);
    }

    app.add_callback([&]() {
        for(const std::string& f : app.dropped) plotter.add(f);
        app.dropped.clear();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("##csvplot", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
        plotter.draw();
        ImGui::End();
        return 0;
    });

    while(app.mainLoopIteration()) {}
    return 0;
}
