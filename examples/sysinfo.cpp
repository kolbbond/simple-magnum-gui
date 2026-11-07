// system monitor: CPU (per core), threads, memory, processes, disks, network, sensors, GPU and displays,
// sampled twice a second with ImPlot history. The data comes from smg::sys (Sys.hh); this file only draws it.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <Corrade/Containers/StringStl.h>
#include <Magnum/GL/Context.h>
#include <SDL_video.h>

#include "GuiBase.hh"
#include "Sys.hh"
#include "imgui.h"
#include "implot.h"

namespace {

constexpr double SamplePeriod = 0.5; // seconds
constexpr int HistoryLength = 240; // two minutes

smg::sys::MonitorConfig monitorConfig() {
    smg::sys::MonitorConfig c;
    c.sample_period = SamplePeriod;
    c.history_length = HistoryLength;
    return c;
}

constexpr ImPlotFlags SparkFlags = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect
    | ImPlotFlags_NoMouseText | ImPlotFlags_NoInputs | ImPlotFlags_NoFrame;

// shaded 0..100 % history
void percentPlot(const char* id, const smg::sys::Monitor& m, const std::vector<float>& h, const ImVec2& size, bool decorated) {
    if(!ImPlot::BeginPlot(id, size, decorated ? ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect : SparkFlags)) return;
    const ImPlotAxisFlags axis = decorated ? ImPlotAxisFlags_None : ImPlotAxisFlags_NoDecorations;
    ImPlot::SetupAxes(decorated ? "seconds" : nullptr, decorated ? "%" : nullptr, axis, axis);
    ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 100.0, ImPlotCond_Always);
    const int n = static_cast<int>(std::min(h.size(), m.times.size()));
    ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.35f);
    ImPlot::PlotShaded("##fill", m.times.data(), h.data(), n);
    ImPlot::PlotLine("##line", m.times.data(), h.data(), n);
    ImPlot::EndPlot();
}

