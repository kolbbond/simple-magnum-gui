# Prerendered Isometric Sprites Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give `smg` a runtime API to display prerendered 2D sprites on an isometric plane (Majesty-style), reusing the existing `ScenePanel` 3D pipeline.

**Architecture:** Sprites are textured, camera-facing billboard quads drawn in world space under an orthographic isometric camera, sharing `ScenePanel`'s depth buffer. Alpha-masked sprites (buildings, units) occlude via the depth buffer with no sorting; a second blended/additive category (glows, shadows) is drawn after, depth-sorted, with bloom applying for free. Pure math (frame→UV, animation, iso grid, camera projection, picking) is split into headless-testable units; GL work lives in a `SpriteRenderer` composed into `ScenePanel` (mirroring `BloomRenderer`).

**Tech Stack:** C++17, Magnum (GL, Shaders::FlatGL3D, Trade), Corrade, ImGui, CMake/Ninja. Optional PNG loading via MagnumPlugins StbImageImporter.

## Global Constraints

- C++17; build is out-of-source (this plan uses `build-native`).
- C++ style: `.clang-format` + `.clang-tidy` are source of truth. Run `clang-format -i` on every modified C++ file before committing.
- **No `auto`** except iterators/unspellable STL types. **No `using`** directives/declarations — spell out `Magnum::`, `smg::`, `std::`.
- Namespace `smg` (or `smg::primitives`-style sub-namespaces). Class names PascalCase; methods/free functions snake_case; `ShXxxPr` = `std::shared_ptr<Xxx>` typedef.
- Laconic comments — the *why*, not the *what*.
- **No `Co-Authored-By` trailers.** Commit subjects topic-prefixed and laconic, e.g. `sprites: add frame-UV math`.
- Two test classes: `unit` (headless, no GL) and `gui` (needs a display). New pure-math tests go in the `unit_test_list`; sprite GL smoke tests go in `gui_test_list`.
- WASM: no `RGBA8`/MSAA on WebGL2 — gate texture format like existing `ScenePanel::ensure_fbo`; keep the importer behind a build flag.
- Public headers live in `include/`; internal-only GL helpers may live in `src/`.

**Build / test commands used throughout:**

```bash
# configure once (Debug, Ninja)
cmake -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Debug
# build everything
cmake --build build-native --parallel
# run headless unit tests
ctest --test-dir build-native -L unit --output-on-failure
# run a single test by name
ctest --test-dir build-native -R test_sprite_types --output-on-failure
```

---

### Task 1: Integrate `feature/scene-bloom` into the working branch

The sprite example/demo uses an additive glow over the bloom pass, and the spec assumes `SMG_WITH_BLOOM` exists on the working branch. Merge bloom first so the rest of the plan builds against it.

**Files:**
- No source edits authored here — this is a git merge that brings in `external/bloom/*`, the `SMG_WITH_BLOOM` plumbing in `ScenePanel`, and `test/test_scene_bloom.cpp`.

**Interfaces:**
- Produces: `SMG_WITH_BLOOM` CMake option + `bloom::BloomRenderer`; the `#ifdef SMG_WITH_BLOOM` pattern that `SMG_WITH_IMAGE_IMPORT` (Task 10) will mirror.

- [ ] **Step 1: Confirm starting state**

```bash
git status              # expect clean
git branch --show-current   # expect cleanup (or a feature branch off it)
git log --oneline -1
```

- [ ] **Step 2: Create the feature branch off the current branch**

```bash
git switch -c feature/iso-sprites
```

- [ ] **Step 3: Merge the bloom branch**

```bash
git merge --no-ff origin/feature/scene-bloom -m "sprites: merge scene-bloom for additive glow support"
```

Expected: clean merge (the bloom commits touch `external/bloom/`, `ScenePanel`, `examples/scene_panel.cpp`, `CMakeLists.txt`). **If there are conflicts in `ScenePanel.hh/.cpp` or `CMakeLists.txt`, STOP and resolve them by keeping both the cleanup changes and the bloom additions, then continue.** This step touches shared-branch history — surface the merge result to the human before proceeding.

- [ ] **Step 4: Build with bloom enabled to confirm integration**

```bash
cmake -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSMG_WITH_BLOOM=ON
cmake --build build-native --parallel
```

Expected: builds clean; `bloom`-related sources compile into `smg`.

- [ ] **Step 5: Run the existing test suite to confirm no regression**

```bash
ctest --test-dir build-native -L unit --output-on-failure
```

Expected: all existing unit tests PASS.

---

### Task 2: Sprite placement math — `SpriteTypes.hh`

Pure, headless value types and frame→UV math. No GL.

**Files:**
- Create: `include/SpriteTypes.hh`
- Test: `test/test_sprite_types.cpp`
- Modify: `test/CMakeLists.txt` (add to `unit_test_list`)

**Interfaces:**
- Produces:
  - `enum class smg::Anchor { Center, BottomCenter }`
  - `enum class smg::SpriteBlend { AlphaMask, Alpha, Additive }`
  - `struct smg::SpriteGrid { int cols; int rows; int count() const; Magnum::Range2D frame_uv(int index) const; }` — UV rect, top-left origin, `(0,0)`..`(1,1)`.
  - `Magnum::Vector2 smg::anchor_offset(Anchor a)` — local-quad shift; `Center`→`{0,0}`, `BottomCenter`→`{0, 0.5}`.

- [ ] **Step 1: Write the failing test**

`test/test_sprite_types.cpp`:

```cpp
#include "SpriteTypes.hh"
#include "test_util.hh"

int main() {
    const smg::SpriteGrid g{ 4, 2 }; // 4 cols, 2 rows => 8 frames
    CHECK(g.count() == 8);

    // frame 0 is the top-left cell
    const Magnum::Range2D f0 = g.frame_uv(0);
    CHECK(smgtest::approx(f0.min().x(), 0.0f));
    CHECK(smgtest::approx(f0.min().y(), 0.0f));
    CHECK(smgtest::approx(f0.max().x(), 0.25f));
    CHECK(smgtest::approx(f0.max().y(), 0.5f));

    // frame 5 = col 1, row 1 (row-major, top-left origin)
    const Magnum::Range2D f5 = g.frame_uv(5);
    CHECK(smgtest::approx(f5.min().x(), 0.25f));
    CHECK(smgtest::approx(f5.min().y(), 0.5f));
    CHECK(smgtest::approx(f5.max().x(), 0.5f));
    CHECK(smgtest::approx(f5.max().y(), 1.0f));

    // out-of-range clamps to frame 0 (defensive, no UB)
    const Magnum::Range2D fbad = g.frame_uv(999);
    CHECK(smgtest::approx(fbad.min().x(), 0.0f));

    // anchor offsets
    CHECK(smgtest::approx(smg::anchor_offset(smg::Anchor::Center).y(), 0.0f));
    CHECK(smgtest::approx(smg::anchor_offset(smg::Anchor::BottomCenter).y(), 0.5f));

    TEST_RETURN();
}
```

- [ ] **Step 2: Register the test and run it to confirm it fails to build**

In `test/CMakeLists.txt`, add `test_sprite_types.cpp` to `unit_test_list`:

```cmake
set(unit_test_list
    test_bounds.cpp
    test_camera.cpp
    test_mesh_bounds.cpp
    test_primitives_axes.cpp
    test_primitives_grid.cpp
    test_sprite_types.cpp
)
```

Run:

```bash
cmake --build build-native --parallel 2>&1 | tail -5
```

Expected: FAIL — `SpriteTypes.hh` not found.

- [ ] **Step 3: Write the implementation**

`include/SpriteTypes.hh`:

