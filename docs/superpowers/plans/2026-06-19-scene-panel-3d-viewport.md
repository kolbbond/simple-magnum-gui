# ScenePanel 3D Viewport Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `smg::ScenePanel` — a standalone, domain-free 3D scene viewport (meshes, primitives, orbit camera, Phong light) rendered into an embeddable ImGui widget.

**Architecture:** A retained scene of objects (`{mesh, transform, color}`) drawn into an off-screen framebuffer and shown inside an ImGui window via `ImGuiIntegration::image`, with mouse-driven orbit/pan/zoom. Pure-math units (`Bounds`, `Camera`, primitive CPU generation) are split out so they are unit-tested headlessly; GL/GUI units are compile-green + manual smoke tests, matching the existing `test_magnum_*` pattern. Ported from `goose-gui`'s `ScenePanel`/`Mesher`/`Camera`, stripped of EM/FEM coupling.

**Tech Stack:** C++17, Magnum (GL, MeshTools, Primitives, Shaders, Trade), Corrade, Dear ImGui via MagnumIntegration. Build: CMake superbuild.

## Global Constraints

- **Namespace** `smg`; shared-ptr factory convention: `typedef std::shared_ptr<class X> ShXPr;` with `static ShXPr create(...)`.
- **C++ style** (`CLAUDE.md` cxx block, `.clang-format`/`.clang-tidy` win): no `using namespace` in headers; no `auto` for definitions (iterators excepted); explicit `std::`/`Magnum::` qualification; laconic comments (the *why*, terse). Run `clang-format -i` on every modified file before committing. Existing smg files indent with tabs — match the file you edit.
- **Commits:** topic-prefixed, laconic subject (e.g. `scene: add Bounds type`). **No `Co-Authored-By` trailer.**
- **No new Magnum components** — `Magnum::{GL,MeshTools,Primitives,Shaders,Trade}` are already linked into `smg` (CMakeLists.txt:148-155). 3D shaders are in the `Shaders` component.
- **Shaders:** Magnum built-ins only (`PhongGL`, `FlatGL3D`, `VertexColorGL3D`, `MeshVisualizerGL3D`). No custom GLSL.
- **Platform:** desktop is primary. Keep the WASM build *configuring* — the new `scene_panel` example is desktop-only in v1 (register it only in the non-Emscripten list).
- **API confirmation:** exact Magnum signatures are confirmed by compiling. Each GL task ends with a build gate; treat a build failure as the RED signal and adjust the call to the vendored header.
- **New smg sources** must be appended to `set(smg_sources ...)` at CMakeLists.txt:103-109 in the task that creates them.
- **New tests** are registered in `test/CMakeLists.txt`'s `test_list`; the headless ones run automatically under `ctest`, the GUI smoke test is run manually.

---

### Task 1: Scene value types — `Vertex`, `Bounds`, `LightProperties`

Pure CPU value types and AABB math. Fully headless-testable — no GL context.

**Files:**
- Create: `include/SceneTypes.hh`
- Create: `src/SceneTypes.cpp`
- Create: `test/test_util.hh` (shared tiny test harness)
- Test: `test/test_bounds.cpp`
- Modify: `CMakeLists.txt:103-109` (append `src/SceneTypes.cpp` to `smg_sources`)
- Modify: `test/CMakeLists.txt` (`test_list`: add `test_bounds.cpp`)

**Interfaces:**
- Produces:
  - `struct smg::Vertex { Magnum::Vector3 position; Magnum::Vector3 normal; Magnum::Color3 color; }`
  - `struct smg::Bounds` with `void expand(const Magnum::Vector3&)`, `void expand(const Bounds&)`, `bool empty() const`, `Magnum::Vector3 center() const`, `Magnum::Vector3 size() const`, `float diagonal() const`
  - `smg::Bounds smg::compute_bounds(Corrade::Containers::ArrayView<const Vertex>)`
  - `struct smg::LightProperties { Magnum::Color3 ambient, diffuse, specular; Magnum::Vector3 position; float shininess; }`

- [ ] **Step 1: Write the shared test harness** — `test/test_util.hh`

```cpp
// minimal headless test harness: CHECK accumulates failures; main returns count
#pragma once
#include <cmath>
#include <cstdio>

namespace smgtest {
inline int& failures() { static int f = 0; return f; }
inline bool approx(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }
} // namespace smgtest

#define CHECK(cond)                                                            \
	do {                                                                       \
		if(!(cond)) {                                                          \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
			++smgtest::failures();                                             \
		}                                                                      \
	} while(0)

#define TEST_RETURN() return smgtest::failures() ? 1 : 0
```

- [ ] **Step 2: Write the failing test** — `test/test_bounds.cpp`

```cpp
#include "SceneTypes.hh"
#include "test_util.hh"

#include <vector>

using smg::Bounds;
using smg::Vertex;

int main() {
	// expand grows the box; center/size/diagonal are correct
	Bounds b;
	CHECK(b.empty());
	b.expand(Magnum::Vector3{-1.0f, -2.0f, -3.0f});
	b.expand(Magnum::Vector3{1.0f, 2.0f, 3.0f});
	CHECK(!b.empty());
	CHECK(smgtest::approx(b.center().x(), 0.0f));
	CHECK(smgtest::approx(b.center().y(), 0.0f));
	CHECK(smgtest::approx(b.size().x(), 2.0f));
	CHECK(smgtest::approx(b.size().z(), 6.0f));
	CHECK(smgtest::approx(b.diagonal(), std::sqrt(4.0f + 16.0f + 36.0f)));

	// compute_bounds over a vertex list
	std::vector<Vertex> verts{
		Vertex{Magnum::Vector3{0.0f, 0.0f, 0.0f}, {}, {}},
		Vertex{Magnum::Vector3{4.0f, 0.0f, 0.0f}, {}, {}},
		Vertex{Magnum::Vector3{0.0f, 5.0f, 0.0f}, {}, {}}};
	Bounds cb = smg::compute_bounds({verts.data(), verts.size()});
	CHECK(smgtest::approx(cb.size().x(), 4.0f));
	CHECK(smgtest::approx(cb.size().y(), 5.0f));

	TEST_RETURN();
}
```

- [ ] **Step 3: Register the test and run it to confirm it fails to build**

Append `test_bounds.cpp` to `test_list` in `test/CMakeLists.txt`. Then:

Run: `cd build && cmake .. >/dev/null && cmake --build . --target test_bounds 2>&1 | tail -5`
Expected: FAIL — `SceneTypes.hh: No such file or directory`.

- [ ] **Step 4: Write the header** — `include/SceneTypes.hh`

