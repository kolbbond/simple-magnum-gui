# Master-readiness scope — bloom + file dialog + cleanup

**Date:** 2026-06-26
**Branch:** `feature/scene-bloom` (on top of `cleanup`; both go to master together)

Consolidated scope for getting the current line of work ready for `master`. Four
independent workstreams; each feature gets its own spec → plan → implement cycle.

## A. Scene bloom (in progress)

Vendor `magnum-bloom` (van Nugteren, Unlicense) into `ScenePanel` as an optional
glow pass with a toggle + strength/radius sliders. **Desktop-only** (`GL330`).

- Spec: `2026-06-26-scene-bloom-design.md` (approved).
- **Phase 1 ✅** — sources vendored to `external/bloom/`, `PROVENANCE.md`,
  README acknowledgement. Committed (`c3327a3`).
- **Phase 2** — CMake wiring (desktop-only build + resource + `SMG_WITH_BLOOM`).
- **Phase 3** — `ScenePanel` integration, example tweak, sliders.
- **Phase 4** — `gui` smoke test.

## B. ImGuiFileDialog integration (new)

Vendor `ImGuiFileDialog` (Aiekick, **MIT**; Project-Rat fork, commit
`721e93f`) so smg consumers get a file open/save picker.

- **Pure Dear ImGui** — only needs `imgui.h` + `ImGuiFileDialogConfig.h` + std.
  No GL/Magnum. smg already runs ImGui's docking branch, which this targets.
- **Cross-platform** — emscripten-aware, so it builds on desktop *and* WASM
  (unlike bloom). `dirent/` is a Windows shim; `stb/` is optional thumbnails.
- Needs its own spec (open product decisions below). Proposed shape:
  vendor `ImGuiFileDialog.{h,cpp}` + config (+ `dirent/` for Windows) under
  `external/imguifiledialog/`, compile the `.cpp` into the `smg` library on all
  platforms, expose a thin `smg::FileDialog` wrapper (`open()` / `save()` →
  optional path) plus a demo example. Keep MIT LICENSE + a README credit.

### Decisions (B) — locked 2026-06-26

1. **Surface** — vendor + thin `smg::FileDialog` wrapper (`open()` / `save()` →
   optional path) + demo example. No `GuiBase` menu wiring.
2. **Thumbnails** — **skipped** for v1; do not vendor `stb/`.
3. **Platforms** — desktop **and** WASM (emscripten-aware).
4. **Windows** — vendor the `dirent/` shim so MSVC builds work.

## C. Cleanup → master (umbrella / housekeeping)

- Both desktop (`make`) and WASM (`scripts/build_wasm.sh`) configure + build with
  A and B present.
- `ctest -L unit` stays green (headless); `gui` smoke tests build.
- README acknowledgements cover both vendored libs.
- Squash/curate commits so the bloom + cleanup history reads cleanly for the PR.

## D. CI (deferred — separate PR)

No CI exists today. Decision: **defer.** Standing up CI requires building the
corrade/magnum/plugins/integration/implot submodules + SDL2/freetype deps before
`ctest -L unit` can run; the `gui` tests need xvfb. That is its own focused PR
and should not ride along with bloom/file-dialog. Tracked, not done here.

## Testing posture (applies to A + B)

- Bloom: `GL330` context required → `gui` smoke test only (no headless unit
  test is honestly possible). Header-compile coverage via existing
  `test_compile_headers.cpp`.
- File dialog: pure ImGui, but still needs an ImGui context to exercise →
  `gui` smoke test; a headless `unit` test can at least construct/configure the
  dialog instance and check the singleton/API compiles and links.

## Sequencing

A (finish Phases 2–4) → B (spec → implement) → C (build matrix + history) →
open PR. D follows as a separate PR.
