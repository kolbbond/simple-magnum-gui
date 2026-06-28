# Animation Core + Telestration Overlay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a headless animation core (easing, `Track<T>`, `Timeline`) plus `GuiBase::dt()` and an animatable 2D annotation overlay for telestration.

**Architecture:** Animation core is header-only templates/inline (no GL): easing functions, a keyframe `Track<T>` using a generic `anim_lerp`, and a seekable `Timeline`. `GuiBase` gains a per-frame `dt()`. The annotation overlay stores marks in normalized [0,1] image-space; pure geometry/progress helpers are headless, and `AnnotationLayer::draw` renders over an app-supplied texture via the ImGui draw list at the timeline's current time (caller advances the timeline).

**Tech Stack:** C++17, Magnum (Math vectors/colors, GL::Texture2D), ImGui (draw list + ImGuiIntegration::image), `GuiBase`, CMake/Ninja.

## Global Constraints

- C++17; out-of-source build. **Use the `build/` dir** (Release, deps prebuilt); `build-native` is a broken cache.
- Single-core builds: `cmake --build build -j1`.
- Style: `.clang-format` + `.clang-tidy` are source of truth. `clang-format -i` every modified C++ file before committing.
- **No `auto`** except iterators/unspellable STL types. **No `using`** directives/declarations — spell out `Magnum::`, `smg::`, `std::`.
- Namespace `smg` (sub-namespace `smg::ease`). Classes PascalCase; methods/free fns snake_case.
- Laconic comments. **No `Co-Authored-By`.** Commit subjects topic-prefixed + laconic, e.g. `anim: add easing functions`.
- Tests: `unit` (headless) → `unit_test_list`; `gui` (needs display, self-terminating) → `gui_test_list`.
- Container input is `const std::vector<T>&` / value types — no Corrade ArrayView in the public API (matches the plotting API decision).
- **Locked design:** annotations in normalized [0,1] over the image rect; background is an app-supplied texture (no video decode); `Timeline` is seekable; `AnnotationLayer::draw` is pure-render (caller advances `timeline()`); rendering via ImGui draw list.

**Build / test commands:**

```bash
cmake -B build >/dev/null
cmake --build build -j1 --target <t>
ctest --test-dir build -R <name> --output-on-failure
ctest --test-dir build -L unit --output-on-failure
```

---

### Task 1: Easing functions

**Files:**
- Create: `include/Easing.hh`
- Test: `test/test_easing.cpp`
- Modify: `test/CMakeLists.txt` (`unit_test_list`)

**Interfaces:**
- Produces:
  - `enum class smg::Easing { Linear, QuadIn, QuadOut, QuadInOut, CubicIn, CubicOut, CubicInOut };`
  - `namespace smg::ease`: `float linear/quad_in/quad_out/quad_in_out/cubic_in/cubic_out/cubic_in_out(float t)`.
  - `float smg::apply(Easing e, float t);`

- [ ] **Step 1: Write the failing test**

`test/test_easing.cpp`:

```cpp
#include "Easing.hh"
#include "test_util.hh"

int main() {
    // endpoints exact for every curve
    CHECK(smgtest::approx(smg::ease::linear(0.0f), 0.0f));
    CHECK(smgtest::approx(smg::ease::linear(1.0f), 1.0f));
    CHECK(smgtest::approx(smg::ease::quad_in(0.0f), 0.0f));
    CHECK(smgtest::approx(smg::ease::quad_in(1.0f), 1.0f));
    CHECK(smgtest::approx(smg::ease::quad_out(1.0f), 1.0f));
    CHECK(smgtest::approx(smg::ease::cubic_in(1.0f), 1.0f));
    CHECK(smgtest::approx(smg::ease::cubic_out(0.0f), 0.0f));

    // known midpoints
    CHECK(smgtest::approx(smg::ease::linear(0.5f), 0.5f));
    CHECK(smgtest::approx(smg::ease::quad_in(0.5f), 0.25f));
    CHECK(smgtest::approx(smg::ease::quad_in_out(0.5f), 0.5f));
    CHECK(smgtest::approx(smg::ease::cubic_in_out(0.5f), 0.5f));

    // apply() dispatches to the matching function
    CHECK(smgtest::approx(smg::apply(smg::Easing::Linear, 0.3f), 0.3f));
    CHECK(smgtest::approx(smg::apply(smg::Easing::QuadIn, 0.5f), 0.25f));
    CHECK(smgtest::approx(smg::apply(smg::Easing::CubicOut, 1.0f), 1.0f));

    TEST_RETURN();
}
```

