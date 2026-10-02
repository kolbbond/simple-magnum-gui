// system monitor: CPU (per core), threads, memory, processes, disks, network, GPU and displays,
// sampled twice a second with ImPlot history. Windows uses Win32/NT APIs, Linux reads /proc.
// Temperatures and fan speeds are not shown: both platforms need a kernel driver or root for them.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <Corrade/Containers/StringStl.h>
#include <Magnum/GL/Context.h>
#include <SDL_video.h>

#include "GuiBase.hh"
#include "imgui.h"
#include "implot.h"

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <winsock2.h>
#    include <windows.h>
#    include <dxgi.h>
#    include <intrin.h>
#    include <ws2ipdef.h> // before iphlpapi: netioapi (GetIfTable2) needs it
#    include <iphlpapi.h>
#    include <netioapi.h>
#    include <powerbase.h>
#    include <psapi.h>
#    include <tlhelp32.h>
#else
#    include <filesystem>
#    include <fstream>
#    include <set>
#    include <sstream>
#    include <sys/statvfs.h>
#    include <sys/utsname.h>
#    include <unistd.h>
#endif

namespace {

constexpr double SamplePeriod = 0.5; // seconds
constexpr int HistoryLength = 240; // two minutes

struct CoreTimes {
    std::uint64_t idle = 0;
    std::uint64_t total = 0;
};

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
    std::string cpuName;
    int sockets = 1;
    int physicalCores = 0;
    int logicalCores = 0;
    std::vector<Adapter> adapters;
};

struct Snapshot {
    std::vector<float> coreUsage; // percent
    std::vector<float> coreMhz;
    float maxMhz = 0.0f;
    double memUsed = 0.0;
    double memTotal = 0.0;
    double swapUsed = 0.0;
    double swapTotal = 0.0;
    int processes = 0;
    int threads = 0;
    int handles = -1; // Windows only
    double netRx = 0.0; // bytes/s
    double netTx = 0.0;
    double uptime = 0.0; // seconds
    int battery = -1; // percent, -1 = no battery
    bool charging = false;
};

std::string trim(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t\r\n");
    if(b == std::string::npos) return {};
    const std::size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string formatBytes(double bytes) {
    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    int u = 0;
    while(bytes >= 1024.0 && u < 4) {
        bytes /= 1024.0;
        ++u;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), u == 0 ? "%.0f %s" : "%.1f %s", bytes, units[u]);
    return buf;
}

std::string formatUptime(double seconds) {
    const long s = static_cast<long>(seconds);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%ldd %02ld:%02ld:%02ld", s / 86400, (s / 3600) % 24, (s / 60) % 60, s % 60);
    return buf;
}

#if defined(_WIN32)

std::string narrow(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if(n <= 1) return {};
    std::string s(static_cast<std::size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

std::uint64_t fileTime(const FILETIME& ft) { return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime; }

// not in the public SDK headers
struct ProcessorPerformance {
    LARGE_INTEGER idle;
    LARGE_INTEGER kernel; // includes idle
    LARGE_INTEGER user;
    LARGE_INTEGER dpc;
    LARGE_INTEGER interrupt;
    ULONG interruptCount;
};
struct ProcessorPower {
    ULONG number;
    ULONG maxMhz;
    ULONG currentMhz;
    ULONG mhzLimit;
    ULONG maxIdleState;
    ULONG currentIdleState;
};
typedef LONG(WINAPI* NtQuerySystemInformationFn)(ULONG, PVOID, ULONG, PULONG);
typedef LONG(WINAPI* RtlGetVersionFn)(OSVERSIONINFOW*);

StaticInfo queryStatic() {
    StaticInfo info;

    char host[256] = {};
    DWORD hostLen = sizeof(host);
    if(GetComputerNameExA(ComputerNameDnsHostname, host, &hostLen)) info.host = host;

    OSVERSIONINFOW ver = {};
    ver.dwOSVersionInfoSize = sizeof(ver);
    const RtlGetVersionFn rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion"));
    if(rtlGetVersion) rtlGetVersion(&ver);
    char display[64] = {};
    DWORD displayLen = sizeof(display);
    RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "DisplayVersion", RRF_RT_REG_SZ, nullptr, display,
        &displayLen);
    char os[128];
    std::snprintf(os, sizeof(os), "Windows %s %s (build %lu)", ver.dwBuildNumber >= 22000 ? "11" : "10", display, ver.dwBuildNumber);
    info.os = os;

    int regs[4];
    char brand[49] = {};
    for(int i = 0; i < 3; ++i) {
        __cpuid(regs, static_cast<int>(0x80000002u + i));
        std::memcpy(brand + 16 * i, regs, 16);
    }
    info.cpuName = trim(brand);

    info.logicalCores = static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationAll, nullptr, &len);
    std::vector<char> buf(len);
    if(GetLogicalProcessorInformationEx(RelationAll, reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buf.data()), &len)) {
        int packages = 0;
        for(DWORD off = 0; off < len;) {
            const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* e = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buf.data() + off);
            if(e->Relationship == RelationProcessorCore) ++info.physicalCores;
            if(e->Relationship == RelationProcessorPackage) ++packages;
            off += e->Size;
        }
        info.sockets = std::max(packages, 1);
    }

    IDXGIFactory1* factory = nullptr;
    if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
        IDXGIAdapter1* adapter = nullptr;
        for(UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc;
            if(SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
                info.adapters.push_back({ narrow(desc.Description), static_cast<double>(desc.DedicatedVideoMemory),
                    static_cast<double>(desc.SharedSystemMemory) });
            adapter->Release();
        }
        factory->Release();
    }
    return info;
}

