// system data for monitors and desktop panels: CPU, memory, processes, disks, network, sensors, GPUs.
// No GL or ImGui here, so it samples headless and the readers can be pointed at a fixture tree.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace smg::sys {

struct ProcessRow {
    std::uint32_t pid = 0;
    std::string name;
    int threads = 0;
    float cpu = 0.0f; // percent of the whole machine
    double memory = 0.0; // resident bytes
};

struct Disk {
    std::string name;
    std::string detail;
    double total = 0.0;
    double free = 0.0;
};

struct Adapter {
    std::string name;
    double dedicated = 0.0;
    double shared = 0.0;
};

struct StaticInfo {
    std::string host;
    std::string os;
    std::string cpu_name;
    int sockets = 1;
    int physical_cores = 0;
    int logical_cores = 0;
    std::vector<Adapter> adapters;
};

// one hwmon channel: degrees C for temps, RPM for fans
struct Sensor {
    std::string chip; // hwmon `name`, e.g. coretemp, it8620
    std::string label; // temp*_label, else "temp1" / "fan2"
    float value = 0.0f;
    [[nodiscard]] std::string key() const { return chip + "/" + label; }
};

// NVML; -1 = not reported by this device
struct GpuStat {
    std::string name;
    float util = -1.0f; // percent
    float temp = -1.0f; // C
    float power = -1.0f; // W
    double mem_used = 0.0; // bytes
    double mem_total = 0.0;
};

struct Snapshot {
    std::vector<float> core_usage; // percent
    std::vector<float> core_mhz;
    float max_mhz = 0.0f;
    double mem_used = 0.0;
    double mem_total = 0.0;
    double swap_used = 0.0;
    double swap_total = 0.0;
    int processes = 0;
    int threads = 0;
    int handles = -1; // Windows only
    double net_rx = 0.0; // bytes/s
    double net_tx = 0.0;
    double uptime = 0.0; // seconds
    int battery = -1; // percent, -1 = no battery
    bool charging = false;
    std::vector<Sensor> temps;
    std::vector<Sensor> fans;
    std::vector<GpuStat> gpus;

    // average over cores, 0 before the first delta
    [[nodiscard]] float cpu() const;
    // first sensor whose key() contains `match`; nullptr if none
    [[nodiscard]] const Sensor* temp(const std::string& match) const;
    [[nodiscard]] const Sensor* fan(const std::string& match) const;
};

struct MonitorConfig {
    double sample_period = 0.5; // seconds
    int history_length = 240;
    // filesystem root the Linux readers resolve /proc and /sys against; tests point it at a fixture tree
    std::string root = "/";
    bool sensors = true; // hwmon temps and fans
    bool gpus = true; // NVML, if libnvidia-ml is present
};

std::string format_bytes(double bytes);
std::string format_uptime(double seconds);
// what the swap figures mean on this platform ("Swap", or "Committed" on Windows)
const char* swap_label();
void push_history(std::vector<float>& history, float value, std::size_t max);

class Monitor {
public:
    StaticInfo info;
    Snapshot now;
    std::vector<ProcessRow> processes;
    std::vector<Disk> disks;

    std::vector<std::vector<float>> core_history;
    std::vector<float> cpu_history, mem_history, swap_history, rx_history, tx_history, thread_history, fps_history;
    std::vector<std::vector<float>> temp_history; // parallel to now.temps
    std::vector<std::vector<float>> gpu_util_history, gpu_temp_history; // parallel to now.gpus
    std::vector<float> times; // x axis: seconds before now

    explicit Monitor(const MonitorConfig& config = MonitorConfig{});
    ~Monitor();
    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;

    // advance the sample clock; `fps` is recorded on it so it lines up with `times`
    void update(double dt, float fps = 0.0f);
    // sample immediately, always walking processes (one-shot use, e.g. --print)
    void sample_now();

    [[nodiscard]] const MonitorConfig& config() const { return _config; }

private:
    struct Impl;
    MonitorConfig _config;
    std::unique_ptr<Impl> _impl;
    double _accum = 0.0;
    int _tick = 0;
    float _fps = 0.0f;

    void sample(bool walk_processes);
};

// a shell command run every `period` seconds on a worker thread; the panel reads its last output
class Command {
public:
    Command(std::string cmd, double period);
    ~Command();
    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;

    [[nodiscard]] const std::string& cmd() const { return _cmd; }
    // last complete stdout, trailing newline trimmed
    [[nodiscard]] std::string text() const;
    // the first number in text(), if any
    [[nodiscard]] bool value(double& out) const;
    // true once the command has completed at least once
    [[nodiscard]] bool ready() const;

private:
    struct State;
    std::string _cmd;
    std::shared_ptr<State> _state;
};

// first number in `text` (sign, decimals, exponent); false if none
bool parse_number(const std::string& text, double& out);

} // namespace smg::sys
