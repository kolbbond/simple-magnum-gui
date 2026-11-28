#!/usr/bin/env bash
# Unpack an smg SDK tarball somewhere it was never built and build test/consumer against it.
# usage: scripts/check_sdk.sh <smg-*.tar.gz>
set -euo pipefail

tarball=$(realpath "${1:?usage: $0 <smg-*.tar.gz>}")
src=$(cd "$(dirname "${BASH_SOURCE[0]}")/../test/consumer" && pwd)

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

tar -C "$tmp" -xzf "$tarball"
tops=("$tmp"/*/)
if [ "${#tops[@]}" -ne 1 ]; then
    echo "check_sdk: expected one top-level dir in $tarball, got ${#tops[@]}" >&2
    exit 1
fi
prefix=${tops[0]%/}

generator=()
command -v ninja >/dev/null 2>&1 && generator=(-G Ninja)

cmake -S "$src" -B "$tmp/build" "${generator[@]}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$prefix"
cmake --build "$tmp/build"
ctest --test-dir "$tmp/build" --output-on-failure
