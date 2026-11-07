#include "Sys.hh"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <utility>

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
#    define popen _popen
#    define pclose _pclose
#else
#    include <dlfcn.h>
#    include <filesystem>
#    include <fstream>
#    include <set>
#    include <sstream>
#    include <sys/statvfs.h>
#    include <sys/utsname.h>
#    include <unistd.h>
#endif

namespace smg::sys {

namespace {

struct CoreTimes {
    std::uint64_t idle = 0;
    std::uint64_t total = 0;
};

std::string trim(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t\r\n");
    if(b == std::string::npos) return {};
    const std::size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

#if defined(_WIN32)

std::string narrow(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if(n <= 1) return {};
    std::string s(static_cast<std::size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

std::uint64_t file_time(const FILETIME& ft) { return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime; }

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

StaticInfo query_static(const std::string&) {
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
    RegGetValueA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        "DisplayVersion",
        RRF_RT_REG_SZ,
        nullptr,
        display,
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
    info.cpu_name = trim(brand);

    info.logical_cores = static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationAll, nullptr, &len);
    std::vector<char> buf(len);
    if(GetLogicalProcessorInformationEx(RelationAll, reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buf.data()), &len)) {
        int packages = 0;
        for(DWORD off = 0; off < len;) {
            const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* e = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buf.data() + off);
            if(e->Relationship == RelationProcessorCore) ++info.physical_cores;
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
                info.adapters.push_back({ narrow(desc.Description),
                    static_cast<double>(desc.DedicatedVideoMemory),
                    static_cast<double>(desc.SharedSystemMemory) });
            adapter->Release();
        }
        factory->Release();
    }
    return info;
}

bool read_core_times(const std::string&, std::vector<CoreTimes>& out, int logical) {
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

void read_core_mhz(const std::string&, Snapshot& s, int logical) {
    std::vector<ProcessorPower> power(static_cast<std::size_t>(logical));
    if(CallNtPowerInformation(ProcessorInformation, nullptr, 0, power.data(), static_cast<ULONG>(power.size() * sizeof(ProcessorPower))) !=
        0)
        return;
    s.core_mhz.resize(power.size());
    for(std::size_t i = 0; i < power.size(); ++i) {
        s.core_mhz[i] = static_cast<float>(power[i].currentMhz);
        s.max_mhz = std::max(s.max_mhz, static_cast<float>(power[i].maxMhz));
    }
}

void read_system(const std::string&, Snapshot& s) {
    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    if(GlobalMemoryStatusEx(&mem)) {
        s.mem_total = static_cast<double>(mem.ullTotalPhys);
        s.mem_used = static_cast<double>(mem.ullTotalPhys - mem.ullAvailPhys);
    }
    PERFORMANCE_INFORMATION perf = {};
    if(GetPerformanceInfo(&perf, sizeof(perf))) {
        const double page = static_cast<double>(perf.PageSize);
        s.swap_used = static_cast<double>(perf.CommitTotal) * page;
        s.swap_total = static_cast<double>(perf.CommitLimit) * page;
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

bool read_net_bytes(const std::string&, std::uint64_t& rx, std::uint64_t& tx) {
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

std::vector<Disk> read_disks(const std::string&) {
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
        disks.push_back({ d,
            std::string(label) + (label[0] ? "  " : "") + fs + (type == DRIVE_REMOVABLE ? "  removable" : ""),
            static_cast<double>(totalBytes.QuadPart),
            static_cast<double>(freeBytes.QuadPart) });
    }
    return disks;
}

// temperatures and fans need a kernel driver on Windows
void read_hwmon(const std::string&, Snapshot&) {}

class ProcessSampler {
public:
    explicit ProcessSampler(std::string) {}

    // cpu is the share of all logical cores since the previous call
    std::vector<ProcessRow> sample(int logical, Snapshot&) {
        std::vector<ProcessRow> rows;
        FILETIME nowFt;
        GetSystemTimeAsFileTime(&nowFt);
        const std::uint64_t now = file_time(nowFt);
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
                    const std::uint64_t t = file_time(kernel) + file_time(user);
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

#else // Linux

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

StaticInfo query_static(const std::string& root) {
    StaticInfo info;
    char host[256] = {};
    if(gethostname(host, sizeof(host) - 1) == 0) info.host = host;

    std::string pretty;
    std::istringstream release(read_file(root + "/etc/os-release"));
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
    std::istringstream cpuinfo(read_file(root + "/proc/cpuinfo"));
    for(std::string line; std::getline(cpuinfo, line);) {
        const std::size_t colon = line.find(':');
        if(colon == std::string::npos) continue;
        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        if(key == "model name" && info.cpu_name.empty()) info.cpu_name = value;
        if(key == "processor") ++info.logical_cores;
        if(key == "physical id") {
            physical = value;
            packages.insert(value);
        }
        if(key == "core id") cores.insert({ physical, value });
    }
    info.physical_cores = cores.empty() ? info.logical_cores : static_cast<int>(cores.size());
    info.sockets = std::max(static_cast<int>(packages.size()), 1);
    return info;
}

bool read_core_times(const std::string& root, std::vector<CoreTimes>& out, int) {
    std::istringstream stat(read_file(root + "/proc/stat"));
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

void read_core_mhz(const std::string& root, Snapshot& s, int) {
    std::istringstream cpuinfo(read_file(root + "/proc/cpuinfo"));
    s.core_mhz.clear();
    for(std::string line; std::getline(cpuinfo, line);)
        if(line.rfind("cpu MHz", 0) == 0) s.core_mhz.push_back(std::stof(line.substr(line.find(':') + 1)));
    const std::string max = read_file(root + "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    if(!max.empty()) s.max_mhz = std::stof(max) / 1000.0f;
}

void read_system(const std::string& root, Snapshot& s) {
    std::map<std::string, double> mem;
    std::istringstream meminfo(read_file(root + "/proc/meminfo"));
    for(std::string line; std::getline(meminfo, line);) {
        const std::size_t colon = line.find(':');
        if(colon != std::string::npos) mem[line.substr(0, colon)] = std::stod(line.substr(colon + 1)) * 1024.0;
    }
    s.mem_total = mem["MemTotal"];
    s.mem_used = mem["MemTotal"] - mem["MemAvailable"];
    s.swap_total = mem["SwapTotal"];
    s.swap_used = mem["SwapTotal"] - mem["SwapFree"];
    const std::string up = read_file(root + "/proc/uptime");
    if(!up.empty()) s.uptime = std::stod(up);
    const std::string capacity = read_file(root + "/sys/class/power_supply/BAT0/capacity");
    if(!capacity.empty()) {
        s.battery = std::stoi(capacity);
        s.charging = trim(read_file(root + "/sys/class/power_supply/BAT0/status")) == "Charging";
    }
}

bool read_net_bytes(const std::string& root, std::uint64_t& rx, std::uint64_t& tx) {
    std::istringstream dev(read_file(root + "/proc/net/dev"));
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

std::vector<Disk> read_disks(const std::string& root) {
    std::vector<Disk> disks;
    std::set<std::string> seen;
    std::istringstream mounts(read_file(root + "/proc/mounts"));
    for(std::string line; std::getline(mounts, line);) {
        std::istringstream fields(line);
        std::string device, mount, fs;
        fields >> device >> mount >> fs;
        if(device.rfind("/dev/", 0) != 0 || device.rfind("/dev/loop", 0) == 0 || !seen.insert(device).second) continue;
        struct statvfs st;
        if(statvfs(mount.c_str(), &st) != 0) continue;
        disks.push_back(
            { mount, device + "  " + fs, static_cast<double>(st.f_blocks) * st.f_frsize, static_cast<double>(st.f_bavail) * st.f_frsize });
    }
    return disks;
}

// hwmon is world-readable: no root needed for temps or fans
void read_hwmon(const std::string& root, Snapshot& s) {
    s.temps.clear();
    s.fans.clear();
    std::error_code ec;
    const std::filesystem::path base = root + "/sys/class/hwmon";
    std::vector<std::filesystem::path> chips;
    for(const std::filesystem::directory_entry& e : std::filesystem::directory_iterator(base, ec)) chips.push_back(e.path());
    std::sort(chips.begin(), chips.end()); // hwmonN order, stable across samples
    for(const std::filesystem::path& chip : chips) {
        const std::string name = trim(read_file((chip / "name").string()));
        std::vector<std::string> inputs;
        for(const std::filesystem::directory_entry& f : std::filesystem::directory_iterator(chip, ec)) {
            const std::string file = f.path().filename().string();
            const std::size_t suffix = file.rfind("_input");
            if(suffix == std::string::npos || suffix + 6 != file.size()) continue;
            if(file.rfind("temp", 0) == 0 || file.rfind("fan", 0) == 0) inputs.push_back(file);
        }
        std::sort(inputs.begin(), inputs.end(), [](const std::string& a, const std::string& b) {
            return a.size() != b.size() ? a.size() < b.size() : a < b; // temp2 before temp10
        });
        for(const std::string& file : inputs) {
            const std::string channel = file.substr(0, file.size() - 6);
            const std::string raw = trim(read_file((chip / file).string()));
            double v = 0.0;
            if(!parse_number(raw, v)) continue;
            std::string label = trim(read_file((chip / (channel + "_label")).string()));
            if(label.empty()) label = channel;
            // it87 and friends report an unconnected thermistor as -128 C
            if(channel.rfind("temp", 0) == 0 && v <= -100000.0) continue;
            if(channel.rfind("temp", 0) == 0)
                s.temps.push_back({ name, label, static_cast<float>(v / 1000.0) });
            else
                s.fans.push_back({ name, label, static_cast<float>(v) });
        }
    }
}

class ProcessSampler {
public:
    explicit ProcessSampler(std::string root) : _root(std::move(root)) {}

    std::vector<ProcessRow> sample(int, Snapshot& s) {
        std::vector<ProcessRow> rows;
        // whole-machine jiffies, so a process's share needs no core count
        std::istringstream stat(read_file(_root + "/proc/stat"));
        std::string first;
        std::getline(stat, first);
        std::istringstream totals(first.size() > 4 ? first.substr(4) : std::string());
        std::uint64_t total = 0;
        for(std::uint64_t x; totals >> x;) total += x;
        const double dtotal = _lastTotal ? static_cast<double>(total - _lastTotal) : 0.0;
        const double page = static_cast<double>(sysconf(_SC_PAGESIZE));

        std::map<std::uint32_t, std::uint64_t> times;
        int threads = 0;
        std::error_code ec;
        for(const std::filesystem::directory_entry& e : std::filesystem::directory_iterator(_root + "/proc", ec)) {
            const std::string name = e.path().filename().string();
            if(name.empty() || name.find_first_not_of("0123456789") != std::string::npos) continue;
            const std::string text = read_file((e.path() / "stat").string());
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
    std::string _root;
    std::map<std::uint32_t, std::uint64_t> _lastTimes;
    std::uint64_t _lastTotal = 0;
};

#endif

// NVML, loaded at runtime so smg never links against the NVIDIA driver
class Nvml {
public:
    Nvml() {
#if !defined(_WIN32)
        _lib = dlopen("libnvidia-ml.so.1", RTLD_NOW | RTLD_LOCAL);
        if(!_lib) return;
        _init = reinterpret_cast<IntFn>(dlsym(_lib, "nvmlInit_v2"));
        _shutdown = reinterpret_cast<IntFn>(dlsym(_lib, "nvmlShutdown"));
        _count = reinterpret_cast<CountFn>(dlsym(_lib, "nvmlDeviceGetCount_v2"));
        _handle = reinterpret_cast<HandleFn>(dlsym(_lib, "nvmlDeviceGetHandleByIndex_v2"));
        _name = reinterpret_cast<NameFn>(dlsym(_lib, "nvmlDeviceGetName"));
        _util = reinterpret_cast<UtilFn>(dlsym(_lib, "nvmlDeviceGetUtilizationRates"));
        _temp = reinterpret_cast<TempFn>(dlsym(_lib, "nvmlDeviceGetTemperature"));
        _memory = reinterpret_cast<MemoryFn>(dlsym(_lib, "nvmlDeviceGetMemoryInfo"));
        _power = reinterpret_cast<UintFn>(dlsym(_lib, "nvmlDeviceGetPowerUsage"));
        _ok = _init && _shutdown && _count && _handle && _init() == 0;
#endif
    }
    ~Nvml() {
#if !defined(_WIN32)
        if(_ok) _shutdown();
        if(_lib) dlclose(_lib);
#endif
    }
    Nvml(const Nvml&) = delete;
    Nvml& operator=(const Nvml&) = delete;

    void read(std::vector<GpuStat>& out) const {
        out.clear();
        unsigned int count = 0;
        if(!_ok || _count(&count) != 0) return;
        for(unsigned int i = 0; i < count; ++i) {
            void* dev = nullptr;
            if(_handle(i, &dev) != 0) continue;
            GpuStat g;
            char name[96] = {};
            if(_name && _name(dev, name, sizeof(name)) == 0) g.name = name;
            Utilization u = {};
            if(_util && _util(dev, &u) == 0) g.util = static_cast<float>(u.gpu);
            unsigned int v = 0;
            if(_temp && _temp(dev, 0, &v) == 0) g.temp = static_cast<float>(v); // 0 = NVML_TEMPERATURE_GPU
            if(_power && _power(dev, &v) == 0) g.power = static_cast<float>(v) / 1000.0f; // mW
            Memory m = {};
            if(_memory && _memory(dev, &m) == 0) {
                g.mem_used = static_cast<double>(m.used);
                g.mem_total = static_cast<double>(m.total);
            }
            out.push_back(g);
        }
    }

private:
    struct Utilization {
        unsigned int gpu;
        unsigned int memory;
    };
    struct Memory {
        unsigned long long total;
        unsigned long long free;
        unsigned long long used;
    };
    typedef int (*IntFn)();
    typedef int (*CountFn)(unsigned int*);
    typedef int (*HandleFn)(unsigned int, void**);
    typedef int (*NameFn)(void*, char*, unsigned int);
    typedef int (*UtilFn)(void*, Utilization*);
    typedef int (*TempFn)(void*, int, unsigned int*);
    typedef int (*MemoryFn)(void*, Memory*);
    typedef int (*UintFn)(void*, unsigned int*);

    void* _lib = nullptr;
    bool _ok = false;
    IntFn _init = nullptr;
    IntFn _shutdown = nullptr;
    CountFn _count = nullptr;
    HandleFn _handle = nullptr;
    NameFn _name = nullptr;
    UtilFn _util = nullptr;
    TempFn _temp = nullptr;
    MemoryFn _memory = nullptr;
    UintFn _power = nullptr;
};

const Sensor* find_sensor(const std::vector<Sensor>& list, const std::string& match) {
    for(const Sensor& s : list)
        if(s.key().find(match) != std::string::npos) return &s;
    return nullptr;
}

} // namespace

float Snapshot::cpu() const {
    if(core_usage.empty()) return 0.0f;
    float total = 0.0f;
    for(float c : core_usage) total += c;
    return total / static_cast<float>(core_usage.size());
}

const Sensor* Snapshot::temp(const std::string& match) const { return find_sensor(temps, match); }
const Sensor* Snapshot::fan(const std::string& match) const { return find_sensor(fans, match); }

std::string format_bytes(double bytes) {
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

std::string format_uptime(double seconds) {
    const long s = static_cast<long>(seconds);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%ldd %02ld:%02ld:%02ld", s / 86400, (s / 3600) % 24, (s / 60) % 60, s % 60);
    return buf;
}

const char* swap_label() {
#if defined(_WIN32)
    return "Committed";
#else
    return "Swap";
#endif
}

void push_history(std::vector<float>& history, float value, std::size_t max) {
    history.push_back(value);
    if(history.size() > max) history.erase(history.begin(), history.begin() + static_cast<std::ptrdiff_t>(history.size() - max));
}

bool parse_number(const std::string& text, double& out) {
    for(std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        const bool sign = (c == '-' || c == '+') && i + 1 < text.size() && (std::isdigit(static_cast<unsigned char>(text[i + 1])) != 0);
        if(!sign && std::isdigit(static_cast<unsigned char>(c)) == 0) continue;
        char* end = nullptr;
        const double v = std::strtod(text.c_str() + i, &end);
        if(end == text.c_str() + i) continue;
        out = v;
        return true;
    }
    return false;
}

struct Monitor::Impl {
    std::string root;
    std::vector<CoreTimes> last_cores;
    ProcessSampler sampler;
    std::uint64_t last_rx = 0, last_tx = 0;
    bool have_net = false;
    std::chrono::steady_clock::time_point last_sample = std::chrono::steady_clock::now();
    std::unique_ptr<Nvml> nvml;

    explicit Impl(const std::string& r) : root(r == "/" ? std::string() : r), sampler(root) {}
};

Monitor::Monitor(const MonitorConfig& config) : _config(config), _impl(std::make_unique<Impl>(config.root)) {
    info = query_static(_impl->root);
    read_core_times(_impl->root, _impl->last_cores, info.logical_cores);
    _impl->have_net = read_net_bytes(_impl->root, _impl->last_rx, _impl->last_tx);
    if(_config.gpus) _impl->nvml = std::make_unique<Nvml>();
    disks = read_disks(_impl->root);
    sample(false);
}

Monitor::~Monitor() = default;

void Monitor::update(double dt, float fps) {
    _fps = fps;
    _accum += dt;
    if(_accum < _config.sample_period) return;
    _accum = 0.0;
    sample(false);
}

void Monitor::sample_now() { sample(true); }

void Monitor::sample(bool walk_processes) {
    Impl& im = *_impl;
    const std::string& root = im.root;
    const std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(t - im.last_sample).count();
    im.last_sample = t;

    std::vector<CoreTimes> cores;
    if(read_core_times(root, cores, info.logical_cores) && cores.size() == im.last_cores.size()) {
        now.core_usage.resize(cores.size());
        for(std::size_t i = 0; i < cores.size(); ++i) {
            const double total = static_cast<double>(cores[i].total - im.last_cores[i].total);
            const double idle = static_cast<double>(cores[i].idle - im.last_cores[i].idle);
            now.core_usage[i] = total > 0.0 ? static_cast<float>(std::clamp(100.0 * (1.0 - idle / total), 0.0, 100.0)) : 0.0f;
        }
    }
    im.last_cores = cores;
    read_core_mhz(root, now, info.logical_cores);
    read_system(root, now);

    std::uint64_t rx = 0, tx = 0;
    if(im.have_net && read_net_bytes(root, rx, tx) && elapsed > 0.05) {
        now.net_rx = rx >= im.last_rx ? static_cast<double>(rx - im.last_rx) / elapsed : 0.0;
        now.net_tx = tx >= im.last_tx ? static_cast<double>(tx - im.last_tx) / elapsed : 0.0;
        im.last_rx = rx;
        im.last_tx = tx;
    }
    if(_config.sensors) read_hwmon(root, now);
    if(im.nvml) im.nvml->read(now.gpus);

    // process walk and disks are heavier: once a second / every 10 s
    if(walk_processes || _tick % 2 == 0) processes = im.sampler.sample(info.logical_cores, now);
    if(_tick % 20 == 19) disks = read_disks(root);
    ++_tick;

    const std::size_t max = static_cast<std::size_t>(std::max(_config.history_length, 1));
    if(core_history.size() != now.core_usage.size()) core_history.assign(now.core_usage.size(), {});
    for(std::size_t i = 0; i < now.core_usage.size(); ++i) push_history(core_history[i], now.core_usage[i], max);
    push_history(cpu_history, now.cpu(), max);
    push_history(mem_history, static_cast<float>(now.mem_used / 1073741824.0), max);
    push_history(swap_history, static_cast<float>(now.swap_used / 1073741824.0), max);
    push_history(rx_history, static_cast<float>(now.net_rx / 1024.0), max);
    push_history(tx_history, static_cast<float>(now.net_tx / 1024.0), max);
    push_history(thread_history, static_cast<float>(now.threads), max);
    push_history(fps_history, _fps, max);
    // a sensor set that changes (hotplug, driver reload) restarts its history
    if(temp_history.size() != now.temps.size()) temp_history.assign(now.temps.size(), {});
    for(std::size_t i = 0; i < now.temps.size(); ++i) push_history(temp_history[i], now.temps[i].value, max);
    if(gpu_util_history.size() != now.gpus.size()) {
        gpu_util_history.assign(now.gpus.size(), {});
        gpu_temp_history.assign(now.gpus.size(), {});
    }
    for(std::size_t i = 0; i < now.gpus.size(); ++i) {
        push_history(gpu_util_history[i], std::max(now.gpus[i].util, 0.0f), max);
        push_history(gpu_temp_history[i], std::max(now.gpus[i].temp, 0.0f), max);
    }

    times.resize(cpu_history.size());
    for(std::size_t i = 0; i < times.size(); ++i)
        times[i] = static_cast<float>((static_cast<double>(i) - static_cast<double>(times.size() - 1)) * _config.sample_period);
}

struct Command::State {
    std::mutex mutex;
    std::condition_variable wake;
    bool stop = false;
    bool ready = false;
    std::string text;
    std::thread worker;
};

Command::Command(std::string cmd, double period) : _cmd(std::move(cmd)), _state(std::make_shared<State>()) {
    const std::chrono::milliseconds interval(static_cast<long long>(std::max(period, 0.05) * 1000.0));
    _state->worker = std::thread([state = _state, cmd = _cmd, interval]() {
        for(;;) {
            std::string out;
            if(FILE* pipe = popen(cmd.c_str(), "r")) {
                char buf[4096];
                for(std::size_t n; (n = std::fread(buf, 1, sizeof(buf), pipe)) > 0;) out.append(buf, n);
                pclose(pipe);
            }
            while(!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
            std::unique_lock<std::mutex> lock(state->mutex);
            state->text = out;
            state->ready = true;
            if(state->wake.wait_for(lock, interval, [&state]() { return state->stop; })) return;
        }
    });
}

Command::~Command() {
    {
        const std::lock_guard<std::mutex> lock(_state->mutex);
        _state->stop = true;
    }
    _state->wake.notify_all();
    if(_state->worker.joinable()) _state->worker.join();
}

std::string Command::text() const {
    const std::lock_guard<std::mutex> lock(_state->mutex);
    return _state->text;
}

bool Command::value(double& out) const { return parse_number(text(), out); }

bool Command::ready() const {
    const std::lock_guard<std::mutex> lock(_state->mutex);
    return _state->ready;
}

} // namespace smg::sys
