#!/usr/bin/env bash
set -euo pipefail

third_party_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
source "$third_party_dir/VulkanSdk.sh"
usd_script="$third_party_dir/OpenUsd_Src/build_scripts/build_usd.py"
usd_install="$third_party_dir/OpenUsd_Build_Release"

if ! command -v python3 >/dev/null 2>&1; then
    echo "ERROR: python3 was not found on PATH." >&2
    exit 1
fi
if [[ ! -f "$usd_script" ]]; then
    echo "ERROR: OpenUSD sources not found at $usd_script." >&2
    exit 1
fi
use_vulkan_sdk

CPLUS_INCLUDE_PATH="$third_party_dir/vma/include${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}" \
python3 "$usd_script" "$usd_install" \
    --usd-imaging --vulkan --no-usdview --no-docs --no-tests --embree --build-variant release \
    --python-install-dir lib/python "$@"