```cpp
// sprite value types: anchors, blend modes, sheet frame-UV math (headless)
#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Math/Vector2.h>

namespace smg {

enum class Anchor { Center, BottomCenter };
enum class SpriteBlend { AlphaMask, Alpha, Additive };

// regular grid of frames over a sheet; top-left origin, row-major
struct SpriteGrid {
    int cols{ 1 };
    int rows{ 1 };

    [[nodiscard]] int count() const { return cols * rows; }

    // normalized UV rect for a frame; out-of-range clamps to frame 0
    [[nodiscard]] Magnum::Range2D frame_uv(int index) const {
        const int n = count();
        const int i = (index < 0 || index >= n) ? 0 : index;
        const int col = i % cols;
        const int row = i / cols;
        const float u = 1.0f / float(cols);
        const float v = 1.0f / float(rows);
        return Magnum::Range2D{ { float(col) * u, float(row) * v }, { float(col + 1) * u, float(row + 1) * v } };
    }
};

// local-quad shift so the chosen anchor lands on the sprite's world position
[[nodiscard]] inline Magnum::Vector2 anchor_offset(Anchor a) {
    return a == Anchor::BottomCenter ? Magnum::Vector2{ 0.0f, 0.5f } : Magnum::Vector2{ 0.0f, 0.0f };
}

} // namespace smg
```

- [ ] **Step 4: Build and run the test**

```bash
cmake --build build-native --parallel && ctest --test-dir build-native -R test_sprite_types --output-on-failure
```

Expected: PASS.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i include/SpriteTypes.hh test/test_sprite_types.cpp
git add include/SpriteTypes.hh test/test_sprite_types.cpp test/CMakeLists.txt
git commit -m "sprites: add frame-UV + anchor math (SpriteTypes)"
```

---

### Task 3: Animation helpers — `SpriteClip` + `dir_row`

**Files:**
- Modify: `include/SpriteTypes.hh` (append)
- Test: `test/test_sprite_anim.cpp`
- Modify: `test/CMakeLists.txt` (add to `unit_test_list`)

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces:
  - `struct smg::SpriteClip { int first; int last; float fps; int frame_at(float t) const; }` — loops over `[first, last]`.
  - `int smg::dir_row(float facing_deg, int num_dirs)` — facing angle → row `0..num_dirs-1`, wrapping.

- [ ] **Step 1: Write the failing test**

`test/test_sprite_anim.cpp`:

```cpp
#include "SpriteTypes.hh"
#include "test_util.hh"

int main() {
    const smg::SpriteClip walk{ 0, 7, 10.0f }; // 8 frames at 10 fps

    CHECK(walk.frame_at(0.0f) == 0);
    CHECK(walk.frame_at(0.05f) == 0);  // 0.5 frames -> floor 0
    CHECK(walk.frame_at(0.15f) == 1);  // 1.5 frames -> 1
    CHECK(walk.frame_at(0.8f) == 0);   // 8 frames elapsed -> wraps to 0
    CHECK(walk.frame_at(0.85f) == 0);  // wraps within range

    // clip starting mid-sheet keeps the offset
    const smg::SpriteClip attack{ 16, 19, 4.0f };
    CHECK(attack.frame_at(0.0f) == 16);
    CHECK(attack.frame_at(0.30f) == 17); // 1.2 frames -> 17
    CHECK(attack.frame_at(1.0f) == 16);  // 4 frames -> wrap

    // fps <= 0 holds the first frame
    const smg::SpriteClip still{ 5, 9, 0.0f };
    CHECK(still.frame_at(123.0f) == 5);

    // 8-direction row selection, wrapping
    CHECK(smg::dir_row(0.0f, 8) == 0);
    CHECK(smg::dir_row(45.0f, 8) == 1);
    CHECK(smg::dir_row(360.0f, 8) == 0);
    CHECK(smg::dir_row(-45.0f, 8) == 7);

    TEST_RETURN();
}
```

- [ ] **Step 2: Register and confirm failure**

Add `test_sprite_anim.cpp` to `unit_test_list` in `test/CMakeLists.txt`, then:

```bash
cmake --build build-native --parallel 2>&1 | tail -5
```

Expected: FAIL — `SpriteClip`/`dir_row` undeclared.

- [ ] **Step 3: Append the implementation to `include/SpriteTypes.hh`**

Add these includes near the top:

```cpp
#include <cmath>
```

Add inside `namespace smg`, after `anchor_offset`:

```cpp
// frame-range animation clip; advances by wall-clock seconds, loops
struct SpriteClip {
    int first{ 0 };
    int last{ 0 };
    float fps{ 0.0f };

    [[nodiscard]] int frame_at(float t) const {
        const int span = last - first + 1;
        if(span <= 1 || fps <= 0.0f) return first;
        const int step = int(std::floor(t * fps));
        const int wrapped = ((step % span) + span) % span;
        return first + wrapped;
    }
};

// facing angle (degrees, CCW) -> sheet row in [0, num_dirs); wraps
[[nodiscard]] inline int dir_row(float facing_deg, int num_dirs) {
    if(num_dirs <= 0) return 0;
    const float step = 360.0f / float(num_dirs);
    const int r = int(std::lround(facing_deg / step));
    return ((r % num_dirs) + num_dirs) % num_dirs;
}
```

- [ ] **Step 4: Build and run**

```bash
cmake --build build-native --parallel && ctest --test-dir build-native -R test_sprite_anim --output-on-failure
```

Expected: PASS.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i include/SpriteTypes.hh test/test_sprite_anim.cpp
git add include/SpriteTypes.hh test/test_sprite_anim.cpp test/CMakeLists.txt
git commit -m "sprites: add SpriteClip + dir_row animation helpers"
```

---

### Task 4: `IsoGrid` — tile ↔ world mapping

**Files:**
- Create: `include/IsoGrid.hh`, `src/IsoGrid.cpp`
- Test: `test/test_iso_grid.cpp`
- Modify: `CMakeLists.txt` (`smg_sources`), `test/CMakeLists.txt` (`unit_test_list`)

**Interfaces:**
- Consumes: `smg::UpAxis` from `Camera.hh`.
- Produces: `struct smg::IsoGrid { float tile_size; UpAxis up; Magnum::Vector3 to_world(const Magnum::Vector2i& cell) const; Magnum::Vector2i to_cell(const Magnum::Vector3& world) const; }`.

- [ ] **Step 1: Write the failing test**

`test/test_iso_grid.cpp`:

```cpp
#include "IsoGrid.hh"
#include "test_util.hh"

int main() {
    // Y-up: cells map onto the XZ ground plane
    const smg::IsoGrid g{ 2.0f, smg::UpAxis::Y };
    const Magnum::Vector3 w = g.to_world(Magnum::Vector2i{ 3, -1 });
    CHECK(smgtest::approx(w.x(), 6.0f));
    CHECK(smgtest::approx(w.y(), 0.0f));
    CHECK(smgtest::approx(w.z(), -2.0f));

    // round-trip through the nearest cell
    const Magnum::Vector2i c = g.to_cell(Magnum::Vector3{ 5.9f, 0.0f, -2.1f });
    CHECK(c.x() == 3);
    CHECK(c.y() == -1);

    // Z-up: cells map onto the XY ground plane
    const smg::IsoGrid gz{ 1.0f, smg::UpAxis::Z };
    const Magnum::Vector3 wz = gz.to_world(Magnum::Vector2i{ 2, 4 });
    CHECK(smgtest::approx(wz.x(), 2.0f));
    CHECK(smgtest::approx(wz.y(), 4.0f));
    CHECK(smgtest::approx(wz.z(), 0.0f));

    TEST_RETURN();
}
```

- [ ] **Step 2: Register and confirm failure**

Add `src/IsoGrid.cpp` to `smg_sources` in `CMakeLists.txt`, and `test_iso_grid.cpp` to `unit_test_list`. Then:

```bash
cmake -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-native --parallel 2>&1 | tail -5
```

Expected: FAIL — `IsoGrid.hh` not found.

- [ ] **Step 3: Write the header**

`include/IsoGrid.hh`:

```cpp
// isometric tile <-> world mapping on the ground plane (headless)
#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>

#include "Camera.hh" // UpAxis

namespace smg {

struct IsoGrid {
    float tile_size{ 1.0f };
    UpAxis up{ UpAxis::Y };

    [[nodiscard]] Magnum::Vector3 to_world(const Magnum::Vector2i& cell) const;
    [[nodiscard]] Magnum::Vector2i to_cell(const Magnum::Vector3& world) const;
};

} // namespace smg
```

- [ ] **Step 4: Write the implementation**

`src/IsoGrid.cpp`:

