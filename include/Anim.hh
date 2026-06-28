// keyframe tracks + timeline clock (headless animation core)
#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "Easing.hh"

namespace smg {

// generic linear interpolation; works for float and Magnum vector/color types
template <class T>
[[nodiscard]] T anim_lerp(const T& a, const T& b, float u) {
    return a + (b - a) * u;
}

template <class T>
struct Keyframe {
    float time{ 0.0f };
    T value{};
    Easing ease{ Easing::Linear }; // applied across the segment ending at this key
};

template <class T>
class Track {
public:
    void add(float time, const T& value, Easing ease = Easing::Linear) {
        const Keyframe<T> k{ time, value, ease };
        typename std::vector<Keyframe<T>>::iterator pos =
            std::lower_bound(_keys.begin(), _keys.end(), time, [](const Keyframe<T>& kf, float b) { return kf.time < b; });
        _keys.insert(pos, k);
    }

    [[nodiscard]] bool empty() const { return _keys.empty(); }
    [[nodiscard]] float duration() const { return _keys.empty() ? 0.0f : _keys.back().time; }

    [[nodiscard]] T sample(float t) const {
        if(_keys.empty()) return T{};
        if(t <= _keys.front().time) return _keys.front().value;
        if(t >= _keys.back().time) return _keys.back().value;
        for(std::size_t i = 1; i < _keys.size(); ++i) {
            if(t <= _keys[i].time) {
                const Keyframe<T>& a = _keys[i - 1];
                const Keyframe<T>& b = _keys[i];
                const float span = b.time - a.time;
                if(span <= 0.0f) return b.value; // coincident keys -> step
                const float u = apply(b.ease, (t - a.time) / span);
                return anim_lerp(a.value, b.value, u);
            }
        }
        return _keys.back().value;
    }

private:
    std::vector<Keyframe<T>> _keys;
};

} // namespace smg
