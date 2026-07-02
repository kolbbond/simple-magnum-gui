# Design: `smg` prerendered isometric sprites

Date: 2026-06-27
Status: approved (design), pending implementation plan

## Purpose

Give `simple-magnum-gui` the **runtime ability and API** to display prerendered
2D sprites on an isometric plane — the classic technique behind kingdom-sim /
RTS games like *Majesty: The Fantasy Kingdom Sim*: directional, animated
hero/creature sprites and large static building sprites placed on a tiled iso
terrain, with spell glows riding the existing bloom pass.

Scope is **runtime renderer only**. `smg` consumes sprite sheets it did not
create (authored in Blender, etc.). A baking tool (render `smg`'s own 3D meshes
to atlases) and any game logic (entities, AI, economy) are explicitly out of
scope — this repo supplies the ability and API, not a game.

This slice builds on the existing `ScenePanel` 3D foundation
(`2026-06-19-scene-panel-3d-viewport-design.md`) and reuses its FBO / MSAA /
ImGui-display / bloom plumbing rather than introducing a parallel renderer.

## Background: what already exists

- `ScenePanel` — retained 3D scene rendered into an off-screen MSAA FBO, blitted
  into an ImGui window. `PhongGL` for lit meshes, `VertexColorGL3D` for
  lines/wireframe. Optional `BloomRenderer` post-pass (gated `SMG_WITH_BLOOM`,
  composed in, desktop-only today).
- `Camera` — orbit (yaw/pitch/zoom/pan), **perspective only**, Y-up or Z-up.
- `Mesh` — wraps `GL::Mesh` + AABB. `Vertex` has position/normal/color, **no UVs**.
- No texturing path: no image importer, no textured shader, no sprite/billboard.

The gap for iso sprites is the entire texturing + iso-projection layer. None of
the core sprite rendering needs a new dependency — `Magnum::Shaders::FlatGL3D`
(textured, alpha-mask) is in core Magnum. Only file loading adds a plugin.

## Architecture decision

Reuse the 3D `ScenePanel` pipeline: draw sprites as **textured, camera-facing
billboard quads in world space** under an **orthographic isometric camera**,
sharing the existing depth buffer so sprites occlude (and are occluded by) live
3D meshes and each other. Rejected alternative: a separate 2D screen-space sprite
layer — it would duplicate the render/display/bloom plumbing and could not mix
with 3D content. A 2D-only layer would only win for pixel-perfect HUD sprites
with no 3D interaction, which is not this use case.

## Module layout (new files, existing conventions)

| File | Responsibility |
|------|----------------|
| `include/SpriteTypes.hh` | Header-only value types: `Anchor` enum, `SpriteParams`, `SpriteClip`, `dir_row()` free fn, `SpriteBlend` enum. Headless. |
| `include/SpriteSheet.hh` / `src/SpriteSheet.cpp` | Texture + grid-of-frames; frame→UV math. `from_texture`/`from_pixels` always; `load(path)` gated `SMG_WITH_IMAGE_IMPORT`. |
| `include/IsoGrid.hh` / `src/IsoGrid.cpp` | `tile(i,j) ↔ world(x,z)` mapping on the ground plane. Pure value type, headless. |
| `src/SpriteRenderer.hh` / `src/SpriteRenderer.cpp` | The GL sprite pass (billboard quad, `FlatGL3D` textured + alpha-mask, blended category). Composed *into* `ScenePanel`, mirroring `BloomRenderer`. Keeps `ScenePanel.cpp` lean. |

Modified: `Camera` gains orthographic mode + `iso()` preset + `unproject()`.
`ScenePanel` gains `add_sprite()` / `sprite(handle)` accessors and a
`tile_under_cursor()` helper, delegating GL work to `SpriteRenderer`.

## Scene model

`ScenePanel` keeps a parallel `std::vector<Sprite>` alongside `_objects`:

```cpp
struct Sprite {
    ShSpriteSheetPr sheet;
    int frame{ 0 };
    Magnum::Vector3 position;            // world anchor point
    Magnum::Vector2 size{ 1.0f, 1.0f };  // world units (W,H)
    Anchor anchor{ Anchor::BottomCenter };
    Magnum::Color4 tint{ 1.0f };
    SpriteBlend blend{ SpriteBlend::AlphaMask };
    bool visible{ true };
};
```

`add_sprite(...)` returns a `std::size_t` handle; `sprite(handle)` returns a
mutable reference so the caller advances `frame` each tick (from a `SpriteClip`
and/or `dir_row()`). This matches the existing `object(handle)` pattern.

## Public API surface