```cpp
#include "IsoGrid.hh"

#include <cmath>

namespace smg {

Magnum::Vector3 IsoGrid::to_world(const Magnum::Vector2i& cell) const {
    const float a = float(cell.x()) * tile_size;
    const float b = float(cell.y()) * tile_size;
    // ground plane is the pair of axes perpendicular to the up axis
    return up == UpAxis::Y ? Magnum::Vector3{ a, 0.0f, b } : Magnum::Vector3{ a, b, 0.0f };
}

Magnum::Vector2i IsoGrid::to_cell(const Magnum::Vector3& world) const {
    const float a = up == UpAxis::Y ? world.x() : world.x();
    const float b = up == UpAxis::Y ? world.z() : world.y();
    return Magnum::Vector2i{ int(std::lround(a / tile_size)), int(std::lround(b / tile_size)) };
}

} // namespace smg
```

- [ ] **Step 5: Build, run, format, commit**

```bash
cmake --build build-native --parallel && ctest --test-dir build-native -R test_iso_grid --output-on-failure
clang-format -i include/IsoGrid.hh src/IsoGrid.cpp test/test_iso_grid.cpp
git add include/IsoGrid.hh src/IsoGrid.cpp test/test_iso_grid.cpp CMakeLists.txt test/CMakeLists.txt
git commit -m "sprites: add IsoGrid tile<->world mapping"
```

Expected: PASS.

---

### Task 5: Camera orthographic mode + `iso()` preset

**Files:**
- Modify: `include/Camera.hh`, `src/Camera.cpp`
- Modify: `test/test_camera.cpp` (extend)

**Interfaces:**
- Consumes: existing `Camera` (`projection`, `view`, `fit`, `_distance`, `_fov_deg`, `_yaw`, `_pitch`).
- Produces:
  - `enum class smg::Camera::Projection { Perspective, Orthographic }`
  - `void Camera::set_projection(Projection)`, `Projection Camera::projection_mode() const`
  - `void Camera::iso()` — orthographic, yaw 45°, pitch ≈ 26.57° (2:1 dimetric)
  - `UpAxis Camera::up_axis() const` (getter, needed by Task 6/9)
  - `projection(aspect)` branches on the mode.

- [ ] **Step 1: Write the failing test (append to `test/test_camera.cpp`, before `TEST_RETURN()`)**

```cpp
    // orthographic mode: the pivot still projects to the NDC origin
    cam.set_projection(Camera::Projection::Orthographic);
    CHECK(cam.projection_mode() == Camera::Projection::Orthographic);
    const Magnum::Vector4 oclip = cam.projection(1.0f) * cam.view() * Magnum::Vector4{ cam.pivot(), 1.0f };
    const Magnum::Vector3 ondc = oclip.xyz() / oclip.w();
    CHECK(smgtest::approx(ondc.x(), 0.0f));
    CHECK(smgtest::approx(ondc.y(), 0.0f));

    // ortho projection has no perspective foreshortening: w == 1
    CHECK(smgtest::approx(oclip.w(), 1.0f));

    // iso preset selects orthographic and the 2:1 dimetric pitch
    Camera iso;
    iso.iso();
    CHECK(iso.projection_mode() == Camera::Projection::Orthographic);
    // tan(pitch) ~= 0.5 for 2:1 dimetric; verify via eye elevation vs ground radius
    const Magnum::Vector3 d = (iso.eye() - iso.pivot()).normalized();
    CHECK(smgtest::approx(d.y(), 0.4472f, 1e-2f)); // sin(atan(0.5)) ~= 0.447
```

Add `#include <Magnum/Math/Functions.h>` to the test if not present (it uses only basic ops, so likely unnecessary).

- [ ] **Step 2: Build to confirm failure**

```bash
cmake --build build-native --parallel 2>&1 | tail -5
```

Expected: FAIL — `Projection`, `set_projection`, `projection_mode`, `iso` undeclared.

- [ ] **Step 3: Edit `include/Camera.hh`**

Add inside `class Camera` public section, near the projection method:

```cpp
    enum class Projection { Perspective, Orthographic };

    void set_projection(Projection p) { _projection = p; }
    [[nodiscard]] Projection projection_mode() const { return _projection; }

    void iso(); // orthographic + 2:1 dimetric preset
    [[nodiscard]] UpAxis up_axis() const { return _up; }
```

Add to the private members:

```cpp
    Projection _projection{ Projection::Perspective };
```

- [ ] **Step 4: Edit `src/Camera.cpp`**

Replace `Camera::projection` with a branching version:

```cpp
Magnum::Matrix4 Camera::projection(float aspect) const {
    if(_projection == Projection::Orthographic) {
        // match the perspective framing at pivot depth so fit()/zoom() stay meaningful
        const float halfFov = float(Magnum::Rad{ Magnum::Deg{ _fov_deg } } * 0.5f);
        const float height = 2.0f * _distance * std::tan(halfFov);
        return Magnum::Matrix4::orthographicProjection(Magnum::Vector2{ height * aspect, height }, _near, _far);
    }
    return Magnum::Matrix4::perspectiveProjection(Magnum::Deg{ _fov_deg }, aspect, _near, _far);
}
```

Add the preset (the pitch is `atan(0.5)` for the 2:1 dimetric look; yaw 45°):

```cpp
void Camera::iso() {
    _projection = Projection::Orthographic;
    _yaw = float(Magnum::Constants::piHalf()) * 0.5f; // 45 deg
    _pitch = std::atan(0.5f);                         // ~26.57 deg, 2:1 dimetric
}
```

Add the needed include at the top of `src/Camera.cpp` if missing:

```cpp
#include <Magnum/Math/Constants.h>
```

- [ ] **Step 5: Build, run, format, commit**

```bash
cmake --build build-native --parallel && ctest --test-dir build-native -R test_camera --output-on-failure
clang-format -i include/Camera.hh src/Camera.cpp test/test_camera.cpp
git add include/Camera.hh src/Camera.cpp test/test_camera.cpp
git commit -m "sprites: add orthographic camera mode + iso preset"
```

Expected: PASS.

---

### Task 6: Camera picking — `unproject` + `ray_ground`

**Files:**
- Modify: `include/Camera.hh`, `src/Camera.cpp`
- Modify: `include/IsoGrid.hh`, `src/IsoGrid.cpp` (add `ray_ground` free helper)
- Test: `test/test_camera_pick.cpp`
- Modify: `test/CMakeLists.txt` (`unit_test_list`)

**Interfaces:**
- Consumes: `Camera::view`, `Camera::projection`, `Camera::up_axis` (Task 5); `IsoGrid` (Task 4).
- Produces:
  - `struct smg::Camera::Ray { Magnum::Vector3 origin; Magnum::Vector3 direction; }`
  - `Ray Camera::unproject(const Magnum::Vector2& screen_px, const Magnum::Vector2& viewport_px) const` — screen pixels (origin top-left, y-down) → world ray.
  - `std::optional<Magnum::Vector3> smg::ray_ground(const Camera::Ray& ray, UpAxis up)` — intersect ray with the ground plane (y=0 for Y-up, z=0 for Z-up).

- [ ] **Step 1: Write the failing test**

`test/test_camera_pick.cpp`:

```cpp
#include "Camera.hh"
#include "IsoGrid.hh"
#include "SceneTypes.hh"
#include "test_util.hh"

#include <optional>

int main() {
    Camera cam;
    Bounds box;
    box.expand(Magnum::Vector3{ -1.0f, -1.0f, -1.0f });
    box.expand(Magnum::Vector3{ 1.0f, 1.0f, 1.0f });
    cam.fit(box);
    cam.iso();

    const Magnum::Vector2 vp{ 800.0f, 600.0f };

    // the screen centre ray should hit the ground near the pivot's ground projection
    const Camera::Ray center = cam.unproject(vp * 0.5f, vp);
    const std::optional<Magnum::Vector3> hit = smg::ray_ground(center, cam.up_axis());
    CHECK(hit.has_value());
    CHECK(smgtest::approx(hit->y(), 0.0f));        // on the ground plane
    CHECK((hit->xz() - cam.pivot().xz()).length() < 1.0f); // near pivot column

    // the ray direction is unit length and points downward (camera looks down at iso)
    CHECK(smgtest::approx(center.direction.length(), 1.0f, 1e-3f));
    CHECK(center.direction.y() < 0.0f);

    TEST_RETURN();
}
```

