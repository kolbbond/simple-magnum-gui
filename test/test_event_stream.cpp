// EventStream against an in-test AF_UNIX server; every wait is bounded.
#include "EventStream.hh"
#include "test_util.hh"

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace std::chrono_literals;
using nlohmann::json;
using smg::EventStream;

namespace {

std::string unique_path(const char* tag) {
    static int n = 0;
    return "/tmp/smg_es_" + std::to_string(::getpid()) + "_" + tag + "_" + std::to_string(n++) + ".sock";
}

bool wait_for(const std::function<bool()>& pred, std::chrono::milliseconds limit = 5000ms) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while(std::chrono::steady_clock::now() < deadline) {
        if(pred()) return true;
        std::this_thread::sleep_for(5ms);
    }
    return pred();
}

struct Listener {
    std::string path;
    int fd = -1;

    explicit Listener(std::string p) : path(std::move(p)) {
        ::unlink(path.c_str());
        fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
        if(::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::listen(fd, 4) != 0) {
            ::close(fd);
            fd = -1;
        }
    }
    ~Listener() { close_all(); }
    void close_all() {
        if(fd >= 0) ::close(fd);
        fd = -1;
        ::unlink(path.c_str());
    }
    // -1 on timeout
    int accept_client(int ms = 5000) {
        pollfd p{ fd, POLLIN, 0 };
        if(::poll(&p, 1, ms) <= 0) return -1;
        return ::accept(fd, nullptr, nullptr);
    }
};

void write_all(int fd, const std::string& s) {
    size_t off = 0;
    while(off < s.size()) {
        ssize_t n = ::send(fd, s.data() + off, s.size() - off, MSG_NOSIGNAL);
        if(n <= 0) return;
        off += size_t(n);
    }
}

// reads until '\n' or timeout
std::string read_line(int fd, int ms = 5000) {
    std::string out;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while(std::chrono::steady_clock::now() < deadline) {
        pollfd p{ fd, POLLIN, 0 };
        if(::poll(&p, 1, 50) <= 0) continue;
        char c;
        if(::recv(fd, &c, 1, 0) <= 0) break;
        if(c == '\n') return out;
        out.push_back(c);
    }
    return out;
}

// collect at least n messages (bounded)
std::vector<json> collect(EventStream& es, size_t n) {
    std::vector<json> got;
    wait_for([&] {
        for(auto& m : es.poll()) got.push_back(std::move(m));
        return got.size() >= n;
    });
    return got;
}

void connects_and_receives_lines() {
    Listener l(unique_path("recv"));
    CHECK(l.fd >= 0);
    EventStream es(l.path);
    es.start();
    int c = l.accept_client();
    CHECK(c >= 0);
    write_all(c, "{\"a\":1}\n{\"b\":\"two\"}\n");
    auto got = collect(es, 2);
    CHECK(got.size() == 2);
    if(got.size() == 2) {
        CHECK(got[0] == json({ { "a", 1 } }));
        CHECK(got[1] == json({ { "b", "two" } }));
    }
    CHECK(es.state() == EventStream::State::Connected);
    es.stop();
    ::close(c);
}

void send_reaches_server() {
    Listener l(unique_path("send"));
    EventStream es(l.path);
    CHECK(!es.send(json::object())); // not started
    es.start();
    int c = l.accept_client();
    CHECK(c >= 0);
    CHECK(wait_for([&] { return es.state() == EventStream::State::Connected; }));
    json msg = { { "op", "ping" }, { "n", 7 } };
    CHECK(es.send(msg));
    std::string line = read_line(c);
    CHECK(json::parse(line, nullptr, false) == msg);
    es.stop();
    ::close(c);
}

void partial_lines_reassembled() {
    Listener l(unique_path("part"));
    EventStream es(l.path);
    es.start();
    int c = l.accept_client();
    CHECK(c >= 0);
    write_all(c, "{\"k\":");
    std::this_thread::sleep_for(30ms);
    write_all(c, "\"val");
    std::this_thread::sleep_for(30ms);
    write_all(c, "ue\"}\n");
    auto got = collect(es, 1);
    CHECK(got.size() == 1);
    if(!got.empty()) CHECK(got[0] == json({ { "k", "value" } }));
    std::this_thread::sleep_for(100ms);
    CHECK(es.poll().empty());
    es.stop();
    ::close(c);
}

void bad_json_dropped_and_counted() {
    Listener l(unique_path("bad"));
    EventStream es(l.path);
    es.start();
    int c = l.accept_client();
    CHECK(c >= 0);
    write_all(c, "not json\n{\"ok\":true}\n");
    auto got = collect(es, 1);
    CHECK(got.size() == 1);
    if(!got.empty()) CHECK(got[0] == json({ { "ok", true } }));
    CHECK(es.dropped() == 1);
    es.stop();
    ::close(c);
}

void test_reconnect_after_server_restart() {
    const std::string path = unique_path("re");
    std::atomic<int> connects{ 0 };
    EventStream es(path);
    es.on_connect = [&] { ++connects; };
    CHECK(!es.send(json::object())); // nothing connected yet
    es.start();
    {
        Listener l(path);
        int c = l.accept_client();
        CHECK(c >= 0);
        CHECK(wait_for([&] { return es.state() == EventStream::State::Connected; }));
        CHECK(connects == 1);
        ::close(c);
        l.close_all();
        CHECK(wait_for([&] { return es.state() != EventStream::State::Connected; }));
        // Down or already retrying: either way not connected
        CHECK(!es.send(json::object()));
    }
    Listener l2(path);
    CHECK(l2.fd >= 0);
    int c2 = l2.accept_client();
    CHECK(c2 >= 0);
    CHECK(wait_for([&] { return es.state() == EventStream::State::Connected; }));
    CHECK(wait_for([&] { return connects == 2; }));
    std::this_thread::sleep_for(100ms);
    CHECK(connects == 2);
    es.stop();
    ::close(c2);
}

} // namespace

int main() {
    connects_and_receives_lines();
    send_reaches_server();
    partial_lines_reassembled();
    bad_json_dropped_and_counted();
    test_reconnect_after_server_restart();
    TEST_RETURN();
}
