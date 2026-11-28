# smg Prebuilt SDK Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** CI publishes a relocatable `smg-<ver>-linux-x86_64.tar.gz` (smg + bundled Corrade/Magnum/plugins/integration) on version tags; goose's `cmake.deps` can consume it instead of building smg from source.

**Architecture:** A `manylinux_2_28` container job runs `cmake --install` into a staging prefix and tars it. A check script unpacks the tarball to a *different* path and builds `test/consumer` against it with `find_package(smg)`, which proves the tarball is relocatable. goose gets a `USE_PREBUILT_SMG` path that downloads the tarball, verifies its hash and extracts it into its deps prefix, so goose's existing `find_package(smg CONFIG REQUIRED)` is unchanged.

**Tech Stack:** CMake ≥3.16, GitHub Actions, `quay.io/pypa/manylinux_2_28_x86_64`, bash.

**Spec:** this conversation (2026-11-25). Decisions: prebuilt is a C++ SDK, not a Python wheel; wheels (goose-tools) are a later phase that *consumes* it.

## Scope

| In | Out (later / separate plan) |
|---|---|
| Linux x86_64 SDK tarball on `v*` tags | Windows/macOS SDK (matrix slot left open) |
| Relocation + plugin-load verification | goose-tools wheel (blocked on: `gosling.gui` vs standalone apps) |
| goose `USE_PREBUILT_SMG` option | goose CI switching to the prebuilt by default |
| Release-workflow dry run on PRs touching packaging | WASM SDK (static libs, different consumer story) |

## Global Constraints

- Build image: `quay.io/pypa/manylinux_2_28_x86_64` (glibc 2.28 floor; same image later builds the wheel → same toolchain/ABI).
- Release config must match goose's ExternalProject args (`cmake.deps/smg/CMakeLists.txt`): `ENABLE_TESTING=OFF ENABLE_EXAMPLES=OFF BUILD_SHARED_LIBS=ON CMAKE_BUILD_TYPE=Release MAGNUM_WITH_FREETYPEFONT=ON MAGNUM_WITH_MAGNUMFONT=ON`.
- Artifact name: `smg-${PROJECT_VERSION}-linux-x86_64.tar.gz` with one top-level dir `smg-${PROJECT_VERSION}-linux-x86_64/`, plus `<name>.sha256`.
- Tag `vX.Y.Z` must equal `PROJECT_VERSION` in `CMakeLists.txt`; mismatch fails the job.
- System runtime deps, not bundled: `libSDL2-2.0.so.0`, `libfreetype.so.6`, `libGL`/`libGLX`. Documented, not vendored (auditwheel vendors them at wheel time).
- Phases per CLAUDE.md: ≤5 files each; stop for approval between phases. Phase 1 = smg repo, Phase 2 = new goose-tools repo.

## Review Focus