- [ ] **Step 2: Register and confirm failure**

Add `test_camera_pick.cpp` to `unit_test_list`, then build:

```bash
cmake -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-native --parallel 2>&1 | tail -5
```

Expected: FAIL — `Camera::Ray` / `unproject` / `ray_ground` undeclared.

- [ ] **Step 3: Edit `include/Camera.hh`**

Add inside `class Camera` public section:

```cpp
    struct Ray {
        Magnum::Vector3 origin;
        Magnum::Vector3 direction;
    };

    // screen pixels (origin top-left, y-down) -> world-space ray; works for both projection modes
    [[nodiscard]] Ray unproject(const Magnum::Vector2& screen_px, const Magnum::Vector2& viewport_px) const;
```

- [ ] **Step 4: Edit `src/Camera.cpp`**

Add `#include <Magnum/Math/Vector4.h>` at the top if missing, then add:

```cpp
Camera::Ray Camera::unproject(const Magnum::Vector2& screen_px, const Magnum::Vector2& viewport_px) const {
    const float aspect = viewport_px.y() > 0.0f ? viewport_px.x() / viewport_px.y() : 1.0f;
    // pixel -> NDC; flip Y because screen origin is top-left
    const float ndcx = 2.0f * screen_px.x() / viewport_px.x() - 1.0f;
    const float ndcy = 1.0f - 2.0f * screen_px.y() / viewport_px.y();
    const Magnum::Matrix4 invVP = (projection(aspect) * view()).inverted();

    const Magnum::Vector4 n = invVP * Magnum::Vector4{ ndcx, ndcy, -1.0f, 1.0f };
    const Magnum::Vector4 f = invVP * Magnum::Vector4{ ndcx, ndcy, 1.0f, 1.0f };
    const Magnum::Vector3 np = n.xyz() / n.w();
    const Magnum::Vector3 fp = f.xyz() / f.w();
    return Ray{ np, (fp - np).normalized() };
}
```

- [ ] **Step 5: Add `ray_ground` to `include/IsoGrid.hh`**

Add includes and declaration:

```cpp
#include <optional>
```

```cpp
// intersect a world ray with the ground plane (y=0 for Y-up, z=0 for Z-up)
[[nodiscard]] std::optional<Magnum::Vector3> ray_ground(const Camera::Ray& ray, UpAxis up);
```

- [ ] **Step 6: Implement `ray_ground` in `src/IsoGrid.cpp`**

```cpp
#include <cmath>

std::optional<Magnum::Vector3> ray_ground(const Camera::Ray& ray, UpAxis up) {
    const float dir_n = up == UpAxis::Y ? ray.direction.y() : ray.direction.z();
    const float org_n = up == UpAxis::Y ? ray.origin.y() : ray.origin.z();
    if(std::fabs(dir_n) < 1e-6f) return {}; // parallel to the ground
    const float t = -org_n / dir_n;
    if(t < 0.0f) return {}; // behind the ray origin
    return ray.origin + ray.direction * t;
}
```

- [ ] **Step 7: Build, run, format, commit**

```bash
cmake --build build-native --parallel && ctest --test-dir build-native -R test_camera_pick --output-on-failure
clang-format -i include/Camera.hh src/Camera.cpp include/IsoGrid.hh src/IsoGrid.cpp test/test_camera_pick.cpp
git add include/Camera.hh src/Camera.cpp include/IsoGrid.hh src/IsoGrid.cpp test/test_camera_pick.cpp test/CMakeLists.txt
git commit -m "sprites: add camera unproject + ground-plane picking"
```

Expected: PASS.

---

### Task 7: `SpriteSheet` — GL texture + grid wrapper

GL-dependent: verified by a `gui` smoke test that opens a context.

**Files:**
- Create: `include/SpriteSheet.hh`, `src/SpriteSheet.cpp`
- Test: `test/test_sprite_sheet.cpp` (gui)
- Modify: `CMakeLists.txt` (`smg_sources`), `test/CMakeLists.txt` (`gui_test_list`)

**Interfaces:**
- Consumes: `smg::SpriteGrid` (Task 2).
- Produces:
  - `typedef std::shared_ptr<class smg::SpriteSheet> smg::ShSpriteSheetPr;`
  - `static ShSpriteSheetPr SpriteSheet::from_texture(Magnum::GL::Texture2D&& tex, int cols, int rows);`
  - `static ShSpriteSheetPr SpriteSheet::from_pixels(Corrade::Containers::ArrayView<const char> pixels, const Magnum::Vector2i& size, Magnum::PixelFormat format, int cols, int rows);`
  - `const SpriteGrid& SpriteSheet::grid() const;`
  - `Magnum::GL::Texture2D& SpriteSheet::texture();`
  - `Magnum::Range2D SpriteSheet::frame_uv(int index) const;` (delegates to grid)

- [ ] **Step 1: Write the failing smoke test**

`test/test_sprite_sheet.cpp`:

```cpp
// gui smoke test: build a sheet from raw pixels under a real GL context
#include "GuiBase.hh"
#include "SpriteSheet.hh"

#include <Corrade/Containers/ArrayView.h>
#include <array>
#include <cstdio>

using namespace smg;

class SheetTest: public GuiBase {
public:
    explicit SheetTest(const Arguments& arguments) : GuiBase(arguments) {
        // 2x2 RGBA checker
        std::array<unsigned char, 2 * 2 * 4> px{};
        px.fill(255);
        ShSpriteSheetPr sheet = SpriteSheet::from_pixels(
            Corrade::Containers::arrayView(reinterpret_cast<const char*>(px.data()), px.size()),
            Magnum::Vector2i{ 2, 2 }, Magnum::PixelFormat::RGBA8Unorm, /*cols*/2, /*rows*/1);
        if(sheet->grid().count() != 2) { std::printf("FAIL sheet count\n"); _rc = 1; }
        if(sheet->texture().id() == 0) { std::printf("FAIL texture id\n"); _rc = 1; }
        exit(_rc); // smoke test: construct, validate, exit
    }

private:
    int _rc{ 0 };
};

MAGNUM_APPLICATION_MAIN(SheetTest)
```

Note: matches the existing `gui` smoke-test pattern (construct under a context, exit). It is excluded from `ctest -L unit`.

- [ ] **Step 2: Register and confirm failure**

Add `src/SpriteSheet.cpp` to `smg_sources`; add `test_sprite_sheet.cpp` to `gui_test_list`. Build:

```bash
cmake -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-native --parallel 2>&1 | tail -5
```

Expected: FAIL — `SpriteSheet.hh` not found.

- [ ] **Step 3: Write the header**

`include/SpriteSheet.hh`:

```cpp
// sprite sheet: a GL texture plus a frame grid over it
#pragma once

#include <memory>

#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/PixelFormat.h>

#include "SpriteTypes.hh"

namespace smg {

typedef std::shared_ptr<class SpriteSheet> ShSpriteSheetPr;

class SpriteSheet {
public:
    SpriteSheet(Magnum::GL::Texture2D&& tex, const SpriteGrid& grid);

    static ShSpriteSheetPr from_texture(Magnum::GL::Texture2D&& tex, int cols, int rows);
    static ShSpriteSheetPr from_pixels(Corrade::Containers::ArrayView<const char> pixels, const Magnum::Vector2i& size,
        Magnum::PixelFormat format, int cols, int rows);

#ifdef SMG_WITH_IMAGE_IMPORT
    static ShSpriteSheetPr load(const char* path, int cols, int rows);
#endif

    [[nodiscard]] const SpriteGrid& grid() const { return _grid; }
    [[nodiscard]] Magnum::GL::Texture2D& texture() { return _texture; }
    [[nodiscard]] Magnum::Range2D frame_uv(int index) const { return _grid.frame_uv(index); }

private:
    Magnum::GL::Texture2D _texture;
    SpriteGrid _grid;
};

} // namespace smg
```

