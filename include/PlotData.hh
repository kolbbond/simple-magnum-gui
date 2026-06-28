// headless plotting helpers: analysis stats + rolling buffer (no GL/ImGui)
#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

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
