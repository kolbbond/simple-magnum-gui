#include "EventStream.hh"

#include <cerrno>
#include <chrono>
#include <algorithm>
#include <cstring>

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
    }
    cv_.notify_all();
    if(thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> wl(write_mu_);
    if(fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    state_ = State::Down;
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
    std::size_t off = 0;
    while(off < line.size()) {
        // MSG_NOSIGNAL: a dead peer must not raise SIGPIPE
        ssize_t n = ::send(fd_, line.data() + off, line.size() - off, MSG_NOSIGNAL);
        if(n < 0 && errno == EINTR) continue;
        if(n <= 0) return false; // the reader notices the dead socket and reconnects
        off += std::size_t(n);
    }
    return true;
}

bool EventStream::wait_backoff(int ms) {
    std::unique_lock<std::mutex> lk(mu_);
    return !cv_.wait_for(lk, std::chrono::milliseconds(ms), [this] { return stop_.load(); });
}

void EventStream::feed(const char* data, std::size_t n, std::string& partial) {
    partial.append(data, n);
    std::size_t start = 0, nl;
    while((nl = partial.find('\n', start)) != std::string::npos) {
        if(nl > start) {
            auto j = nlohmann::json::parse(partial.begin() + std::ptrdiff_t(start), partial.begin() + std::ptrdiff_t(nl), nullptr, false);
            if(j.is_discarded()) {
                ++dropped_;
            } else {
                std::lock_guard<std::mutex> lk(mu_);
                queue_.push_back(std::move(j));
            }
        }
        start = nl + 1;
    }
    partial.erase(0, start);
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
        if(on_connect) on_connect();

        std::string partial;
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
            feed(buf, std::size_t(n), partial);
        }

        {
            std::lock_guard<std::mutex> wl(write_mu_);
            if(fd_ >= 0) ::close(fd_);
            fd_ = -1;
        }
        if(stop_) break;
        set_state(State::Down, why);
        if(!wait_backoff(backoff)) break;
        backoff = std::min(backoff * 2, 4000);
    }
}

} // namespace smg