```cpp
// sheet
ShSpriteSheetPr sheet = SpriteSheet::from_texture(std::move(tex), /*cols*/8, /*rows*/6);
// SpriteSheet::from_pixels(span, size, format, cols, rows);
// SpriteSheet::load("orc.png", cols, rows);          // only if SMG_WITH_IMAGE_IMPORT
Magnum::Range2D uv = sheet->frame_uv(frameIndex);     // normalized UV rect

// place on the iso plane (BottomCenter anchor => feet on the tile)
IsoGrid grid{ /*tile_size*/1.0f };
std::size_t h = panel->add_sprite(sheet, /*frame*/0, grid.to_world({ 3, 5 }), SpriteParams{});
panel->sprite(h).frame = walk.frame_at(t);            // caller advances animation

// optional helpers (separable; core works without them)
SpriteClip walk{ "walk", /*first*/0, /*last*/7, /*fps*/12.0f };
int f   = walk.frame_at(elapsedSeconds);              // loops
int row = dir_row(facingDeg, /*numDirs*/8);           // facing angle -> sheet row

// iso camera
panel->camera().iso();                                // ortho + 2:1 dimetric preset

// picking
std::optional<Magnum::Vector3> p = panel->cursor_world();      // ground-plane hit
std::optional<Magnum::Vector2i> cell = panel->tile_under_cursor(grid);
```

## Rendering pass — shared depth, two categories

`ScenePanel::render_scene` order, all into the same FBO:

1. **Opaque 3D meshes** (existing Phong/VertexColor pass) — depth write on.
2. **Alpha-mask sprites** (`SpriteBlend::AlphaMask`) — `FlatGL3D` with
   `Flag::Textured | Flag::AlphaMask` + `setAlphaMask(threshold)`: `discard`
   below threshold, depth write on. Upright camera-facing billboards that are
   spatially separated occlude correctly with **no manual sort** (a nearer
   card's pixels are uniformly nearer; a building card hides a hero on a farther
   tile). This covers buildings and units.
3. **Translucent/additive sprites** (`SpriteBlend::Alpha` / `Additive`) — spell
   glows, auras, soft shadows. Blending on, depth **test** on but depth **write**
   off, drawn back-to-front sorted by camera-space depth key. Additive variant
   pairs with bloom for the "magic" glow.

Billboards are **upright and camera-facing** (locked to world up; quad oriented
from the camera's right + world-up). Because the iso camera is fixed, the
orientation is constant and recomputed only when the camera changes.

Optional smooth edges (desktop): enable sample-alpha-to-coverage on the existing
MSAA target for order-independent anti-aliased sprite edges. Falls back to plain
alpha-mask on WASM (MSAA already disabled there).

Bloom, when enabled, applies to sprites for free — it is a post-pass on the
composited FBO.

## Iso camera + grid + picking

- `Camera` gains `enum class Projection { Perspective, Orthographic }`,
  `set_projection()`, and an ortho extent derived from `fit()`/`distance` so zoom
  still works. `projection(aspect)` branches on the mode (ortho via
  `Matrix4::orthographicProjection`).
- `Camera::iso()` preset: ortho + yaw 45°, pitch **≈26.57° (2:1 dimetric)** for
  the Majesty look. The pitch is exposed so true isometric (≈35.26°) is one call
  away.
- `IsoGrid{ tile_size }` maps cell↔world on the ground plane, honoring the
  camera's `UpAxis` (XZ for Y-up, XY for Z-up).
- `Camera::unproject(screenPos, viewportSize)` returns a world-space ray;
  `ScenePanel::cursor_world()` intersects it with the ground plane and
  `tile_under_cursor(grid)` rounds to a cell. Headless-testable given a fixed
  camera + viewport.

## Build, platform

- **No new core dependency.** Sprite rendering uses core `Magnum::Shaders::FlatGL3D`.
- `SpriteSheet::load(path)` adds `MagnumPlugins AnyImageImporter` +
  `StbImageImporter`, compiled only under `SMG_WITH_IMAGE_IMPORT`, gated exactly
  like `SMG_WITH_BLOOM`. `from_texture`/`from_pixels` are always available.
- WASM: textured quads + alpha-mask work on WebGL2; alpha-to-coverage falls back
  off; importer stays gated.

## Testing

Headless unit tests (matching existing `test/` pattern):

- `SpriteSheet::frame_uv` — frame index → correct normalized UV rect (grid math).
- `SpriteClip::frame_at` — looping, fps timing, range bounds.
- `dir_row` — facing angle → row for 4/8 directions, wrap-around.
- `IsoGrid` — `to_world`/`to_cell` round-trip, Y-up and Z-up.
- `Camera` — orthographic projection matrix; `unproject` ground hit for a known
  camera/viewport.

GUI smoke test + `examples/sprite_panel.cpp`: a few sprites (from `from_pixels`,
or `load` when enabled) on an iso grid under the ortho camera, an animated
directional sprite, and one additive glow sprite over bloom.

## Out of scope (later slices)

- Sprite-baking tool (3D mesh → atlas).
- Tile terrain rendering / autotiling, fog-of-war, minimap.
- Entity system, selection state, AI, game logic.
- On-demand redraw, sprite instancing/batching for large counts.