```cpp
// scene value types: vertex, AABB, light properties
#pragma once

#include <Corrade/Containers/ArrayView.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector3.h>

namespace smg {

// interleaved vertex for lit, colored meshes
struct Vertex {
	Magnum::Vector3 position;
	Magnum::Vector3 normal{0.0f, 0.0f, 1.0f};
	Magnum::Color3 color{1.0f, 1.0f, 1.0f};
};

// axis-aligned bounding box; default-constructed is empty (min > max)
struct Bounds {
	Magnum::Vector3 min;
	Magnum::Vector3 max;

	Bounds();
	[[nodiscard]] bool empty() const;
	void expand(const Magnum::Vector3& p);
	void expand(const Bounds& b);
	[[nodiscard]] Magnum::Vector3 center() const;
	[[nodiscard]] Magnum::Vector3 size() const;
	[[nodiscard]] float diagonal() const;
};

// compute an AABB over a vertex span
[[nodiscard]] Bounds compute_bounds(Corrade::Containers::ArrayView<const Vertex> verts);

// single Phong light + material defaults (ported from goose-gui)
struct LightProperties {
	Magnum::Color3 ambient{0.6f, 0.6f, 0.6f};
	Magnum::Color3 diffuse{1.0f, 1.0f, 1.0f};
	Magnum::Color3 specular{0.9f, 0.9f, 0.9f};
	Magnum::Vector3 position{5.0f, 5.0f, 5.0f};
	float shininess{64.0f};
};

} // namespace smg
```

- [ ] **Step 5: Write the implementation** — `src/SceneTypes.cpp`

```cpp
#include "SceneTypes.hh"

#include <limits>

namespace smg {

Bounds::Bounds()
	: min{std::numeric_limits<float>::max()}, max{std::numeric_limits<float>::lowest()} {}

bool Bounds::empty() const { return min.x() > max.x(); }

void Bounds::expand(const Magnum::Vector3& p) {
	min = Magnum::Math::min(min, p);
	max = Magnum::Math::max(max, p);
}

void Bounds::expand(const Bounds& b) {
	if(b.empty())
		return;
	expand(b.min);
	expand(b.max);
}

Magnum::Vector3 Bounds::center() const { return (min + max) * 0.5f; }

Magnum::Vector3 Bounds::size() const { return max - min; }

float Bounds::diagonal() const { return size().length(); }

Bounds compute_bounds(Corrade::Containers::ArrayView<const Vertex> verts) {
	Bounds b;
	for(const Vertex& v: verts)
		b.expand(v.position);
	return b;
}

} // namespace smg
```

- [ ] **Step 6: Append source to CMake** — `CMakeLists.txt:103-109`

Add `src/SceneTypes.cpp` to the `set(smg_sources ...)` list (after `src/Statics.cpp`).

- [ ] **Step 7: Build and run the test — expect PASS**

Run: `cd build && cmake .. >/dev/null && cmake --build . --target test_bounds 2>&1 | tail -3 && ctest -R test_bounds --output-on-failure`
Expected: build succeeds; `test_bounds` PASSES (exit 0).

- [ ] **Step 8: Commit**

```bash
clang-format -i include/SceneTypes.hh src/SceneTypes.cpp test/test_bounds.cpp test/test_util.hh
git add include/SceneTypes.hh src/SceneTypes.cpp test/test_util.hh test/test_bounds.cpp CMakeLists.txt test/CMakeLists.txt
git commit -m "scene: add Vertex, Bounds, LightProperties value types"
```

---

### Task 2: `smg::Camera` — orbit/pan/zoom + fit

Pure matrix math, headless-testable. Y-up default, Z-up optional.

**Files:**
- Create: `include/Camera.hh`
- Create: `src/Camera.cpp`
- Test: `test/test_camera.cpp`
- Modify: `CMakeLists.txt:103-109` (append `src/Camera.cpp`)
- Modify: `test/CMakeLists.txt` (`test_list`: add `test_camera.cpp`)

**Interfaces:**
- Consumes: `smg::Bounds` (Task 1)
- Produces: `class smg::Camera` with
  - `Magnum::Matrix4 view() const` (world→camera)
  - `Magnum::Matrix4 projection(float aspect) const`
  - `Magnum::Vector3 eye() const`
  - `void orbit(float dx, float dy)`, `void pan(float dx, float dy)`, `void zoom(float delta)`
  - `void fit(const Bounds& b, float margin = 1.5f)`
  - `const Magnum::Vector3& pivot() const`, `float distance() const`
  - `void set_fov_deg(float)`, `void set_clip(float n, float f)`, `void set_up_axis(smg::UpAxis)`
  - `enum class smg::UpAxis { Y, Z }`

- [ ] **Step 1: Write the failing test** — `test/test_camera.cpp`

```cpp
#include "Camera.hh"
#include "SceneTypes.hh"
#include "test_util.hh"

#include <Magnum/Math/Vector4.h>

using smg::Bounds;
using smg::Camera;

int main() {
	Camera cam;

	// fit a unit cube: pivot at centre, looking at it, centre projects to NDC origin
	Bounds box;
	box.expand(Magnum::Vector3{-1.0f, -1.0f, -1.0f});
	box.expand(Magnum::Vector3{1.0f, 1.0f, 1.0f});
	cam.fit(box);
	CHECK(smgtest::approx(cam.pivot().x(), 0.0f));
	CHECK(cam.distance() > 1.0f);

	const Magnum::Vector4 clip =
		cam.projection(1.0f) * cam.view() * Magnum::Vector4{cam.pivot(), 1.0f};
	const Magnum::Vector3 ndc = clip.xyz() / clip.w();
	CHECK(smgtest::approx(ndc.x(), 0.0f));
	CHECK(smgtest::approx(ndc.y(), 0.0f));

	// orbit preserves distance from pivot
	const float d0 = (cam.eye() - cam.pivot()).length();
	cam.orbit(30.0f, 12.0f);
	const float d1 = (cam.eye() - cam.pivot()).length();
	CHECK(smgtest::approx(d0, d1, 1e-3f));

	// zoom: positive = closer, negative = farther
	const float before = cam.distance();
	cam.zoom(1.0f);
	CHECK(cam.distance() < before);
	cam.zoom(-2.0f);
	CHECK(cam.distance() > before);

	// pan moves the pivot
	const Magnum::Vector3 p0 = cam.pivot();
	cam.pan(10.0f, 0.0f);
	CHECK((cam.pivot() - p0).length() > 0.0f);

	TEST_RETURN();
}
```

- [ ] **Step 2: Register and confirm build failure**

Append `test_camera.cpp` to `test_list`.
Run: `cd build && cmake .. >/dev/null && cmake --build . --target test_camera 2>&1 | tail -5`
Expected: FAIL — `Camera.hh: No such file or directory`.

- [ ] **Step 3: Write the header** — `include/Camera.hh`

