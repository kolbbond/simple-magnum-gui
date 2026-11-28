#!/usr/bin/env bash
# Stage a built tree's install into dist/smg-<ver>-<os>-<arch>.tar.gz (+ .sha256).
# usage: scripts/package_sdk.sh <build-dir> <out-dir>; last stdout line is the tarball path.
set -euo pipefail

build=$(realpath "${1:?usage: $0 <build-dir> <out-dir>}")
mkdir -p "${2:?usage: $0 <build-dir> <out-dir>}"
out=$(realpath "$2")

version=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$build/CMakeCache.txt")
if [ -z "$version" ]; then
    echo "package_sdk: no CMAKE_PROJECT_VERSION in $build/CMakeCache.txt" >&2
    exit 1
fi
name="smg-${version}-$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)"

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

cmake --install "$build" --prefix "$stage/$name" --strip >&2
# Debian/Ubuntu find_package skips lib64/cmake; configure with CMAKE_INSTALL_LIBDIR=lib LIB_SUFFIX=
if [ -d "$stage/$name/lib64" ]; then
    echo "package_sdk: install has lib64/, which Debian/Ubuntu consumers can't find" >&2
    exit 1
fi
tar -C "$stage" -czf "$out/$name.tar.gz" "$name"
(cd "$out" && sha256sum "$name.tar.gz" > "$name.tar.gz.sha256")

echo "$out/$name.tar.gz"