void drawHeader(const smg::sys::Monitor& m) {
    ImGui::TextUnformatted(m.info.host.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s   up %s", m.info.os.c_str(), smg::sys::format_uptime(m.now.uptime).c_str());
    ImGui::TextDisabled("%s   %d socket(s), %d cores, %d threads", m.info.cpu_name.c_str(), m.info.sockets, m.info.physical_cores,
        m.info.logical_cores);
    ImGui::Separator();
}

void drawOverview(const smg::sys::Monitor& m) {
    const float h = std::max(140.0f, (ImGui::GetContentRegionAvail().y - 80.0f) / 2.0f);
    const float cpu = m.cpu_history.empty() ? 0.0f : m.cpu_history.back();

    if(ImGui::BeginTable("##overview", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        ImGui::Text("CPU  %.0f %%", cpu);
        percentPlot("##cpu", m, m.cpu_history, ImVec2(-1, h), true);

        ImGui::TableNextColumn();
        ImGui::Text("Memory  %s / %s", smg::sys::format_bytes(m.now.mem_used).c_str(), smg::sys::format_bytes(m.now.mem_total).c_str());
        if(ImPlot::BeginPlot("##mem", ImVec2(-1, h), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {
            ImPlot::SetupAxes("seconds", "GB");
            ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, m.now.mem_total / 1073741824.0, ImPlotCond_Always);
            ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.35f);
            ImPlot::PlotShaded("used", m.times.data(), m.mem_history.data(), static_cast<int>(m.mem_history.size()));
            ImPlot::EndPlot();
        }

        ImGui::TableNextColumn();
        ImGui::Text("Network  down %s/s  up %s/s", smg::sys::format_bytes(m.now.net_rx).c_str(), smg::sys::format_bytes(m.now.net_tx).c_str());
        if(ImPlot::BeginPlot("##net", ImVec2(-1, h), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {
            ImPlot::SetupAxes("seconds", "KB/s", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
            ImPlot::PlotLine("down", m.times.data(), m.rx_history.data(), static_cast<int>(m.rx_history.size()));
            ImPlot::PlotLine("up", m.times.data(), m.tx_history.data(), static_cast<int>(m.tx_history.size()));
            ImPlot::EndPlot();
        }

        ImGui::TableNextColumn();
        ImGui::Text("Per core now");
        if(ImPlot::BeginPlot("##cores", ImVec2(-1, h), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoLegend)) {
            ImPlot::SetupAxes("core", "%", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_None);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 100.0, ImPlotCond_Always);
            ImPlot::PlotBars("usage", m.now.core_usage.data(), static_cast<int>(m.now.core_usage.size()), 0.7);
            ImPlot::EndPlot();
        }
        ImGui::EndTable();
    }

    ImGui::Text("Processes %d   Threads %d", m.now.processes, m.now.threads);
    if(m.now.handles >= 0) {
        ImGui::SameLine();
        ImGui::Text("  Handles %d", m.now.handles);
    }
    if(m.now.battery >= 0) {
        ImGui::SameLine();
        ImGui::Text("  Battery %d %%%s", m.now.battery, m.now.charging ? " (charging)" : "");
    }
}

void drawCpu(const smg::sys::Monitor& m) {
    const int n = static_cast<int>(m.core_history.size());
    if(n == 0) return;
    const int columns = n <= 4 ? n : (n <= 16 ? 4 : 8);

    if(ImGui::BeginTable("##coregrid", columns, ImGuiTableFlags_SizingStretchSame)) {
        for(int i = 0; i < n; ++i) {
            ImGui::TableNextColumn();
            const float mhz = i < static_cast<int>(m.now.core_mhz.size()) ? m.now.core_mhz[i] : 0.0f;
            ImGui::Text("CPU %d  %3.0f %%  %.2f GHz", i, m.now.core_usage[i], mhz / 1000.0f);
            char id[16];
            std::snprintf(id, sizeof(id), "##core%d", i);
            percentPlot(id, m, m.core_history[i], ImVec2(-1, 70), false);
        }
        ImGui::EndTable();
    }
    if(m.now.max_mhz > 0.0f) ImGui::TextDisabled("rated %.2f GHz (Windows reports a coarse current clock)", m.now.max_mhz / 1000.0f);

    // cores x time heatmap
    const int cols = static_cast<int>(m.times.size());
    if(cols < 2) return;
    std::vector<float> values(static_cast<std::size_t>(n) * cols);
    for(int r = 0; r < n; ++r)
        for(int c = 0; c < cols; ++c) values[static_cast<std::size_t>(r) * cols + c] = m.core_history[r][c];
    ImGui::Spacing();
    ImPlot::PushColormap(ImPlotColormap_Plasma);
    const float h = std::max(120.0f, ImGui::GetContentRegionAvail().y);
    if(ImPlot::BeginPlot("Load by core", ImVec2(ImGui::GetContentRegionAvail().x - 90.0f, h), ImPlotFlags_NoMenus | ImPlotFlags_NoLegend)) {
        ImPlot::SetupAxes("seconds", "core", ImPlotAxisFlags_None, ImPlotAxisFlags_Invert);
        ImPlot::SetupAxisLimits(ImAxis_X1, m.times.front(), 0.0, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, n, ImPlotCond_Always);
        ImPlot::PlotHeatmap("load", values.data(), n, cols, 0.0, 100.0, nullptr, ImPlotPoint(m.times.front(), 0.0), ImPlotPoint(0.0, n));
        ImPlot::EndPlot();
    }
    ImGui::SameLine();
    ImPlot::ColormapScale("%", 0.0, 100.0, ImVec2(80.0f, h));
    ImPlot::PopColormap();
}

void drawMemory(const smg::sys::Monitor& m) {
    const double gb = 1073741824.0;
    ImGui::Text("Physical  %s / %s", smg::sys::format_bytes(m.now.mem_used).c_str(), smg::sys::format_bytes(m.now.mem_total).c_str());
    ImGui::ProgressBar(m.now.mem_total > 0.0 ? static_cast<float>(m.now.mem_used / m.now.mem_total) : 0.0f, ImVec2(-1, 0));
    ImGui::Text("%s  %s / %s", smg::sys::swap_label(), smg::sys::format_bytes(m.now.swap_used).c_str(), smg::sys::format_bytes(m.now.swap_total).c_str());
    ImGui::ProgressBar(m.now.swap_total > 0.0 ? static_cast<float>(m.now.swap_used / m.now.swap_total) : 0.0f, ImVec2(-1, 0));

    if(ImPlot::BeginPlot("##memory", ImVec2(-1, ImGui::GetContentRegionAvail().y), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {
        ImPlot::SetupAxes("seconds", "GB");
        ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, std::max(m.now.mem_total, m.now.swap_total) / gb * 1.05, ImPlotCond_Always);
        const int n = static_cast<int>(m.mem_history.size());
        ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.35f);
        ImPlot::PlotShaded("physical", m.times.data(), m.mem_history.data(), n);
        ImPlot::PlotLine(smg::sys::swap_label(), m.times.data(), m.swap_history.data(), n);
        const double total = m.now.mem_total / gb;
        ImPlot::PlotInfLines("installed", &total, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::EndPlot();
    }
}

void drawProcesses(const smg::sys::Monitor& m) {
    static ImGuiTextFilter filter;
    ImGui::Text("%d processes, %d threads", m.now.processes, m.now.threads);
    ImGui::SameLine();
    filter.Draw("filter", 200.0f);

    if(ImPlot::BeginPlot("##threads", ImVec2(-1, 110), SparkFlags)) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
        ImPlot::PlotLine("threads", m.times.data(), m.thread_history.data(), static_cast<int>(m.thread_history.size()));
        ImPlot::EndPlot();
    }

    const ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_BordersInnerV;
    if(!ImGui::BeginTable("##procs", 5, flags)) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("CPU %", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending,
        70.0f);
    ImGui::TableSetupColumn("Threads", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 70.0f);
    ImGui::TableSetupColumn("Memory", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 90.0f);
    ImGui::TableHeadersRow();

    std::vector<const smg::sys::ProcessRow*> rows;
    for(const smg::sys::ProcessRow& p : m.processes)
        if(filter.PassFilter(p.name.c_str())) rows.push_back(&p);
    const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
    if(specs && specs->SpecsCount > 0) {
        const int column = specs->Specs[0].ColumnIndex;
        const bool ascending = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
        std::stable_sort(rows.begin(), rows.end(), [column, ascending](const smg::sys::ProcessRow* a, const smg::sys::ProcessRow* b) {
            int c = 0;
            switch(column) {
                case 0: c = a->name.compare(b->name); break;
                case 1: c = a->pid < b->pid ? -1 : (a->pid > b->pid ? 1 : 0); break;
                case 2: c = a->cpu < b->cpu ? -1 : (a->cpu > b->cpu ? 1 : 0); break;
                case 3: c = a->threads - b->threads; break;
                default: c = a->memory < b->memory ? -1 : (a->memory > b->memory ? 1 : 0); break;
            }
            return ascending ? c < 0 : c > 0;
        });
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while(clipper.Step())
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const smg::sys::ProcessRow& p = *rows[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(p.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u", p.pid);
            ImGui::TableNextColumn();
            ImGui::Text("%5.1f", p.cpu);
            ImGui::TableNextColumn();
            ImGui::Text("%d", p.threads);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(p.memory > 0.0 ? smg::sys::format_bytes(p.memory).c_str() : "-");
        }
    ImGui::EndTable();
}

void drawStorageNetwork(const smg::sys::Monitor& m) {
    ImGui::SeparatorText("Disks");
    for(const smg::sys::Disk& d : m.disks) {
        const double used = d.total - d.free;
        ImGui::Text("%s", d.name.c_str());
        ImGui::SameLine(120.0f);
        ImGui::TextDisabled("%s", d.detail.c_str());
        char overlay[64];
        std::snprintf(overlay, sizeof(overlay), "%s free of %s", smg::sys::format_bytes(d.free).c_str(), smg::sys::format_bytes(d.total).c_str());
        ImGui::ProgressBar(d.total > 0.0 ? static_cast<float>(used / d.total) : 0.0f, ImVec2(-1, 0), overlay);
    }

    ImGui::SeparatorText("Network");
    ImGui::Text("down %s/s   up %s/s", smg::sys::format_bytes(m.now.net_rx).c_str(), smg::sys::format_bytes(m.now.net_tx).c_str());
    if(ImPlot::BeginPlot("##network", ImVec2(-1, ImGui::GetContentRegionAvail().y), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {
        ImPlot::SetupAxes("seconds", "KB/s", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
        const int n = static_cast<int>(m.rx_history.size());
        ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.3f);
        ImPlot::PlotShaded("down", m.times.data(), m.rx_history.data(), n);
        ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.3f);
        ImPlot::PlotShaded("up", m.times.data(), m.tx_history.data(), n);
        ImPlot::EndPlot();
    }
}

void drawGpuDisplays(const smg::sys::Monitor& m) {
    Magnum::GL::Context& gl = Magnum::GL::Context::current();
    const std::string vendor = gl.vendorString();
    const std::string renderer = gl.rendererString();
    const std::string version = gl.versionString();
    const std::string glsl = gl.shadingLanguageVersionString();

    ImGui::SeparatorText("OpenGL (this window)");
    ImGui::Text("Renderer  %s", renderer.c_str());
    ImGui::Text("Vendor    %s", vendor.c_str());
    ImGui::Text("Version   %s", version.c_str());
    ImGui::Text("GLSL      %s", glsl.c_str());
    ImGui::Text("Extensions %zu", gl.extensionStrings().size());

    if(!m.info.adapters.empty()) {
        ImGui::SeparatorText("Adapters");
        for(const smg::sys::Adapter& a : m.info.adapters)
            ImGui::Text("%s   %s dedicated, %s shared", a.name.c_str(), smg::sys::format_bytes(a.dedicated).c_str(), smg::sys::format_bytes(a.shared).c_str());
    }

    ImGui::SeparatorText("Displays");
    const int displays = SDL_GetNumVideoDisplays();
    for(int i = 0; i < displays; ++i) {
        SDL_DisplayMode mode = {};
        SDL_GetCurrentDisplayMode(i, &mode);
        float ddpi = 0.0f;
        SDL_GetDisplayDPI(i, &ddpi, nullptr, nullptr);
        const char* name = SDL_GetDisplayName(i);
        ImGui::Text("%d  %s   %dx%d @ %d Hz   %.0f dpi", i, name ? name : "?", mode.w, mode.h, mode.refresh_rate, ddpi);
    }

    ImGui::SeparatorText("This app");
    ImGui::Text("%.1f fps", ImGui::GetIO().Framerate);
    if(ImPlot::BeginPlot("##fps", ImVec2(-1, ImGui::GetContentRegionAvail().y), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoLegend)) {
        ImPlot::SetupAxes("seconds", "fps", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
        ImPlot::PlotLine("fps", m.times.data(), m.fps_history.data(), static_cast<int>(m.fps_history.size()));
        ImPlot::EndPlot();
    }
}

// boards without a power sensor report -1
std::string watts(const smg::sys::GpuStat& g) {
    if(g.power < 0.0f) return "-";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.0f", g.power);
    return buf;
}

void drawSensors(const smg::sys::Monitor& m) {
    if(!m.now.gpus.empty()) {
        ImGui::SeparatorText("GPUs (NVML)");
        for(std::size_t i = 0; i < m.now.gpus.size(); ++i) {
            const smg::sys::GpuStat& g = m.now.gpus[i];
            ImGui::Text("%zu  %s   %.0f %%   %.0f C   %s W   %s / %s", i, g.name.c_str(), g.util, g.temp, watts(g).c_str(),
                smg::sys::format_bytes(g.mem_used).c_str(), smg::sys::format_bytes(g.mem_total).c_str());
            char id[16];
            std::snprintf(id, sizeof(id), "##gpu%zu", i);
            percentPlot(id, m, m.gpu_util_history[i], ImVec2(-1, 60), false);
        }
    }

    ImGui::SeparatorText("Temperatures");
    if(m.now.temps.empty()) ImGui::TextDisabled("none reported");
    for(const smg::sys::Sensor& t : m.now.temps) ImGui::Text("%-12s %-20s %5.1f C", t.chip.c_str(), t.label.c_str(), t.value);
    const float h = std::max(160.0f, ImGui::GetContentRegionAvail().y - 140.0f);
    if(!m.temp_history.empty() && ImPlot::BeginPlot("##temps", ImVec2(-1, h), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {
        ImPlot::SetupAxes("seconds", "C", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
        for(std::size_t i = 0; i < m.temp_history.size() && i < m.now.temps.size(); ++i) {
            const std::string key = m.now.temps[i].key();
            const int n = static_cast<int>(std::min(m.temp_history[i].size(), m.times.size()));
            ImPlot::PlotLine(key.c_str(), m.times.data(), m.temp_history[i].data(), n);
        }
        ImPlot::EndPlot();
    }

    ImGui::SeparatorText("Fans");
    if(m.now.fans.empty()) ImGui::TextDisabled("none reported");
    for(const smg::sys::Sensor& f : m.now.fans) ImGui::Text("%-12s %-20s %6.0f RPM", f.chip.c_str(), f.label.c_str(), f.value);
}

struct Tab {
    const char* name;
    const char* key; // for --tab
    void (*draw)(const smg::sys::Monitor&);
};

const Tab Tabs[] = {
    { "Overview", "overview", drawOverview },
    { "CPU", "cpu", drawCpu },
    { "Memory", "memory", drawMemory },
    { "Processes", "processes", drawProcesses },
    { "Disks & Network", "disks", drawStorageNetwork },
    { "GPU & Displays", "gpu", drawGpuDisplays },
    { "Sensors", "sensors", drawSensors },
};

} // namespace

// --print: one text snapshot, no window
int printSnapshot() {
    smg::sys::Monitor m(monitorConfig());
    std::this_thread::sleep_for(std::chrono::seconds(1)); // let the rates (CPU, network, per-process) span a full second
    m.sample_now();
    const smg::sys::Snapshot& s = m.now;
    std::printf("%s | %s | up %s\n", m.info.host.c_str(), m.info.os.c_str(), smg::sys::format_uptime(s.uptime).c_str());
    std::printf("CPU  %s | %d socket(s) %d cores %d threads | %.0f %%\n", m.info.cpu_name.c_str(), m.info.sockets, m.info.physical_cores,
        m.info.logical_cores, m.cpu_history.back());
    for(std::size_t i = 0; i < s.core_usage.size(); ++i)
        std::printf("  core %2zu %5.1f %%  %6.0f MHz\n", i, s.core_usage[i], i < s.core_mhz.size() ? s.core_mhz[i] : 0.0f);
    std::printf("Memory %s / %s | %s %s / %s\n", smg::sys::format_bytes(s.mem_used).c_str(), smg::sys::format_bytes(s.mem_total).c_str(), smg::sys::swap_label(),
        smg::sys::format_bytes(s.swap_used).c_str(), smg::sys::format_bytes(s.swap_total).c_str());
    std::printf("Processes %d  threads %d  handles %d  battery %d\n", s.processes, s.threads, s.handles, s.battery);
    std::printf("Network down %s/s up %s/s\n", smg::sys::format_bytes(s.net_rx).c_str(), smg::sys::format_bytes(s.net_tx).c_str());
    for(const smg::sys::Disk& d : m.disks)
        std::printf("Disk %s (%s) %s free of %s\n", d.name.c_str(), d.detail.c_str(), smg::sys::format_bytes(d.free).c_str(), smg::sys::format_bytes(d.total).c_str());
    for(const smg::sys::Adapter& a : m.info.adapters)
        std::printf("GPU %s  %s dedicated\n", a.name.c_str(), smg::sys::format_bytes(a.dedicated).c_str());
    for(const smg::sys::GpuStat& g : s.gpus)
        std::printf("NVML %s  %.0f %%  %.0f C  %s W  %s / %s\n", g.name.c_str(), g.util, g.temp, watts(g).c_str(),
            smg::sys::format_bytes(g.mem_used).c_str(), smg::sys::format_bytes(g.mem_total).c_str());
    for(const smg::sys::Sensor& t : s.temps) std::printf("Temp %s  %.1f C\n", t.key().c_str(), t.value);
    for(const smg::sys::Sensor& f : s.fans) std::printf("Fan %s  %.0f RPM\n", f.key().c_str(), f.value);
    std::vector<smg::sys::ProcessRow> top = m.processes;
    std::sort(top.begin(), top.end(), [](const smg::sys::ProcessRow& a, const smg::sys::ProcessRow& b) { return a.cpu > b.cpu; });
    for(std::size_t i = 0; i < std::min<std::size_t>(5, top.size()); ++i)
        std::printf("  top %-28s pid %6u  %5.1f %%  %3d thr  %s\n", top[i].name.c_str(), top[i].pid, top[i].cpu, top[i].threads,
            smg::sys::format_bytes(top[i].memory).c_str());
    return 0;
}

int main(int argc, char** argv) {
    for(int i = 1; i < argc; ++i)
        if(std::strcmp(argv[i], "--print") == 0) return printSnapshot();

    // --tab <overview|cpu|memory|processes|disks|gpu|sensors> opens on that tab
    int initialTab = 0;
    for(int i = 1; i + 1 < argc; ++i)
        if(std::strcmp(argv[i], "--tab") == 0)
            for(int t = 0; t < static_cast<int>(sizeof(Tabs) / sizeof(Tabs[0])); ++t)
                if(std::strcmp(argv[i + 1], Tabs[t].key) == 0) initialTab = t;

    smg::GuiConfig config;
    config.title = "smg sysinfo";
    config.size = { 1200, 800 };
    smg::GuiBase gui({ argc, argv }, config);

    smg::sys::Monitor monitor(monitorConfig());
    bool firstFrame = true;

    gui.add_callback([&gui, &monitor, &firstFrame, initialTab]() {
        monitor.update(gui.dt(), ImGui::GetIO().Framerate);

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("sysinfo", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking);
        drawHeader(monitor);
        if(ImGui::BeginTabBar("##tabs")) {
            for(int t = 0; t < static_cast<int>(sizeof(Tabs) / sizeof(Tabs[0])); ++t) {
                const ImGuiTabItemFlags flags = firstFrame && t == initialTab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
                if(ImGui::BeginTabItem(Tabs[t].name, nullptr, flags)) {
                    Tabs[t].draw(monitor);
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
        firstFrame = false;
        return 0;
    });

    while(gui.mainLoopIteration()) {}
    return 0;
}
