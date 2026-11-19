#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
build_dir="${1:-$root_dir/UnixBuild}"
if [[ ! -x "$build_dir/bin/UsdStudio" ]]; then
    echo "ERROR: UsdStudio executable not found at $build_dir/bin/UsdStudio." >&2
    exit 1
fi

export HGI_ENABLE_VULKAN="${HGI_ENABLE_VULKAN:-1}"
exec "$build_dir/bin/UsdStudio"
