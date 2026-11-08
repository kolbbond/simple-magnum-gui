#include "EventStream.hh"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <utility>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace smg {

EventStream::EventStream(std::string socket_path) : path_(std::move(socket_path)), status_("not started") {}

EventStream::~EventStream() { stop(); }

void EventStream::start() {
    std::lock_guard<std::mutex> lk(mu_);
    if(thread_.joinable()) return;
    stop_ = false;
    state_ = State::Connecting;
    status_ = "connecting";
    thread_ = std::thread([this] { run(); });
}

void EventStream::stop() {
    {
        std::lock_guard<std::mutex> lk(mu_);
        stop_ = true;
        status_ = "stopped";
    }
    cv_.notify_all();
    if(thread_.joinable()) {
        if(std::this_thread::get_id() == thread_.get_id()) return; // on_connect; reader exits by itself
        thread_.join();
    }
    {
        std::lock_guard<std::mutex> wl(write_mu_);
        if(fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
    std::lock_guard<std::mutex> lk(mu_);
    state_ = State::Down;
    status_ = "stopped";
}

std::string EventStream::status() const {
    std::lock_guard<std::mutex> lk(mu_);
    return status_;
}

void EventStream::set_state(State s, const std::string& status) {
    std::lock_guard<std::mutex> lk(mu_);
    state_ = s;
    status_ = status;
}

std::vector<nlohmann::json> EventStream::poll() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<nlohmann::json> out;
    out.swap(queue_);
    return out;
}

bool EventStream::send(const nlohmann::json& msg) {
    std::string line = msg.dump();
    line.push_back('\n');
    std::lock_guard<std::mutex> wl(write_mu_);
    if(fd_ < 0 || state_ != State::Connected) return false;
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + std::chrono::seconds(1);
    std::size_t off = 0;
    bool ok = true;
    while(off < line.size()) {
        // MSG_NOSIGNAL: a dead peer must not raise SIGPIPE; DONTWAIT: never block on a full buffer
        ssize_t n = ::send(fd_, line.data() + off, line.size() - off, MSG_NOSIGNAL | MSG_DONTWAIT);
        if(n > 0) {
            off += std::size_t(n);
            continue;
        }
        if(n < 0 && errno == EINTR) continue;
        if(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now()).count();
            if(left <= 0) {
                ok = false;
                break;
            }
            pollfd p{ fd_, POLLOUT, 0 };
            int r = ::poll(&p, 1, int(left));
            if(r == 0 || (r < 0 && errno != EINTR)) {
                ok = false;
                break;
            }
            continue;
        }
        ok = false;
        break;
    }
    // a failed or partial write corrupts framing: kill the stream; the reader sees EOF, closes and reconnects
    if(!ok) ::shutdown(fd_, SHUT_RDWR);
    return ok;
}

bool EventStream::wait_backoff(int ms) {
    std::unique_lock<std::mutex> lk(mu_);
    return !cv_.wait_for(lk, std::chrono::milliseconds(ms), [this] { return stop_.load(); });
}

void EventStream::emit(const std::string& line) {
    if(line.empty()) return;
    auto j = nlohmann::json::parse(line, nullptr, false);
    if(j.is_discarded()) {
        ++dropped_;
        return;
    }
    std::lock_guard<std::mutex> lk(mu_);
    queue_.push_back(std::move(j));
}

// single pass over the new bytes: linear, and an over-long line is skipped to its '\n'
void EventStream::feed(const char* data, std::size_t n, LineBuf& lb) {
    std::size_t i = 0;
    while(i < n) {
        const char* nl = static_cast<const char*>(std::memchr(data + i, '\n', n - i));
        const std::size_t chunk = nl ? std::size_t(nl - (data + i)) : n - i;
        const std::size_t adv = chunk + (nl ? 1 : 0);
        if(lb.skipping) {
            if(nl) lb.skipping = false;
        } else if(lb.partial.size() + chunk > max_line_) {
            ++dropped_;
            lb.partial.clear();
            lb.skipping = !nl;
        } else {
            lb.partial.append(data + i, chunk);
            if(nl) {
                emit(lb.partial);
                lb.partial.clear();
            }
        }
        i += adv;
    }
}

void EventStream::run() {
    int backoff = 250;
    while(!stop_) {
        set_state(State::Connecting, "connecting");

        int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        std::string err;
        if(fd < 0) {
            err = std::string("socket: ") + std::strerror(errno);
        } else {
            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            if(path_.size() >= sizeof(addr.sun_path)) {
                err = "connect: path too long";
            } else {
                std::memcpy(addr.sun_path, path_.c_str(), path_.size() + 1);
                if(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
                    err = std::string("connect: ") + std::strerror(errno);
            }
            if(!err.empty()) {
                ::close(fd);
                fd = -1;
            }
        }

        if(fd < 0) {
            set_state(State::Down, err);
            if(!wait_backoff(backoff)) break;
            backoff = std::min(backoff * 2, 4000);
            continue;
        }

        {
            std::lock_guard<std::mutex> wl(write_mu_);
            fd_ = fd;
        }
        backoff = 250;
        set_state(State::Connected, "connected");
        if(on_connect) {
            try {
                on_connect();
            } catch(...) {
                std::lock_guard<std::mutex> lk(mu_);
                status_ = "on_connect threw";
            }
        }

        LineBuf lb;
        std::string why = "connection closed";
        char buf[4096];
        while(!stop_) {
            pollfd p{ fd, POLLIN, 0 };
            int r = ::poll(&p, 1, 100); // short timeout keeps stop() prompt
            if(r < 0) {
                if(errno == EINTR) continue;
                why = std::string("poll: ") + std::strerror(errno);
                break;
            }
            if(r == 0) continue;
            ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
            if(n < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            if(n < 0) why = std::string("recv: ") + std::strerror(errno);
            if(n <= 0) break;
            feed(buf, std::size_t(n), lb);
        }

        {
            std::lock_guard<std::mutex> wl(write_mu_);
            if(fd_ >= 0) ::close(fd_);
            fd_ = -1;
        }
        if(stop_) break;
        if(!lb.partial.empty() && !lb.skipping) ++dropped_; // unterminated tail
        set_state(State::Down, why);
        if(!wait_backoff(backoff)) break;
        backoff = std::min(backoff * 2, 4000);
    }
    state_ = State::Down; // self-stop from on_connect leaves no one else to say so
}

} // namespace smg
