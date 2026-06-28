# Scene Bloom — vendor magnum-bloom into `ScenePanel`

**Date:** 2026-06-26
**Branch:** `feature/scene-bloom`
**Goal:** Pull the physics-based bloom post-process effect from `../magnum-bloom`
into `simple-magnum-gui` so the embedded 3D `ScenePanel` viewport renders with a
tunable bloom glow.

## Background

`ScenePanel` renders a retained scene into an off-screen `_color` texture
(MSAA → blit) and displays it inside an ImGui window via
`Magnum::ImGuiIntegration::image(_color, …)`.

`magnum-bloom` (Jeroen van Nugteren, Unlicense / public domain) is a mip-chain
physics-based bloom renderer for Magnum. Its API:

1. `initialize(viewportSize, force=false)` — allocate the mip-chain FBOs.
2. `render_bloom_texture(srcTex)` — downsample/upsample `srcTex` into a blurred
   mip chain.
3. `render_final(srcTex)` — composite `srcTex` + bloom into the **currently
   bound** framebuffer.

Bloom inserts between scene render and ImGui display: after `_color` is
produced, run `render_bloom_texture(_color)`, composite into a new post-process
FBO, and hand that texture to ImGui instead of `_color`.

## Hard constraints (verified in source)

- **Desktop-only.** Bloom shaders compile at `GL330` and the renderer asserts
  `MAGNUM_ASSERT_GL_VERSION_SUPPORTED(Magnum::GL::Version::GL330)`. It cannot run
  on the Emscripten/WebGL2 build. All bloom code is guarded with
  `SMG_WITH_BLOOM` (CMake, desktop only) and `#ifndef CORRADE_TARGET_EMSCRIPTEN`.
  On WASM, `ScenePanel` falls back to displaying `_color` directly (current
  behaviour).
- **Shaders via Corrade resources.** Bloom loads its six shaders from a Corrade
  resource group named `"data"` (`resources.conf` + `corrade_add_resource`). smg
  already uses `corrade_add_resource` for fonts/images; we add a third resource
  object to the `smg` library on desktop builds. Confirm `"data"` does not
  collide with smg's existing resource group names; rename the group if it does.
- **License is clean.** Unlicense / public domain — vendoring (copying) the
  sources is permitted.

## Decisions

- **Vendoring:** copy the sources (not a submodule). Bloom is a tiny, stable,
  public-domain unit; its own CMakeLists is example-shaped and not consumable as
  `add_subdirectory`.
- **UI:** a bloom on/off toggle plus strength and filter-radius sliders inside
  the `ScenePanel` window.

## Design

### 1. Vendoring layout

Copy bloom verbatim into a self-contained third-party directory:

```
external/bloom/
  include/   bloomrenderer.hh, bloomfbo.hh, bloomdownsample.hh,
             bloomupsample.hh, bloomfinal.hh
  src/       bloomrenderer.cpp, bloomfbo.cpp, bloomdownsample.cpp,
             bloomupsample.cpp, bloomfinal.cpp
  shaders/   bloom_downsample.{vert,frag}, bloom_upsample.{vert,frag},
             bloom_final.{vert,frag}
  resources.conf
  PROVENANCE.md   (upstream URL + commit hash + Unlicense note)
```

- Namespace `bloom::`, include guards, and the `"data"` resource group stay
  intact. **No edits to the copied code** (except a possible `"data"` → unique
  rename if a collision is found — that touches `resources.conf` plus the three
  `Magnum::Utility::Resource rs{"data"}` call sites).
- `resources.conf` keeps relative `shaders/…` paths; `corrade_add_resource`
  resolves them relative to the `.conf` file's directory, so the layout above
  works unchanged.

### 2. CMake wiring — desktop only

Inside the existing `if(NOT SMG_PLATFORM_WASM)` path in the top-level
`CMakeLists.txt`:

- `set(SMG_BLOOM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/bloom)`
- append the five `${SMG_BLOOM_DIR}/src/*.cpp` to `smg_sources`
- `corrade_add_resource(smg_RESOURCE_bloom "${SMG_BLOOM_DIR}/resources.conf")`
- add `${smg_RESOURCE_bloom}` to the `smg` library sources (desktop branch)
- add `${SMG_BLOOM_DIR}/include` to the `smg` target includes
- `target_compile_definitions(smg PUBLIC SMG_WITH_BLOOM)` on desktop

On WASM none of this is added, so the WebGL build is untouched and bloom is
absent. Bloom needs no extra Magnum components beyond what `smg` already links
(`GL`, `Shaders`, `MeshTools`, `Trade`, `Primitives`); it does **not** need
`Magnum::Application` (only the upstream example did).

### 3. `ScenePanel` integration

All additions guarded by `#ifdef SMG_WITH_BLOOM` (which implies non-WASM).

New members:

- `bloom::ShBloomRendererPr _bloom;`
- post-process FBO: `Magnum::GL::Texture2D _postColor{NoCreate};`
  `Magnum::GL::Framebuffer _postFbo{NoCreate};` (color-only, RGBA8 — the
  composite pass needs no depth)
- `bool _bloom_enabled{true};`
- `float _bloom_strength{0.3f};` (upstream example value; default member field is
  0.04, too subtle for an LDR scene)
- `float _bloom_radius{0.005f};`

Flow:

- `ensure_fbo(size)` also (re)builds `_postFbo` / `_postColor` and calls
  `_bloom->initialize(size)` when the size changes (create the renderer lazily
  via `bloom::BloomRenderer::create()` on first use).
- `render_scene(size)` is **unchanged** — still produces `_color`.
- New compositing step in `draw()`:
  - if bloom enabled: push current strength/radius (`set_bloom_strength`,
    `set_filter_radius`), `render_bloom_texture(_color)`, bind `_postFbo` and
    clear, `render_final(_color)`, then rebind `Magnum::GL::defaultFramebuffer`;
    the texture passed to `ImGuiIntegration::image(...)` is `_postColor`.
  - if bloom disabled (or WASM): display `_color`, as today.
- Controls inside the panel window: `ImGui::Checkbox("Bloom", &_bloom_enabled)`
  and two `ImGui::SliderFloat` for strength and filter radius. Compiled out on
  WASM.

### 4. Known integration risk (verify during implementation)

Bloom's `render_bloom_texture` / `render_final` set GL viewports for the mip
chain. After they run, confirm the viewport is correct for `_postFbo`
(`Framebuffer::bind()` resets the viewport to the framebuffer's construction
rect, which should suffice). If bloom leaves the viewport at the window size,
set it explicitly before `render_final`. **Read `bloomrenderer.cpp`'s viewport
calls during implementation rather than assume.**

### 5. Example + verification

- Update `examples/scene_panel.cpp` to add a bright/emissive-looking object so
  the bloom is visibly demonstrated.
- **Verification gates:**
  - Desktop build (`make`) succeeds; run `scene_panel` and confirm the glow.
  - WASM cross-build (`scripts/build_wasm.sh`) still compiles with bloom absent
    (guards hold).
  - Existing headless unit tests stay green.
- **Testing note:** bloom requires a real GL330 context plus the resource group,
  so it is not cleanly headless-testable. Visual verification is via the example;
  no fake/headless bloom test will be added.

## Out of scope (YAGNI)

- HDR float pipeline — bloom runs on the existing RGBA8 `_color`, matching the
  upstream example.
- Per-object emissive material system — diffuse brightness drives the effect.
- WASM/WebGL bloom — hard `GL330` requirement.
- Tracking bloom upstream via submodule — vendored copy with `PROVENANCE.md`.