bool readCoreTimes(std::vector<CoreTimes>& out, int logical) {
    static const NtQuerySystemInformationFn query =
        reinterpret_cast<NtQuerySystemInformationFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation"));
    if(!query) return false;
    std::vector<ProcessorPerformance> perf(static_cast<std::size_t>(logical));
    ULONG got = 0;
    // 8 = SystemProcessorPerformanceInformation; covers the calling thread's processor group (<= 64 cores)
    if(query(8, perf.data(), static_cast<ULONG>(perf.size() * sizeof(ProcessorPerformance)), &got) != 0) return false;
    out.resize(got / sizeof(ProcessorPerformance));
    for(std::size_t i = 0; i < out.size(); ++i) {
        out[i].idle = static_cast<std::uint64_t>(perf[i].idle.QuadPart);
        out[i].total = static_cast<std::uint64_t>(perf[i].kernel.QuadPart + perf[i].user.QuadPart);
    }
    return true;
}

void readCoreMhz(Snapshot& s, int logical) {
    std::vector<ProcessorPower> power(static_cast<std::size_t>(logical));
    if(CallNtPowerInformation(ProcessorInformation, nullptr, 0, power.data(), static_cast<ULONG>(power.size() * sizeof(ProcessorPower)))
        != 0)
        return;
    s.coreMhz.resize(power.size());
    for(std::size_t i = 0; i < power.size(); ++i) {
        s.coreMhz[i] = static_cast<float>(power[i].currentMhz);
        s.maxMhz = std::max(s.maxMhz, static_cast<float>(power[i].maxMhz));
    }
}

void readSystem(Snapshot& s) {
    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    if(GlobalMemoryStatusEx(&mem)) {
        s.memTotal = static_cast<double>(mem.ullTotalPhys);
        s.memUsed = static_cast<double>(mem.ullTotalPhys - mem.ullAvailPhys);
    }
    PERFORMANCE_INFORMATION perf = {};
    if(GetPerformanceInfo(&perf, sizeof(perf))) {
        const double page = static_cast<double>(perf.PageSize);
        s.swapUsed = static_cast<double>(perf.CommitTotal) * page;
        s.swapTotal = static_cast<double>(perf.CommitLimit) * page;
        s.processes = static_cast<int>(perf.ProcessCount);
        s.threads = static_cast<int>(perf.ThreadCount);
        s.handles = static_cast<int>(perf.HandleCount);
    }
    s.uptime = static_cast<double>(GetTickCount64()) / 1000.0;
    SYSTEM_POWER_STATUS power;
    if(GetSystemPowerStatus(&power) && power.BatteryFlag != 128 && power.BatteryLifePercent != 255) {
        s.battery = power.BatteryLifePercent;
        s.charging = power.ACLineStatus == 1;
    }
}

bool readNetBytes(std::uint64_t& rx, std::uint64_t& tx) {
    MIB_IF_TABLE2* table = nullptr;
    if(GetIfTable2(&table) != NO_ERROR) return false;
    rx = 0;
    tx = 0;
    for(ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IF_ROW2& row = table->Table[i];
        // filter/WFP layers repeat each adapter; the hardware row counts it once
        if(row.InterfaceAndOperStatusFlags.HardwareInterface && row.OperStatus == IfOperStatusUp) {
            rx += row.InOctets;
            tx += row.OutOctets;
        }
    }
    FreeMibTable(table);
    return true;
}

