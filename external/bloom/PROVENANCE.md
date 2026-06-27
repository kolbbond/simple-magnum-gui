# Vendored: magnum-bloom

Physics-based bloom post-process effect for Magnum, used by `smg::ScenePanel`
to add an optional glow pass to the embedded 3D viewport.

- **Author:** Jeroen van Nugteren
- **Upstream:** https://gitlab.com/jeroen.van.nugteren/magnum-bloom
- **Commit:** `b24597bffa10e17a204b10087b62d5e4dfa5c9f1`
- **License:** Unlicense / public domain (see the header in every source file)
- **Technique:** mip-chain physically-based bloom, after
  https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

## What was copied

The renderer sources (`src/`, `include/`), its six GLSL shaders (`shaders/`),
and the Corrade resource manifest (`resources.conf`). The upstream
`CMakeLists.txt` and SDL2 example were **not** copied — smg builds these sources
directly into the `smg` library (desktop only) and drives bloom from
`ScenePanel`.

## Local modifications

None. The sources are byte-for-byte upstream. The shaders are loaded from the
Corrade resource group `"data"` (no collision with smg's `font` / `image`
groups). If this is ever updated from upstream, re-copy the same file set and
bump the commit hash above.

## Constraints

The shaders require desktop `GL330`; the renderer asserts
`MAGNUM_ASSERT_GL_VERSION_SUPPORTED(GL330)`. Bloom is therefore compiled and
used on desktop builds only (`SMG_WITH_BLOOM`), and is absent from the
Emscripten/WebGL2 build.