```cpp
// orbit camera: pivot-centred rotate / pan / zoom with auto-fit
#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Vector3.h>

namespace smg {

struct Bounds;

enum class UpAxis { Y, Z };

class Camera {
public:
	Camera();

	[[nodiscard]] Magnum::Matrix4 view() const;             // world -> camera
	[[nodiscard]] Magnum::Matrix4 projection(float aspect) const;
	[[nodiscard]] Magnum::Vector3 eye() const;

	void orbit(float dx, float dy); // screen-pixel deltas
	void pan(float dx, float dy);
	void zoom(float delta); // + = closer
	void fit(const Bounds& b, float margin = 1.5f);

	[[nodiscard]] const Magnum::Vector3& pivot() const { return _pivot; }
	[[nodiscard]] float distance() const { return _distance; }

	void set_fov_deg(float d) { _fov_deg = d; }
	void set_clip(float n, float f) { _near = n; _far = f; }
	void set_up_axis(UpAxis a) { _up = a; }

private:
	[[nodiscard]] Magnum::Vector3 up_vector() const;

	Magnum::Vector3 _pivot{0.0f};
	float _distance{5.0f};
	float _yaw{0.6f};   // radians, around up axis
	float _pitch{0.4f}; // radians, elevation
	float _fov_deg{45.0f};
	float _near{0.05f};
	float _far{500.0f};
	UpAxis _up{UpAxis::Y};
};

} // namespace smg
```

- [ ] **Step 4: Write the implementation** — `src/Camera.cpp`

```cpp
#include "Camera.hh"

#include <algorithm>
#include <cmath>

#include <Magnum/Math/Angle.h>
#include <Magnum/Math/Functions.h>

#include "SceneTypes.hh"

namespace smg {

namespace {
constexpr float kOrbitRate = 0.01f; // radians per pixel
constexpr float kPanRate = 0.0015f; // pivot units per pixel, scaled by distance
constexpr float kZoomRate = 0.1f;
constexpr float kPitchLimit = 1.55f; // ~89 deg
} // namespace

Camera::Camera() = default;

Magnum::Vector3 Camera::up_vector() const {
	return _up == UpAxis::Y ? Magnum::Vector3::yAxis() : Magnum::Vector3::zAxis();
}

Magnum::Vector3 Camera::eye() const {
	const float cp = std::cos(_pitch);
	const float sp = std::sin(_pitch);
	// direction from pivot toward eye; elevation along the up axis
	const Magnum::Vector3 dir = _up == UpAxis::Y
		? Magnum::Vector3{cp * std::sin(_yaw), sp, cp * std::cos(_yaw)}
		: Magnum::Vector3{cp * std::sin(_yaw), cp * std::cos(_yaw), sp};
	return _pivot + dir * _distance;
}

Magnum::Matrix4 Camera::view() const {
	return Magnum::Matrix4::lookAt(eye(), _pivot, up_vector()).invertedRigid();
}

Magnum::Matrix4 Camera::projection(float aspect) const {
	return Magnum::Matrix4::perspectiveProjection(
		Magnum::Deg{_fov_deg}, aspect, _near, _far);
}

void Camera::orbit(float dx, float dy) {
	_yaw += dx * kOrbitRate;
	_pitch = std::clamp(_pitch - dy * kOrbitRate, -kPitchLimit, kPitchLimit);
}

void Camera::pan(float dx, float dy) {
	const Magnum::Vector3 forward = (_pivot - eye()).normalized();
	const Magnum::Vector3 right = Magnum::Math::cross(forward, up_vector()).normalized();
	const Magnum::Vector3 trueUp = Magnum::Math::cross(right, forward);
	const float scale = kPanRate * _distance;
	_pivot += (-dx * right + dy * trueUp) * scale;
}

void Camera::zoom(float delta) {
	_distance *= std::exp(-delta * kZoomRate);
	_distance = std::max(_distance, 1e-3f);
}

void Camera::fit(const Bounds& b, float margin) {
	if(b.empty())
		return;
	_pivot = b.center();
	const float radius = std::max(b.diagonal() * 0.5f, 1e-3f);
	const float halfFov = Magnum::Rad{Magnum::Deg{_fov_deg}} * 0.5f;
	_distance = (radius * margin) / std::tan(halfFov);
}

} // namespace smg
```

- [ ] **Step 5: Append source to CMake** — `CMakeLists.txt:103-109` add `src/Camera.cpp`.

- [ ] **Step 6: Build and run — expect PASS**

Run: `cd build && cmake .. >/dev/null && cmake --build . --target test_camera 2>&1 | tail -3 && ctest -R test_camera --output-on-failure`
Expected: build succeeds; `test_camera` PASSES. (If `lookAt`/`invertedRigid`/`cross` signatures differ, adjust to the vendored `Magnum/Math/` headers — this is the build gate.)

- [ ] **Step 7: Commit**

```bash
clang-format -i include/Camera.hh src/Camera.cpp test/test_camera.cpp
git add include/Camera.hh src/Camera.cpp test/test_camera.cpp CMakeLists.txt test/CMakeLists.txt
git commit -m "scene: add orbit Camera with fit"
```

---

### Task 3: `smg::Mesh` — GL mesh wrapper + bounds

GL upload needs a context (smoke), but bounds-from-`Trade::MeshData` is headless (tested against a real `Primitives` mesh).

**Files:**
- Create: `include/Mesh.hh`
- Create: `src/Mesh.cpp`
- Test: `test/test_mesh_bounds.cpp` (headless: bounds over `Primitives::cubeSolid()`)
- Modify: `CMakeLists.txt:103-109` (append `src/Mesh.cpp`)
- Modify: `test/CMakeLists.txt` (`test_list`: add `test_mesh_bounds.cpp`)

**Interfaces:**
- Consumes: `smg::Vertex`, `smg::Bounds`, `smg::compute_bounds` (Task 1)
- Produces:
  - `typedef std::shared_ptr<class smg::Mesh> smg::ShMeshPr`
  - `static ShMeshPr smg::Mesh::create(const Magnum::Trade::MeshData&)`
  - `static ShMeshPr smg::Mesh::create(Corrade::Containers::ArrayView<const Vertex>, Corrade::Containers::ArrayView<const Magnum::UnsignedInt>, Magnum::MeshPrimitive)`
  - `Magnum::GL::Mesh& smg::Mesh::gl()`
  - `const smg::Bounds& smg::Mesh::bounds() const`
  - `smg::Bounds smg::compute_bounds(const Magnum::Trade::MeshData&)` (headless overload)

- [ ] **Step 1: Write the failing headless test** — `test/test_mesh_bounds.cpp`

```cpp
#include "Mesh.hh"
#include "test_util.hh"

#include <Magnum/Primitives/Cube.h>
#include <Magnum/Trade/MeshData.h>

int main() {
	// cubeSolid spans [-1, 1] on each axis; bounds derived from CPU mesh data
	const Magnum::Trade::MeshData cube = Magnum::Primitives::cubeSolid();
	const smg::Bounds b = smg::compute_bounds(cube);
	CHECK(smgtest::approx(b.size().x(), 2.0f));
	CHECK(smgtest::approx(b.size().y(), 2.0f));
	CHECK(smgtest::approx(b.size().z(), 2.0f));
	CHECK(smgtest::approx(b.center().x(), 0.0f));
	TEST_RETURN();
}
```