- [ ] **Step 2: Register and confirm failure**

Add `test_easing.cpp` to `unit_test_list` in `test/CMakeLists.txt`, then:

```bash
cmake -B build >/dev/null && cmake --build build -j1 --target test_easing 2>&1 | tail -5
```

Expected: FAIL — `Easing.hh` not found.

- [ ] **Step 3: Write `include/Easing.hh`**

```cpp
// easing curves: pure t in [0,1] -> eased [0,1], endpoints exact
#pragma once

namespace smg {

enum class Easing { Linear, QuadIn, QuadOut, QuadInOut, CubicIn, CubicOut, CubicInOut };

namespace ease {

[[nodiscard]] inline float linear(float t) { return t; }
[[nodiscard]] inline float quad_in(float t) { return t * t; }
[[nodiscard]] inline float quad_out(float t) { return t * (2.0f - t); }
[[nodiscard]] inline float quad_in_out(float t) {
    return t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * 0.5f;
}
[[nodiscard]] inline float cubic_in(float t) { return t * t * t; }
[[nodiscard]] inline float cubic_out(float t) {
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}
[[nodiscard]] inline float cubic_in_out(float t) {
    if(t < 0.5f) return 4.0f * t * t * t;
    const float u = -2.0f * t + 2.0f;
    return 1.0f - u * u * u * 0.5f;
}

} // namespace ease

[[nodiscard]] inline float apply(Easing e, float t) {
    switch(e) {
        case Easing::QuadIn: return ease::quad_in(t);
        case Easing::QuadOut: return ease::quad_out(t);
        case Easing::QuadInOut: return ease::quad_in_out(t);
        case Easing::CubicIn: return ease::cubic_in(t);
        case Easing::CubicOut: return ease::cubic_out(t);
        case Easing::CubicInOut: return ease::cubic_in_out(t);
        case Easing::Linear: break;
    }
    return ease::linear(t);
}

} // namespace smg
```

- [ ] **Step 4: Build and run**

```bash
cmake --build build -j1 --target test_easing && ctest --test-dir build -R test_easing 2>&1 | grep -E "tests passed|failed"
```

Expected: `100% tests passed`.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i include/Easing.hh test/test_easing.cpp
git add include/Easing.hh test/test_easing.cpp test/CMakeLists.txt
git commit -m "anim: add easing curves"
```

---

### Task 2: `anim_lerp` + `Track<T>`

**Files:**
- Create: `include/Anim.hh`
- Test: `test/test_anim_track.cpp`
- Modify: `test/CMakeLists.txt` (`unit_test_list`)

**Interfaces:**
- Consumes: `Easing` / `smg::apply` (Task 1).
- Produces:
  - `template<class T> T smg::anim_lerp(const T& a, const T& b, float u);` (generic: `a + (b - a) * u`).
  - `template<class T> struct smg::Keyframe { float time; T value; Easing ease; };`
  - `template<class T> class smg::Track` with `void add(float time, const T& value, Easing ease = Easing::Linear)`, `bool empty() const`, `float duration() const`, `T sample(float t) const`.

- [ ] **Step 1: Write the failing test**

`test/test_anim_track.cpp`:

```cpp
#include "Anim.hh"
#include "test_util.hh"

