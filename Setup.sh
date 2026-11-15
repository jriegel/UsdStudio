#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: cmake was not found on PATH." >&2
    exit 1
fi

if [[ -z "${VCPKG_ROOT:-}" ]]; then
    if ! vcpkg_path="$(command -v vcpkg)"; then
        echo "ERROR: Set VCPKG_ROOT or put vcpkg on PATH." >&2
        exit 1
    fi
    while [[ -L "$vcpkg_path" ]]; do
        link_dir="$(cd -- "$(dirname -- "$vcpkg_path")" && pwd -P)"
        vcpkg_path="$(readlink "$vcpkg_path")"
        [[ "$vcpkg_path" = /* ]] || vcpkg_path="$link_dir/$vcpkg_path"
    done
    VCPKG_ROOT="$(cd -- "$(dirname -- "$vcpkg_path")" && pwd -P)"
fi

toolchain="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
if [[ ! -f "$toolchain" ]]; then
    echo "ERROR: vcpkg toolchain not found at $toolchain." >&2
    exit 1
fi

if [[ ! -f "$root_dir/ThirdParty/OpenUsd_Build_Release/pxrConfig.cmake" ]]; then
    echo "OpenUSD installation not found. Building it now..."
    "$root_dir/ThirdParty/BuildUsd.sh"
fi

cmake -S "$root_dir" -B "$root_dir/UnixBuild" \
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" \
    -DCMAKE_BUILD_TYPE=Release "$@"