- [ ] **Step 2: Register and confirm build failure**

Append `test_mesh_bounds.cpp` to `test_list`.
Run: `cd build && cmake .. >/dev/null && cmake --build . --target test_mesh_bounds 2>&1 | tail -5`
Expected: FAIL — `Mesh.hh: No such file or directory`.

- [ ] **Step 3: Write the header** — `include/Mesh.hh`

```cpp
// GL mesh wrapper: owns a GL::Mesh and its AABB
#pragma once

#include <memory>

#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Magnum.h>
#include <Magnum/Trade/Trade.h>

#include "SceneTypes.hh"

namespace smg {

typedef std::shared_ptr<class Mesh> ShMeshPr;

class Mesh {
public:
	Mesh(Magnum::GL::Mesh&& mesh, const Bounds& bounds);

	// compile from Magnum mesh data (e.g. Primitives::*Solid())
	static ShMeshPr create(const Magnum::Trade::MeshData& data);
	// upload interleaved smg::Vertex data
	static ShMeshPr create(Corrade::Containers::ArrayView<const Vertex> verts,
		Corrade::Containers::ArrayView<const Magnum::UnsignedInt> indices,
		Magnum::MeshPrimitive primitive);

	[[nodiscard]] Magnum::GL::Mesh& gl() { return _mesh; }
	[[nodiscard]] const Bounds& bounds() const { return _bounds; }

private:
	Magnum::GL::Mesh _mesh;
	Bounds _bounds;
};

// AABB over the position attribute of mesh data (headless)
[[nodiscard]] Bounds compute_bounds(const Magnum::Trade::MeshData& data);

} // namespace smg
```

- [ ] **Step 4: Write the implementation** — `src/Mesh.cpp`

```cpp
#include "Mesh.hh"

#include <Corrade/Containers/Array.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Shaders/GenericGL.h>
#include <Magnum/Trade/MeshData.h>

namespace smg {

Mesh::Mesh(Magnum::GL::Mesh&& mesh, const Bounds& bounds)
	: _mesh{std::move(mesh)}, _bounds{bounds} {}

Bounds compute_bounds(const Magnum::Trade::MeshData& data) {
	Bounds b;
	const Corrade::Containers::Array<Magnum::Vector3> positions =
		data.positions3DAsArray();
	for(const Magnum::Vector3& p: positions)
		b.expand(p);
	return b;
}

ShMeshPr Mesh::create(const Magnum::Trade::MeshData& data) {
	Magnum::GL::Mesh glmesh = Magnum::MeshTools::compile(data);
	return std::make_shared<Mesh>(std::move(glmesh), compute_bounds(data));
}

ShMeshPr Mesh::create(Corrade::Containers::ArrayView<const Vertex> verts,
	Corrade::Containers::ArrayView<const Magnum::UnsignedInt> indices,
	Magnum::MeshPrimitive primitive) {

	Magnum::GL::Buffer vertexBuffer;
	vertexBuffer.setData(verts);

	Magnum::GL::Mesh glmesh;
	glmesh.setPrimitive(primitive)
		.addVertexBuffer(std::move(vertexBuffer), 0,
			Magnum::Shaders::GenericGL3D::Position{},
			Magnum::Shaders::GenericGL3D::Normal{},
			Magnum::Shaders::GenericGL3D::Color3{});

	if(!indices.isEmpty()) {
		Magnum::GL::Buffer indexBuffer;
		indexBuffer.setData(indices);
		glmesh.setCount(static_cast<Magnum::Int>(indices.size()))
			.setIndexBuffer(std::move(indexBuffer), 0,
				Magnum::MeshIndexType::UnsignedInt);
	} else {
		glmesh.setCount(static_cast<Magnum::Int>(verts.size()));
	}

	return std::make_shared<Mesh>(std::move(glmesh), compute_bounds(verts));
}

} // namespace smg
```

- [ ] **Step 5: Append source to CMake** — `CMakeLists.txt:103-109` add `src/Mesh.cpp`.

- [ ] **Step 6: Build and run — expect PASS**

Run: `cd build && cmake .. >/dev/null && cmake --build . --target test_mesh_bounds 2>&1 | tail -3 && ctest -R test_mesh_bounds --output-on-failure`
Expected: build succeeds; PASS. (If `positions3DAsArray`/`GenericGL3D` attribute names differ, adjust to vendored headers.)

- [ ] **Step 7: Commit**

```bash
clang-format -i include/Mesh.hh src/Mesh.cpp test/test_mesh_bounds.cpp
git add include/Mesh.hh src/Mesh.cpp test/test_mesh_bounds.cpp CMakeLists.txt test/CMakeLists.txt
git commit -m "scene: add Mesh wrapper and mesh-data bounds"
```

---

### Task 4: `smg::primitives` — cube/sphere/plane/grid/axes

Delegates solids to `Magnum::Primitives`; axes are hand-built lines. The axes CPU generation is factored out and headless-tested.

**Files:**
- Create: `include/Primitives.hh`
- Create: `src/Primitives.cpp`
- Test: `test/test_primitives_axes.cpp` (headless: `axes_vertices()` shape/colors)
- Modify: `CMakeLists.txt:103-109` (append `src/Primitives.cpp`)
- Modify: `test/CMakeLists.txt` (`test_list`: add `test_primitives_axes.cpp`)

**Interfaces:**
- Consumes: `smg::Vertex` (Task 1), `smg::Mesh::create` (Task 3)
- Produces (in `namespace smg::primitives`):
  - `std::vector<Vertex> axes_vertices(float scale)` (headless helper)
  - `ShMeshPr cube()`, `ShMeshPr sphere(unsigned subdivisions = 2)`, `ShMeshPr plane()`, `ShMeshPr grid(const Magnum::Vector2i& subdivisions = {10, 10})`, `ShMeshPr axes(float scale = 1.0f)`

- [ ] **Step 1: Write the failing headless test** — `test/test_primitives_axes.cpp`

```cpp
#include "Primitives.hh"
#include "test_util.hh"

int main() {
	// three axis lines: 6 vertices, each pair coloured R/G/B, tips at `scale`
	const std::vector<smg::Vertex> v = smg::primitives::axes_vertices(2.0f);
	CHECK(v.size() == 6);

	// +X line red, tip at (2,0,0)
	CHECK(smgtest::approx(v[0].color.r(), 1.0f));
	CHECK(smgtest::approx(v[0].color.g(), 0.0f));
	CHECK(smgtest::approx(v[1].position.x(), 2.0f));

	// +Y line green, tip at (0,2,0)
	CHECK(smgtest::approx(v[2].color.g(), 1.0f));
	CHECK(smgtest::approx(v[3].position.y(), 2.0f));

	// +Z line blue, tip at (0,0,2)
	CHECK(smgtest::approx(v[4].color.b(), 1.0f));
	CHECK(smgtest::approx(v[5].position.z(), 2.0f));

	TEST_RETURN();
}
```

- [ ] **Step 2: Register and confirm build failure**