- [ ] **Step 4: Write the implementation**

`src/SpriteSheet.cpp` (the `load` path is added in Task 10):

```cpp
#include "SpriteSheet.hh"

#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImageView.h>

namespace smg {

SpriteSheet::SpriteSheet(Magnum::GL::Texture2D&& tex, const SpriteGrid& grid) : _texture{ std::move(tex) }, _grid{ grid } {}

ShSpriteSheetPr SpriteSheet::from_texture(Magnum::GL::Texture2D&& tex, int cols, int rows) {
    return std::make_shared<SpriteSheet>(std::move(tex), SpriteGrid{ cols, rows });
}

ShSpriteSheetPr SpriteSheet::from_pixels(Corrade::Containers::ArrayView<const char> pixels, const Magnum::Vector2i& size,
    Magnum::PixelFormat format, int cols, int rows) {
    Magnum::GL::Texture2D tex;
#ifdef SMG_PLATFORM_WASM
    const Magnum::GL::TextureFormat tf = Magnum::GL::TextureFormat::RGBA;
#else
    const Magnum::GL::TextureFormat tf = Magnum::GL::TextureFormat::RGBA8;
#endif
    tex.setMinificationFilter(Magnum::GL::SamplerFilter::Nearest) // crisp prerendered sprites
        .setMagnificationFilter(Magnum::GL::SamplerFilter::Nearest)
        .setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
        .setStorage(1, tf, size)
        .setSubImage(0, {}, Magnum::ImageView2D{ format, size, pixels });
    return from_texture(std::move(tex), cols, rows);
}

} // namespace smg
```

- [ ] **Step 5: Build and run the smoke test (needs a display)**

```bash
cmake --build build-native --parallel && ctest --test-dir build-native -R test_sprite_sheet --output-on-failure
```

Expected: PASS (exit 0). If running headless/CI without a display, build success alone is the gate; note that explicitly.

- [ ] **Step 6: Format and commit**

```bash
clang-format -i include/SpriteSheet.hh src/SpriteSheet.cpp test/test_sprite_sheet.cpp
git add include/SpriteSheet.hh src/SpriteSheet.cpp test/test_sprite_sheet.cpp CMakeLists.txt test/CMakeLists.txt
git commit -m "sprites: add SpriteSheet (texture + frame grid)"
```

---

### Task 8: `SpriteRenderer` — the GL sprite pass

Internal GL helper (header in `src/`). Verified by its own `gui` smoke test.

**Files:**
- Create: `src/SpriteRenderer.hh`, `src/SpriteRenderer.cpp`
- Modify: `include/ScenePanel.hh` (define `Sprite` instance struct + `SpriteParams`, used by Task 9; place them in `SpriteTypes.hh` instead — see below)
- Modify: `include/SpriteTypes.hh` (append `SpriteParams`)
- Test: `test/test_sprite_renderer.cpp` (gui)
- Modify: `CMakeLists.txt` (`smg_sources`), `test/CMakeLists.txt` (`gui_test_list`)

**Interfaces:**
- Consumes: `SpriteSheet` (Task 7), `SpriteTypes` (Tasks 2–3), `Camera` view/proj matrices.
- Produces:
  - `struct smg::SpriteParams { Magnum::Vector2 size{1.0f}; Anchor anchor{Anchor::BottomCenter}; Magnum::Color4 tint{1.0f}; SpriteBlend blend{SpriteBlend::AlphaMask}; bool visible{true}; };`
  - `struct smg::Sprite { ShSpriteSheetPr sheet; int frame; Magnum::Vector3 position; SpriteParams params; };`
  - `class smg::SpriteRenderer { void draw(const std::vector<Sprite>& sprites, const Magnum::Matrix4& view, const Magnum::Matrix4& projection); };`

- [ ] **Step 1: Append `SpriteParams` + `Sprite` to `include/SpriteTypes.hh`**

Add includes:

```cpp
#include <memory>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>
```

Add inside `namespace smg` (forward-declare the sheet to avoid a heavy include):

```cpp
class SpriteSheet;
typedef std::shared_ptr<SpriteSheet> ShSpriteSheetPr;

struct SpriteParams {
    Magnum::Vector2 size{ 1.0f, 1.0f }; // world units (W, H)
    Anchor anchor{ Anchor::BottomCenter };
    Magnum::Color4 tint{ 1.0f };
    SpriteBlend blend{ SpriteBlend::AlphaMask };
    bool visible{ true };
};

struct Sprite {
    ShSpriteSheetPr sheet;
    int frame{ 0 };
    Magnum::Vector3 position{ 0.0f };
    SpriteParams params;
};
```

> Note: `SpriteSheet.hh` already defines `ShSpriteSheetPr`. Guard against a redefinition by removing the typedef from `SpriteSheet.hh` and relying on the one in `SpriteTypes.hh` (which `SpriteSheet.hh` already includes). Make that edit now: delete the `typedef std::shared_ptr<class SpriteSheet> ShSpriteSheetPr;` line in `include/SpriteSheet.hh`.

- [ ] **Step 2: Write the failing smoke test**

`test/test_sprite_renderer.cpp`:

```cpp
// gui smoke test: build a renderer + sheet and issue one draw to the default framebuffer
#include "Camera.hh"
#include "GuiBase.hh"
#include "SceneTypes.hh"
#include "SpriteRenderer.hh"
#include "SpriteSheet.hh"

#include <array>
#include <cstdio>
#include <vector>

using namespace smg;

class RendererTest: public GuiBase {
public:
    explicit RendererTest(const Arguments& arguments) : GuiBase(arguments) {
        std::array<unsigned char, 4> px{ 255, 255, 255, 255 };
        ShSpriteSheetPr sheet = SpriteSheet::from_pixels(
            Corrade::Containers::arrayView(reinterpret_cast<const char*>(px.data()), px.size()),
            Magnum::Vector2i{ 1, 1 }, Magnum::PixelFormat::RGBA8Unorm, 1, 1);

        std::vector<Sprite> sprites;
        sprites.push_back(Sprite{ sheet, 0, Magnum::Vector3{ 0.0f }, SpriteParams{} });

        Camera cam;
        cam.iso();
        SpriteRenderer renderer;
        renderer.draw(sprites, cam.view(), cam.projection(1.0f)); // must not crash
        std::printf("OK sprite renderer draw\n");
        exit(0);
    }
};

MAGNUM_APPLICATION_MAIN(RendererTest)
```

- [ ] **Step 3: Register and confirm failure**

Add `src/SpriteRenderer.cpp` to `smg_sources`; add `test_sprite_renderer.cpp` to `gui_test_list`. Build → FAIL (`SpriteRenderer.hh` not found).

- [ ] **Step 4: Write `src/SpriteRenderer.hh`**

```cpp
// internal GL sprite pass: textured camera-facing billboards (alpha-mask + blended)
#pragma once

#include <vector>

#include <Magnum/GL/Mesh.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Shaders/FlatGL.h>

#include "SpriteTypes.hh"

namespace smg {

class SpriteRenderer {
public:
    SpriteRenderer();

    // draw all visible sprites; expects DepthTest already enabled by the caller
    void draw(const std::vector<Sprite>& sprites, const Magnum::Matrix4& view, const Magnum::Matrix4& projection);

private:
    void draw_one(Magnum::Shaders::FlatGL3D& shader, const Sprite& s, const Magnum::Matrix4& view, const Magnum::Matrix4& projection,
        const Magnum::Vector3& right, const Magnum::Vector3& up, const Magnum::Vector3& fwd);

    Magnum::GL::Mesh _quad{ Magnum::NoCreate };
    Magnum::Shaders::FlatGL3D _alphaMask{ Magnum::NoCreate };
    Magnum::Shaders::FlatGL3D _blended{ Magnum::NoCreate };
};

} // namespace smg
```

- [ ] **Step 5: Write `src/SpriteRenderer.cpp`**

