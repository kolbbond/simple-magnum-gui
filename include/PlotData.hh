// headless plotting helpers: analysis stats + rolling buffer (no GL/ImGui)
#pragma once

#include <cmath>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace smg {

// fixed-capacity ring buffer for live/streaming plot data
template <class T>
class RollingBuffer {
public:
    explicit RollingBuffer(std::size_t capacity) : _buf(capacity == 0 ? 1 : capacity) {}

    void push(T v) {
        _buf[_next] = v;
        _next = (_next + 1) % _buf.size();
        if(_count < _buf.size()) ++_count;
    }

    void clear() {
        _count = 0;
        _next = 0;
    }

    [[nodiscard]] std::size_t size() const { return _count; }
    [[nodiscard]] std::size_t capacity() const { return _buf.size(); }

    // oldest-to-newest snapshot, ready to hand to Plot::line
    [[nodiscard]] std::vector<T> ordered() const {
        std::vector<T> out;
        out.reserve(_count);
        const std::size_t start = _count < _buf.size() ? 0 : _next;
        for(std::size_t i = 0; i < _count; ++i) out.push_back(_buf[(start + i) % _buf.size()]);
        return out;
    }

private:
    std::vector<T> _buf;
    std::size_t _count{ 0 };
    std::size_t _next{ 0 };
};

} // namespace smg

namespace smg::analysis {

template <class T>
struct Stats {
    T min{ 0 };
    T max{ 0 };
    double mean{ 0.0 };
    double stddev{ 0.0 };
};

template <class T>
[[nodiscard]] Stats<T> stats(const std::vector<T>& v) {
    Stats<T> s;
    if(v.empty()) return s;
    s.min = v[0];
    s.max = v[0];
    double sum = 0.0;
    for(const T& x : v) {
        if(x < s.min) s.min = x;
        if(x > s.max) s.max = x;
        sum += double(x);
    }
    s.mean = sum / double(v.size());
    double acc = 0.0;
    for(const T& x : v) {
        const double d = double(x) - s.mean;
        acc += d * d;
    }
    s.stddev = std::sqrt(acc / double(v.size())); // population std
    return s;
}

template <class T>
[[nodiscard]] std::vector<T> normalize(const std::vector<T>& v) {
    // integral T would truncate every non-max element to 0; force floating-point
    static_assert(std::is_floating_point<T>::value, "smg::analysis::normalize requires a floating-point element type");
    std::vector<T> out(v.size());
    if(v.empty()) return out;
    const Stats<T> s = stats(v);
    const double span = double(s.max) - double(s.min);
    for(std::size_t i = 0; i < v.size(); ++i) out[i] = span > 0.0 ? T((double(v[i]) - double(s.min)) / span) : T(0);
    return out;
}

template <class T>
[[nodiscard]] std::vector<T> decimate(const std::vector<T>& v, std::size_t max_points) {
    const std::size_t cap = max_points == 0 ? 1 : max_points;
    std::vector<T> out;
    if(v.size() <= cap) {
        out = v;
        return out;
    }
    const std::size_t stride = (v.size() + cap - 1) / cap; // ceil -> at most cap samples
    for(std::size_t i = 0; i < v.size(); i += stride) out.push_back(v[i]);
    if(out.back() != v.back()) out.back() = v.back(); // pin the endpoint without exceeding cap
    return out;
}

} // namespace smg::analysis
