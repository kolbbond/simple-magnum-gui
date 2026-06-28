# Plotting & Analysis API Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an ergonomic, library-level plotting API (`smg::Plot`, `smg::Plot3D`, one-liners) over the vendored ImPlot/ImPlot3D, plus a headless `RollingBuffer` + `analysis` layer, with unit/gui tests and CI.

**Architecture:** Plot wrappers are header-only templates: scoped RAII objects (`BeginPlot`/`EndPlot` in ctor/dtor, `operator bool` = success) with series methods that forward to ImPlot, drawing into the current ImGui window. ImPlot already instantiates its templates for `float`/`double` inside `smg`, so headers need no new `.cpp`. The analysis/rolling-buffer layer is pure, header-only, and headless-unit-tested.

**Tech Stack:** C++17, ImPlot + ImPlot3D (vendored in `external/`), Corrade Containers (`ArrayView`, `ArrayViewStl`), ImGui, `GuiBase`, CMake/Ninja, GitHub Actions.

## Global Constraints

- C++17; out-of-source build. **Use the `build/` dir** (Release, deps prebuilt). `build-native` is a broken cache — do not use it.
- Builds single-core: `cmake --build build -j1` (honor the maintainer's machine-load preference).
- Style: `.clang-format` + `.clang-tidy` are source of truth. Run `clang-format -i` on every modified C++ file before committing.
- **No `auto`** except iterators/unspellable STL types. **No `using`** directives/declarations — spell out `Magnum::`, `smg::`, `std::`, `Corrade::`.
- Namespace `smg` (sub-namespaces `smg::plot`, `smg::analysis`). Classes PascalCase; methods/free fns snake_case; `ShXxxPr` shared-ptr typedefs.
- Laconic comments — the *why*, not the *what*.
- **No `Co-Authored-By` trailers.** Commit subjects topic-prefixed + laconic, e.g. `plot: add rolling buffer`.
- Tests: `unit` (headless, no GL/ImGui) and `gui` (need a display). Pure layer → `unit_test_list`; plot objects → `gui_test_list`, self-terminating (exit after one frame), mirroring the sprite gui smoke tests.
- Inputs: series methods take `Corrade::Containers::ArrayView<const T>` (`T` = `float`/`double`); `std::vector<T>` converts implicitly because `Plot.hh` includes `<Corrade/Containers/ArrayViewStl.h>`.
- Plot/Plot3D wrap **only** `BeginPlot`/`EndPlot` — they draw into the current ImGui window, never open their own.

**Build / test commands used throughout:**

```bash
cmake -B build >/dev/null            # reconfigure (deps already built)
cmake --build build -j1 --target <t> # build one target, single core
ctest --test-dir build -R <name> --output-on-failure
ctest --test-dir build -L unit --output-on-failure   # headless gate
```

---

### Task 1: `analysis` — stats / normalize / decimate (headless)

**Files:**
- Create: `include/PlotData.hh`
- Test: `test/test_plot_analysis.cpp`
- Modify: `test/CMakeLists.txt` (`unit_test_list`)

**Interfaces:**
- Produces (in `namespace smg::analysis`):
  - `template<class T> struct Stats { T min; T max; double mean; double stddev; };`
  - `template<class T> Stats<T> stats(Corrade::Containers::ArrayView<const T> v);` — empty → all-zero `Stats`.
  - `template<class T> std::vector<T> normalize(Corrade::Containers::ArrayView<const T> v);` — to [0,1]; constant series → zeros; empty → empty.
  - `template<class T> std::vector<T> decimate(Corrade::Containers::ArrayView<const T> v, std::size_t max_points);` — uniform stride; first & last preserved; `max_points==0` treated as 1.

- [ ] **Step 1: Write the failing test**

`test/test_plot_analysis.cpp`:

```cpp
#include "PlotData.hh"
#include "test_util.hh"

#include <vector>

int main() {
    const std::vector<float> v{ 0.0f, 2.0f, 4.0f, 6.0f, 8.0f };
    const smg::analysis::Stats<float> s = smg::analysis::stats(Corrade::Containers::arrayView(v));
    CHECK(smgtest::approx(s.min, 0.0f));
    CHECK(smgtest::approx(s.max, 8.0f));
    CHECK(smgtest::approx(float(s.mean), 4.0f));
    CHECK(s.stddev > 2.8 && s.stddev < 2.9); // population std = sqrt(8) ~= 2.828

    const std::vector<float> n = smg::analysis::normalize(Corrade::Containers::arrayView(v));
    CHECK(n.size() == 5);
    CHECK(smgtest::approx(n.front(), 0.0f));
    CHECK(smgtest::approx(n.back(), 1.0f));
    CHECK(smgtest::approx(n[2], 0.5f));

    // constant series -> zeros, no divide-by-zero
    const std::vector<float> c{ 3.0f, 3.0f, 3.0f };
    const std::vector<float> cn = smg::analysis::normalize(Corrade::Containers::arrayView(c));
    CHECK(cn.size() == 3);
    CHECK(smgtest::approx(cn[0], 0.0f));
    CHECK(smgtest::approx(cn[2], 0.0f));

    // decimate caps the count and preserves endpoints
    std::vector<float> big(1000);
    for(int i = 0; i < 1000; ++i) big[std::size_t(i)] = float(i);
    const std::vector<float> d = smg::analysis::decimate(Corrade::Containers::arrayView(big), 100);
    CHECK(d.size() <= 100);
    CHECK(d.size() >= 2);
    CHECK(smgtest::approx(d.front(), 0.0f));
    CHECK(smgtest::approx(d.back(), 999.0f));

    // small series returned as-is
    const std::vector<float> d2 = smg::analysis::decimate(Corrade::Containers::arrayView(v), 100);
    CHECK(d2.size() == 5);

    // empty handling
    const std::vector<float> e;
    const smg::analysis::Stats<float> es = smg::analysis::stats(Corrade::Containers::arrayView(e));
    CHECK(smgtest::approx(es.min, 0.0f));
    CHECK(smgtest::approx(float(es.mean), 0.0f));

    TEST_RETURN();
}
```

- [ ] **Step 2: Register and confirm failure**

Add `test_plot_analysis.cpp` to `unit_test_list` in `test/CMakeLists.txt`, then:

```bash
cmake -B build >/dev/null && cmake --build build -j1 --target test_plot_analysis 2>&1 | tail -5
```

Expected: FAIL — `PlotData.hh` not found.

- [ ] **Step 3: Write the header (analysis only for now)**

`include/PlotData.hh`:

```cpp
// headless plotting helpers: analysis stats + rolling buffer (no GL/ImGui)
#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

#include <Corrade/Containers/ArrayView.h>

namespace smg::analysis {

template<class T> struct Stats {
    T min{ 0 };
    T max{ 0 };
    double mean{ 0.0 };
    double stddev{ 0.0 };
};

template<class T> [[nodiscard]] Stats<T> stats(Corrade::Containers::ArrayView<const T> v) {
    Stats<T> s;
    if(v.isEmpty()) return s;
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

template<class T> [[nodiscard]] std::vector<T> normalize(Corrade::Containers::ArrayView<const T> v) {
    std::vector<T> out(v.size());
    if(v.isEmpty()) return out;
    const Stats<T> s = stats(v);
    const double span = double(s.max) - double(s.min);
    for(std::size_t i = 0; i < v.size(); ++i)
        out[i] = span > 0.0 ? T((double(v[i]) - double(s.min)) / span) : T(0);
    return out;
}

template<class T> [[nodiscard]] std::vector<T> decimate(Corrade::Containers::ArrayView<const T> v, std::size_t max_points) {
    const std::size_t cap = max_points == 0 ? 1 : max_points;
    std::vector<T> out;
    if(v.size() <= cap) {
        out.assign(v.begin(), v.end());
        return out;
    }
    const std::size_t stride = (v.size() + cap - 1) / cap; // ceil
    for(std::size_t i = 0; i < v.size(); i += stride) out.push_back(v[i]);
    if(out.empty() || out.back() != v[v.size() - 1]) out.push_back(v[v.size() - 1]); // keep last
    return out;
}

} // namespace smg::analysis
```

- [ ] **Step 4: Build and run**

```bash
cmake --build build -j1 --target test_plot_analysis && ctest --test-dir build -R test_plot_analysis --output-on-failure 2>&1 | grep -E "tests passed|failed"
```

Expected: `100% tests passed`.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i include/PlotData.hh test/test_plot_analysis.cpp
git add include/PlotData.hh test/test_plot_analysis.cpp test/CMakeLists.txt
git commit -m "plot: add analysis stats/normalize/decimate (headless)"
```

---

### Task 2: `RollingBuffer` (headless)

**Files:**
- Modify: `include/PlotData.hh` (append)
- Test: `test/test_plot_rolling.cpp`
- Modify: `test/CMakeLists.txt` (`unit_test_list`)

**Interfaces:**
- Produces (in `namespace smg`):
  - `template<class T> class RollingBuffer` with `RollingBuffer(std::size_t capacity)`, `void push(T)`, `void clear()`, `std::size_t size() const`, `std::size_t capacity() const`, `std::vector<T> ordered() const` (oldest→newest).

- [ ] **Step 1: Write the failing test**

`test/test_plot_rolling.cpp`:

```cpp
#include "PlotData.hh"
#include "test_util.hh"

int main() {
    smg::RollingBuffer<int> b{ 3 };
    CHECK(b.capacity() == 3);
    CHECK(b.size() == 0);

    b.push(1);
    b.push(2);
    const std::vector<int> two = b.ordered();
    CHECK(b.size() == 2);
    CHECK(two.size() == 2);
    CHECK(two[0] == 1 && two[1] == 2);

    // fill then overflow: oldest dropped, order preserved
    b.push(3);
    b.push(4); // drops 1
    b.push(5); // drops 2
    const std::vector<int> o = b.ordered();
    CHECK(b.size() == 3);
    CHECK(o.size() == 3);
    CHECK(o[0] == 3 && o[1] == 4 && o[2] == 5);

    b.clear();
    CHECK(b.size() == 0);
    CHECK(b.ordered().empty());

    TEST_RETURN();
}
```

- [ ] **Step 2: Register and confirm failure**

Add `test_plot_rolling.cpp` to `unit_test_list`, then:

```bash
cmake -B build >/dev/null && cmake --build build -j1 --target test_plot_rolling 2>&1 | tail -5
```

Expected: FAIL — `RollingBuffer` undeclared.

- [ ] **Step 3: Append `RollingBuffer` to `include/PlotData.hh`**

Add a `namespace smg { ... }` block (separate from `smg::analysis`), e.g. above the `analysis` namespace:

```cpp
namespace smg {

// fixed-capacity ring buffer for live/streaming plot data
template<class T> class RollingBuffer {
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
```

- [ ] **Step 4: Build and run**

```bash
cmake --build build -j1 --target test_plot_rolling && ctest --test-dir build -R test_plot_rolling --output-on-failure 2>&1 | grep -E "tests passed|failed"
```

Expected: `100% tests passed`.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i include/PlotData.hh test/test_plot_rolling.cpp
git add include/PlotData.hh test/test_plot_rolling.cpp test/CMakeLists.txt
git commit -m "plot: add RollingBuffer ring buffer (headless)"
```

---

### Task 3: `Plot` (2D) + one-liners

**Files:**
- Create: `include/Plot.hh`
- Modify: `cmake/smg.hh.in` (umbrella)
- Test: `test/test_plot.cpp` (gui)
- Modify: `test/CMakeLists.txt` (`gui_test_list`)

**Interfaces:**
- Consumes: ImPlot (`external/implot`, public include on `smg`); `PlotData.hh` not required here.
- Produces (in `namespace smg`):
  - `class Plot` — `explicit Plot(const char* title, const Magnum::Vector2& size = {-1.0f, 0.0f})`, dtor, `explicit operator bool() const`, and series methods (each with `(label, y)` and `(label, x, y)` overloads over `Corrade::Containers::ArrayView<const T>`): `line`, `scatter`, `bar` (`(label, y, width=0.67)`), `stairs`, `shaded` (`(label, x, lo, hi)`), `histogram` (`(label, samples, bins=-1)`).
  - `namespace plot`: `line`, `scatter`, `bar` one-liners — `(title, ArrayView<const T> y)`.

- [ ] **Step 1: Write the failing gui smoke test**

`test/test_plot.cpp`:

```cpp
// gui smoke test: render one frame of 2D plots, then exit
#include "GuiBase.hh"
#include "Plot.hh"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace smg;

static std::vector<float> g_x, g_y;

int plot_cb(void* /*data*/) {
    ImGui::Begin("Plots");
    if(Plot p{ "signals" }) {
        p.line("y", Corrade::Containers::arrayView(g_y));
        p.scatter("pts", Corrade::Containers::arrayView(g_x), Corrade::Containers::arrayView(g_y));
        p.bar("b", Corrade::Containers::arrayView(g_y));
    }
    smg::plot::line("quick", Corrade::Containers::arrayView(g_y));
    ImGui::End();
    std::printf("OK plot frame\n");
    std::exit(0);
    return 0;
}

class PlotTest: public GuiBase {
public:
    explicit PlotTest(const Arguments& arguments) : GuiBase(arguments) {
        for(int i = 0; i < 16; ++i) {
            g_x.push_back(float(i));
            g_y.push_back(float(i) * 0.5f);
        }
        _cb = DrawCallback::create();
        _cb->set_callback(plot_cb);
        add_callback(_cb);
    }

private:
    ShDrawCallbackPr _cb;
};

MAGNUM_APPLICATION_MAIN(PlotTest)
```

- [ ] **Step 2: Register and confirm failure**

Add `test_plot.cpp` to `gui_test_list`, then:

```bash
cmake -B build >/dev/null && cmake --build build -j1 --target test_plot 2>&1 | tail -5
```

Expected: FAIL — `Plot.hh` not found.

- [ ] **Step 3: Write `include/Plot.hh`**

```cpp
// ergonomic 2D plotting over ImPlot: scoped Plot (groups series) + one-liners.
// Draws into the current ImGui window; needs an active ImGui+ImPlot frame.
#pragma once

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/ArrayViewStl.h> // std::vector -> ArrayView
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

#include "implot.h"

namespace smg {

class Plot {
public:
    explicit Plot(const char* title, const Magnum::Vector2& size = Magnum::Vector2{ -1.0f, 0.0f })
        : _open{ ImPlot::BeginPlot(title, ImVec2{ size.x(), size.y() }) } {}
    ~Plot() {
        if(_open) ImPlot::EndPlot();
    }
    Plot(const Plot&) = delete;
    Plot& operator=(const Plot&) = delete;

    explicit operator bool() const { return _open; }

    template<class T> void line(const char* label, Corrade::Containers::ArrayView<const T> y) {
        if(_open && !y.isEmpty()) ImPlot::PlotLine(label, y.data(), int(y.size()));
    }
    template<class T> void line(const char* label, Corrade::Containers::ArrayView<const T> x, Corrade::Containers::ArrayView<const T> y) {
        const int n = int(x.size() < y.size() ? x.size() : y.size());
        if(_open && n > 0) ImPlot::PlotLine(label, x.data(), y.data(), n);
    }
    template<class T> void scatter(const char* label, Corrade::Containers::ArrayView<const T> y) {
        if(_open && !y.isEmpty()) ImPlot::PlotScatter(label, y.data(), int(y.size()));
    }
    template<class T> void scatter(const char* label, Corrade::Containers::ArrayView<const T> x, Corrade::Containers::ArrayView<const T> y) {
        const int n = int(x.size() < y.size() ? x.size() : y.size());
        if(_open && n > 0) ImPlot::PlotScatter(label, x.data(), y.data(), n);
    }
    template<class T> void bar(const char* label, Corrade::Containers::ArrayView<const T> y, double width = 0.67) {
        if(_open && !y.isEmpty()) ImPlot::PlotBars(label, y.data(), int(y.size()), width);
    }
    template<class T> void stairs(const char* label, Corrade::Containers::ArrayView<const T> y) {
        if(_open && !y.isEmpty()) ImPlot::PlotStairs(label, y.data(), int(y.size()));
    }
    template<class T> void stairs(const char* label, Corrade::Containers::ArrayView<const T> x, Corrade::Containers::ArrayView<const T> y) {
        const int n = int(x.size() < y.size() ? x.size() : y.size());
        if(_open && n > 0) ImPlot::PlotStairs(label, x.data(), y.data(), n);
    }
    template<class T>
    void shaded(const char* label, Corrade::Containers::ArrayView<const T> x, Corrade::Containers::ArrayView<const T> lo,
        Corrade::Containers::ArrayView<const T> hi) {
        std::size_t m = x.size() < lo.size() ? x.size() : lo.size();
        m = m < hi.size() ? m : hi.size();
        if(_open && m > 0) ImPlot::PlotShaded(label, x.data(), lo.data(), hi.data(), int(m));
    }
    template<class T> void histogram(const char* label, Corrade::Containers::ArrayView<const T> samples, int bins = -1) {
        if(_open && !samples.isEmpty()) ImPlot::PlotHistogram(label, samples.data(), int(samples.size()), bins > 0 ? bins : ImPlotBin_Sturges);
    }

private:
    bool _open{ false };
};

namespace plot {

template<class T> void line(const char* title, Corrade::Containers::ArrayView<const T> y) {
    if(ImPlot::BeginPlot(title)) {
        if(!y.isEmpty()) ImPlot::PlotLine(title, y.data(), int(y.size()));
        ImPlot::EndPlot();
    }
}
template<class T> void scatter(const char* title, Corrade::Containers::ArrayView<const T> y) {
    if(ImPlot::BeginPlot(title)) {
        if(!y.isEmpty()) ImPlot::PlotScatter(title, y.data(), int(y.size()));
        ImPlot::EndPlot();
    }
}
template<class T> void bar(const char* title, Corrade::Containers::ArrayView<const T> y) {
    if(ImPlot::BeginPlot(title)) {
        if(!y.isEmpty()) ImPlot::PlotBars(title, y.data(), int(y.size()), 0.67);
        ImPlot::EndPlot();
    }
}

} // namespace plot

} // namespace smg
```

- [ ] **Step 4: Add to the umbrella header `cmake/smg.hh.in`**

After the sprites block, append:

```cpp
// plotting & analysis
#include "PlotData.hh"
#include "Plot.hh"
```

- [ ] **Step 5: Build and run**

```bash
cmake -B build >/dev/null && cmake --build build -j1 --target test_plot 2>&1 | grep -iE "error:|Built target test_plot"
ctest --test-dir build -R "test_plot$" --output-on-failure 2>&1 | grep -E "tests passed|failed|OK plot"
```

Expected: builds, `100% tests passed` (`OK plot frame`). Headless without a display → build success is the gate; note it.

- [ ] **Step 6: Format and commit**

```bash
clang-format -i include/Plot.hh test/test_plot.cpp
git add include/Plot.hh cmake/smg.hh.in test/test_plot.cpp test/CMakeLists.txt
git commit -m "plot: add 2D Plot scoped object + one-liners"
```

---

### Task 4: `Plot3D` (ImPlot3D)

**Files:**
- Create: `include/Plot3D.hh`
- Modify: `CMakeLists.txt` (define `SMG_WITH_IMPLOT3D` under `ENABLE_IMPLOT3D`)
- Modify: `src/GuiBase.cpp` (create ImPlot3D context when enabled)
- Modify: `cmake/smg.hh.in` (conditional include)
- Test: `test/test_plot3d.cpp` (gui, only when `ENABLE_IMPLOT3D`)
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: ImPlot3D (`external/implot3d`, public include when `ENABLE_IMPLOT3D`).
- Produces (in `namespace smg`, guarded by `SMG_WITH_IMPLOT3D`):
  - `class Plot3D` — `explicit Plot3D(const char* title, const Magnum::Vector2& size = {-1.0f, 0.0f})`, dtor, `explicit operator bool() const`, series: `line`/`scatter` (`(label, x, y, z)` over `ArrayView<const T>`), `surface` (`(label, x, y, z, nx, ny)`).

- [ ] **Step 1: Add the build define (`CMakeLists.txt`)**

In the existing `if(ENABLE_IMPLOT3D)` block near line 210 (the one adding the implot3d include dir), add the compile definition:

```cmake
if(ENABLE_IMPLOT3D)
    target_include_directories(smg PUBLIC
        $<BUILD_INTERFACE:${SMG_IMPLOT3D_DIR}>
    )
    target_compile_definitions(smg PUBLIC SMG_WITH_IMPLOT3D)
endif()
```

- [ ] **Step 2: Create the ImPlot3D context in `GuiBase` (`src/GuiBase.cpp`)**

After `ImPlot::CreateContext();` (around line 201), add:

```cpp
#ifdef SMG_WITH_IMPLOT3D
	ImPlot3D::CreateContext();
#endif
```

And ensure the header is included near the other implot include (top of `src/GuiBase.cpp`):

```cpp
#ifdef SMG_WITH_IMPLOT3D
#include "implot3d.h"
#endif
```

- [ ] **Step 3: Write the failing gui smoke test**

`test/test_plot3d.cpp`:

```cpp
// gui smoke test: render one frame of a 3D plot, then exit
#include "GuiBase.hh"
#include "Plot3D.hh"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace smg;

static std::vector<float> g_x, g_y, g_z;

int plot3d_cb(void* /*data*/) {
    ImGui::Begin("Plot3D");
    if(Plot3D p{ "orbit" }) {
        p.line("path", Corrade::Containers::arrayView(g_x), Corrade::Containers::arrayView(g_y), Corrade::Containers::arrayView(g_z));
        p.scatter("pts", Corrade::Containers::arrayView(g_x), Corrade::Containers::arrayView(g_y), Corrade::Containers::arrayView(g_z));
    }
    ImGui::End();
    std::printf("OK plot3d frame\n");
    std::exit(0);
    return 0;
}

class Plot3DTest: public GuiBase {
public:
    explicit Plot3DTest(const Arguments& arguments) : GuiBase(arguments) {
        for(int i = 0; i < 16; ++i) {
            g_x.push_back(float(i));
            g_y.push_back(float(i) * 0.5f);
            g_z.push_back(float(i) * 0.25f);
        }
        _cb = DrawCallback::create();
        _cb->set_callback(plot3d_cb);
        add_callback(_cb);
    }

private:
    ShDrawCallbackPr _cb;
};

MAGNUM_APPLICATION_MAIN(Plot3DTest)
```

- [ ] **Step 4: Register the test (only when ImPlot3D is on) — `test/CMakeLists.txt`**

Inside the `if(SMG_BUILD_GUI_TESTS)` block, after the existing gui loop and the sprite-renderer include line, add:

```cmake
    if(ENABLE_IMPLOT3D)
        smg_add_test(test_plot3d test_plot3d.cpp "gui")
    endif()
```

- [ ] **Step 5: Confirm failure**

```bash
cmake -B build >/dev/null && cmake --build build -j1 --target test_plot3d 2>&1 | tail -5
```

Expected: FAIL — `Plot3D.hh` not found.

- [ ] **Step 6: Write `include/Plot3D.hh`**

```cpp
// ergonomic 3D plotting over ImPlot3D: scoped Plot3D (groups series).
// Draws into the current ImGui window; needs an active ImGui+ImPlot3D frame.
#pragma once

#ifdef SMG_WITH_IMPLOT3D

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

#include "implot3d.h"

namespace smg {

class Plot3D {
public:
    explicit Plot3D(const char* title, const Magnum::Vector2& size = Magnum::Vector2{ -1.0f, 0.0f })
        : _open{ ImPlot3D::BeginPlot(title, ImVec2{ size.x(), size.y() }) } {}
    ~Plot3D() {
        if(_open) ImPlot3D::EndPlot();
    }
    Plot3D(const Plot3D&) = delete;
    Plot3D& operator=(const Plot3D&) = delete;

    explicit operator bool() const { return _open; }

    template<class T>
    void line(const char* label, Corrade::Containers::ArrayView<const T> x, Corrade::Containers::ArrayView<const T> y,
        Corrade::Containers::ArrayView<const T> z) {
        const int n = min3(x.size(), y.size(), z.size());
        if(_open && n > 0) ImPlot3D::PlotLine(label, x.data(), y.data(), z.data(), n);
    }
    template<class T>
    void scatter(const char* label, Corrade::Containers::ArrayView<const T> x, Corrade::Containers::ArrayView<const T> y,
        Corrade::Containers::ArrayView<const T> z) {
        const int n = min3(x.size(), y.size(), z.size());
        if(_open && n > 0) ImPlot3D::PlotScatter(label, x.data(), y.data(), z.data(), n);
    }
    template<class T>
    void surface(const char* label, Corrade::Containers::ArrayView<const T> x, Corrade::Containers::ArrayView<const T> y,
        Corrade::Containers::ArrayView<const T> z, int nx, int ny) {
        if(_open && nx > 0 && ny > 0 && z.size() >= std::size_t(nx * ny)) ImPlot3D::PlotSurface(label, x.data(), y.data(), z.data(), nx, ny);
    }

private:
    static int min3(std::size_t a, std::size_t b, std::size_t c) {
        const std::size_t m = a < b ? a : b;
        return int(m < c ? m : c);
    }

    bool _open{ false };
};

} // namespace smg

#endif // SMG_WITH_IMPLOT3D
```

- [ ] **Step 7: Add conditional include to the umbrella (`cmake/smg.hh.in`)**

After the `Plot.hh` include:

```cpp
#ifdef SMG_WITH_IMPLOT3D
#include "Plot3D.hh"
#endif
```

- [ ] **Step 8: Build and run (full `smg` rebuilds once for the new public define)**

```bash
cmake -B build >/dev/null && cmake --build build -j1 --target test_plot3d 2>&1 | grep -iE "error:|Built target test_plot3d"
ctest --test-dir build -R test_plot3d --output-on-failure 2>&1 | grep -E "tests passed|failed|OK plot3d"
```

Expected: builds, `100% tests passed` (`OK plot3d frame`).

- [ ] **Step 9: Format and commit**

```bash
clang-format -i include/Plot3D.hh src/GuiBase.cpp test/test_plot3d.cpp
git add include/Plot3D.hh CMakeLists.txt src/GuiBase.cpp cmake/smg.hh.in test/test_plot3d.cpp test/CMakeLists.txt
git commit -m "plot: add 3D Plot3D over ImPlot3D (gated SMG_WITH_IMPLOT3D)"
```

---

### Task 5: CI — headless unit gate

**Files:**
- Create: `.github/workflows/ci.yml`

**Interfaces:** none (infra). Builds `smg` + unit tests from vendored source and runs `ctest -L unit`.

- [ ] **Step 1: Write the workflow**

`.github/workflows/ci.yml`:

```yaml
name: ci

on:
  push:
    branches: [master, dev]
  pull_request:

jobs:
  unit:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install build deps
        run: |
          sudo apt-get update
          sudo apt-get install -y --no-install-recommends \
            cmake ninja-build g++ libsdl2-dev libgl1-mesa-dev xorg-dev ccache

      - name: ccache
        uses: actions/cache@v4
        with:
          path: ~/.ccache
          key: ccache-${{ runner.os }}-${{ github.sha }}
          restore-keys: ccache-${{ runner.os }}-

      - name: Configure
        run: >
          cmake -B build -G Ninja
          -DCMAKE_BUILD_TYPE=Release
          -DENABLE_EXAMPLES=OFF
          -DSMG_BUILD_GUI_TESTS=OFF
          -DCMAKE_C_COMPILER_LAUNCHER=ccache
          -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

      - name: Build
        run: cmake --build build

      - name: Unit tests (headless)
        run: ctest --test-dir build -L unit --output-on-failure
```

Notes captured in the workflow: GUI tests are disabled (no display on the runner; several are infinite-loop demos), examples are skipped for speed, and only the `unit` label runs — matching the repo's "never run unlabeled ctest in CI" rule. First run builds vendored Corrade/Magnum (slow); ccache makes subsequent runs fast.

- [ ] **Step 2: Validate the YAML locally**

```bash
python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/ci.yml')); print('YAML OK')"
```

Expected: `YAML OK`.

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: headless unit-test workflow (build + ctest -L unit)"
```

---

## Self-Review

**Spec coverage:**
- `Plot` 2D (line/scatter/bar/stairs/shaded/histogram, float+double, vector/ArrayView, current-window) → Task 3. ✓
- One-liners `smg::plot::line/scatter/bar` → Task 3. ✓
- `Plot3D` (line/scatter/surface, `ENABLE_IMPLOT3D`-gated) → Task 4. ✓
- `RollingBuffer` → Task 2; `analysis` (stats/normalize/decimate) → Task 1. ✓
- Headless unit tests (analysis, rolling) → Tasks 1–2; gui smoke (plot, plot3d) → Tasks 3–4. ✓
- CI `ctest -L unit` → Task 5. ✓
- Umbrella header updates → Tasks 3–4. ✓
- Error handling (mismatched lengths use min; empty no-ops; constant-series normalize → zeros) → encoded in Task 1 + Task 3/4 method bodies. ✓

**Decisions locked (spec open points):** `decimate` is uniform-stride (min/max-per-bucket deferred); CI builds vendored deps from source with ccache (no system-Magnum assumption), GUI tests + examples off in CI.

**Placeholder scan:** none — all steps carry complete code/commands.

**Type consistency:** `Stats<T>{min,max,mean,stddev}` consistent across Task 1 and tests. `Plot`/`Plot3D` ctor `(const char*, const Magnum::Vector2&)`, `operator bool`, and `ArrayView<const T>` series signatures consistent across header + tests. `RollingBuffer` `push/clear/size/capacity/ordered` consistent across Task 2 + test. `SMG_WITH_IMPLOT3D` used identically in CMake, GuiBase, Plot3D.hh, umbrella, and test registration.

**Header-only note:** Plotting wrappers add no `smg_sources`; ImPlot's float/double template instantiations already live in `smg`, and `include/` is installed wholesale, so no install-rule changes are needed.