int main() {
    // empty track -> default value, zero duration
    smg::Track<float> empty;
    CHECK(empty.empty());
    CHECK(smgtest::approx(empty.duration(), 0.0f));
    CHECK(smgtest::approx(empty.sample(1.0f), 0.0f));

    smg::Track<float> t;
    t.add(0.0f, 0.0f);
    t.add(2.0f, 10.0f); // linear segment to t=2

    CHECK(!t.empty());
    CHECK(smgtest::approx(t.duration(), 2.0f));

    // clamp before first / after last
    CHECK(smgtest::approx(t.sample(-1.0f), 0.0f));
    CHECK(smgtest::approx(t.sample(5.0f), 10.0f));

    // linear interpolation midpoint
    CHECK(smgtest::approx(t.sample(1.0f), 5.0f));

    // out-of-order insertion stays sorted
    smg::Track<float> u;
    u.add(2.0f, 20.0f);
    u.add(0.0f, 0.0f);
    CHECK(smgtest::approx(u.sample(1.0f), 10.0f));

    // eased segment: cubic_in at u=0.5 -> 0.125 of the span
    smg::Track<float> e;
    e.add(0.0f, 0.0f);
    e.add(1.0f, 8.0f, smg::Easing::CubicIn);
    CHECK(smgtest::approx(e.sample(0.5f), 1.0f)); // 0.125 * 8

    // anim_lerp generic
    CHECK(smgtest::approx(smg::anim_lerp(2.0f, 4.0f, 0.25f), 2.5f));

    TEST_RETURN();
}
```

- [ ] **Step 2: Register and confirm failure**

Add `test_anim_track.cpp` to `unit_test_list`, then build → FAIL (`Anim.hh` not found).

- [ ] **Step 3: Write `include/Anim.hh`**

```cpp
// keyframe tracks + timeline clock (headless animation core)
#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "Easing.hh"

