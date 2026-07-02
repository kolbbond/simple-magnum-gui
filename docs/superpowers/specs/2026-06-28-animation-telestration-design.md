# Design: `smg` animation core + telestration annotation overlay

Date: 2026-06-28
Status: approved (design), pending implementation plan

## Purpose

Give `simple-magnum-gui` time-based animation support and a 2D annotation
overlay, motivated by a **telestration** use case (drawing animated marks over a
background image/video, Madden-style). Today smg has no time/animation
infrastructure beyond `SpriteClip` (frame-index sprite playback) and does not
surface per-frame delta time.

Two layers, designed as independent units:

1. **Animation core** (headless, reusable everywhere): easing, keyframe
   `Track<T>`, and a seekable `Timeline`. Reusable far beyond telestration
   (camera moves, UI transitions, the scene/sprite work).
2. **Annotation overlay** (telestration-facing): animatable 2D marks (stroke,
   line, arrow, circle, text) in normalized image-space, drawn over a background
   texture via the ImGui draw list, driven by a `Timeline`.

Plus one small integration change: `GuiBase` exposes per-frame `dt()`.

The telestration *application* (video decode, UI, export) lives in the user's
other project and consumes this; smg provides the engine + drawable/animatable
primitives only.

## Background: what already exists

- `GuiBase` runs the frame loop (`drawEvent`/`mainLoopIteration`) but exposes no
  delta time to draw callbacks (callbacks get only `void*`).
- `SpriteClip` (`SpriteTypes.hh`) does frame-index playback — unrelated to
  property animation.
- Texture loading exists: `SpriteSheet::from_texture`/`from_pixels`/`load`
  (gated `SMG_WITH_IMAGE_IMPORT`), and `ScenePanel` already maps a normalized
  cursor over a displayed image via `ImGui::GetItemRectMin()` (the same
  normalized→screen mapping the overlay needs).
- ImGui is available in any draw callback (draw-list: `AddPolyline`, `AddLine`,
  `AddCircle`, `AddText`, `AddTriangleFilled` for arrowheads).

## Locked decisions

- **Coordinate space:** annotations stored in **normalized [0,1]** over the
  background image rect; mapped to screen at draw time so marks stay locked to
  the play under resize/letterbox.
- **Background:** an app-supplied texture (static image *or* a per-frame video
  frame the app decodes). smg does **not** decode video.
- **Timeline:** independent and seekable — free-run, or slave to the app's video
  clock via `seek(videoTime)`.
- **Animation binding:** each annotation carries a simple lifecycle
  (`appear_time`, `draw_on` duration with an easing, optional fade-in), built on
  the core; the core stays generic.
- **Rendering:** ImGui draw list over the image in the same window.

## Modules (new files, existing conventions)

| File | Responsibility |
|------|----------------|
| `include/Easing.hh` (header-only) | `smg::Easing` enum + `smg::ease::` pure functions + `apply(Easing, t)`. Headless. |
| `include/Anim.hh` (header-only) | `smg::Keyframe<T>`, `smg::Track<T>` (keyframe interpolation + easing), `anim_lerp` overloads, `smg::Timeline` (clock). Headless. |
| `include/Annotations.hh` / `src/Annotations.cpp` | `smg::Annotation` value types (normalized coords) + `smg::AnnotationLayer` (holds annotations + a `Timeline`, draws over a background texture). Geometry/progress math is headless; the GL/ImGui draw is gui-tested. |

`GuiBase` gains `float dt() const`. Umbrella `cmake/smg.hh.in` adds the new
headers. No new third-party dependency.

## Animation core API

```cpp
namespace smg {

enum class Easing { Linear, QuadIn, QuadOut, QuadInOut, CubicIn, CubicOut, CubicInOut };

namespace ease {
// pure t in [0,1] -> eased [0,1]; endpoints exact: f(0)=0, f(1)=1
[[nodiscard]] float linear(float t);
[[nodiscard]] float quad_in(float t);
[[nodiscard]] float quad_out(float t);
[[nodiscard]] float quad_in_out(float t);
[[nodiscard]] float cubic_in(float t);
[[nodiscard]] float cubic_out(float t);
[[nodiscard]] float cubic_in_out(float t);
} // namespace ease

[[nodiscard]] float apply(Easing e, float t); // dispatch enum -> ease:: fn

// interpolation helpers (overloaded): float, Magnum Vector2/Vector3/Vector4, Color3/Color4
template<class T> [[nodiscard]] T anim_lerp(const T& a, const T& b, float u);

template<class T> struct Keyframe {
    float time{ 0.0f };
    T value{};
    Easing ease{ Easing::Linear }; // easing applied across the segment ENDING at this key
};

template<class T> class Track {
public:
    void add(float time, const T& value, Easing ease = Easing::Linear); // kept sorted by time
    [[nodiscard]] bool empty() const;
    [[nodiscard]] float duration() const;          // time of last key (0 if empty)
    [[nodiscard]] T sample(float t) const;          // clamps before first / after last key
private:
    std::vector<Keyframe<T>> _keys;
};

class Timeline {
public:
    explicit Timeline(float duration = 0.0f);
    void advance(float dt);   // += dt*speed when playing; wraps if loop, clamps+stops if once
    void play();
    void pause();
    void stop();              // pause + seek(0)
    void seek(float t);       // clamped to [0, duration]
    void set_speed(float s);
    void set_loop(bool on);
    void set_duration(float d);
    [[nodiscard]] float time() const;
    [[nodiscard]] float duration() const;
    [[nodiscard]] bool playing() const;
    [[nodiscard]] bool finished() const; // non-loop reached duration
};

} // namespace smg
```

