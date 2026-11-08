// JSONL over an AF_UNIX stream socket: a reader thread queues incoming messages, send() writes from any thread.
// Domain-agnostic; reconnects with backoff so the peer may start late or restart. POSIX only.
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
    void stop(); // idempotent; joins the reader

    bool send(const nlohmann::json& msg); // false if not connected or the write fails
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
    void feed(const char* data, std::size_t n, std::string& partial);

    std::string path_;
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
