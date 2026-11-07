// unit: smg::sys readers against a fixture /proc + /sys tree, plus the pure helpers
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "Sys.hh"
#include "test_util.hh"

namespace {

void helpers() {
    double v = 0.0;
    CHECK(smg::sys::parse_number("42", v) && v == 42.0);
    CHECK(smg::sys::parse_number("temp: -12.5 C", v) && v == -12.5);
    CHECK(smg::sys::parse_number("load 1e3", v) && v == 1000.0);
    CHECK(!smg::sys::parse_number("no digits - here", v));

    std::vector<float> h;
    for(int i = 0; i < 10; ++i) smg::sys::push_history(h, static_cast<float>(i), 4);
    CHECK(h.size() == 4);
    CHECK(h.front() == 6.0f && h.back() == 9.0f);

    CHECK(smg::sys::format_bytes(512.0) == "512 B");
    CHECK(smg::sys::format_bytes(1536.0) == "1.5 KB");
    CHECK(smg::sys::format_uptime(90061.0) == "1d 01:01:01");
}

void fixture_tree() {
    smg::sys::MonitorConfig c;
    c.root = std::string(SMG_TEST_ASSET_DIR) + "/fixtures/sys";
    c.gpus = false; // NVML reads the real driver, not the root
    c.history_length = 3;
    smg::sys::Monitor m(c);

    CHECK(m.info.cpu_name == "Fixture CPU @ 1.00GHz");
    CHECK(m.info.logical_cores == 2);
    CHECK(m.info.physical_cores == 2);
    CHECK(m.info.os.rfind("Fixture OS", 0) == 0);

    const double kb = 1024.0;
    CHECK(m.now.mem_total == 8000000.0 * kb);
    CHECK(m.now.mem_used == 2000000.0 * kb); // total - available
    CHECK(m.now.swap_used == 500000.0 * kb);
    CHECK(smgtest::approx(static_cast<float>(m.now.uptime), 12345.67f, 1e-2f));
    CHECK(m.now.core_mhz.size() == 2);
    CHECK(m.now.battery == -1);

    // processes: comm with parens and spaces, threads and rss parsed
    m.sample_now();
    CHECK(m.processes.size() == 1);
    if(!m.processes.empty()) {
        CHECK(m.processes[0].pid == 4242);
        CHECK(m.processes[0].name == "fixture (proc)");
        CHECK(m.processes[0].threads == 3);
        CHECK(m.processes[0].memory > 0.0);
    }

    // hwmon: hwmonN order, temp2 before temp10, labels, -128 C dropped, fans kept
    CHECK(m.now.temps.size() == 4);
    if(m.now.temps.size() == 4) {
        CHECK(m.now.temps[0].key() == "coretemp/Package id 0");
        CHECK(m.now.temps[0].value == 45.0f);
        CHECK(m.now.temps[1].key() == "coretemp/temp2");
        CHECK(m.now.temps[2].key() == "coretemp/temp10");
        CHECK(m.now.temps[3].key() == "it8620/temp2");
    }
    CHECK(m.now.fans.size() == 2);
    const smg::sys::Sensor* pkg = m.now.temp("Package");
    CHECK(pkg && pkg->value == 45.0f);
    const smg::sys::Sensor* fan = m.now.fan("it8620/fan1");
    CHECK(fan && fan->value == 1200.0f);
    CHECK(m.now.temp("nope") == nullptr);

    // histories stay parallel and capped
    for(int i = 0; i < 5; ++i) m.sample_now();
    CHECK(m.cpu_history.size() == 3);
    CHECK(m.times.size() == 3);
    CHECK(m.temp_history.size() == m.now.temps.size());
    CHECK(m.times.back() == 0.0f);
}

void command() {
    smg::sys::Command cmd("echo 'load: 42.5'", 0.1);
    for(int i = 0; i < 100 && !cmd.ready(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(cmd.ready());
    CHECK(cmd.text() == "load: 42.5");
    double v = 0.0;
    CHECK(cmd.value(v) && v == 42.5);
}

} // namespace

int main() {
    helpers();
#if !defined(_WIN32)
    fixture_tree();
    command();
#endif
    TEST_RETURN();
}
