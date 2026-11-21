#!/usr/bin/env bash
set -euo pipefail

third_party_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
source "$third_party_dir/VulkanSdk.sh"

test_dir="$(mktemp -d)"
vulkan_sdk_root="$test_dir/sdk"
archive="$vulkan_sdk_root/vulkan-sdk-$vulkan_sdk_version.tar.xz"
trap 'rm -f -- "$archive" "$test_dir/custom/include/vulkan/vulkan.h" "$test_dir/custom/lib/libshaderc_combined.a"; rmdir -- "$vulkan_sdk_root/$vulkan_sdk_version" "$vulkan_sdk_root" "$test_dir/custom/include/vulkan" "$test_dir/custom/include" "$test_dir/custom/lib" "$test_dir/custom" "$test_dir" 2>/dev/null || true' EXIT

mkdir -p "$test_dir/custom/include/vulkan" "$test_dir/custom/lib" "$vulkan_sdk_root"
touch "$test_dir/custom/include/vulkan/vulkan.h" "$test_dir/custom/lib/libshaderc_combined.a"
VULKAN_SDK="$test_dir/custom"
install_vulkan_sdk
[[ "$VULKAN_SDK" == "$test_dir/custom" ]] || {
    echo "An explicitly selected SDK was not preserved." >&2
    exit 1
}

VULKAN_SDK="$test_dir/missing"
if use_vulkan_sdk 2>/dev/null; then
    echo "A missing explicit SDK was accepted." >&2
    exit 1
fi

rm "$test_dir/custom/lib/libshaderc_combined.a"
VULKAN_SDK="$test_dir/custom"
if use_vulkan_sdk 2>/dev/null; then
    echo "An incomplete explicit SDK was accepted." >&2
    exit 1
fi

if [[ "$(uname -s)" == Linux && "$(uname -m)" == x86_64 ]]; then
    unset VULKAN_SDK
    printf 'not an SDK\n' > "$archive"
    if install_vulkan_sdk >/dev/null 2>&1; then
        echo "A corrupt SDK archive was accepted." >&2
        exit 1
    fi
    [[ ! -e "$vulkan_sdk_root/$vulkan_sdk_version" ]] || {
        echo "A corrupt archive was extracted." >&2
        exit 1
    }

    mkdir "$vulkan_sdk_root/$vulkan_sdk_version"
    if install_vulkan_sdk >/dev/null 2>&1; then
        echo "An incomplete existing SDK was accepted." >&2
        exit 1
    fi
fi

echo "Vulkan SDK setup checks passed."