- `Track::sample` on an empty track returns `T{}`. With one key returns its
  value. Between keys `k[i-1]`→`k[i]`: `u = (t - k[i-1].time)/(k[i].time -
  k[i-1].time)`, eased by `k[i].ease`, then `anim_lerp`. Coincident-time keys
  (zero span) return the later value (step).

## GuiBase delta time

```cpp
[[nodiscard]] float GuiBase::dt() const; // seconds since the previous frame
```

Computed in `drawEvent` from a stored timestamp (Magnum frame timer /
`std::chrono::steady_clock`); clamped to a sane max (e.g. 0.1 s) to avoid huge
jumps after a stall/breakpoint. Draw callbacks advance their `Timeline` with it
(or the app slaves the timeline to its own video clock and ignores `dt`).

## Annotation overlay API

```cpp
namespace smg {

enum class AnnotationKind { Stroke, Line, Arrow, Circle, Text };

struct Annotation {
    AnnotationKind kind{ AnnotationKind::Stroke };
    std::vector<Magnum::Vector2> points;   // normalized [0,1]; Stroke=polyline, Line/Arrow=2 pts, Circle=center+rim, Text=anchor
    Magnum::Color4 color{ 1.0f };
    float thickness{ 2.0f };               // screen px
    std::string text;                      // for Text
    // animation lifecycle (built on the core)
    float appear_time{ 0.0f };             // timeline seconds when it starts
    float draw_on{ 0.0f };                 // reveal duration; 0 = instant
    Easing ease{ Easing::CubicOut };
    bool fade_in{ true };
};

// progress in [0,1] of an annotation at timeline time t (headless, pure)
[[nodiscard]] float annotation_progress(const Annotation& a, float t);

// map a normalized point to screen px given the image rect (headless, pure)
[[nodiscard]] Magnum::Vector2 to_screen(const Magnum::Vector2& norm, const Magnum::Vector2& image_min,
    const Magnum::Vector2& image_size);

class AnnotationLayer {
public:
    std::size_t add(const Annotation& a);   // returns handle
    [[nodiscard]] Annotation& at(std::size_t handle);
    void clear();
    [[nodiscard]] Timeline& timeline();

    // render background image + overlay annotations at the timeline's CURRENT time
    // (pure render; caller advances timeline() beforehand via GuiBase::dt() or a video clock)
    void draw(const char* title, Magnum::GL::Texture2D& background, const Magnum::Vector2i& size);

private:
    std::vector<Annotation> _annotations;
    Timeline _timeline;
};

} // namespace smg
```

- **Reveal semantics by kind** (using `annotation_progress`):
  - *Stroke*: draw the first `progress * (n-1)` segments (partial last segment
    interpolated) — the "draw-on" effect.
  - *Arrow/Line*: endpoint interpolated from start by `progress` (grows in);
    arrowhead drawn at the current tip.
  - *Circle*: radius scaled by `progress` (or sweep — radius scale for v1).
  - *Text*: appears at `progress > 0`; `fade_in` ramps alpha by `progress`.
- `fade_in` multiplies `color.a()` by `progress` for all kinds.
- `draw()` shows the texture via `ImGuiIntegration::image`, reads
  `GetItemRectMin()`/size for the image rect, maps each annotation's normalized
  points with `to_screen`, and renders via the ImGui draw list. It advances the
  timeline by `GuiBase::dt()` is the caller's job *or* `draw` takes no dt and the
  caller advances `timeline()` — **decision: the caller advances the timeline**
  (keeps `draw` pure-render and lets the app slave to a video clock).

## Error handling

- `Track::sample` / `Timeline` clamp out-of-range times; never throw.
- Empty annotation `points` or zero `size` → that annotation draws nothing.
- `annotation_progress`: `draw_on <= 0` → returns 1 once `t >= appear_time` else
  0; before `appear_time` → 0; after `appear_time + draw_on` → 1.
- `GuiBase::dt()` clamps spikes.

## Testing

- **Headless unit tests**:
  - `ease`: each fn has `f(0)≈0`, `f(1)≈1`, `in_out(0.5)≈0.5`; `apply` dispatch.
  - `Track`: empty→`T{}`; single key; linear interpolation midpoint; eased
    segment; clamp before-first/after-last; sorted insertion.
  - `Timeline`: `advance` accumulates `dt*speed`; `loop` wraps; `once`
    clamps + `finished()`; `seek`/`stop` clamp.
  - `annotation_progress`: before/after/instant/mid cases.
  - `to_screen`: normalized→pixel mapping for a known image rect.
- **GUI smoke test**: `AnnotationLayer::draw` over a generated texture renders one
  frame (stroke + arrow + text), advances the timeline, and exits (self-
  terminating, like the sprite/plot smoke tests).

## Out of scope (later slices)

- Video decoding / playback (app-side; smg takes a texture per frame).
- Annotation persistence/serialization (save/load telestration sessions).
- A generic multi-track `Animator` binding arbitrary tracks to arbitrary targets
  (v1 uses the per-annotation lifecycle; `Track<T>` is available for app use).
- Spline/path motion, bezier easing, per-vertex pressure for strokes.
- Undo/redo, hit-testing/selecting annotations (overlaps the parked picking work).