Append `test_primitives_axes.cpp` to `test_list`.
Run: `cd build && cmake .. >/dev/null && cmake --build . --target test_primitives_axes 2>&1 | tail -5`
Expected: FAIL — `Primitives.hh: No such file or directory`.

- [ ] **Step 3: Write the header** — `include/Primitives.hh`

```cpp
// primitive mesh generators (cube/sphere/plane/grid/axes)
#pragma once

#include <vector>

#include <Magnum/Math/Vector2.h>

#include "Mesh.hh"
#include "SceneTypes.hh"

namespace smg::primitives {

// CPU-side axis gizmo geometry (headless-testable)
[[nodiscard]] std::vector<Vertex> axes_vertices(float scale);

[[nodiscard]] ShMeshPr cube();
[[nodiscard]] ShMeshPr sphere(unsigned subdivisions = 2);
[[nodiscard]] ShMeshPr plane();
[[nodiscard]] ShMeshPr grid(const Magnum::Vector2i& subdivisions = Magnum::Vector2i{10, 10});
[[nodiscard]] ShMeshPr axes(float scale = 1.0f);

} // namespace smg::primitives
```

- [ ] **Step 4: Write the implementation** — `src/Primitives.cpp`

```cpp
#include "Primitives.hh"

#include <Magnum/GL/Mesh.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Primitives/Grid.h>
#include <Magnum/Primitives/Icosphere.h>
#include <Magnum/Primitives/Plane.h>
#include <Magnum/Trade/MeshData.h>

namespace smg::primitives {

std::vector<Vertex> axes_vertices(float scale) {
	const Magnum::Color3 red{1.0f, 0.0f, 0.0f};
	const Magnum::Color3 green{0.0f, 1.0f, 0.0f};
	const Magnum::Color3 blue{0.0f, 0.0f, 1.0f};
	const Magnum::Vector3 origin{0.0f, 0.0f, 0.0f};
	return {
		Vertex{origin, {}, red}, Vertex{Magnum::Vector3{scale, 0.0f, 0.0f}, {}, red},
		Vertex{origin, {}, green}, Vertex{Magnum::Vector3{0.0f, scale, 0.0f}, {}, green},
		Vertex{origin, {}, blue}, Vertex{Magnum::Vector3{0.0f, 0.0f, scale}, {}, blue}};
}

ShMeshPr cube() { return Mesh::create(Magnum::Primitives::cubeSolid()); }

ShMeshPr sphere(unsigned subdivisions) {
	return Mesh::create(Magnum::Primitives::icosphereSolid(subdivisions));
}

ShMeshPr plane() { return Mesh::create(Magnum::Primitives::planeSolid()); }

ShMeshPr grid(const Magnum::Vector2i& subdivisions) {
	return Mesh::create(Magnum::Primitives::grid3DWireframe(subdivisions));
}

ShMeshPr axes(float scale) {
	const std::vector<Vertex> verts = axes_vertices(scale);
	return Mesh::create({verts.data(), verts.size()}, {}, Magnum::MeshPrimitive::Lines);
}

} // namespace smg::primitives
```

- [ ] **Step 5: Append source to CMake** — `CMakeLists.txt:103-109` add `src/Primitives.cpp`.

- [ ] **Step 6: Build and run — expect PASS**

Run: `cd build && cmake .. >/dev/null && cmake --build . --target test_primitives_axes 2>&1 | tail -3 && ctest -R test_primitives_axes --output-on-failure`
Expected: build succeeds; PASS.

- [ ] **Step 7: Commit**

```bash
clang-format -i include/Primitives.hh src/Primitives.cpp test/test_primitives_axes.cpp
git add include/Primitives.hh src/Primitives.cpp test/test_primitives_axes.cpp CMakeLists.txt test/CMakeLists.txt
git commit -m "scene: add primitive generators (cube/sphere/plane/grid/axes)"
```

---

### Task 5: `smg::ScenePanel` core — render a static scene to an ImGui window

Retained objects + lazy GL init + non-MSAA FBO + Phong/vertex-color draw + `ImGuiIntegration::image`. Camera is auto-fit; no input yet. This is the first demoable milestone (a static, framed scene shows in an ImGui window). GL/GUI — compile-green gate, behavior confirmed in Task 7's example.

**Files:**
- Create: `include/ScenePanel.hh`
- Create: `src/ScenePanel.cpp`
- Modify: `CMakeLists.txt:103-109` (append `src/ScenePanel.cpp`)

**Interfaces:**
- Consumes: `smg::Mesh`/`ShMeshPr` (Task 3), `smg::primitives::*` (Task 4), `smg::Camera` (Task 2), `smg::LightProperties` (Task 1)
- Produces:
  - `typedef std::shared_ptr<class smg::ScenePanel> smg::ShScenePanelPr`
  - `struct smg::ScenePanel::Object { ShMeshPr mesh; Magnum::Matrix4 transform; Magnum::Color3 color; bool wireframe; bool visible; }`
  - `static ShScenePanelPr smg::ScenePanel::create()`
  - `std::size_t add(ShMeshPr, const Magnum::Matrix4& = {}, const Magnum::Color3& = Magnum::Color3{1.0f})`
  - `std::size_t add_cube(...)`, `add_sphere(...)`, `add_plane(...)`, `add_grid(...)`, `add_axes(...)` (each forwards to `add`)
  - `void clear()`, `Object& object(std::size_t)`, `Camera& camera()`, `LightProperties& light()`, `void fit()`
  - `void draw(const char* title, const Magnum::Vector2i& size = Magnum::Vector2i{640, 480})`

- [ ] **Step 1: Write the header** — `include/ScenePanel.hh`

