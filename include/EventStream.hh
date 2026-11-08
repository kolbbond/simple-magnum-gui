// JSONL over an AF_UNIX stream socket: a reader thread queues incoming messages, send() writes from any thread.
// Domain-agnostic; reconnects with backoff so the peer may start late or restart. POSIX only.
// Threading: set on_connect/set_max_line before start(); start()/stop() from one owner thread;
// poll() once per frame (the queue is unbounded if it is never called). send() is safe from any
// thread but gives up after ~1s and kills the connection (a partial write would corrupt framing).
#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace smg {

class EventStream {
public:
    enum class State { Connecting, Connected, Down };

    explicit EventStream(std::string socket_path);
    ~EventStream();
    EventStream(const EventStream&) = delete;
    EventStream& operator=(const EventStream&) = delete;

    void start(); // idempotent; reconnects with backoff 0.25s -> 4s
    void stop(); // idempotent; joins the reader (from on_connect it only flags; the reader then exits)

    static constexpr std::size_t default_max_line = 16 * 1024 * 1024;
    void set_max_line(std::size_t n) { max_line_ = n; } // longer lines are dropped; before start()

    bool send(const nlohmann::json& msg); // false if not connected, the write fails or stalls >1s
    std::vector<nlohmann::json> poll(); // drains the queue; call once per frame

    State state() const { return state_.load(); }
    std::string status() const; // e.g. "connect: No such file or directory"
    std::size_t dropped() const { return dropped_.load(); }

    // runs on the reader thread after each (re)connect; may call send()
    std::function<void()> on_connect;

private:
    void run();
    void set_state(State s, const std::string& status);
    bool wait_backoff(int ms); // false if stopped meanwhile

    struct LineBuf {
        std::string partial;
        bool skipping = false; // inside an over-long line, discarding to '\n'
    };
    void feed(const char* data, std::size_t n, LineBuf& lb);
    void emit(const std::string& line);

    std::string path_;
    std::size_t max_line_ = default_max_line;
    std::thread thread_;
    std::atomic<bool> stop_{ false };
    std::atomic<State> state_{ State::Down };
    std::atomic<std::size_t> dropped_{ 0 };

    mutable std::mutex mu_; // queue_, status_, fd_ lifetime, stop wakeups
    std::condition_variable cv_;
    std::vector<nlohmann::json> queue_;
    std::string status_;

    std::mutex write_mu_;
    int fd_ = -1; // guarded by write_mu_
};

} // namespace smg
