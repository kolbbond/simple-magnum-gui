# smg roadmap: library features beyond smg-desk

Surveyed 2026-11-07 (read-only). It covers the public API, all examples, CI, packaging and plans. No GitHub issues were open. smg-desk, X11 click-through and the dependency bump are tracked in `../plans/2026-11-07-smg-desk.md` and are not repeated here.

The theme: the examples (files, fview, csvplot, diskusage, sysinfo, overlay, visualizer) have outgrown the library. Each re-implements the same plumbing, so the biggest wins move that plumbing into smg.

## First five

| # | What | Why (evidence) | Size |
|---|---|---|---|
| 1 | Commit `.clang-format` / `.clang-tidy` / `.clangd`; add a `clang-format --dry-run -Werror` CI step | CLAUDE.md says they're in the repo root, but they aren't. Tools fall back to `~/defcon/dotfiles`, whose style differs from the code, so formatting a file rewraps unrelated lines | S |
| 2 | Run the `example_*` screenshot tests in CI and upload the PNGs | `examples/CMakeLists.txt:100-120` registers them, but `ci.yml` only runs `-L unit` / `-L gui`, so about 8k lines of tools never run in CI | S |
| 3 | `smg/util`: `format_size`, `to_lower`, time formatting, `open_with_system`, `places()` | Duplicated in files.cpp:71-96,216,232 / fview.cpp:119-144,232 / diskusage.cpp:51,227. **Bug:** `open_with_system` forks `xdg-open` and never reaps it, leaving zombies (files.cpp:221) | S |
| 4 | Per-app ImGui settings: `GuiConfig::settings` (none / cwd / XDG `$XDG_CONFIG_HOME/smg/<title>.ini`) plus a small key/value store | `io.IniFilename` is never set, so every installed `smg-<tool>` writes `imgui.ini` into whatever directory it was launched from | S |
| 5 | `gui.args()`: argv with `--smg-*` / `--magnum-*` removed | fview.cpp:1853 and csvplot.cpp:519 skip these by hand, inconsistently. csvplot treats a value after unknown smg flags as a file | S |

## Library API

- **Drag-and-drop:** `gui.on_drop(fn)` / `dropped()`. csvplot subclasses GuiBase just to catch `SDL_DROPFILE` (csvplot.cpp:496). S
- **Fonts and themes:** `gui.font(Font::Mono)`, `GuiConfig::font_size`, `smg::apply_theme()`. Six fonts load at a fixed 18 px into protected `_fonts`, and the style is hard-coded (GuiBase.cpp:160-200). smg-desk's style becomes one theme among several. Do this **after** the ImGui 1.92 integration bump, which changes font handling. S/M
- **`smg::fullscreen_window(id)`:** replaces the viewport-sized host-window block copied into 7 examples. S
- **`smg::clipboard`:** `set_image` / `get_image` / `files`, promoted from about 500 lines in fview.cpp:587-1068 (Win32 / wl-copy / PowerShell). M

## Components extracted from examples

- **`smg::Texture` / `load_texture(path)`** plus pixel readout. Importer-to-`Texture2D` code is repeated in files.cpp:304 and fview.cpp:570,803,1281. M
- **`smg::BackgroundJob<T>`:** run on a worker, poll each frame, cancel. Replaces five hand-rolled thread-plus-atomic classes: diskusage Scanner, fview Scanner/Worker, files Search, csvplot Loader. `sys::Command` is a sixth. M
- **`Csv.hh` + `smg::FileWatch`:** `parseNumber` / `splitLine` / `detectDelimiter` / `trim` are copied verbatim in csvplot.cpp:57-115 and fview.cpp:399-458, and `sys::parse_number` is a third number parser. csvplot's follow loop (:295) becomes `FileWatch`. S/M
- **Widgets:** `smg::treemap()` (diskusage `squarify`) and a sortable table (sysinfo processes). Overlay's `CpuLoad` should use `smg::sys`. M

## Platform, infra and docs

- **Windows CI:** a `windows-latest` + vcpkg job that builds, runs `-L unit` and builds the cpack zip. Right now none of the Win32 code (`smg::sys`, clipboard, fview WSL) is compiled in CI. M
- **Releases:**
  - Leave `VERSION 0.1.0` and tag releases.
  - Tag-triggered artifacts: a Windows zip and a Linux tools tarball.
  - A CHANGELOG.
  - An Arch `PKGBUILD` for the `smg-*` tools.
  
  S/M
- **API docs:** Doxygen published to the existing Pages site. S
- **Housekeeping:**
  - `include/Events.hpp` is the only header not named `.hh`.
  - `specs/2026-06-26-master-readiness-scope.md:54` still says "No CI exists".
  - `lib/ImGuiFileDialog/` is a stale untracked copy.
  
  S

## Suggested sequencing

1. **Do now; they don't depend on the dependency bump:** the first five, then BackgroundJob and Csv/FileWatch.
2. **The dependency bump (smg-desk plan, phase 6) comes before** the font/theme API and any new ImPlot-heavy widgets, so they're written against ImGui 1.92 dynamic fonts and `ImPlotSpec` once.
3. **Windows CI before** moving more Win32 code (clipboard) into the library.