```cpp
// embeddable 3D scene viewport: renders a retained scene into an ImGui window
#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Shaders/FlatGL.h>
#include <Magnum/Shaders/MeshVisualizerGL.h>
#include <Magnum/Shaders/PhongGL.h>
#include <Magnum/Shaders/VertexColorGL.h>

#include "Camera.hh"
#include "Mesh.hh"
#include "SceneTypes.hh"

namespace smg {

typedef std::shared_ptr<class ScenePanel> ShScenePanelPr;

class ScenePanel {
public:
	struct Object {
		ShMeshPr mesh;
		Magnum::Matrix4 transform;
		Magnum::Color3 color{1.0f};
		bool wireframe{false};
		bool visible{true};
	};

	ScenePanel();
	static ShScenePanelPr create();

	std::size_t add(ShMeshPr mesh, const Magnum::Matrix4& transform = {},
		const Magnum::Color3& color = Magnum::Color3{1.0f});
	std::size_t add_cube(const Magnum::Matrix4& transform = {},
		const Magnum::Color3& color = Magnum::Color3{1.0f});
	std::size_t add_sphere(const Magnum::Matrix4& transform = {},
		const Magnum::Color3& color = Magnum::Color3{1.0f});
	std::size_t add_plane(const Magnum::Matrix4& transform = {},
		const Magnum::Color3& color = Magnum::Color3{1.0f});
	std::size_t add_grid(const Magnum::Matrix4& transform = {},
		const Magnum::Color3& color = Magnum::Color3{0.4f});
	std::size_t add_axes(float scale = 1.0f, const Magnum::Matrix4& transform = {});

	void clear();
	[[nodiscard]] Object& object(std::size_t handle) { return _objects[handle]; }
	[[nodiscard]] Camera& camera() { return _camera; }
	[[nodiscard]] LightProperties& light() { return _light; }
	void fit();

	void draw(const char* title, const Magnum::Vector2i& size = Magnum::Vector2i{640, 480});

protected:
	void ensure_gl();
	void ensure_fbo(const Magnum::Vector2i& size);
	void render_scene(const Magnum::Vector2i& size);

	std::vector<Object> _objects;
	Camera _camera;
	LightProperties _light;
	bool _fitted{false};

	bool _gl_ready{false};
	Magnum::Vector2i _fbo_size{0, 0};
	Magnum::Shaders::PhongGL _phong{Magnum::NoCreate};
	Magnum::Shaders::VertexColorGL3D _vcolor{Magnum::NoCreate};
	Magnum::Shaders::FlatGL3D _flat{Magnum::NoCreate};
	Magnum::Shaders::MeshVisualizerGL3D _wire{Magnum::NoCreate};
	Magnum::GL::Texture2D _color{Magnum::NoCreate};
	Magnum::GL::Renderbuffer _depth{Magnum::NoCreate};
	Magnum::GL::Framebuffer _fbo{Magnum::NoCreate};
};

} // namespace smg
```

- [ ] **Step 2: Write the implementation** — `src/ScenePanel.cpp`

```cpp
#include "ScenePanel.hh"

#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImGuiIntegration/Integration.h>
#include <Magnum/ImGuiIntegration/Widgets.h>

#include <imgui.h>

#include "Primitives.hh"

namespace smg {

ScenePanel::ScenePanel() = default;

ShScenePanelPr ScenePanel::create() { return std::make_shared<ScenePanel>(); }

std::size_t ScenePanel::add(ShMeshPr mesh, const Magnum::Matrix4& transform,
	const Magnum::Color3& color) {
	_objects.push_back(Object{std::move(mesh), transform, color, false, true});
	_fitted = false; // refit when the scene changes
	return _objects.size() - 1;
}

std::size_t ScenePanel::add_cube(const Magnum::Matrix4& t, const Magnum::Color3& c) {
	return add(primitives::cube(), t, c);
}
std::size_t ScenePanel::add_sphere(const Magnum::Matrix4& t, const Magnum::Color3& c) {
	return add(primitives::sphere(), t, c);
}
std::size_t ScenePanel::add_plane(const Magnum::Matrix4& t, const Magnum::Color3& c) {
	return add(primitives::plane(), t, c);
}
std::size_t ScenePanel::add_grid(const Magnum::Matrix4& t, const Magnum::Color3& c) {
	const std::size_t h = add(primitives::grid(), t, c);
	_objects[h].wireframe = true;
	return h;
}
std::size_t ScenePanel::add_axes(float scale, const Magnum::Matrix4& t) {
	const std::size_t h = add(primitives::axes(scale), t, Magnum::Color3{1.0f});
	_objects[h].wireframe = true; // unlit, vertex-coloured lines
	return h;
}

void ScenePanel::clear() {
	_objects.clear();
	_fitted = false;
}

void ScenePanel::fit() {
	Bounds scene;
	for(const Object& o: _objects)
		if(o.mesh)
			scene.expand(o.mesh->bounds());
	_camera.fit(scene);
	_fitted = true;
}

void ScenePanel::ensure_gl() {
	if(_gl_ready)
		return;
	_phong = Magnum::Shaders::PhongGL{
		Magnum::Shaders::PhongGL::Configuration{}.setFlags(
			Magnum::Shaders::PhongGL::Flag::VertexColor)};
	_vcolor = Magnum::Shaders::VertexColorGL3D{};
	_flat = Magnum::Shaders::FlatGL3D{};
	_wire = Magnum::Shaders::MeshVisualizerGL3D{
		Magnum::Shaders::MeshVisualizerGL3D::Configuration{}.setFlags(
			Magnum::Shaders::MeshVisualizerGL3D::Flag::Wireframe)};
	_gl_ready = true;
}

void ScenePanel::ensure_fbo(const Magnum::Vector2i& size) {
	if(_fbo_size == size && _color.id() != 0)
		return;
	_fbo_size = size;
	_color = Magnum::GL::Texture2D{};
	_color.setStorage(1, Magnum::GL::TextureFormat::RGBA8, size)
		.setMinificationFilter(Magnum::GL::SamplerFilter::Linear)
		.setMagnificationFilter(Magnum::GL::SamplerFilter::Linear);
	_depth = Magnum::GL::Renderbuffer{};
	_depth.setStorage(Magnum::GL::RenderbufferFormat::Depth24Stencil8, size);
	_fbo = Magnum::GL::Framebuffer{Magnum::Range2Di::fromSize({}, size)};
	_fbo.attachTexture(Magnum::GL::Framebuffer::ColorAttachment{0}, _color, 0)
		.attachRenderbuffer(
			Magnum::GL::Framebuffer::BufferAttachment::DepthStencil, _depth);
}

void ScenePanel::render_scene(const Magnum::Vector2i& size) {
	const float aspect = size.y() > 0 ? float(size.x()) / float(size.y()) : 1.0f;
	const Magnum::Matrix4 view = _camera.view();
	const Magnum::Matrix4 proj = _camera.projection(aspect);

	_fbo.clearColor(0, Magnum::Color4{0.12f, 0.12f, 0.14f, 1.0f})
		.clearDepth(1.0f)
		.bind();

	Magnum::GL::Renderer::enable(Magnum::GL::Renderer::Feature::DepthTest);
	Magnum::GL::Renderer::enable(Magnum::GL::Renderer::Feature::FaceCulling);

	for(Object& o: _objects) {
		if(!o.visible || !o.mesh)
			continue;
		const Magnum::Matrix4 tv = view * o.transform;
		if(o.wireframe) {
			// unlit, vertex-coloured lines/wireframe
			_vcolor.setTransformationProjectionMatrix(proj * tv).draw(o.mesh->gl());
		} else {
			_phong
				.setTransformationMatrix(tv)
				.setNormalMatrix(tv.normalMatrix())
				.setProjectionMatrix(proj)
				.setLightPositions({Magnum::Vector4{_light.position, 0.0f}})
				.setAmbientColor(_light.ambient)
				.setDiffuseColor(o.color)
				.setSpecularColor(_light.specular)
				.setShininess(_light.shininess)
				.draw(o.mesh->gl());
		}
	}

	Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::FaceCulling);
	Magnum::GL::defaultFramebuffer.bind();
}

void ScenePanel::draw(const char* title, const Magnum::Vector2i& size) {
	ensure_gl();
	ensure_fbo(size);
	if(!_fitted)
		fit();
	render_scene(size);

	ImGui::Begin(title);
	Magnum::ImGuiIntegration::image(_color, Magnum::Vector2{size});
	ImGui::End();
}

} // namespace smg
```