namespace smg {

// generic linear interpolation; works for float and Magnum vector/color types
template<class T> [[nodiscard]] T anim_lerp(const T& a, const T& b, float u) { return a + (b - a) * u; }

template<class T> struct Keyframe {
    float time{ 0.0f };
    T value{};
    Easing ease{ Easing::Linear }; // applied across the segment ending at this key
};

template<class T> class Track {
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
```

- [ ] **Step 4: Build and run**

```bash
cmake --build build -j1 --target test_anim_track && ctest --test-dir build -R test_anim_track 2>&1 | grep -E "tests passed|failed"
```

Expected: `100% tests passed`.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i include/Anim.hh test/test_anim_track.cpp
git add include/Anim.hh test/test_anim_track.cpp test/CMakeLists.txt
git commit -m "anim: add anim_lerp + keyframe Track"
```

---

### Task 3: `Timeline`

**Files:**
- Modify: `include/Anim.hh` (append)
- Test: `test/test_anim_timeline.cpp`
- Modify: `test/CMakeLists.txt` (`unit_test_list`)

**Interfaces:**
- Produces: `class smg::Timeline` with `explicit Timeline(float duration = 0.0f)`, `void advance(float dt)`, `play/pause/stop()`, `void seek(float)`, `void set_speed(float)`, `void set_loop(bool)`, `void set_duration(float)`, `float time() const`, `float duration() const`, `bool playing() const`, `bool finished() const`.

- [ ] **Step 1: Write the failing test**

`test/test_anim_timeline.cpp`:

```cpp
#include "Anim.hh"
#include "test_util.hh"

int main() {
    smg::Timeline tl{ 10.0f };
    CHECK(smgtest::approx(tl.time(), 0.0f));
    CHECK(!tl.playing());

    // paused: advance does nothing
    tl.advance(1.0f);
    CHECK(smgtest::approx(tl.time(), 0.0f));

    tl.play();
    tl.advance(3.0f);
    CHECK(smgtest::approx(tl.time(), 3.0f));
    CHECK(tl.playing());

    // speed scales advance
    tl.set_speed(2.0f);
    tl.advance(1.0f);
    CHECK(smgtest::approx(tl.time(), 5.0f));

    // non-loop clamps at duration and finishes
    tl.set_speed(1.0f);
    tl.advance(100.0f);
    CHECK(smgtest::approx(tl.time(), 10.0f));
    CHECK(tl.finished());
    CHECK(!tl.playing());

    // seek clamps
    tl.seek(-5.0f);
    CHECK(smgtest::approx(tl.time(), 0.0f));
    tl.seek(999.0f);
    CHECK(smgtest::approx(tl.time(), 10.0f));

    // loop wraps
    smg::Timeline lp{ 4.0f };
    lp.set_loop(true);
    lp.play();
    lp.advance(5.0f); // wraps to 1.0
    CHECK(smgtest::approx(lp.time(), 1.0f));
    CHECK(!lp.finished()); // loop never finishes

    // stop resets
    lp.stop();
    CHECK(smgtest::approx(lp.time(), 0.0f));
    CHECK(!lp.playing());

    TEST_RETURN();
}
```

- [ ] **Step 2: Register and confirm failure**

Add `test_anim_timeline.cpp` to `unit_test_list`, then build → FAIL (`Timeline` undeclared).

- [ ] **Step 3: Append `Timeline` to `include/Anim.hh`**

Add `#include <cmath>` to the includes, then inside `namespace smg` (after `Track`):

```cpp
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
```

- [ ] **Step 4: Build and run**

```bash
cmake --build build -j1 --target test_anim_timeline && ctest --test-dir build -R test_anim_timeline 2>&1 | grep -E "tests passed|failed"
```

Expected: `100% tests passed`.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i include/Anim.hh test/test_anim_timeline.cpp
git add include/Anim.hh test/test_anim_timeline.cpp test/CMakeLists.txt
git commit -m "anim: add seekable Timeline clock"
```

---

### Task 4: `GuiBase::dt()`

**Files:**
- Modify: `include/GuiBase.hh`, `src/GuiBase.cpp`
- Test: `test/test_guibase_dt.cpp` (gui)
- Modify: `test/CMakeLists.txt` (`gui_test_list`)

**Interfaces:**
- Produces: `float GuiBase::dt() const;` — seconds since the previous frame (0 on the first frame), clamped to ≤ 0.1 s.

- [ ] **Step 1: Write the failing gui test**

`test/test_guibase_dt.cpp`:

```cpp
// gui smoke test: dt() is finite, non-negative, and clamped after a few frames
#include "GuiBase.hh"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    smg::GuiBase gui({ argc, argv });
    for(int i = 0; i < 3; ++i) gui.mainLoopIteration();
    const float d = gui.dt();
    std::printf("dt = %f\n", d);
    if(!(d >= 0.0f && d <= 0.1f)) {
        std::printf("FAIL dt out of range\n");
        gui.exit();
        std::exit(1);
    }
    gui.exit();
    std::exit(0);
}
```

- [ ] **Step 2: Register and confirm failure**

Add `test_guibase_dt.cpp` to `gui_test_list`, then build → FAIL (`dt` not a member).

- [ ] **Step 3: Edit `include/GuiBase.hh`**

Add `#include <chrono>` near the top includes. Add a public accessor (next to other public methods):

```cpp
    [[nodiscard]] float dt() const { return _dt; }
```

Add private members (with the other private state):

```cpp
    float _dt{ 0.0f };
    std::chrono::steady_clock::time_point _last_frame{};
    bool _have_last_frame{ false };
```

- [ ] **Step 4: Edit `src/GuiBase.cpp` — compute dt at the top of `drawEvent`**

Find `void GuiBase::drawEvent()` and insert at its very start (before the framebuffer clear):

```cpp
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if(_have_last_frame) {
        const float elapsed = std::chrono::duration<float>(now - _last_frame).count();
        _dt = elapsed > 0.1f ? 0.1f : elapsed; // clamp spikes (stalls/breakpoints)
    } else {
        _dt = 0.0f;
        _have_last_frame = true;
    }
    _last_frame = now;
```

Add `#include <chrono>` to `src/GuiBase.cpp` if not already present transitively (the header now provides it, but include explicitly for clarity).

- [ ] **Step 5: Build and run**

```bash
cmake -B build >/dev/null && cmake --build build -j1 --target test_guibase_dt 2>&1 | grep -iE "error:|Built target test_guibase_dt"
ctest --test-dir build -R test_guibase_dt --output-on-failure 2>&1 | grep -E "tests passed|failed|dt ="
```

Expected: builds, `100% tests passed`. Headless without a display → build success is the gate; note it.

- [ ] **Step 6: Format and commit**

```bash
clang-format -i include/GuiBase.hh src/GuiBase.cpp test/test_guibase_dt.cpp
git add include/GuiBase.hh src/GuiBase.cpp test/test_guibase_dt.cpp test/CMakeLists.txt
git commit -m "anim: expose per-frame GuiBase::dt()"
```

---

### Task 5: Annotation value types + pure helpers

**Files:**
- Create: `include/Annotations.hh`
- Test: `test/test_annotation_math.cpp`
- Modify: `test/CMakeLists.txt` (`unit_test_list`)

**Interfaces:**
- Consumes: `Easing`/`apply` (Task 1), `Timeline` (Task 3).
- Produces:
  - `enum class smg::AnnotationKind { Stroke, Line, Arrow, Circle, Text };`
  - `struct smg::Annotation { AnnotationKind kind; std::vector<Magnum::Vector2> points; Magnum::Color4 color; float thickness; std::string text; float appear_time; float draw_on; Easing ease; bool fade_in; };`
  - `float smg::annotation_progress(const Annotation& a, float t);` — eased [0,1].
  - `Magnum::Vector2 smg::to_screen(const Magnum::Vector2& norm, const Magnum::Vector2& image_min, const Magnum::Vector2& image_size);`

- [ ] **Step 1: Write the failing test**

`test/test_annotation_math.cpp`:

```cpp
#include "Annotations.hh"
#include "test_util.hh"

int main() {
    // to_screen maps normalized -> pixels over the image rect
    const Magnum::Vector2 p = smg::to_screen(Magnum::Vector2{ 0.5f, 0.25f }, Magnum::Vector2{ 10.0f, 20.0f },
        Magnum::Vector2{ 100.0f, 200.0f });
    CHECK(smgtest::approx(p.x(), 60.0f));
    CHECK(smgtest::approx(p.y(), 70.0f));

    smg::Annotation a;
    a.appear_time = 1.0f;
    a.draw_on = 2.0f;
    a.ease = smg::Easing::Linear;

    CHECK(smgtest::approx(smg::annotation_progress(a, 0.0f), 0.0f)); // before appear
    CHECK(smgtest::approx(smg::annotation_progress(a, 1.0f), 0.0f)); // at appear
    CHECK(smgtest::approx(smg::annotation_progress(a, 2.0f), 0.5f)); // halfway
    CHECK(smgtest::approx(smg::annotation_progress(a, 3.0f), 1.0f)); // done
    CHECK(smgtest::approx(smg::annotation_progress(a, 9.0f), 1.0f)); // after

    // instant (draw_on == 0): 0 before appear, 1 at/after
    smg::Annotation inst;
    inst.appear_time = 1.0f;
    inst.draw_on = 0.0f;
    CHECK(smgtest::approx(smg::annotation_progress(inst, 0.5f), 0.0f));
    CHECK(smgtest::approx(smg::annotation_progress(inst, 1.0f), 1.0f));

    TEST_RETURN();
}
```

- [ ] **Step 2: Register and confirm failure**

Add `test_annotation_math.cpp` to `unit_test_list`, then build → FAIL (`Annotations.hh` not found).

- [ ] **Step 3: Write `include/Annotations.hh`** (types + pure helpers; `AnnotationLayer` added in Task 6)

```cpp
// telestration annotations: animatable 2D marks in normalized [0,1] image-space
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector2.h>

#include "Easing.hh"

namespace smg {

enum class AnnotationKind { Stroke, Line, Arrow, Circle, Text };

struct Annotation {
    AnnotationKind kind{ AnnotationKind::Stroke };
    std::vector<Magnum::Vector2> points; // normalized [0,1]
    Magnum::Color4 color{ 1.0f };
    float thickness{ 2.0f }; // screen px
    std::string text;
    float appear_time{ 0.0f };
    float draw_on{ 0.0f };
    Easing ease{ Easing::CubicOut };
    bool fade_in{ true };
};

// eased reveal progress [0,1] at timeline time t
[[nodiscard]] inline float annotation_progress(const Annotation& a, float t) {
    if(t < a.appear_time) return 0.0f;
    if(a.draw_on <= 0.0f) return 1.0f;
    float p = (t - a.appear_time) / a.draw_on;
    if(p < 0.0f) p = 0.0f;
    if(p > 1.0f) p = 1.0f;
    return apply(a.ease, p);
}

// normalized point -> screen pixels over the image rect
[[nodiscard]] inline Magnum::Vector2 to_screen(const Magnum::Vector2& norm, const Magnum::Vector2& image_min,
    const Magnum::Vector2& image_size) {
    return image_min + norm * image_size;
}

} // namespace smg
```

- [ ] **Step 4: Build and run**

```bash
cmake -B build >/dev/null && cmake --build build -j1 --target test_annotation_math && ctest --test-dir build -R test_annotation_math 2>&1 | grep -E "tests passed|failed"
```

Expected: `100% tests passed`.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i include/Annotations.hh test/test_annotation_math.cpp
git add include/Annotations.hh test/test_annotation_math.cpp test/CMakeLists.txt
git commit -m "anim: add annotation types + progress/mapping helpers"
```

---

### Task 6: `AnnotationLayer` (draw) + example + umbrella

**Files:**
- Modify: `include/Annotations.hh` (append `AnnotationLayer`)
- Create: `src/Annotations.cpp`
- Modify: `CMakeLists.txt` (`smg_sources`), `cmake/smg.hh.in` (umbrella)
- Test: `test/test_annotation_layer.cpp` (gui)
- Modify: `test/CMakeLists.txt` (`gui_test_list`)

**Interfaces:**
- Consumes: `Annotation`, `annotation_progress`, `to_screen` (Task 5); `Timeline` (Task 3); `GuiBase` frame (Task 4 not required but pairs with it).
- Produces: `class smg::AnnotationLayer` with `std::size_t add(const Annotation&)`, `Annotation& at(std::size_t)`, `void clear()`, `Timeline& timeline()`, `void draw(const char* title, Magnum::GL::Texture2D& background, const Magnum::Vector2i& size)`.

- [ ] **Step 1: Write the failing gui smoke test**

`test/test_annotation_layer.cpp`:

```cpp
// gui smoke test: render one annotated frame over a generated texture, then exit
#include "Annotations.hh"
#include "GuiBase.hh"
#include "SpriteSheet.hh" // reuse from_pixels to make a background texture

#include <array>
#include <cstdio>
#include <cstdlib>

using namespace smg;

static AnnotationLayer* g_layer = nullptr;
static Magnum::GL::Texture2D* g_bg = nullptr;

int anno_cb(void* /*data*/) {
    g_layer->timeline().advance(0.016f); // ~60fps step
    g_layer->draw("Telestration", *g_bg, Magnum::Vector2i{ 320, 240 });
    std::printf("OK annotation frame\n");
    std::exit(0);
    return 0;
}

class AnnoTest: public GuiBase {
public:
    explicit AnnoTest(const Arguments& arguments) : GuiBase(arguments) {
        std::array<unsigned char, 4> px{ 40, 60, 40, 255 };
        _sheet = SpriteSheet::from_pixels(
            Corrade::Containers::arrayView(reinterpret_cast<const char*>(px.data()), px.size()), Magnum::Vector2i{ 1, 1 },
            Magnum::PixelFormat::RGBA8Unorm, 1, 1);
        g_bg = &_sheet->texture();

        _layer.timeline().set_duration(3.0f);
        _layer.timeline().play();

        Annotation stroke;
        stroke.kind = AnnotationKind::Stroke;
        stroke.points = { { 0.1f, 0.1f }, { 0.4f, 0.5f }, { 0.8f, 0.2f } };
        stroke.color = Magnum::Color4{ 1.0f, 0.9f, 0.2f, 1.0f };
        stroke.draw_on = 1.0f;
        _layer.add(stroke);

        Annotation arrow;
        arrow.kind = AnnotationKind::Arrow;
        arrow.points = { { 0.2f, 0.8f }, { 0.7f, 0.6f } };
        arrow.color = Magnum::Color4{ 0.3f, 0.7f, 1.0f, 1.0f };
        arrow.appear_time = 0.5f;
        arrow.draw_on = 0.5f;
        _layer.add(arrow);

        Annotation label;
        label.kind = AnnotationKind::Text;
        label.points = { { 0.3f, 0.05f } };
        label.text = "play";
        label.color = Magnum::Color4{ 1.0f };
        _layer.add(label);

        g_layer = &_layer;
        _cb = DrawCallback::create();
        _cb->set_callback(anno_cb);
        add_callback(_cb);
    }

private:
    ShSpriteSheetPr _sheet;
    AnnotationLayer _layer;
    ShDrawCallbackPr _cb;
};

MAGNUM_APPLICATION_MAIN(AnnoTest)
```

- [ ] **Step 2: Register and confirm failure**

Add `test_annotation_layer.cpp` to `gui_test_list`, then build → FAIL (`AnnotationLayer` undeclared).

- [ ] **Step 3: Append `AnnotationLayer` to `include/Annotations.hh`**

Add includes at the top:

```cpp
#include <Magnum/GL/Texture.h>
#include <Magnum/Math/Vector2.h>

#include "Anim.hh"
```

Add inside `namespace smg` (after the helpers):

```cpp
class AnnotationLayer {
public:
    std::size_t add(const Annotation& a) {
        _annotations.push_back(a);
        return _annotations.size() - 1;
    }
    [[nodiscard]] Annotation& at(std::size_t handle) { return _annotations[handle]; }
    void clear() { _annotations.clear(); }
    [[nodiscard]] Timeline& timeline() { return _timeline; }

