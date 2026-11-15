#!/usr/bin/env bash
set -euo pipefail

third_party_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
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

python3 "$usd_script" "$usd_install" \
    --imaging --no-docs --no-tests --embree --build-variant release \
    --python-install-dir lib/python "$@"
