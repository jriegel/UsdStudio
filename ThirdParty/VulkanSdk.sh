#!/usr/bin/env bash

vulkan_sdk_version=1.4.363.1
vulkan_sdk_sha256=b7867d8ca658563813af666f34b93db87f1b1c7d9d69d97e924c210c7ba33eb8
vulkan_sdk_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)/VulkanSDK"

use_vulkan_sdk() {
    if [[ -z "${VULKAN_SDK:-}" && "$(uname -s)" == Linux ]]; then
        VULKAN_SDK="$vulkan_sdk_root/$vulkan_sdk_version/x86_64"
    fi
    if [[ -z "${VULKAN_SDK:-}" || ! -d "$VULKAN_SDK" ]]; then
        echo "ERROR: Vulkan SDK not found. Run Setup.sh or set VULKAN_SDK to an installed SDK." >&2
        return 1
    fi
    if [[ "$(uname -s)" == Linux ]] &&
       { [[ ! -f "$VULKAN_SDK/include/vulkan/vulkan.h" ]] ||
         { [[ ! -f "$VULKAN_SDK/lib/libshaderc_combined.a" ]] &&
           [[ ! -f "$VULKAN_SDK/lib/libshaderc_combined.so" ]]; }; }; then
        echo "ERROR: Vulkan SDK at $VULKAN_SDK is missing Vulkan headers or shaderc_combined." >&2
        return 1
    fi
    export VULKAN_SDK
    if [[ -d "$VULKAN_SDK/lib/VulkanLoader" ]]; then
        export CMAKE_PREFIX_PATH="$VULKAN_SDK/lib/VulkanLoader${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
    fi
}

install_vulkan_sdk() {
    if [[ -n "${VULKAN_SDK:-}" ]]; then
        use_vulkan_sdk
        return
    fi
    if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
        echo "ERROR: Automatic Vulkan SDK installation requires Linux x86_64; set VULKAN_SDK instead." >&2
        return 1
    fi

    local sdk_dir="$vulkan_sdk_root/$vulkan_sdk_version"
    if [[ -f "$sdk_dir/x86_64/include/vulkan/vulkan.h" &&
          -f "$sdk_dir/x86_64/lib/libshaderc_combined.a" ]]; then
        use_vulkan_sdk
        return
    fi
    if [[ -e "$sdk_dir" ]]; then
        echo "ERROR: Incomplete Vulkan SDK at $sdk_dir; remove or repair it before retrying." >&2
        return 1
    fi

    local tool
    for tool in curl sha256sum tar xz; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            echo "ERROR: $tool is required to install the Vulkan SDK." >&2
            return 1
        fi
    done
    mkdir -p "$vulkan_sdk_root"
    local archive="$vulkan_sdk_root/vulkan-sdk-$vulkan_sdk_version.tar.xz"
    if [[ ! -f "$archive" ]]; then
        echo "Downloading Vulkan SDK $vulkan_sdk_version from LunarG..."
        local download
        download="$(mktemp "$vulkan_sdk_root/.download.XXXXXX")"
        if ! curl -fL --retry 2 -o "$download" \
            "https://sdk.lunarg.com/sdk/download/$vulkan_sdk_version/linux/vulkan_sdk.tar.xz"; then
            rm -f -- "$download"
            echo "ERROR: Vulkan SDK download failed." >&2
            return 1
        fi
        mv -- "$download" "$archive"
    fi
    if ! printf '%s  %s\n' "$vulkan_sdk_sha256" "$archive" | sha256sum -c -; then
        echo "ERROR: Vulkan SDK checksum mismatch for $archive." >&2
        return 1
    fi

    local staging
    staging="$(mktemp -d "$vulkan_sdk_root/.extract.XXXXXX")"
    if ! tar -xJf "$archive" -C "$staging"; then
        echo "ERROR: Vulkan SDK extraction failed; partial files remain at $staging." >&2
        return 1
    fi
    if [[ ! -f "$staging/$vulkan_sdk_version/x86_64/include/vulkan/vulkan.h" ||
          ! -f "$staging/$vulkan_sdk_version/x86_64/lib/libshaderc_combined.a" ]]; then
        echo "ERROR: Unexpected Vulkan SDK archive layout at $staging." >&2
        return 1
    fi
    mv -T -- "$staging/$vulkan_sdk_version" "$sdk_dir"
    rmdir -- "$staging"
    rm -f -- "$archive"
    use_vulkan_sdk
    echo "Installed Vulkan SDK at $VULKAN_SDK"
}