```cpp
#include "SpriteRenderer.hh"

#include <algorithm>

#include <Corrade/Containers/Array.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/Shaders/GenericGL.h>

#include "SpriteSheet.hh"

namespace smg {

namespace {
struct QuadVertex {
    Magnum::Vector3 position;
    Magnum::Vector2 uv;
};
} // namespace

SpriteRenderer::SpriteRenderer() {
    // unit quad in the local XY plane; UV origin top-left (v grows downward)
    const QuadVertex verts[]{
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } }, // bottom-left
        { { 0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } },  // bottom-right
        { { 0.5f, 0.5f, 0.0f }, { 1.0f, 0.0f } },   // top-right
        { { -0.5f, 0.5f, 0.0f }, { 0.0f, 0.0f } },  // top-left
    };
    const Magnum::UnsignedInt indices[]{ 0, 1, 2, 0, 2, 3 };

    Magnum::GL::Buffer vb;
    vb.setData(verts);
    Magnum::GL::Buffer ib;
    ib.setData(indices);

    _quad = Magnum::GL::Mesh{};
    _quad.setPrimitive(Magnum::MeshPrimitive::Triangles)
        .addVertexBuffer(std::move(vb), 0, Magnum::Shaders::GenericGL3D::Position{}, Magnum::Shaders::GenericGL3D::TextureCoordinates{})
        .setCount(6)
        .setIndexBuffer(std::move(ib), 0, Magnum::MeshIndexType::UnsignedInt);

    _alphaMask = Magnum::Shaders::FlatGL3D{ Magnum::Shaders::FlatGL3D::Configuration{}.setFlags(
        Magnum::Shaders::FlatGL3D::Flag::Textured | Magnum::Shaders::FlatGL3D::Flag::TextureTransformation |
        Magnum::Shaders::FlatGL3D::Flag::AlphaMask) };
    _blended = Magnum::Shaders::FlatGL3D{ Magnum::Shaders::FlatGL3D::Configuration{}.setFlags(
        Magnum::Shaders::FlatGL3D::Flag::Textured | Magnum::Shaders::FlatGL3D::Flag::TextureTransformation) };
}

void SpriteRenderer::draw_one(Magnum::Shaders::FlatGL3D& shader, const Sprite& s, const Magnum::Matrix4& view,
    const Magnum::Matrix4& projection, const Magnum::Vector3& right, const Magnum::Vector3& up, const Magnum::Vector3& fwd) {
    if(!s.sheet) return;
    const Magnum::Vector2 off = anchor_offset(s.params.anchor);
    const Magnum::Vector3 center =
        s.position + right * (off.x() * s.params.size.x()) + up * (off.y() * s.params.size.y());
    const Magnum::Matrix3 basis{ right * s.params.size.x(), up * s.params.size.y(), fwd };
    const Magnum::Matrix4 model = Magnum::Matrix4::from(basis, center);

    const Magnum::Range2D uv = s.sheet->frame_uv(s.frame);
    const Magnum::Matrix3 texMat = Magnum::Matrix3::translation(uv.min()) * Magnum::Matrix3::scaling(uv.size());

    shader.setTransformationProjectionMatrix(projection * view * model)
        .setTextureMatrix(texMat)
        .setColor(s.params.tint)
        .bindTexture(s.sheet->texture());
    shader.draw(_quad);
}

void SpriteRenderer::draw(const std::vector<Sprite>& sprites, const Magnum::Matrix4& view, const Magnum::Matrix4& projection) {
    const Magnum::Matrix4 camWorld = view.invertedRigid();
    const Magnum::Vector3 right = camWorld[0].xyz().normalized();
    const Magnum::Vector3 up = camWorld[1].xyz().normalized();
    const Magnum::Vector3 fwd = camWorld[2].xyz().normalized();

    // pass 1: alpha-masked sprites, depth write on, no sorting needed
    _alphaMask.setAlphaMask(0.5f);
    for(const Sprite& s : sprites) {
        if(!s.params.visible || s.params.blend != SpriteBlend::AlphaMask) continue;
        draw_one(_alphaMask, s, view, projection, right, up, fwd);
    }

    // pass 2: blended/additive sprites, back-to-front, depth test on but no depth write
    std::vector<const Sprite*> blended;
    for(const Sprite& s : sprites) {
        if(s.params.visible && s.params.blend != SpriteBlend::AlphaMask) blended.push_back(&s);
    }
    if(blended.empty()) return;

    std::sort(blended.begin(), blended.end(), [&view](const Sprite* a, const Sprite* b) {
        // view-space z: more negative = farther; draw farthest first
        const float za = (view * Magnum::Vector4{ a->position, 1.0f }).z();
        const float zb = (view * Magnum::Vector4{ b->position, 1.0f }).z();
        return za < zb;
    });

    Magnum::GL::Renderer::enable(Magnum::GL::Renderer::Feature::Blending);
    Magnum::GL::Renderer::setDepthMask(false);
    for(const Sprite* s : blended) {
        if(s->params.blend == SpriteBlend::Additive)
            Magnum::GL::Renderer::setBlendFunction(Magnum::GL::Renderer::BlendFunction::SourceAlpha, Magnum::GL::Renderer::BlendFunction::One);
        else
            Magnum::GL::Renderer::setBlendFunction(
                Magnum::GL::Renderer::BlendFunction::SourceAlpha, Magnum::GL::Renderer::BlendFunction::OneMinusSourceAlpha);
        draw_one(_blended, *s, view, projection, right, up, fwd);
    }
    Magnum::GL::Renderer::setDepthMask(true);
    Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::Blending);
}

} // namespace smg
```

- [ ] **Step 6: Build and run the smoke test**

```bash
cmake --build build-native --parallel && ctest --test-dir build-native -R test_sprite_renderer --output-on-failure
```

Expected: PASS (prints `OK sprite renderer draw`, exit 0). Headless without a display → build success is the gate; note it.

- [ ] **Step 7: Format and commit**

```bash
clang-format -i src/SpriteRenderer.hh src/SpriteRenderer.cpp include/SpriteTypes.hh include/SpriteSheet.hh test/test_sprite_renderer.cpp
git add src/SpriteRenderer.hh src/SpriteRenderer.cpp include/SpriteTypes.hh include/SpriteSheet.hh test/test_sprite_renderer.cpp CMakeLists.txt test/CMakeLists.txt
git commit -m "sprites: add SpriteRenderer billboard pass"
```

---

### Task 9: `ScenePanel` integration + example + umbrella header

**Files:**
- Modify: `include/ScenePanel.hh`, `src/ScenePanel.cpp`
- Modify: `cmake/smg.hh.in` (umbrella export)
- Create: `examples/sprite_panel.cpp`
- Modify: `examples/CMakeLists.txt` (`example_list`, non-emscripten)
- Test: `test/test_sprite_panel.cpp` (gui), `test/CMakeLists.txt` (`gui_test_list`)

**Interfaces:**
- Consumes: `SpriteRenderer` (Task 8), `Sprite`/`SpriteParams` (Task 8), `Camera::unproject`/`ray_ground` (Task 6), `IsoGrid` (Task 4).
- Produces:
  - `std::size_t ScenePanel::add_sprite(ShSpriteSheetPr sheet, int frame, const Magnum::Vector3& position, const SpriteParams& params = {});`
  - `Sprite& ScenePanel::sprite(std::size_t handle);`
  - `std::optional<Magnum::Vector3> ScenePanel::cursor_world() const;`
  - `std::optional<Magnum::Vector2i> ScenePanel::tile_under_cursor(const IsoGrid& grid) const;`

- [ ] **Step 1: Write the failing smoke test**

`test/test_sprite_panel.cpp`:

