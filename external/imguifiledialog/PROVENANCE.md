# Vendored: ImGuiFileDialog

A file selection dialog for Dear ImGui, used by smg's `FileDialog` wrapper to
offer open/save pickers to consumers.

- **Original author:** Stephane Cuillerdier (aka Aiekick) —
  https://github.com/aiekick/ImGuiFileDialog
- **License:** MIT (see `LICENSE`)
- **Vendored from:** the Project-Rat fork
  https://gitlab.com/Project-Rat/ImGuiFileDialog
- **Commit:** `721e93f90c46c8de919cfcb36287357b00f7b1b9`
  (= aiekick `v0.6.7-82` + a local fix)
- **Fork delta:** one bugfix by Jeroen van Nugteren on top of upstream —
  *"[FIX] guard against duplicate places groups on re-init"* (`ImGuiFileDialog.cpp`),
  which prevents `m_InitPlaces` from registering bookmark/device groups more than
  once across re-initialization. Dormant unless `USE_PLACES_FEATURE` is enabled,
  but kept for correctness when it is.

## What was copied

`ImGuiFileDialog.{h,cpp}`, `ImGuiFileDialogConfig.h`, the upstream `LICENSE`, and
`dirent/` (a POSIX-`dirent` shim used on Windows). The `stb/` thumbnail helpers,
demo app, CMake, CI, and docs were **not** copied.

## Configuration choices

- **`ImGuiFileDialogConfig.h` is upstream-default**, which means:
  - `USE_STD_FILESYSTEM` off → directory iteration via `dirent`: the system
    `<dirent.h>` on Linux/WASM (emscripten provides it), the bundled
    `dirent/dirent.h` on Windows.
  - `USE_THUMBNAILS` off → no `stb` dependency.
  - `USE_PLACES_FEATURE` off.
- Builds on **desktop and WASM**. It depends only on Dear ImGui (which smg
  already provides via `MagnumIntegration::ImGui`) plus the standard library.

## Updating

Re-copy the same file set from the Project-Rat fork and bump the commit hash
above. Credit Aiekick (MIT) as the original author.
