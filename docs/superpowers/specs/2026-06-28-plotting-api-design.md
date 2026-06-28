# Design: `smg` ergonomic plotting & analysis API

Date: 2026-06-28
Status: approved (design), pending implementation plan

## Purpose

Give `simple-magnum-gui` an ergonomic, library-level plotting API over the
already-vendored **ImPlot** (2D) and **ImPlot3D** (3D), plus a small headless
**analysis** layer and a **live/rolling buffer**. Today users hand-write raw
`ImPlot::BeginPlot`/`PlotLine`/`EndPlot` with C arrays inside a `DrawCallback`
(see `test/test_magnum_implot.cpp`); the goal is to replace that boilerplate with
a few well-bounded types.

This is the **top backlog item** and the headline feature for a `dev → master`
push. Scope is a usable *starting point*: the common plot types, basic analysis,
and live data — not an exhaustive ImPlot mirror.

## Background: what already exists

- `GuiBase` (a `Magnum::Platform::Application`) owns the ImGui + ImPlot frame.
  Users register a `DrawCallback` (`int(*)(void*)`) and call ImGui/ImPlot inside
  it, within an `ImGui::Begin`/`End` (or ImGui's implicit "Debug" window).
- `external/implot` and `external/implot3d` are vendored and compiled into `smg`
  (ImPlot3D behind `ENABLE_IMPLOT3D`, default ON). No wrapper exists yet.
- `typedefs.hh` defines `smg::fltp = double`.

## Design principles

- **Scoped RAII objects group series; one-liners cover the quick case.** ImPlot
  requires `BeginPlot`/`EndPlot` around grouped series, so a scoped object is the
  natural grouping unit. Single-series convenience gets free functions.
- **Compose, don't capture.** Plot objects wrap only `BeginPlot`/`EndPlot`; they
  draw into the *current* ImGui window and never open their own. This avoids
  nested-window surprises and keeps them usable anywhere in a callback.
- **Separate pure logic from GL.** Analysis + rolling buffer are headless and
  unit-tested; plot objects need a frame and get gui smoke tests.

## Modules (new files, existing conventions)

| File | Responsibility |
|------|----------------|
| `include/PlotData.hh` (header-only where practical) / `src/PlotData.cpp` | Headless: `RollingBuffer<T>` ring buffer + `analysis::` (`Stats`, `stats`, `normalize`, `decimate`). No GL/ImGui. |
| `include/Plot.hh` / `src/Plot.cpp` | `smg::Plot` scoped 2D object + `smg::plot::` one-liner free functions. |
| `include/Plot3D.hh` / `src/Plot3D.cpp` | `smg::Plot3D` scoped 3D object over ImPlot3D; compiled only under `ENABLE_IMPLOT3D`. |

All three are added to the umbrella `cmake/smg.hh.in`.

## `Plot` (2D) — core API

```cpp
namespace smg {

class Plot {
public:
    explicit Plot(const char* title, const Magnum::Vector2& size = { -1.0f, 0.0f });
    ~Plot();                              // EndPlot iff BeginPlot succeeded
    Plot(const Plot&) = delete;
    Plot& operator=(const Plot&) = delete;

    explicit operator bool() const { return _open; }  // BeginPlot result

    // series; X optional (defaults to sample index 0..n-1). Templated on float/double.
    template<class T> void line(const char* label, Corrade::Containers::ArrayView<const T> y);
    template<class T> void line(const char* label, Corrade::Containers::ArrayView<const T> x,
        Corrade::Containers::ArrayView<const T> y);
    template<class T> void scatter(const char* label, /* x?, */ Corrade::Containers::ArrayView<const T> y);
    template<class T> void bar(const char* label, Corrade::Containers::ArrayView<const T> y, double width = 0.67);
    template<class T> void stairs(const char* label, /* x?, */ Corrade::Containers::ArrayView<const T> y);
    template<class T> void shaded(const char* label, Corrade::Containers::ArrayView<const T> x,
        Corrade::Containers::ArrayView<const T> lo, Corrade::Containers::ArrayView<const T> hi);
    template<class T> void histogram(const char* label, Corrade::Containers::ArrayView<const T> samples, int bins = -1);

private:
    bool _open{ false };
};

// one-liner convenience (self-contained BeginPlot/EndPlot in the current window)
namespace plot {
template<class T> void line(const char* title, Corrade::Containers::ArrayView<const T> y);
template<class T> void scatter(const char* title, Corrade::Containers::ArrayView<const T> y);
template<class T> void bar(const char* title, Corrade::Containers::ArrayView<const T> y);
} // namespace plot

} // namespace smg
```

- **Input overloads:** alongside `ArrayView<const T>`, provide `std::vector<T>` and
  raw `(const T* ptr, std::size_t n)` overloads so callers pass containers
  directly. `T` is `float` or `double` (ImPlot supports both).
- **Series methods no-op when `!_open`**, so the `if(Plot p{...})` guard is
  recommended but not required.
- **Window ownership:** `Plot` calls only `ImPlot::BeginPlot`/`EndPlot`. It draws
  into the current ImGui window. Callers wrap in `ImGui::Begin`/`End` for a named
  window; quick use lands in ImGui's implicit window.

## `Plot3D`

Same scoped pattern over ImPlot3D, compiled only when `ENABLE_IMPLOT3D`:

```cpp
if(smg::Plot3D p{ "Orbit" }) {
    p.line("path", xs, ys, zs);
    p.scatter("pts", xs, ys, zs);
    p.surface("field", xs, ys, zs, nx, ny); // gridded z over nx*ny
}
```

`operator bool` mirrors `ImPlot3D::BeginPlot`. Series methods are templated on
`float`/`double` with `ArrayView`/`std::vector`/`ptr,count` overloads.

## `RollingBuffer` + `analysis` (headless)

```cpp
namespace smg {

template<class T> class RollingBuffer {
public:
    explicit RollingBuffer(std::size_t capacity);
    void push(T v);                       // overwrites oldest when full
    void clear();
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t capacity() const;
    // oldest-to-newest contiguous snapshot (for plotting); cost O(n)
    [[nodiscard]] std::vector<T> ordered() const;
};

namespace analysis {
template<class T> struct Stats { T min; T max; double mean; double stddev; };
template<class T> [[nodiscard]] Stats<T> stats(Corrade::Containers::ArrayView<const T> v);
template<class T> [[nodiscard]] std::vector<T> normalize(Corrade::Containers::ArrayView<const T> v); // to [0,1]
template<class T> [[nodiscard]] std::vector<T> decimate(Corrade::Containers::ArrayView<const T> v, std::size_t max_points);
} // namespace analysis

} // namespace smg
```

- `RollingBuffer::ordered()` returns the logical oldest→newest sequence so it can
  be handed straight to `Plot::line`.
- `decimate` caps a large series to `max_points` by uniform stride (simple,
  predictable; min/max-per-bucket decimation is a later refinement).
- `normalize` of a constant series returns zeros (no divide-by-zero).
- Pure functions/containers; no GL or ImGui dependency.

## Error handling

- Mismatched x/y lengths: series methods use the smaller length (no UB, no throw).
- Empty input: series methods draw nothing; `analysis::stats` on empty returns a
  zeroed `Stats` (documented), `normalize`/`decimate` return empty.
- `Plot`/`Plot3D` constructed outside an active ImGui frame is caller error
  (same contract as raw ImPlot); `operator bool` will be false and methods no-op.

## Testing

- **Headless unit tests** (added to `unit_test_list`):
  - `RollingBuffer`: push/size/capacity, wraparound order via `ordered()`, clear.
  - `analysis`: `stats` (known mean/std), `normalize` (range + constant-series),
    `decimate` (count cap, endpoints preserved).
- **GUI smoke tests** (self-terminating under `GuiBase`, like the sprite tests):
  - `test_plot`: render one frame using `Plot` (line+scatter+bar) and a one-liner.
  - `test_plot3d`: render one frame using `Plot3D` (guarded by `ENABLE_IMPLOT3D`).

## Hardening: CI

Add `.github/workflows/ci.yml` (Linux): install deps (or build vendored
Corrade/Magnum), configure, build `smg`, and run `ctest -L unit` — the headless
gate. Never runs unlabeled ctest (the GUI demos return 0 regardless and some loop
forever). This enforces the unit suite on every push/PR before `master`.

## Out of scope (later slices)

- Full ImPlot coverage (heatmaps, pie, candlestick, error bars, digital, images).
- Plot styling/theme API, axis-format callbacks, drag tools, query/selection.
- Min/max-per-bucket decimation; FFT/filtering in `analysis`.
- Saving plots to PNG; CSV import.
- macOS/Windows CI runners.