```cpp
// gui smoke test: sprites placed on an iso grid render through ScenePanel
#include "GuiBase.hh"
#include "IsoGrid.hh"
#include "ScenePanel.hh"
#include "SpriteSheet.hh"

#include <array>
#include <cstdio>

using namespace smg;

static ScenePanel* g_panel = nullptr;

int sprite_cb(void* data) {
    reinterpret_cast<ScenePanel*>(data)->draw("Sprites", Magnum::Vector2i{ 320, 240 });
    return 0;
}

class SpritePanelTest: public GuiBase {
public:
    explicit SpritePanelTest(const Arguments& arguments) : GuiBase(arguments) {
        _panel = ScenePanel::create();
        _panel->camera().iso();

        std::array<unsigned char, 4> px{ 255, 255, 255, 255 };
        ShSpriteSheetPr sheet = SpriteSheet::from_pixels(
            Corrade::Containers::arrayView(reinterpret_cast<const char*>(px.data()), px.size()),
            Magnum::Vector2i{ 1, 1 }, Magnum::PixelFormat::RGBA8Unorm, 1, 1);

        IsoGrid grid{ 1.0f };
        const std::size_t h = _panel->add_sprite(sheet, 0, grid.to_world({ 0, 0 }), SpriteParams{});
        if(_panel->sprite(h).frame != 0) { std::printf("FAIL sprite handle\n"); exit(1); }

        _cb = DrawCallback::create();
        _cb->set_callback(sprite_cb);
        _cb->set_data(_panel.get());
        add_callback(_cb);
        g_panel = _panel.get();
    }

private:
    ShScenePanelPr _panel;
    ShDrawCallbackPr _cb;
};

MAGNUM_APPLICATION_MAIN(SpritePanelTest)
```

- [ ] **Step 2: Register and confirm failure**

Add `test_sprite_panel.cpp` to `gui_test_list`. Build → FAIL (`add_sprite`/`sprite`/`tile_under_cursor` undeclared).

- [ ] **Step 3: Edit `include/ScenePanel.hh`**

Add includes:

```cpp
#include <optional>

#include "IsoGrid.hh"
#include "SpriteSheet.hh"
#include "SpriteTypes.hh"
```

Add a forward decl for the renderer near the top of `namespace smg`:

```cpp
class SpriteRenderer;
```

Add public methods (next to the mesh `add_*` group):

```cpp
    std::size_t add_sprite(ShSpriteSheetPr sheet, int frame, const Magnum::Vector3& position, const SpriteParams& params = {});
    [[nodiscard]] Sprite& sprite(std::size_t handle) { return _sprites[handle]; }
    [[nodiscard]] std::optional<Magnum::Vector3> cursor_world() const;
    [[nodiscard]] std::optional<Magnum::Vector2i> tile_under_cursor(const IsoGrid& grid) const;
```

Add private members:

```cpp
    std::vector<Sprite> _sprites;
    std::shared_ptr<SpriteRenderer> _sprite_renderer;
    Magnum::Vector2 _image_min{ 0.0f, 0.0f }; // last drawn image top-left, screen px
```

(`std::shared_ptr` avoids needing the full `SpriteRenderer` definition in the header; include `<memory>` is already present.)

- [ ] **Step 4: Edit `src/ScenePanel.cpp`**

Add includes:

```cpp
#include "SpriteRenderer.hh"
```

In `ensure_gl()`, after creating shaders, construct the renderer:

```cpp
    _sprite_renderer = std::make_shared<SpriteRenderer>();
```

Add the new methods:

```cpp
std::size_t ScenePanel::add_sprite(ShSpriteSheetPr sheet, int frame, const Magnum::Vector3& position, const SpriteParams& params) {
    _sprites.push_back(Sprite{ std::move(sheet), frame, position, params });
    return _sprites.size() - 1;
}

std::optional<Magnum::Vector3> ScenePanel::cursor_world() const {
    if(_fbo_size.x() <= 0 || _fbo_size.y() <= 0) return {};
    const ImVec2 m = ImGui::GetMousePos();
    const Magnum::Vector2 local{ m.x - _image_min.x(), m.y - _image_min.y() };
    if(local.x() < 0.0f || local.y() < 0.0f || local.x() > float(_fbo_size.x()) || local.y() > float(_fbo_size.y())) return {};
    const Camera::Ray ray = _camera.unproject(local, Magnum::Vector2{ _fbo_size });
    return ray_ground(ray, _camera.up_axis());
}

std::optional<Magnum::Vector2i> ScenePanel::tile_under_cursor(const IsoGrid& grid) const {
    const std::optional<Magnum::Vector3> w = cursor_world();
    if(!w) return {};
    return grid.to_cell(*w);
}
```

In `render_scene`, after the mesh loop and **before** the MSAA resolve/`disable` calls, draw sprites into the same target (so they share the depth buffer and feed bloom):

```cpp
    if(_sprite_renderer && !_sprites.empty()) _sprite_renderer->draw(_sprites, view, proj);
```

In `draw()`, capture the image top-left right after emitting the image so `cursor_world` can map the mouse:

```cpp
    Magnum::ImGuiIntegration::image(_color, Magnum::Vector2{ size });
    const ImVec2 imin = ImGui::GetItemRectMin();
    _image_min = Magnum::Vector2{ imin.x, imin.y };
    handle_input(Magnum::Vector2{ size });
```

(`_image_min` is mutated in `draw()`; if `cursor_world()` being `const` conflicts, mark `_image_min` `mutable` or drop `const` from `cursor_world`. Prefer making `_image_min` `mutable`.)

- [ ] **Step 5: Update the umbrella header `cmake/smg.hh.in`**

Append after the scene viewport block:

```cpp
// 2D sprites on an isometric plane
#include "SpriteTypes.hh"
#include "SpriteSheet.hh"
#include "IsoGrid.hh"
```

- [ ] **Step 6: Write the example `examples/sprite_panel.cpp`**

```cpp
// sprite_panel example: a grid of billboard sprites under an iso camera, plus an additive glow
#include "GuiBase.hh"
#include "IsoGrid.hh"
#include "ScenePanel.hh"
#include "SpriteSheet.hh"

#include <array>
#include <vector>

using namespace Magnum;
using namespace smg;

// build a tiny solid-color RGBA sheet at runtime (stand-in for a prerendered atlas)
static ShSpriteSheetPr solid_sheet(unsigned char r, unsigned char g, unsigned char b) {
    std::array<unsigned char, 4> px{ r, g, b, 255 };
    return SpriteSheet::from_pixels(Corrade::Containers::arrayView(reinterpret_cast<const char*>(px.data()), px.size()),
        Vector2i{ 1, 1 }, PixelFormat::RGBA8Unorm, 1, 1);
}

int sprite_callback(void* data) {
    reinterpret_cast<ScenePanel*>(data)->draw("Iso Sprites", Vector2i{ 800, 600 });
    return 0;
}

class SpriteExample: public GuiBase {
public:
    explicit SpriteExample(const Arguments& arguments) : GuiBase(arguments) {
        _panel = ScenePanel::create();
        _panel->add_grid();
        _panel->camera().iso();

        ShSpriteSheetPr unit = solid_sheet(220, 90, 70);
        IsoGrid grid{ 1.0f };
        for(int j = -2; j <= 2; ++j)
            for(int i = -2; i <= 2; ++i) {
                SpriteParams p;
                p.size = Vector2{ 0.8f, 1.2f };
                _panel->add_sprite(unit, 0, grid.to_world({ i, j }), p);
            }

        // an additive glow sprite (pairs with bloom when SMG_WITH_BLOOM is on)
        ShSpriteSheetPr glow = solid_sheet(120, 160, 255);
        SpriteParams gp;
        gp.size = Vector2{ 1.5f, 1.5f };
        gp.anchor = Anchor::Center;
        gp.blend = SpriteBlend::Additive;
        gp.tint = Color4{ 1.0f, 1.0f, 1.0f, 0.6f };
        _panel->add_sprite(glow, 0, grid.to_world({ 0, 0 }) + Vector3{ 0.0f, 1.0f, 0.0f }, gp);

        _cb = DrawCallback::create();
        _cb->set_callback(sprite_callback);
        _cb->set_data(_panel.get());
        add_callback(_cb);
    }

private:
    ShScenePanelPr _panel;
    ShDrawCallbackPr _cb;
};

MAGNUM_APPLICATION_MAIN(SpriteExample)
```

- [ ] **Step 7: Register the example (non-emscripten list in `examples/CMakeLists.txt`)**

```cmake
    set(example_list
        implot_ex.cpp
        draw_cube.cpp
        draw_triangle.cpp
        guibase.cpp
        scene_panel.cpp
        sprite_panel.cpp
    )
```

- [ ] **Step 8: Build, run smoke test, format, commit**

