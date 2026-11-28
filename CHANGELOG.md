# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

To release: move the Unreleased entries into a new `## [X.Y.Z] - YYYY-MM-DD` section, commit, then
push a `vX.Y.Z` tag. `release.yml` builds both packages, smoke-tests them and attaches them with
their `.sha256` files; that section becomes the release notes.

## [Unreleased]

## [0.1.0] - 2026-11-28

First tagged release.

### Added

- Prebuilt binaries on GitHub Releases, built by a tag-triggered workflow (`.github/workflows/release.yml`):
  - `smg-tools-<version>-linux-x86_64.tar.gz`: relocatable `bin/smg-*` tools with private
    smg/Magnum libraries and plugins (`scripts/package_linux.sh`)
  - `smg-examples-<version>-win64.zip`: the examples plus every DLL and Magnum plugin they load
  - a `.sha256` file for each, in `sha256sum` format
- Arch package `smg-tools-bin` for the release tarball (`packaging/arch/PKGBUILD`).
- `GuiBase`: window plus ImGui/ImPlot/ImPlot3D context and a per-frame callback loop; MSAA, HiDPI
  fonts, docking.
- `ScenePanel`: 3D viewport inside an ImGui window with orbit camera, meshes, bloom and iso sprites.
- `Plot`/`Plot3D` helpers over ImPlot, `Anim`/`Timeline` keyframes and easing, on-screen annotations.
- `FileDialog` open/save pickers (ImGuiFileDialog).
- Overlays: transparent, borderless, always-on-top windows with click-through (Windows, Wayland/Hyprland).
- Capture: screenshots, frame recording and hidden off-screen rendering, from code or via
  `--smg-frames`, `--smg-screenshot`, `--smg-record`, `--smg-hidden`.
- `smg::sys` data module (hwmon, NVML, command sources).
- Examples: `guibase`, `draw_triangle`, `draw_cube`, `implot_ex`, `scene_panel`, `sprite_panel`,
  `file_dialog`, `csvplot`, `diskusage`, `files`, `fview`, `sysinfo`, `visualizer`, `overlay`.
- Linux tool installs (`make install-tools`, one `tool-<name>` install component per example).
- Windows `cpack` zip and NSIS installer; WebAssembly build with live demos on GitHub Pages.
- MIT license, with third-party license texts shipped in the packages.

[Unreleased]: https://github.com/kolbbond/simple-magnum-gui/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/kolbbond/simple-magnum-gui/releases/tag/v0.1.0
