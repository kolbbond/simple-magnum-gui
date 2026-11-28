#!/usr/bin/env bash
# Relocatable Linux tools tarball from a configured + built tree (ENABLE_EXAMPLES=ON, Release):
#   scripts/package_linux.sh BUILD_DIR VERSION [OUT_DIR=dist]
#     -> OUT_DIR/smg-tools-VERSION-linux-x86_64.tar.gz + .sha256
#   scripts/package_linux.sh --smoke TARBALL
#     unpacks into a temp dir, checks the license notices, and runs every bin/smg-* for a few
#     hidden frames in a scrubbed env
# The tarball holds bin/smg-<name> links into lib/smg-tools/bin, beside private libs and Magnum
# plugins (cmake/InstallTools.cmake); glibc, libGL and SDL2 stay with the OS.
set -euo pipefail

usage() {
    sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'
}

die() {
    echo "package_linux: $*" >&2
    exit 1
}

# a tool must resolve every library with no LD_LIBRARY_PATH, and none from the build tree
check_closure() {
    local root=$1 build=${2:-} bad=0 f out
    while IFS= read -r -d '' f; do
        file -b "$f" | grep -q '^ELF' || continue
        out=$(env -i PATH=/usr/bin:/bin ldd "$f" 2>&1) || true
        if grep -q 'not found' <<<"$out"; then
            echo "missing libraries for ${f#"$root"/}:" >&2
            grep 'not found' <<<"$out" >&2
            bad=1
        fi
        if [ -n "$build" ] && grep -qF "$build" <<<"$out"; then
            echo "${f#"$root"/} resolves into the build tree:" >&2
            grep -F "$build" <<<"$out" >&2
            bad=1
        fi
    done < <(find "$root" -type f \( -path '*/bin/*' -o -name '*.so*' \) -print0)
    return $bad
}

# notices cmake/SmgLicenses.cmake installs; every redistributed component must keep its own
LICENSES=(smg.txt corrade.txt magnum.txt magnum-integration.txt magnum-plugins.txt imgui.txt implot.txt
    implot3d.txt imguifiledialog.txt magnum-bloom.md fonts/README.md fonts/OFL-1.1.txt fonts/Apache-2.0.txt)

check_licenses() {
    local dir=$1/share/licenses/smg f bad=0
    for f in "${LICENSES[@]}"; do
        if [ ! -s "$dir/$f" ]; then
            echo "missing license: share/licenses/smg/$f" >&2
            bad=1
        fi
    done
    [ $bad -eq 0 ] && echo "ok   ${#LICENSES[@]} license files"
    return $bad
}

smoke() {
    local tarball=$1 root sums exe name rc fail=0 count=0
    [ -f "$tarball" ] || die "no such tarball: $tarball"
    sums="$tarball.sha256"
    # fail closed: an unverified tarball is not the one that was built
    [ -f "$sums" ] || die "no checksum file: $sums"
    (cd "$(dirname "$tarball")" && sha256sum -c "$(basename "$sums")") || die "checksum mismatch"
    tmp=$(mktemp -d)
    tar -xzf "$tarball" -C "$tmp"
    root=$(find "$tmp" -mindepth 1 -maxdepth 1 -type d | head -n 1)
    [ -d "$root/bin" ] || die "tarball has no top-level <dir>/bin"
    check_closure "$root" || die "library closure is incomplete"
    check_licenses "$root" || die "license notices are incomplete"

    # XAUTHORITY: xvfb-run's server refuses clients without its cookie
    local -a keep=()
    local v
    for v in HOME DISPLAY WAYLAND_DISPLAY XDG_RUNTIME_DIR XAUTHORITY PATH; do
        [ -n "${!v:-}" ] && keep+=("$v=${!v}")
    done
    for exe in "$root"/bin/smg-*; do
        name=$(basename "$exe")
        count=$((count + 1))
        rc=0
        (cd "$tmp" && env -i "${keep[@]}" timeout 120 "$exe" --smg-frames 5 --smg-hidden) >"$tmp/$name.log" 2>&1 || rc=$?
        if [ $rc -eq 0 ]; then
            echo "ok   $name"
        else
            echo "FAIL $name (exit $rc)"
            sed 's/^/    /' "$tmp/$name.log" | tail -n 20
            fail=$((fail + 1))
        fi
    done
    [ $count -gt 0 ] || die "no bin/smg-* in $tarball"
    echo "$((count - fail))/$count tools ran"
    [ $fail -eq 0 ]
}

package() {
    local build version out name prefix tools t
    build=$(cd "$1" && pwd)
    version=$2
    out=${3:-dist}
    [ -f "$build/cmake_install.cmake" ] || die "$build is not a configured build dir"
    grep -q '^CMAKE_BUILD_TYPE:STRING=Release$' "$build/CMakeCache.txt" ||
        echo "package_linux: warning: $build is not a Release build" >&2

    # every tool-<example> install component cmake/InstallTools.cmake declared
    tools=$({ grep -rhoE 'CMAKE_INSTALL_COMPONENT STREQUAL "tool-[^"]+"' "$build" --include=cmake_install.cmake || true; } |
        sed -E 's/.*"tool-([^"]+)"/\1/' | sort -u)
    [ -n "$tools" ] || die "no tool-* components in $build (ENABLE_EXAMPLES=ON on Linux?)"

    name="smg-tools-$version-linux-x86_64"
    mkdir -p "$out"
    out=$(cd "$out" && pwd)
    stage=$(mktemp -d)
    prefix="$stage/$name"
    for t in $tools; do
        cmake --install "$build" --component "tool-$t" --prefix "$prefix" >/dev/null || die "install of tool-$t failed"
    done
    check_closure "$prefix" "$build" || die "library closure is incomplete"
    check_licenses "$prefix" || die "license notices are incomplete"

    # reproducible-ish: fixed owner and order, no build-host user names
    tar --sort=name --owner=0 --group=0 --numeric-owner -C "$stage" -czf "$out/$name.tar.gz" "$name"
    (cd "$out" && sha256sum "$name.tar.gz" >"$name.tar.gz.sha256")
    echo "tools: $(echo $tools)"
    echo "$out/$name.tar.gz"
    cat "$out/$name.tar.gz.sha256"
}

# global so the EXIT trap still sees them after the function returns
tmp=""
stage=""
trap 'rm -rf ${tmp:+"$tmp"} ${stage:+"$stage"}' EXIT

case "${1:-}" in
    --smoke)
        [ $# -eq 2 ] || { usage >&2; exit 2; }
        smoke "$2"
        ;;
    -h | --help) usage ;;
    "") usage >&2; exit 2 ;;
    *)
        [ $# -ge 2 ] && [ $# -le 3 ] || { usage >&2; exit 2; }
        package "$@"
        ;;
esac
