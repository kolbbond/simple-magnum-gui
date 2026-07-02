# Design: `smg::ScenePanel` — embeddable 3D scene viewport

Date: 2026-06-19
Status: approved (design), pending implementation plan

## Purpose

Give `simple-magnum-gui` a generic, domain-free 3D rendering foundation aimed at
gamedev-style use: meshes, primitives, an orbit camera, and Phong lighting,
rendered into an embeddable ImGui widget.

This is a port of the reusable gamedev parts of the sibling project
`goose-gui` (its `ScenePanel`, `Mesher`, `Mesh`, camera/orbit controls, Phong
lighting, axes gizmo), stripped of goose's EM/FEM domain coupling (`rat::`,
Armadillo, coil networks) and with the gap goose never filled — built-in
primitive generation — added on top.

It is the **foundation slice**. Picking, gizmos, materials, textures, multiple
lights, model loading, and a SceneGraph are explicitly deferred to follow-up
slices that build on this.

## Background: what we are porting from

`goose-gui` (`/home/ohr4/programs/cxx/goose/goose-gui`) already implements, but
entangled with its domain:

- `ScenePanel` — self-contained 3D viewport embedded in an ImGui window via
  render-to-texture (FBO + MSAA resolve).
- Camera — orbit/pan/zoom around a pivot, isometric default, auto-center on
  mesh AABB, FOV/zoom control.
- `Mesh` — wraps vertices + indices into `Magnum::GL::Mesh` (Points/Lines/Triangles).
- Phong lighting — single light, `LightProperties` struct (ambient/diffuse/
  specular/shininess).
- Axes gizmo — scale-aware XYZ lines.

What goose lacks and we add: **primitive generators** (cube/sphere/plane/grid)
wrapping `Magnum::Primitives`.

`simple-magnum-gui` today: subclass `GuiBase` (a `Magnum::Platform::Application`),
register `DrawCallback`s (raw `int(*)(void*)` + `void*`), and call ImGui/ImPlot
inside them. The viewport must compose with this, not replace it.

## Architecture / API

A standalone `smg::ScenePanel`. The user creates one and calls
`panel.draw("3D View")` inside any ImGui draw-callback. `GuiBase` is untouched —
no coupling, no base-class bloat. Multiple panels are allowed (e.g. perspective
+ top-down). A `GuiBase` convenience wrapper may be added in a later slice if the
boilerplate proves annoying (YAGNI for now).

Internally `draw()`:
1. renders the scene into an off-screen FBO (depth + MSAA), resolves to a texture;
2. shows that texture inside an ImGui child window via
   `Magnum::ImGuiIntegration::image`;
3. handles camera input when the image is hovered, read from ImGui IO.

v1 redraws every frame (simpler than goose's on-demand `_needs_redraw`); on-demand
redraw is a later optimization.

## Scene model (retained)

Two layers, mirroring goose but domain-free:

- `smg::Mesh` — thin wrapper owning a `GL::Mesh` built once from vertices +
  indices, with a `MeshPrimitive` (Points/Lines/Triangles). A GPU resource,
  created up front, shared via `shared_ptr` (`ShMeshPr`), matching smg's existing
  factory/`create()` convention.
- `ScenePanel` holds a list of **objects**:
  `{ ShMeshPr mesh, Matrix4 transform, Color3 color, bool wireframe, bool visible }`.

API surface:
- `add(mesh, transform = identity, color = white) -> ObjectHandle`
- primitive conveniences: `add_cube()`, `add_sphere()`, `add_plane()`,
  `add_grid()`, `add_axes()`
- `clear()`
- mutate an object's `transform` / `color` / flags via its handle

No SceneGraph — raw `Matrix4` transforms per object, like goose.

## Primitives

`smg::primitives` (free functions or a `Primitives` helper) wrap
`Magnum::Primitives`, each returning a `ShMeshPr`:

- `cube()` — `Primitives::cubeSolid` (with normals for Phong)
- `sphere()` — `Primitives::icosphereSolid(subdivisions)`
- `plane()` — `Primitives::planeSolid`
- `grid()` — ground grid lines (`Primitives::grid3DWireframe` or hand-built lines)
- `axes()` — XYZ colored lines (ported from goose `create_axes_mesh`)

## Camera + input

`smg::Camera`: orbit/pan/zoom around a pivot, perspective projection.

- **Auto-center + auto-fit** on the scene AABB (port goose's fit math:
  `distance = model_scale / (2 * tan(fov/2))`).
- Mouse, only when the panel image is hovered (ImGui IO): LMB = orbit (pivot-aware
  rotation), MMB = pan (scaled by model size), RMB / scroll = zoom.
- Configurable FOV, near/far.
- **Up-axis: Y-up default** (gamedev convention), settable to Z-up. This is the
  one convention flipped relative to goose (which is Z-up).

## Lighting + shaders

- Single global Phong light with a `LightProperties` / material struct
  (ambient/diffuse/specular/shininess) ported from goose. Per-object base
  `Color3`.
- Shaders are Magnum built-ins, no custom GLSL:
  - `PhongGL` — lit solids (with `Flag::VertexColor` where per-vertex color is used)
  - `FlatGL3D` / `VertexColorGL3D` — axes / grid lines (unlit)
  - `MeshVisualizerGL3D` — wireframe (`Flag::Wireframe`)

## Rendering

Render-to-texture with a depth attachment, MSAA + resolve (goose's FBO pattern:
multisampled renderbuffers → blit-resolve into the display texture → mipmap).
Depth test + back-face culling enabled for the 3D pass; restored after. Desktop
is the primary target.

## Out of scope for v1

Picking / selection, gizmo manipulation, materials beyond a base color, textures,
multiple lights, model loading (glTF/OBJ/FBX), SceneGraph, on-demand redraw,
`GuiBase` convenience wrapper. Each is a candidate follow-up slice.

## Risks

- **WASM/WebGL2**: the MSAA-resolve-to-texture path may not port cleanly. v1
  keeps the WASM build compiling but may need a non-MSAA fallback there; not
  solved in v1, flagged here.
- **CMake**: `smg` must gain `Magnum::Primitives` and the 3D shader components
  (`Shaders` already linked for 2D; need the 3D variants). Verify the superbuild
  enables these Magnum features.

## File layout

New:
- `include/Mesh.hh` + `src/Mesh.cpp` — `smg::Mesh` wrapper
- `include/Primitives.hh` + `src/Primitives.cpp` — primitive generators
- `include/Camera.hh` + `src/Camera.cpp` — orbit camera
- `include/ScenePanel.hh` + `src/ScenePanel.cpp` — the widget
- `examples/scene_panel.cpp` — cube + sphere + grid + axes, orbitable
- `test/test_scene_panel.cpp` — compiling smoke test (manual GUI run, like
  the other GUI tests)

Touched:
- `CMakeLists.txt` — link `Magnum::Primitives` + 3D shaders; add new sources
- `cmake/smg.hh.in` — export the new public headers via the umbrella
- `examples/CMakeLists.txt`, `test/CMakeLists.txt` — register new targets

## Testing

- `test_scene_panel.cpp`: builds green, registered with ctest; interactive GUI
  smoke test run manually (consistent with the existing `test_magnum_*` tests).
- `examples/scene_panel.cpp`: the living demo — a scene with a cube, sphere,
  ground grid, and axes you can orbit/pan/zoom.
- Full `cmake --build` must stay green on desktop; WASM build must still configure
  and build (with fallback if needed).