- [ ] **Step 3: Append source to CMake** — `CMakeLists.txt:103-109` add `src/ScenePanel.cpp`.

- [ ] **Step 4: Build the library — expect GREEN**

Run: `cd build && cmake .. >/dev/null && cmake --build . --target smg 2>&1 | tail -8`
Expected: `smg` builds. (Confirm `PhongGL` setter names — `setTransformationMatrix`/`setNormalMatrix`/`setProjectionMatrix`/`setLightPositions`/`setAmbientColor`/`setDiffuseColor`/`setSpecularColor`/`setShininess` — and `Matrix4::normalMatrix()`, `Framebuffer::clearColor/clearDepth`, `RenderbufferFormat::Depth24Stencil8` against vendored headers; adjust if the build flags a signature.)

- [ ] **Step 5: Commit**

```bash
clang-format -i include/ScenePanel.hh src/ScenePanel.cpp
git add include/ScenePanel.hh src/ScenePanel.cpp CMakeLists.txt
git commit -m "scene: add ScenePanel core (static render to ImGui window)"
```

---

### Task 6: `ScenePanel` — camera input + MSAA

Add mouse orbit/pan/zoom (read from ImGui IO when the panel image is hovered) and an MSAA render path with blit-resolve. GL/GUI — compile-green gate; behavior confirmed in Task 7.

**Files:**
- Modify: `include/ScenePanel.hh` (add MSAA members + input helper declaration)
- Modify: `src/ScenePanel.cpp` (MSAA FBO + resolve; input handling in `draw`)

**Interfaces:**
- Consumes: everything from Task 5.
- Produces: unchanged public API; `draw()` now responds to mouse when hovered. New protected members: `bool _use_msaa{true}; int _samples{4}; Magnum::GL::Renderbuffer _colorMsaa, _depthMsaa; Magnum::GL::Framebuffer _msaaFbo;` and `void handle_input(const Magnum::Vector2& image_size);`.

- [ ] **Step 1: Add MSAA members + input declaration to the header**

In `include/ScenePanel.hh`, add to the protected section (after the non-MSAA GL members):

```cpp
	void handle_input(const Magnum::Vector2& image_size);

	bool _use_msaa{true};
	int _samples{4};
	Magnum::GL::Renderbuffer _colorMsaa{Magnum::NoCreate};
	Magnum::GL::Renderbuffer _depthMsaa{Magnum::NoCreate};
	Magnum::GL::Framebuffer _msaaFbo{Magnum::NoCreate};
```

- [ ] **Step 2: Build MSAA buffers in `ensure_fbo`**

In `src/ScenePanel.cpp`, at the end of `ensure_fbo(size)`, append:

```cpp
	if(_use_msaa) {
		_colorMsaa = Magnum::GL::Renderbuffer{};
		_colorMsaa.setStorageMultisample(
			_samples, Magnum::GL::RenderbufferFormat::RGBA8, size);
		_depthMsaa = Magnum::GL::Renderbuffer{};
		_depthMsaa.setStorageMultisample(
			_samples, Magnum::GL::RenderbufferFormat::Depth24Stencil8, size);
		_msaaFbo = Magnum::GL::Framebuffer{Magnum::Range2Di::fromSize({}, size)};
		_msaaFbo
			.attachRenderbuffer(Magnum::GL::Framebuffer::ColorAttachment{0}, _colorMsaa)
			.attachRenderbuffer(
				Magnum::GL::Framebuffer::BufferAttachment::DepthStencil, _depthMsaa);
	}
```

- [ ] **Step 3: Render to the MSAA FBO and resolve into the texture**

In `render_scene`, replace the single `_fbo...bind();` line with a selection of the draw target, and after drawing, resolve. Replace:

```cpp
	_fbo.clearColor(0, Magnum::Color4{0.12f, 0.12f, 0.14f, 1.0f})
		.clearDepth(1.0f)
		.bind();
```

with:

```cpp
	Magnum::GL::Framebuffer& target = _use_msaa ? _msaaFbo : _fbo;
	target.clearColor(0, Magnum::Color4{0.12f, 0.12f, 0.14f, 1.0f})
		.clearDepth(1.0f)
		.bind();
```

and, immediately before the closing `Magnum::GL::defaultFramebuffer.bind();`, add the resolve blit:

```cpp
	if(_use_msaa) {
		Magnum::GL::Framebuffer::blit(_msaaFbo, _fbo,
			Magnum::Range2Di::fromSize({}, size), Magnum::Range2Di::fromSize({}, size),
			Magnum::GL::FramebufferBlit::Color, Magnum::GL::FramebufferBlitFilter::Linear);
	}
```

(The resolve target `_fbo` owns the display `_color` texture, so the resolved image lands in the texture shown by ImGui.)

- [ ] **Step 4: Add `handle_input` and call it from `draw`**

Add the method:

```cpp
void ScenePanel::handle_input(const Magnum::Vector2& image_size) {
	(void)image_size;
	if(!ImGui::IsItemHovered())
		return;
	const ImGuiIO& io = ImGui::GetIO();
	const float dx = io.MouseDelta.x;
	const float dy = io.MouseDelta.y;
	if(ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		_camera.orbit(dx, dy);
	else if(ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
		_camera.pan(dx, dy);
	else if(ImGui::IsMouseDragging(ImGuiMouseButton_Right))
		_camera.zoom(-dy * 0.1f);
	if(io.MouseWheel != 0.0f)
		_camera.zoom(io.MouseWheel);
}
```

In `draw`, call `handle_input` right after the `image(...)` call (the image must be the last drawn item so `IsItemHovered` refers to it):

```cpp
	Magnum::ImGuiIntegration::image(_color, Magnum::Vector2{size});
	handle_input(Magnum::Vector2{size});
```

- [ ] **Step 5: Build the library — expect GREEN**

Run: `cd build && cmake .. >/dev/null && cmake --build . --target smg 2>&1 | tail -8`
Expected: `smg` builds. (Confirm `setStorageMultisample`, `Framebuffer::blit`, `FramebufferBlit::Color`, `FramebufferBlitFilter::Linear` against vendored headers.)

- [ ] **Step 6: Commit**

```bash
clang-format -i include/ScenePanel.hh src/ScenePanel.cpp
git add include/ScenePanel.hh src/ScenePanel.cpp
git commit -m "scene: add ScenePanel camera input and MSAA resolve"
```

---

### Task 7: Example, smoke test, umbrella export, full green

Wire the feature into the public umbrella header, ship a runnable example, add a GUI smoke test, and verify the whole desktop build is green and the WASM build still configures.