```bash
cmake --build build-native --parallel && ctest --test-dir build-native -R test_sprite_panel --output-on-failure
clang-format -i include/ScenePanel.hh src/ScenePanel.cpp examples/sprite_panel.cpp test/test_sprite_panel.cpp
git add include/ScenePanel.hh src/ScenePanel.cpp cmake/smg.hh.in examples/sprite_panel.cpp examples/CMakeLists.txt test/test_sprite_panel.cpp test/CMakeLists.txt
git commit -m "sprites: integrate sprite pass + picking into ScenePanel, add example"
```

Expected: PASS. Also run the full unit suite to confirm no regression:

```bash
ctest --test-dir build-native -L unit --output-on-failure
```

- [ ] **Step 9: Manually run the example to eyeball it (needs a display)**

```bash
./build-native/bin/sprite_panel
```

Expected: an iso-projected grid of upright sprite cards on the ground, with a glowing additive card above the centre. Close the window to exit.

---

### Task 10: Optional PNG loading — `SMG_WITH_IMAGE_IMPORT`

**Files:**
- Modify: `CMakeLists.txt` (option + conditional importer link/define)
- Modify: `src/SpriteSheet.cpp` (implement `load`)
- Test: `test/test_sprite_load.cpp` (gui, only when enabled) + `test/CMakeLists.txt`
- Add: a small PNG asset under `assets/` for the test (e.g. `assets/test_sprite.png`)

**Interfaces:**
- Consumes: `SpriteSheet::from_pixels` (Task 7).
- Produces: `static ShSpriteSheetPr SpriteSheet::load(const char* path, int cols, int rows);` (compiled only under `SMG_WITH_IMAGE_IMPORT`).

- [ ] **Step 1: Add the CMake option and wiring (`CMakeLists.txt`)**

After the other `option(...)` lines:

```cmake
option(SMG_WITH_IMAGE_IMPORT "Build SpriteSheet::load(path) via Magnum image importers" OFF)
```

After `target_link_libraries(smg PUBLIC ...)` and the bloom block, add:

```cmake
if(SMG_WITH_IMAGE_IMPORT)
    if(USE_SYSTEM_MAGNUM)
        find_package(Magnum REQUIRED AnyImageImporter)
        find_package(MagnumPlugins REQUIRED StbImageImporter)
    endif()
    target_link_libraries(smg PUBLIC Magnum::AnyImageImporter MagnumPlugins::StbImageImporter)
    target_compile_definitions(smg PUBLIC SMG_WITH_IMAGE_IMPORT)
endif()
```

- [ ] **Step 2: Implement `load` in `src/SpriteSheet.cpp` (guarded)**

Add includes at the top, guarded:

```cpp
#ifdef SMG_WITH_IMAGE_IMPORT
#include <Corrade/PluginManager/Manager.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>
#endif
```

Add the method at the end of `namespace smg`:

```cpp
#ifdef SMG_WITH_IMAGE_IMPORT
ShSpriteSheetPr SpriteSheet::load(const char* path, int cols, int rows) {
    Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter> manager;
    Corrade::Containers::Pointer<Magnum::Trade::AbstractImporter> importer = manager.loadAndInstantiate("AnyImageImporter");
    if(!importer || !importer->openFile(path)) return nullptr;
    const Corrade::Containers::Optional<Magnum::Trade::ImageData2D> image = importer->image2D(0);
    if(!image) return nullptr;
    return from_pixels(image->data(), image->size(), image->format(), cols, rows);
}
#endif
```

(`#include <Corrade/Containers/Optional.h>` and `<Corrade/Containers/Pointer.h>` may be needed — add them inside the guard if the build complains.)

- [ ] **Step 3: Add a guarded smoke test `test/test_sprite_load.cpp`**

```cpp
#include "GuiBase.hh"
#include "SpriteSheet.hh"

#include <cstdio>

using namespace smg;

class LoadTest: public GuiBase {
public:
    explicit LoadTest(const Arguments& arguments) : GuiBase(arguments) {
        ShSpriteSheetPr sheet = SpriteSheet::load(SMG_TEST_ASSET_DIR "/../assets/test_sprite.png", 1, 1);
        if(!sheet) { std::printf("FAIL load returned null\n"); exit(1); }
        if(sheet->texture().id() == 0) { std::printf("FAIL texture id\n"); exit(1); }
        std::printf("OK sprite load\n");
        exit(0);
    }
};

MAGNUM_APPLICATION_MAIN(LoadTest)
```

- [ ] **Step 4: Register the test only when the option is on (`test/CMakeLists.txt`)**

Inside the `if(SMG_BUILD_GUI_TESTS)` block, after the `gui_test_list` loop:

```cmake
    if(SMG_WITH_IMAGE_IMPORT)
        smg_add_test(test_sprite_load test_sprite_load.cpp "gui")
    endif()
```

- [ ] **Step 5: Provide the test asset**

Add a small PNG at `assets/test_sprite.png` (any tiny RGBA PNG). If none exists, create one:

```bash
python3 -c "from PIL import Image; Image.new('RGBA',(4,4),(255,0,0,255)).save('assets/test_sprite.png')"
```

(If PIL is unavailable, copy any existing small PNG to that path.)

- [ ] **Step 6: Configure with the option, build, run**

```bash
cmake -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSMG_WITH_BLOOM=ON -DSMG_WITH_IMAGE_IMPORT=ON
cmake --build build-native --parallel
ctest --test-dir build-native -R test_sprite_load --output-on-failure
```

Expected: PASS (`OK sprite load`).

- [ ] **Step 7: Confirm the default build (importer OFF) still works**

```bash
cmake -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-native --parallel && ctest --test-dir build-native -L unit --output-on-failure
```

Expected: builds without the importer; `load` is compiled out; all unit tests PASS.

- [ ] **Step 8: Format and commit**

```bash
clang-format -i src/SpriteSheet.cpp test/test_sprite_load.cpp
git add CMakeLists.txt src/SpriteSheet.cpp test/test_sprite_load.cpp test/CMakeLists.txt assets/test_sprite.png
git commit -m "sprites: optional SpriteSheet::load via StbImageImporter (SMG_WITH_IMAGE_IMPORT)"
```

---

## Self-Review

**Spec coverage:**
- Module layout (SpriteTypes, SpriteSheet, IsoGrid, SpriteRenderer) → Tasks 2,3,4,7,8. ✓
- Camera ortho + iso preset → Task 5. ✓
- Picking (unproject + ground hit + tile) → Tasks 6, 9. ✓
- Public API (from_texture/from_pixels, frame_uv, add_sprite/sprite, clips, dir_row) → Tasks 2,3,7,8,9. ✓
- Rendering: shared depth, alpha-mask pass, translucent/additive sorted pass → Task 8; integrated into ScenePanel before resolve so bloom applies → Task 9. ✓
- `load(path)` gated by `SMG_WITH_IMAGE_IMPORT` mirroring `SMG_WITH_BLOOM` → Task 10. ✓
- Bloom integration into the working branch → Task 1. ✓
- Tests: headless unit (frame_uv, clip, dir_row, IsoGrid, camera ortho, picking) + gui smoke (sheet, renderer, panel, load) → Tasks 2–10. ✓
- WASM format gating in `from_pixels` → Task 7. ✓

**Known scope notes (intentional):**
- `Anchor` ships only `Center` + `BottomCenter` (YAGNI — Majesty needs feet-on-tile + centered glows). More anchors are a trivial later add.
- Example uses runtime-generated solid-color sheets so it builds with the importer OFF; real prerendered PNGs flow through `load` (Task 10) or caller-supplied `from_texture`.
- Upright camera-facing billboards use the full camera basis (cards parallel to the screen), matching prerendered-sprite convention.

**Type consistency:** `ShSpriteSheetPr` defined once (in `SpriteTypes.hh`, Task 8 removes the duplicate from `SpriteSheet.hh`). `SpriteGrid::frame_uv` ↔ `SpriteSheet::frame_uv` ↔ renderer usage consistent. `Camera::Ray` ↔ `ray_ground` ↔ `unproject` consistent. `SpriteParams`/`Sprite` field names match across renderer and ScenePanel.