    // render background + overlay at the timeline's current time (caller advances the timeline)
    void draw(const char* title, Magnum::GL::Texture2D& background, const Magnum::Vector2i& size);

private:
    std::vector<Annotation> _annotations;
    Timeline _timeline;
};
```

- [ ] **Step 4: Write `src/Annotations.cpp`**

```cpp
#include "Annotations.hh"

#include <cmath>

#include <Magnum/ImGuiIntegration/Integration.h>
#include <Magnum/ImGuiIntegration/Widgets.h>

#include <imgui.h>

namespace smg {

namespace {
ImU32 to_col(const Magnum::Color4& c, float alpha_scale) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4{ c.r(), c.g(), c.b(), c.a() * alpha_scale });
}
ImVec2 iv(const Magnum::Vector2& v) { return ImVec2{ v.x(), v.y() }; }
} // namespace

void AnnotationLayer::draw(const char* title, Magnum::GL::Texture2D& background, const Magnum::Vector2i& size) {
    ImGui::Begin(title);
    Magnum::ImGuiIntegration::image(background, Magnum::Vector2{ size });
    const ImVec2 rmin = ImGui::GetItemRectMin();
    const ImVec2 rsz = ImGui::GetItemRectSize();
    const Magnum::Vector2 image_min{ rmin.x, rmin.y };
    const Magnum::Vector2 image_size{ rsz.x, rsz.y };
    const float t = _timeline.time();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for(const Annotation& a : _annotations) {
        const float p = annotation_progress(a, t);
        if(p <= 0.0f) continue;
        const float alpha = a.fade_in ? p : 1.0f;
        const ImU32 col = to_col(a.color, alpha);

        if(a.kind == AnnotationKind::Stroke && a.points.size() >= 2) {
            // reveal the first p-fraction of the polyline length (by segment count)
            const std::size_t segs = a.points.size() - 1;
            const float fseg = p * float(segs);
            const std::size_t full = std::size_t(fseg);
            std::vector<ImVec2> pts;
            for(std::size_t i = 0; i <= full && i < a.points.size(); ++i) pts.push_back(iv(to_screen(a.points[i], image_min, image_size)));
            if(full < segs) { // partial last segment
                const float frac = fseg - float(full);
                const Magnum::Vector2 mid = a.points[full] + (a.points[full + 1] - a.points[full]) * frac;
                pts.push_back(iv(to_screen(mid, image_min, image_size)));
            }
            if(pts.size() >= 2) dl->AddPolyline(pts.data(), int(pts.size()), col, ImDrawFlags_None, a.thickness);
        } else if((a.kind == AnnotationKind::Line || a.kind == AnnotationKind::Arrow) && a.points.size() >= 2) {
            const Magnum::Vector2 s = a.points[0];
            const Magnum::Vector2 tip = s + (a.points[1] - s) * p; // grows in
            const ImVec2 ps = iv(to_screen(s, image_min, image_size));
            const ImVec2 pt = iv(to_screen(tip, image_min, image_size));
            dl->AddLine(ps, pt, col, a.thickness);
            if(a.kind == AnnotationKind::Arrow) {
                const float dx = pt.x - ps.x, dy = pt.y - ps.y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if(len > 1.0f) {
                    const float ux = dx / len, uy = dy / len;
                    const float h = 12.0f; // arrowhead size px
                    const ImVec2 b1{ pt.x - ux * h - uy * h * 0.5f, pt.y - uy * h + ux * h * 0.5f };
                    const ImVec2 b2{ pt.x - ux * h + uy * h * 0.5f, pt.y - uy * h - ux * h * 0.5f };
                    dl->AddTriangleFilled(pt, b1, b2, col);
                }
            }
        } else if(a.kind == AnnotationKind::Circle && a.points.size() >= 2) {
            const Magnum::Vector2 c = to_screen(a.points[0], image_min, image_size);
            const Magnum::Vector2 rim = to_screen(a.points[1], image_min, image_size);
            const float r = (rim - c).length() * p; // grows in
            dl->AddCircle(iv(c), r, col, 0, a.thickness);
        } else if(a.kind == AnnotationKind::Text && !a.points.empty() && !a.text.empty()) {
            dl->AddText(iv(to_screen(a.points[0], image_min, image_size)), col, a.text.c_str());
        }
    }

    ImGui::End();
}

} // namespace smg
```

- [ ] **Step 5: Wire CMake + umbrella**

Add `src/Annotations.cpp` to `smg_sources` in `CMakeLists.txt`:

```cmake
    src/SpriteRenderer.cpp
    src/Annotations.cpp
