// keyframe tracks + timeline clock (headless animation core)
#pragma once

#include <algorithm>
#include <cmath>
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

class Timeline {
public:
    explicit Timeline(float duration = 0.0f) : _duration(duration < 0.0f ? 0.0f : duration) {}

    void advance(float dt) {
        if(!_playing) return;
        _time += dt * _speed;
        if(_loop) {
            if(_duration > 0.0f) {
                _time = std::fmod(_time, _duration);
                if(_time < 0.0f) _time += _duration;
            } else {
                _time = 0.0f;
            }
        } else if(_time >= _duration) {
            _time = _duration;
            _playing = false;
        } else if(_time < 0.0f) {
            _time = 0.0f;
        }
    }

    void play() { _playing = true; }
    void pause() { _playing = false; }
    void stop() {
        _playing = false;
        _time = 0.0f;
    }
    void seek(float t) { _time = std::clamp(t, 0.0f, _duration); }
    void set_speed(float s) { _speed = s; }
    void set_loop(bool on) { _loop = on; }
    void set_duration(float d) {
        _duration = d < 0.0f ? 0.0f : d;
        if(_time > _duration) _time = _duration;
    }

    [[nodiscard]] float time() const { return _time; }
    [[nodiscard]] float duration() const { return _duration; }
    [[nodiscard]] bool playing() const { return _playing; }
    [[nodiscard]] bool finished() const { return !_loop && _duration > 0.0f && _time >= _duration; }

private:
    float _time{ 0.0f };
    float _duration{ 0.0f };
    float _speed{ 1.0f };
    bool _playing{ false };
    bool _loop{ false };
};

} // namespace smg