std::vector<Disk> readDisks() {
    std::vector<Disk> disks;
    char drives[512] = {};
    const DWORD n = GetLogicalDriveStringsA(sizeof(drives), drives);
    for(const char* d = drives; d < drives + n && *d; d += std::strlen(d) + 1) {
        const UINT type = GetDriveTypeA(d);
        if(type != DRIVE_FIXED && type != DRIVE_REMOVABLE) continue;
        ULARGE_INTEGER freeBytes, totalBytes;
        if(!GetDiskFreeSpaceExA(d, &freeBytes, &totalBytes, nullptr)) continue;
        char label[MAX_PATH + 1] = {};
        char fs[MAX_PATH + 1] = {};
        GetVolumeInformationA(d, label, sizeof(label), nullptr, nullptr, nullptr, fs, sizeof(fs));
        disks.push_back({ d, std::string(label) + (label[0] ? "  " : "") + fs + (type == DRIVE_REMOVABLE ? "  removable" : ""),
            static_cast<double>(totalBytes.QuadPart), static_cast<double>(freeBytes.QuadPart) });
    }
    return disks;
}

class ProcessSampler {
public:
    // cpu is the share of all logical cores since the previous call
    std::vector<ProcessRow> sample(int logical, Snapshot&) {
        std::vector<ProcessRow> rows;
        FILETIME nowFt;
        GetSystemTimeAsFileTime(&nowFt);
        const std::uint64_t now = fileTime(nowFt);
        const double wall = _lastWall ? static_cast<double>(now - _lastWall) * logical : 0.0;

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if(snap == INVALID_HANDLE_VALUE) return rows;
        std::map<std::uint32_t, std::uint64_t> times;
        PROCESSENTRY32W entry = {};
        entry.dwSize = sizeof(entry);
        for(BOOL ok = Process32FirstW(snap, &entry); ok; ok = Process32NextW(snap, &entry)) {
            ProcessRow row;
            row.pid = entry.th32ProcessID;
            row.name = narrow(entry.szExeFile);
            row.threads = static_cast<int>(entry.cntThreads);
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if(h) {
                FILETIME created, exited, kernel, user;
                if(GetProcessTimes(h, &created, &exited, &kernel, &user)) {
                    const std::uint64_t t = fileTime(kernel) + fileTime(user);
                    times[row.pid] = t;
                    const std::map<std::uint32_t, std::uint64_t>::const_iterator prev = _lastTimes.find(row.pid);
                    if(wall > 0.0 && prev != _lastTimes.end() && t >= prev->second)
                        row.cpu = static_cast<float>(100.0 * static_cast<double>(t - prev->second) / wall);
                }
                PROCESS_MEMORY_COUNTERS mem = {};
                if(GetProcessMemoryInfo(h, &mem, sizeof(mem))) row.memory = static_cast<double>(mem.WorkingSetSize);
                CloseHandle(h);
            }
            rows.push_back(row);
        }
        CloseHandle(snap);
        _lastTimes.swap(times);
        _lastWall = now;
        return rows;
    }

private:
    std::map<std::uint32_t, std::uint64_t> _lastTimes;
    std::uint64_t _lastWall = 0;
};

constexpr const char* SwapLabel = "Committed";

#else // Linux