```

Append to `cmake/smg.hh.in` after the plotting block:

```cpp
// animation + telestration overlay
#include "Easing.hh"
#include "Anim.hh"
#include "Annotations.hh"
```

- [ ] **Step 6: Build and run**

```bash
cmake -B build >/dev/null && cmake --build build -j1 --target test_annotation_layer 2>&1 | grep -iE "error:|Built target test_annotation_layer"
ctest --test-dir build -R test_annotation_layer --output-on-failure 2>&1 | grep -E "tests passed|failed|OK annotation"
```

Expected: builds, `100% tests passed` (`OK annotation frame`).

- [ ] **Step 7: Full unit suite + format + commit**

```bash
cmake --build build -j1 2>&1 | grep -iE "error:" | head
ctest --test-dir build -L unit 2>&1 | grep -E "tests passed|failed"
clang-format -i include/Annotations.hh src/Annotations.cpp test/test_annotation_layer.cpp
git add include/Annotations.hh src/Annotations.cpp CMakeLists.txt cmake/smg.hh.in test/test_annotation_layer.cpp test/CMakeLists.txt
git commit -m "anim: add AnnotationLayer telestration overlay"
```

Expected: full unit suite green.

---

## Self-Review

**Spec coverage:**
- Easing (`Easing` enum + `ease::` + `apply`) → Task 1. ✓
- `Track<T>` + `anim_lerp` → Task 2. ✓
- `Timeline` (advance/play/pause/stop/seek/speed/loop/finished) → Task 3. ✓
- `GuiBase::dt()` (spike-clamped) → Task 4. ✓
- Annotation types + normalized coords + `annotation_progress` + `to_screen` → Task 5. ✓
- `AnnotationLayer::draw` (ImGui draw list, reveal-by-kind, fade, pure-render) → Task 6. ✓
- Umbrella header → Task 6. ✓
- Reveal semantics: Stroke partial-polyline, Line/Arrow grow-in + arrowhead, Circle radius scale, Text fade → Task 6 `draw`. ✓
- Testing: headless units (easing, track, timeline, annotation math) + gui smoke (dt, annotation layer) → Tasks 1–6. ✓

**Decisions locked:** annotations normalized; `draw` pure-render (caller advances timeline; the gui test advances then draws); background app-supplied texture; ImGui draw list. Video decode / persistence / multi-track Animator deferred (spec out-of-scope).

**Placeholder scan:** none — every step carries complete, copy-ready code and commands.

**Type consistency:** `Easing`/`apply` used identically in Tasks 1, 2, 5. `Track<T>` `add/empty/duration/sample` consistent (Task 2 ↔ test). `Timeline` method set consistent (Task 3 ↔ Task 6 `timeline()` usage ↔ tests). `Annotation` fields (`kind/points/color/thickness/text/appear_time/draw_on/ease/fade_in`) consistent across Task 5 struct, Task 6 `draw`, and both tests. `to_screen`/`annotation_progress` signatures consistent. `AnnotationLayer` `add/at/clear/timeline/draw` consistent (Task 6 ↔ test).