**Files:**
- Create: `examples/scene_panel.cpp`
- Create: `test/test_scene_panel.cpp`
- Modify: `cmake/smg.hh.in` (export new public headers)
- Modify: `examples/CMakeLists.txt` (add `scene_panel.cpp` to the **non-Emscripten** `example_list`)
- Modify: `test/CMakeLists.txt` (`test_list`: add `test_scene_panel.cpp`)

**Interfaces:**
- Consumes: the full public API (`GuiBase`, `ScenePanel`, `primitives`).

- [ ] **Step 1: Export new headers via the umbrella** — `cmake/smg.hh.in`

After the existing `#include "DrawCallback.hh"` / public-API block, add:

```cpp
// 3D scene viewport
#include "SceneTypes.hh"
#include "Camera.hh"
#include "Mesh.hh"
#include "Primitives.hh"
#include "ScenePanel.hh"
```

- [ ] **Step 2: Write the example** — `examples/scene_panel.cpp`

```cpp
// scene_panel example: orbit a cube + sphere + grid + axes
#include "GuiBase.hh"
#include "ScenePanel.hh"

#include <Magnum/Math/Matrix4.h>

using namespace Magnum;
using namespace smg;

// draw callback: render the panel each frame
int scene_callback(void* data) {
	ScenePanel* panel = reinterpret_cast<ScenePanel*>(data);
	panel->draw("3D Scene", Vector2i{800, 600});
	return 0;
}

class SceneExample: public GuiBase {
public:
	explicit SceneExample(const Arguments& arguments): GuiBase(arguments) {
		_panel = ScenePanel::create();
		_panel->add_grid();
		_panel->add_axes(1.5f);
		_panel->add_cube(Matrix4::translation({-1.5f, 0.5f, 0.0f}), Color3{0.9f, 0.4f, 0.3f});
		_panel->add_sphere(Matrix4::translation({1.5f, 0.5f, 0.0f}), Color3{0.3f, 0.6f, 0.9f});

		_cb = DrawCallback::create();
		_cb->set_callback(scene_callback);
		_cb->set_data(_panel.get());
		add_callback(_cb);
	}

private:
	ShScenePanelPr _panel;
	ShDrawCallbackPr _cb;
};

MAGNUM_APPLICATION_MAIN(SceneExample)
```

- [ ] **Step 3: Write the smoke test** — `test/test_scene_panel.cpp`

```cpp
// interactive smoke test: a scene panel renders cube/sphere/grid/axes.
// run manually (opens a window); registered with ctest like the other GUI tests.
#include "GuiBase.hh"
#include "ScenePanel.hh"

#include <Magnum/Math/Matrix4.h>

using namespace Magnum;
using namespace smg;

int panel_cb(void* data) {
	reinterpret_cast<ScenePanel*>(data)->draw("smoke", Vector2i{640, 480});
	return 0;
}

int main(int argc, char** argv) {
	GuiBase gui({argc, argv});

	ScenePanel panel;
	panel.add_axes();
	panel.add_cube();
	panel.add_sphere(Matrix4::translation({2.0f, 0.0f, 0.0f}));
	panel.add_grid();

	ShDrawCallbackPr cb = DrawCallback::create();
	cb->set_callback(panel_cb);
	cb->set_data(&panel);
	gui.add_callback(cb);

	bool done = false;
	while(!done)
		done = !gui.mainLoopIteration();
	gui.exit();
}
```

- [ ] **Step 4: Register example and test**

- `examples/CMakeLists.txt`: add `scene_panel.cpp` to the `else()` (non-Emscripten) `example_list` only.
- `test/CMakeLists.txt`: add `test_scene_panel.cpp` to `test_list`.

- [ ] **Step 5: Full desktop build — expect GREEN**

Run: `cd build && cmake .. >/dev/null && cmake --build . -j64 2>&1 | tail -8 && ls bin/ | grep -E 'scene_panel|test_scene_panel'`
Expected: build exit 0; both `scene_panel` and `test_scene_panel` binaries present.

- [ ] **Step 6: Run the headless unit tests — expect PASS**

Run: `cd build && ctest -R 'test_bounds|test_camera|test_mesh_bounds|test_primitives_axes' --output-on-failure`
Expected: 4/4 pass.

- [ ] **Step 7: Manual visual check (interactive)**

Run: `./build/bin/scene_panel`
Expected: a window with an ImGui panel "3D Scene" showing a grid, RGB axes, a red cube, and a blue sphere; left-drag orbits, middle-drag pans, right-drag / wheel zooms. Close the window to exit. (Skip if no display; note it was skipped.)

- [ ] **Step 8: Verify the WASM build still configures**

Run: `./scripts/build_wasm.sh 2>&1 | tail -15` (requires emsdk; if unavailable, note as not-run).
Expected: configuration succeeds. If the MSAA path fails under WebGL2, set `_use_msaa = false` for `CORRADE_TARGET_EMSCRIPTEN` in `ensure_fbo`/`render_scene` as the documented fallback, rebuild, and note the change.

- [ ] **Step 9: Commit**

```bash
clang-format -i examples/scene_panel.cpp test/test_scene_panel.cpp
git add examples/scene_panel.cpp test/test_scene_panel.cpp cmake/smg.hh.in examples/CMakeLists.txt test/CMakeLists.txt
git commit -m "scene: add scene_panel example, smoke test, umbrella export"
```

---

## Self-Review

**Spec coverage:**
- Architecture / embeddable widget → Tasks 5, 6, example in 7. ✓
- Scene model (retained `Mesh` + objects) → Tasks 3, 5. ✓
- Primitives (cube/sphere/plane/grid/axes) → Task 4. ✓
- Camera (orbit/pan/zoom, pivot, auto-fit, Y-up default) → Task 2; input wiring Task 6. ✓
- Lighting + shaders (Phong + Flat/VertexColor/MeshVisualizer) → `LightProperties` Task 1, shaders Tasks 5/6. ✓
- Rendering (FBO, depth, MSAA + resolve) → Tasks 5, 6. ✓
- WASM risk + non-MSAA fallback → Task 7 Step 8. ✓
- File layout (Mesh/Primitives/Camera/ScenePanel + example + test) → all tasks; umbrella export Task 7. ✓
- Testing (headless units + manual GUI smoke + full green) → Tasks 1-4 headless, 7 smoke/green. ✓
- Out-of-scope items (picking, materials, textures, multi-light, model loading, SceneGraph, on-demand redraw, GuiBase wrapper) → not implemented. ✓

**Placeholder scan:** No TBD/TODO; every code step has concrete content. GL signature uncertainty is handled by explicit per-task build gates, not deferred work.

**Type consistency:** `ShMeshPr` (Task 3) consumed in 4/5/6/7; `Bounds`/`compute_bounds` (Task 1) used in 2/3/4; `Camera` methods (`view`/`projection`/`eye`/`orbit`/`pan`/`zoom`/`fit`) defined in Task 2, called in 5/6; `ScenePanel::Object` fields and `add_*`/`draw` signatures consistent across 5/6/7. ✓