std::string readFile(const char* path) {
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

StaticInfo queryStatic() {
    StaticInfo info;
    char host[256] = {};
    if(gethostname(host, sizeof(host) - 1) == 0) info.host = host;

    std::string pretty;
    std::istringstream release(readFile("/etc/os-release"));
    for(std::string line; std::getline(release, line);)
        if(line.rfind("PRETTY_NAME=", 0) == 0) {
            pretty = line.substr(12);
            pretty.erase(std::remove(pretty.begin(), pretty.end(), '"'), pretty.end());
        }
    utsname uts = {};
    uname(&uts);
    info.os = (pretty.empty() ? std::string(uts.sysname) : pretty) + "  " + uts.release;

    std::set<std::pair<std::string, std::string>> cores;
    std::set<std::string> packages;
    std::string physical;
    std::istringstream cpuinfo(readFile("/proc/cpuinfo"));
    for(std::string line; std::getline(cpuinfo, line);) {
        const std::size_t colon = line.find(':');
        if(colon == std::string::npos) continue;
        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        if(key == "model name" && info.cpuName.empty()) info.cpuName = value;
        if(key == "processor") ++info.logicalCores;
        if(key == "physical id") {
            physical = value;
            packages.insert(value);
        }
        if(key == "core id") cores.insert({ physical, value });
    }
    info.physicalCores = cores.empty() ? info.logicalCores : static_cast<int>(cores.size());
    info.sockets = std::max(static_cast<int>(packages.size()), 1);
    return info;
}

bool readCoreTimes(std::vector<CoreTimes>& out, int) {
    std::istringstream stat(readFile("/proc/stat"));
    out.clear();
    for(std::string line; std::getline(stat, line);) {
        // per-core lines only: "cpu0 ...", not the "cpu " total
        if(line.rfind("cpu", 0) != 0 || line.size() < 4 || line[3] == ' ') continue;
        std::istringstream fields(line.substr(line.find(' ')));
        std::uint64_t v[8] = {};
        for(std::uint64_t& x : v) fields >> x;
        CoreTimes t;
        t.idle = v[3] + v[4]; // idle + iowait
        for(std::uint64_t x : v) t.total += x;
        out.push_back(t);
    }
    return !out.empty();
}

void readCoreMhz(Snapshot& s, int) {
    std::istringstream cpuinfo(readFile("/proc/cpuinfo"));
    s.coreMhz.clear();
    for(std::string line; std::getline(cpuinfo, line);)
        if(line.rfind("cpu MHz", 0) == 0) s.coreMhz.push_back(std::stof(line.substr(line.find(':') + 1)));
    const std::string max = readFile("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    if(!max.empty()) s.maxMhz = std::stof(max) / 1000.0f;
}

void readSystem(Snapshot& s) {
    std::map<std::string, double> mem;
    std::istringstream meminfo(readFile("/proc/meminfo"));
    for(std::string line; std::getline(meminfo, line);) {
        const std::size_t colon = line.find(':');
        if(colon != std::string::npos) mem[line.substr(0, colon)] = std::stod(line.substr(colon + 1)) * 1024.0;
    }
    s.memTotal = mem["MemTotal"];
    s.memUsed = mem["MemTotal"] - mem["MemAvailable"];
    s.swapTotal = mem["SwapTotal"];
    s.swapUsed = mem["SwapTotal"] - mem["SwapFree"];
    const std::string up = readFile("/proc/uptime");
    if(!up.empty()) s.uptime = std::stod(up);
    const std::string capacity = readFile("/sys/class/power_supply/BAT0/capacity");
    if(!capacity.empty()) {
        s.battery = std::stoi(capacity);
        s.charging = trim(readFile("/sys/class/power_supply/BAT0/status")) == "Charging";
    }
}

bool readNetBytes(std::uint64_t& rx, std::uint64_t& tx) {
    std::istringstream dev(readFile("/proc/net/dev"));
    rx = 0;
    tx = 0;
    bool any = false;
    for(std::string line; std::getline(dev, line);) {
        const std::size_t colon = line.find(':');
        if(colon == std::string::npos || trim(line.substr(0, colon)) == "lo") continue;
        std::istringstream fields(line.substr(colon + 1));
        std::uint64_t v[9] = {};
        for(std::uint64_t& x : v) fields >> x;
        rx += v[0];
        tx += v[8];
        any = true;
    }
    return any;
}

std::vector<Disk> readDisks() {
    std::vector<Disk> disks;
    std::set<std::string> seen;
    std::istringstream mounts(readFile("/proc/mounts"));
    for(std::string line; std::getline(mounts, line);) {
        std::istringstream fields(line);
        std::string device, mount, fs;
        fields >> device >> mount >> fs;
        if(device.rfind("/dev/", 0) != 0 || device.rfind("/dev/loop", 0) == 0 || !seen.insert(device).second) continue;
        struct statvfs st;
        if(statvfs(mount.c_str(), &st) != 0) continue;
        disks.push_back({ mount, device + "  " + fs, static_cast<double>(st.f_blocks) * st.f_frsize,
            static_cast<double>(st.f_bavail) * st.f_frsize });
    }
    return disks;
}

class ProcessSampler {
public:
    std::vector<ProcessRow> sample(int, Snapshot& s) {
        std::vector<ProcessRow> rows;
        // whole-machine jiffies, so a process's share needs no core count
        std::istringstream stat(readFile("/proc/stat"));
        std::string first;
        std::getline(stat, first);
        std::istringstream totals(first.substr(4));
        std::uint64_t total = 0;
        for(std::uint64_t x; totals >> x;) total += x;
        const double dtotal = _lastTotal ? static_cast<double>(total - _lastTotal) : 0.0;
        const double page = static_cast<double>(sysconf(_SC_PAGESIZE));

        std::map<std::uint32_t, std::uint64_t> times;
        int threads = 0;
        for(const std::filesystem::directory_entry& e : std::filesystem::directory_iterator("/proc")) {
            const std::string name = e.path().filename().string();
            if(name.empty() || name.find_first_not_of("0123456789") != std::string::npos) continue;
            const std::string text = readFile((e.path() / "stat").c_str());
            const std::size_t open = text.find('(');
            const std::size_t close = text.rfind(')'); // comm may contain spaces and parens
            if(open == std::string::npos || close == std::string::npos) continue;
            ProcessRow row;
            row.pid = static_cast<std::uint32_t>(std::stoul(name));
            row.name = text.substr(open + 1, close - open - 1);
            std::istringstream fields(text.substr(close + 2));
            std::vector<std::string> f;
            for(std::string x; fields >> x;) f.push_back(x);
            if(f.size() < 22) continue;
            // f[0] is field 3 (state): utime 14, stime 15, num_threads 20, rss 24
            const std::uint64_t t = std::stoull(f[11]) + std::stoull(f[12]);
            row.threads = std::stoi(f[17]);
            row.memory = std::stod(f[21]) * page;
            times[row.pid] = t;
            const std::map<std::uint32_t, std::uint64_t>::const_iterator prev = _lastTimes.find(row.pid);
            if(dtotal > 0.0 && prev != _lastTimes.end() && t >= prev->second)
                row.cpu = static_cast<float>(100.0 * static_cast<double>(t - prev->second) / dtotal);
            threads += row.threads;
            rows.push_back(row);
        }
        s.processes = static_cast<int>(rows.size());
        s.threads = threads;
        _lastTimes.swap(times);
        _lastTotal = total;
        return rows;
    }

private:
    std::map<std::uint32_t, std::uint64_t> _lastTimes;
    std::uint64_t _lastTotal = 0;
};

constexpr const char* SwapLabel = "Swap";

#endif

void pushHistory(std::vector<float>& h, float v) {
    h.push_back(v);
    if(h.size() > HistoryLength) h.erase(h.begin());
}

class Monitor {
public:
    StaticInfo info;
    Snapshot now;
    std::vector<ProcessRow> processes;
    std::vector<Disk> disks;

    std::vector<std::vector<float>> coreHistory;
    std::vector<float> cpuHistory, memHistory, swapHistory, rxHistory, txHistory, threadHistory, fpsHistory;
    std::vector<float> times; // x axis: seconds before now

    Monitor() : info(queryStatic()) {
        readCoreTimes(_lastCores, info.logicalCores);
        _haveNet = readNetBytes(_lastRx, _lastTx);
        disks = readDisks();
        sample();
    }

    // outside the frame clock (--print): always walks processes
    void sampleNow() { sample(true); }

    void update(float dt) {
        _fps = ImGui::GetIO().Framerate; // recorded on the sample clock so it lines up with `times`
        _accum += dt;
        if(_accum < SamplePeriod) return;
        _accum = 0.0;
        sample();
    }

private:
    std::vector<CoreTimes> _lastCores;
    ProcessSampler _sampler;
    std::uint64_t _lastRx = 0, _lastTx = 0;
    bool _haveNet = false;
    double _accum = 0.0;
    int _tick = 0;
    std::chrono::steady_clock::time_point _lastSample = std::chrono::steady_clock::now();

    float _fps = 0.0f;

    void sample(bool walkProcesses = false) {
        const std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(t - _lastSample).count();
        _lastSample = t;

        std::vector<CoreTimes> cores;
        if(readCoreTimes(cores, info.logicalCores) && cores.size() == _lastCores.size()) {
            now.coreUsage.resize(cores.size());
            for(std::size_t i = 0; i < cores.size(); ++i) {
                const double total = static_cast<double>(cores[i].total - _lastCores[i].total);
                const double idle = static_cast<double>(cores[i].idle - _lastCores[i].idle);
                now.coreUsage[i] = total > 0.0 ? static_cast<float>(std::clamp(100.0 * (1.0 - idle / total), 0.0, 100.0)) : 0.0f;
            }
        }
        _lastCores = cores;
        readCoreMhz(now, info.logicalCores);
        readSystem(now);

        std::uint64_t rx = 0, tx = 0;
        if(_haveNet && readNetBytes(rx, tx) && elapsed > 0.05) {
            now.netRx = rx >= _lastRx ? static_cast<double>(rx - _lastRx) / elapsed : 0.0;
            now.netTx = tx >= _lastTx ? static_cast<double>(tx - _lastTx) / elapsed : 0.0;
            _lastRx = rx;
            _lastTx = tx;
        }

        // process walk and disks are heavier: once a second / every 10 s
        if(walkProcesses || _tick % 2 == 0) processes = _sampler.sample(info.logicalCores, now);
        if(_tick % 20 == 19) disks = readDisks();
        ++_tick;

        if(coreHistory.size() != now.coreUsage.size()) coreHistory.assign(now.coreUsage.size(), {});
        float total = 0.0f;
        for(std::size_t i = 0; i < now.coreUsage.size(); ++i) {
            pushHistory(coreHistory[i], now.coreUsage[i]);
            total += now.coreUsage[i];
        }
        pushHistory(cpuHistory, now.coreUsage.empty() ? 0.0f : total / static_cast<float>(now.coreUsage.size()));
        pushHistory(memHistory, static_cast<float>(now.memUsed / 1073741824.0));
        pushHistory(swapHistory, static_cast<float>(now.swapUsed / 1073741824.0));
        pushHistory(rxHistory, static_cast<float>(now.netRx / 1024.0));
        pushHistory(txHistory, static_cast<float>(now.netTx / 1024.0));
        pushHistory(threadHistory, static_cast<float>(now.threads));
        pushHistory(fpsHistory, _fps);

        times.resize(cpuHistory.size());
        for(std::size_t i = 0; i < times.size(); ++i) times[i] = static_cast<float>((static_cast<double>(i) - (times.size() - 1)) * SamplePeriod);
    }
};

constexpr ImPlotFlags SparkFlags = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect
    | ImPlotFlags_NoMouseText | ImPlotFlags_NoInputs | ImPlotFlags_NoFrame;

// shaded 0..100 % history
void percentPlot(const char* id, const Monitor& m, const std::vector<float>& h, const ImVec2& size, bool decorated) {
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

void drawHeader(const Monitor& m) {
    ImGui::TextUnformatted(m.info.host.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s   up %s", m.info.os.c_str(), formatUptime(m.now.uptime).c_str());
    ImGui::TextDisabled("%s   %d socket(s), %d cores, %d threads", m.info.cpuName.c_str(), m.info.sockets, m.info.physicalCores,
        m.info.logicalCores);
    ImGui::Separator();
}

void drawOverview(const Monitor& m) {
    const float h = std::max(140.0f, (ImGui::GetContentRegionAvail().y - 80.0f) / 2.0f);
    const float cpu = m.cpuHistory.empty() ? 0.0f : m.cpuHistory.back();

    if(ImGui::BeginTable("##overview", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        ImGui::Text("CPU  %.0f %%", cpu);
        percentPlot("##cpu", m, m.cpuHistory, ImVec2(-1, h), true);

        ImGui::TableNextColumn();
        ImGui::Text("Memory  %s / %s", formatBytes(m.now.memUsed).c_str(), formatBytes(m.now.memTotal).c_str());
        if(ImPlot::BeginPlot("##mem", ImVec2(-1, h), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {
            ImPlot::SetupAxes("seconds", "GB");
            ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, m.now.memTotal / 1073741824.0, ImPlotCond_Always);
            ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.35f);
            ImPlot::PlotShaded("used", m.times.data(), m.memHistory.data(), static_cast<int>(m.memHistory.size()));
            ImPlot::EndPlot();
        }

        ImGui::TableNextColumn();
        ImGui::Text("Network  down %s/s  up %s/s", formatBytes(m.now.netRx).c_str(), formatBytes(m.now.netTx).c_str());
        if(ImPlot::BeginPlot("##net", ImVec2(-1, h), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {
            ImPlot::SetupAxes("seconds", "KB/s", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
            ImPlot::PlotLine("down", m.times.data(), m.rxHistory.data(), static_cast<int>(m.rxHistory.size()));
            ImPlot::PlotLine("up", m.times.data(), m.txHistory.data(), static_cast<int>(m.txHistory.size()));
            ImPlot::EndPlot();
        }

        ImGui::TableNextColumn();
        ImGui::Text("Per core now");
        if(ImPlot::BeginPlot("##cores", ImVec2(-1, h), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoLegend)) {
            ImPlot::SetupAxes("core", "%", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_None);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 100.0, ImPlotCond_Always);
            ImPlot::PlotBars("usage", m.now.coreUsage.data(), static_cast<int>(m.now.coreUsage.size()), 0.7);
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

void drawCpu(const Monitor& m) {
    const int n = static_cast<int>(m.coreHistory.size());
    if(n == 0) return;
    const int columns = n <= 4 ? n : (n <= 16 ? 4 : 8);

    if(ImGui::BeginTable("##coregrid", columns, ImGuiTableFlags_SizingStretchSame)) {
        for(int i = 0; i < n; ++i) {
            ImGui::TableNextColumn();
            const float mhz = i < static_cast<int>(m.now.coreMhz.size()) ? m.now.coreMhz[i] : 0.0f;
            ImGui::Text("CPU %d  %3.0f %%  %.2f GHz", i, m.now.coreUsage[i], mhz / 1000.0f);
            char id[16];
            std::snprintf(id, sizeof(id), "##core%d", i);
            percentPlot(id, m, m.coreHistory[i], ImVec2(-1, 70), false);
        }
        ImGui::EndTable();
    }
    if(m.now.maxMhz > 0.0f) ImGui::TextDisabled("rated %.2f GHz (Windows reports a coarse current clock)", m.now.maxMhz / 1000.0f);

    // cores x time heatmap
    const int cols = static_cast<int>(m.times.size());
    if(cols < 2) return;
    std::vector<float> values(static_cast<std::size_t>(n) * cols);
    for(int r = 0; r < n; ++r)
        for(int c = 0; c < cols; ++c) values[static_cast<std::size_t>(r) * cols + c] = m.coreHistory[r][c];
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

void drawMemory(const Monitor& m) {
    const double gb = 1073741824.0;
    ImGui::Text("Physical  %s / %s", formatBytes(m.now.memUsed).c_str(), formatBytes(m.now.memTotal).c_str());
    ImGui::ProgressBar(m.now.memTotal > 0.0 ? static_cast<float>(m.now.memUsed / m.now.memTotal) : 0.0f, ImVec2(-1, 0));
    ImGui::Text("%s  %s / %s", SwapLabel, formatBytes(m.now.swapUsed).c_str(), formatBytes(m.now.swapTotal).c_str());
    ImGui::ProgressBar(m.now.swapTotal > 0.0 ? static_cast<float>(m.now.swapUsed / m.now.swapTotal) : 0.0f, ImVec2(-1, 0));

    if(ImPlot::BeginPlot("##memory", ImVec2(-1, ImGui::GetContentRegionAvail().y), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {
        ImPlot::SetupAxes("seconds", "GB");
        ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, std::max(m.now.memTotal, m.now.swapTotal) / gb * 1.05, ImPlotCond_Always);
        const int n = static_cast<int>(m.memHistory.size());
        ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.35f);
        ImPlot::PlotShaded("physical", m.times.data(), m.memHistory.data(), n);
        ImPlot::PlotLine(SwapLabel, m.times.data(), m.swapHistory.data(), n);
        const double total = m.now.memTotal / gb;
        ImPlot::PlotInfLines("installed", &total, 1, ImPlotInfLinesFlags_Horizontal);
        ImPlot::EndPlot();
    }
}

void drawProcesses(const Monitor& m) {
    static ImGuiTextFilter filter;
    ImGui::Text("%d processes, %d threads", m.now.processes, m.now.threads);
    ImGui::SameLine();
    filter.Draw("filter", 200.0f);

    if(ImPlot::BeginPlot("##threads", ImVec2(-1, 110), SparkFlags)) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
        ImPlot::PlotLine("threads", m.times.data(), m.threadHistory.data(), static_cast<int>(m.threadHistory.size()));
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

    std::vector<const ProcessRow*> rows;
    for(const ProcessRow& p : m.processes)
        if(filter.PassFilter(p.name.c_str())) rows.push_back(&p);
    const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
    if(specs && specs->SpecsCount > 0) {
        const int column = specs->Specs[0].ColumnIndex;
        const bool ascending = specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
        std::stable_sort(rows.begin(), rows.end(), [column, ascending](const ProcessRow* a, const ProcessRow* b) {
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
            const ProcessRow& p = *rows[i];
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
            ImGui::TextUnformatted(p.memory > 0.0 ? formatBytes(p.memory).c_str() : "-");
        }
    ImGui::EndTable();
}

void drawStorageNetwork(const Monitor& m) {
    ImGui::SeparatorText("Disks");
    for(const Disk& d : m.disks) {
        const double used = d.total - d.free;
        ImGui::Text("%s", d.name.c_str());
        ImGui::SameLine(120.0f);
        ImGui::TextDisabled("%s", d.detail.c_str());
        char overlay[64];
        std::snprintf(overlay, sizeof(overlay), "%s free of %s", formatBytes(d.free).c_str(), formatBytes(d.total).c_str());
        ImGui::ProgressBar(d.total > 0.0 ? static_cast<float>(used / d.total) : 0.0f, ImVec2(-1, 0), overlay);
    }

    ImGui::SeparatorText("Network");
    ImGui::Text("down %s/s   up %s/s", formatBytes(m.now.netRx).c_str(), formatBytes(m.now.netTx).c_str());
    if(ImPlot::BeginPlot("##network", ImVec2(-1, ImGui::GetContentRegionAvail().y), ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {
        ImPlot::SetupAxes("seconds", "KB/s", ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
        ImPlot::SetupAxisLimits(ImAxis_X1, -HistoryLength * SamplePeriod, 0.0, ImPlotCond_Always);
        const int n = static_cast<int>(m.rxHistory.size());
        ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.3f);
        ImPlot::PlotShaded("down", m.times.data(), m.rxHistory.data(), n);
        ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.3f);
        ImPlot::PlotShaded("up", m.times.data(), m.txHistory.data(), n);
        ImPlot::EndPlot();
    }
}

void drawGpuDisplays(const Monitor& m) {
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
        for(const Adapter& a : m.info.adapters)
            ImGui::Text("%s   %s dedicated, %s shared", a.name.c_str(), formatBytes(a.dedicated).c_str(), formatBytes(a.shared).c_str());
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
        ImPlot::PlotLine("fps", m.times.data(), m.fpsHistory.data(), static_cast<int>(m.fpsHistory.size()));
        ImPlot::EndPlot();
    }
}

struct Tab {
    const char* name;
    const char* key; // for --tab
    void (*draw)(const Monitor&);
};

const Tab Tabs[] = {
    { "Overview", "overview", drawOverview },
    { "CPU", "cpu", drawCpu },
    { "Memory", "memory", drawMemory },
    { "Processes", "processes", drawProcesses },
    { "Disks & Network", "disks", drawStorageNetwork },
    { "GPU & Displays", "gpu", drawGpuDisplays },
};

} // namespace

// --print: one text snapshot, no window
int printSnapshot() {
    Monitor m;
    std::this_thread::sleep_for(std::chrono::seconds(1)); // let the rates (CPU, network, per-process) span a full second
    m.sampleNow();
    const Snapshot& s = m.now;
    std::printf("%s | %s | up %s\n", m.info.host.c_str(), m.info.os.c_str(), formatUptime(s.uptime).c_str());
    std::printf("CPU  %s | %d socket(s) %d cores %d threads | %.0f %%\n", m.info.cpuName.c_str(), m.info.sockets, m.info.physicalCores,
        m.info.logicalCores, m.cpuHistory.back());
    for(std::size_t i = 0; i < s.coreUsage.size(); ++i)
        std::printf("  core %2zu %5.1f %%  %6.0f MHz\n", i, s.coreUsage[i], i < s.coreMhz.size() ? s.coreMhz[i] : 0.0f);
    std::printf("Memory %s / %s | %s %s / %s\n", formatBytes(s.memUsed).c_str(), formatBytes(s.memTotal).c_str(), SwapLabel,
        formatBytes(s.swapUsed).c_str(), formatBytes(s.swapTotal).c_str());
    std::printf("Processes %d  threads %d  handles %d  battery %d\n", s.processes, s.threads, s.handles, s.battery);
    std::printf("Network down %s/s up %s/s\n", formatBytes(s.netRx).c_str(), formatBytes(s.netTx).c_str());
    for(const Disk& d : m.disks)
        std::printf("Disk %s (%s) %s free of %s\n", d.name.c_str(), d.detail.c_str(), formatBytes(d.free).c_str(), formatBytes(d.total).c_str());
    for(const Adapter& a : m.info.adapters)
        std::printf("GPU %s  %s dedicated\n", a.name.c_str(), formatBytes(a.dedicated).c_str());
    std::vector<ProcessRow> top = m.processes;
    std::sort(top.begin(), top.end(), [](const ProcessRow& a, const ProcessRow& b) { return a.cpu > b.cpu; });
    for(std::size_t i = 0; i < std::min<std::size_t>(5, top.size()); ++i)
        std::printf("  top %-28s pid %6u  %5.1f %%  %3d thr  %s\n", top[i].name.c_str(), top[i].pid, top[i].cpu, top[i].threads,
            formatBytes(top[i].memory).c_str());
    return 0;
}

int main(int argc, char** argv) {
    for(int i = 1; i < argc; ++i)
        if(std::strcmp(argv[i], "--print") == 0) return printSnapshot();

    // --tab <overview|cpu|memory|processes|disks|gpu> opens on that tab
    int initialTab = 0;
    for(int i = 1; i + 1 < argc; ++i)
        if(std::strcmp(argv[i], "--tab") == 0)
            for(int t = 0; t < static_cast<int>(sizeof(Tabs) / sizeof(Tabs[0])); ++t)
                if(std::strcmp(argv[i + 1], Tabs[t].key) == 0) initialTab = t;

    smg::GuiConfig config;
    config.title = "smg sysinfo";
    config.size = { 1200, 800 };
    smg::GuiBase gui({ argc, argv }, config);

    Monitor monitor;
    bool firstFrame = true;

    gui.add_callback([&gui, &monitor, &firstFrame, initialTab]() {
        monitor.update(gui.dt());

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
