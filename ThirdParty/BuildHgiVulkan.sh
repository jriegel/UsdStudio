#!/usr/bin/env bash
set -euo pipefail

third_party_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
source "$third_party_dir/VulkanSdk.sh"
use_vulkan_sdk

if [[ "$(uname -s)" != Linux ]]; then
    echo "ERROR: BuildHgiVulkan.sh currently supports Linux only." >&2
    exit 1
fi

usd_src="${OPENUSD_SRC:-$third_party_dir/OpenUsd_Src}"
usd_root="${OPENUSD_ROOT:-$third_party_dir/OpenUsd_Build_Release}"
build_dir="${HGIVULKAN_BUILD_DIR:-$third_party_dir/HgiVulkan_Build}"

if [[ ! -f "$usd_root/pxrConfig.cmake" ]]; then
    echo "ERROR: OpenUSD is not installed at $usd_root. Run BuildUsd.sh first." >&2
    exit 1
fi
if [[ ! -f "$usd_root/lib/libusd_hgiVulkan.so" &&
      -f "$usd_root/cmake/pxrTargets.cmake" ]] &&
   grep -q '^add_library(hgiVulkan ' "$usd_root/cmake/pxrTargets.cmake"; then
    echo "ERROR: OpenUSD exports HgiVulkan but its library is missing. Rebuild OpenUSD with BuildUsd.sh." >&2
    exit 1
fi

cmake -S "$third_party_dir/BuildHgiVulkan" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    "-DOPENUSD_SRC=$usd_src" "-DOPENUSD_ROOT=$usd_root" "$@"
cmake --build "$build_dir" --config Release
cmake --install "$build_dir" --config Release --prefix "$usd_root"

echo "Installed Hgi, HgiInterop and HgiVulkan into $usd_root/lib"