1. **Unpacked at a path other than the build prefix**: Magnum must still find its plugins (`FreeTypeFont`, `StbImageImporter`) through its library-relative search path, not the baked-in `MAGNUM_PLUGINS_DIR`. Pinned by Task 1's plugin test run from a fresh temp dir.
2. **Consumer has a system Magnum/Corrade installed** (`/usr/lib/x86_64-linux-gnu` is prepended in smg's own CMakeLists): `find_package` must resolve to the tarball's copies. Pinned by Task 1 asserting `Magnum_DIR`/`Corrade_DIR` sit under the unpacked prefix.
3. **Consumer built with the system GCC on Ubuntu, SDK built with gcc-toolset in manylinux**: the same CXX11 ABI must hold. Pinned by the existing `scene_panel_abi_size()` check; Task 4 also runs the check on `ubuntu-latest` (not the container).
4. **Tag/version drift** (`v0.2.0` tagged while CMake says `0.1.0`): the job must fail before upload. Pinned in Task 3.
5. **goose with a stale extracted SDK in `.deps/usr` after the pinned URL changes**: a re-configure must re-extract. Pinned in Task 5 by keying the extract stamp on the SHA256.

---

## Phase 1: smg repo

### Task 1: Relocatable-SDK check

**Files:**
- Modify: `test/consumer/main.cpp`
- Modify: `test/consumer/CMakeLists.txt`
- Create: `scripts/check_sdk.sh`

**Interfaces:**
- Produces: `scripts/check_sdk.sh <tarball>` → exit 0 when it passes; used by Tasks 3, 4.

- [ ] **Step 1: Add plugin-load checks to `test/consumer/main.cpp`**

Two more failure checks, headless (no GL context):
```cpp
Corrade::PluginManager::Manager<Magnum::Text::AbstractFont> fonts;
// fail unless fonts.load("FreeTypeFont") & Corrade::PluginManager::LoadState::Loaded
Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter> importers;
// fail unless importers.load("StbImageImporter") & LoadState::Loaded
```
On failure, print the plugin name and `pluginDirectory()`.

- [ ] **Step 2: In `test/consumer/CMakeLists.txt`, fail configure if deps resolve outside the smg prefix**

After `find_package(smg)`: derive the prefix from `smg_DIR` (three levels up from `lib/cmake/smg`) and `message(FATAL_ERROR ...)` unless `Magnum_DIR` (or `MAGNUM_INCLUDE_DIR`, whichever FindMagnum sets) and `Corrade_INCLUDE_DIR` start with it.

- [ ] **Step 3: Write `scripts/check_sdk.sh`**

`set -euo pipefail`; `mktemp -d` → extract the tarball there → `cmake -S test/consumer -B $tmp/build -G Ninja -DCMAKE_PREFIX_PATH=$tmp/<topdir>` → build → `ctest --output-on-failure`. Trap removes `$tmp`.

- [ ] **Step 4: Verify against the current CI-style install**

Run: `cmake --install build --prefix /tmp/smg-stage && tar -C /tmp -czf /tmp/smg-test.tar.gz smg-stage && scripts/check_sdk.sh /tmp/smg-test.tar.gz`
Expected: `smg 0.1.0 consumer: ok`. Then temporarily move `stage/lib/magnum` aside, rerun, and expect a FAIL naming `StbImageImporter`. That proves the check actually bites.

- [ ] **Step 5: Commit** — `sdk: consumer test checks plugin load + relocated prefix`

### Task 2: Package script

**Files:**
- Create: `scripts/package_sdk.sh`

**Interfaces:**
- Consumes: a configured+built build dir.
- Produces: `scripts/package_sdk.sh <build-dir> <out-dir>` → writes `<out-dir>/smg-<ver>-linux-x86_64.tar.gz` and `.sha256`, prints the tarball path on stdout's last line.

- [ ] **Step 1: Implement**

Version comes from `<build-dir>/CMakeCache.txt` (`CMAKE_PROJECT_VERSION:STATIC=`). `cmake --install <build-dir> --prefix <stage>/<topdir>` → strip `.so` files (`--strip` on install) → `tar -czf` → `sha256sum > .sha256`.

- [ ] **Step 2: Verify** — `scripts/package_sdk.sh build dist && scripts/check_sdk.sh dist/smg-0.1.0-linux-x86_64.tar.gz` → ok; `tar -tzf` shows exactly one top-level dir.

- [ ] **Step 3: Commit** — `sdk: package_sdk.sh stages install into a versioned tarball`

### Task 3: Release workflow

**Files:**
- Create: `.github/workflows/release.yml`

**Interfaces:**
- Consumes: Tasks 1, 2 scripts.
- Produces: GitHub Release `vX.Y.Z` with the tarball + `.sha256`; the URL pattern used in Task 5 is
  `https://github.com/kolbbond/simple-magnum-gui/releases/download/v<ver>/smg-<ver>-linux-x86_64.tar.gz`.

- [ ] **Step 1: Write the workflow**

- Triggers: `push: tags: ['v*']`, `workflow_dispatch`, and `pull_request` with `paths:` limited to `scripts/package_sdk.sh`, `scripts/check_sdk.sh`, `test/consumer/**`, `.github/workflows/release.yml`, `cmake/smg*.cmake.in`.
- Job `sdk-linux`: `container: quay.io/pypa/manylinux_2_28_x86_64`; `dnf install -y SDL2-devel freetype-devel mesa-libGL-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel ninja-build`; checkout with submodules; configure with the Global Constraints args; build; `package_sdk.sh`; `check_sdk.sh`; `upload-artifact`.
- On tags only: compare `${GITHUB_REF_NAME#v}` to the cache's `CMAKE_PROJECT_VERSION` and fail on mismatch *before* packaging. Then job `publish` (`needs: [sdk-linux, consumer-ubuntu]`, `permissions: contents: write`) runs `gh release create "$GITHUB_REF_NAME" dist/*`.

- [ ] **Step 2: Verify** — push the branch, open a draft PR, and confirm `sdk-linux` passes and the artifact downloads. Run `workflow_dispatch` once and confirm no release gets created.

- [ ] **Step 3: Commit** — `ci: release workflow builds manylinux_2_28 SDK tarball`

### Task 4: Cross-toolchain consumer job + docs

**Files:**
- Modify: `.github/workflows/release.yml`
- Modify: `README.md`

- [ ] **Step 1:** Add job `consumer-ubuntu` (`runs-on: ubuntu-latest`, `needs: sdk-linux`): download the artifact, `apt-get install libsdl2-2.0-0 libfreetype6 cmake ninja-build g++`, then run `check_sdk.sh`. This covers Review Focus #3.
- [ ] **Step 2:** README section "Prebuilt SDK": the URL pattern, the `find_package` / `CMAKE_PREFIX_PATH` snippet, the runtime deps list, the glibc 2.28 floor, and the rule: don't mix with another Magnum/Corrade, and load only one `libsmg.so` per process.
- [ ] **Step 3: Verify** — both jobs green on the PR.
- [ ] **Step 4: Commit** — `ci: verify SDK against ubuntu toolchain; readme: prebuilt SDK`

**Phase 1 gate:** stop for approval. Bump `PROJECT_VERSION` and push the first `v*` tag **together with the user** (it publishes a release).

---

## Phase 2 (to be planned): standalone goose-tools repo

Decided 2026-11-25: goose-tools is its **own repo** that consumes the Phase 1 SDK, not a part of goose. That keeps it fast to iterate on, with no goose PR review in the loop.

- A CMake app finds smg through `FetchContent` on the release URL (Task 3 pattern) plus `find_package(smg)`.
- The wheel is built with scikit-build-core + cibuildwheel in `manylinux_2_28`. The binaries become console scripts, and auditwheel vendors SDL2/freetype.
- Still open: what the tool does first (MVP), and whether it needs goose libraries or only reads goose output files.

## Later: goose consumes the prebuilt (deferred)

Only needed if goose itself should stop building smg from source. Kept as written:

### goose repo (`../goose`)

### Task 5: `USE_PREBUILT_SMG` in cmake.deps

**Files:**
- Modify: `cmake.deps/CMakeLists.txt` (options block ~L27, smg block ~L216)
- Modify: `cmake.deps/dependencies.cmake` (next to `SMG_URL`, L61)
- Create: `cmake.deps/smg/prebuilt.cmake`

**Interfaces:**
- Consumes: the Task 3 URL pattern.
- Produces: option `USE_PREBUILT_SMG` (default OFF), cache vars `SMG_PREBUILT_URL`, `SMG_PREBUILT_SHA256`. When ON: skip `add_subdirectory(smg)` and extract into the deps install prefix so `find_package(smg CONFIG REQUIRED)` in the top-level CMakeLists finds it unchanged.

- [ ] **Step 1:** `option(USE_PREBUILT_SMG ...)`. If both it and `USE_BUNDLED_SMG` are ON, fail with `FATAL_ERROR` (two Magnums in one prefix).
- [ ] **Step 2:** `prebuilt.cmake`: use `file(DOWNLOAD ... EXPECTED_HASH SHA256=${SMG_PREBUILT_SHA256})` into the build dir, then `file(ARCHIVE_EXTRACT)` into a temp dir and copy `<topdir>/` into the deps prefix. Write a stamp file containing the SHA256 and skip the step when the stamp matches. This covers Review Focus #5.
- [ ] **Step 3: Verify** — run a clean deps build with `-DUSE_BUNDLED_SMG=OFF -DUSE_PREBUILT_SMG=ON`, then build goose with `BUILD_GUI=ON`, then run `xvfb-run -a <goose gui binary> --frames 60`. Expected: it exits 0 and text renders (FreeTypeFont loaded). Change the hash, reconfigure, and expect a hash-mismatch error.
- [ ] **Step 4: Commit** — `deps: USE_PREBUILT_SMG consumes smg release tarball`


---

## Wheel shape notes

Option B was chosen. Notes on both:
- **A. `gosling.gui`**: cibuildwheel in the same manylinux_2_28 image, goose configured with `USE_PREBUILT_SMG=ON`, and auditwheel vendors SDL2/freetype. smg `.so` files ship once inside the gosling wheel.
- **B. Standalone apps** (fview, csvplot, …): a wheel built in the smg repo with the executables as console scripts. It doesn't need goose.

Constraint for either: only one `libsmg.so` per process. If both wheels exist, B depends on A's smg (or the reverse); never vendor it twice.
