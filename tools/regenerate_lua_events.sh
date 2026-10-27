#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

export_args=()
if [[ $# -gt 0 ]]; then
    if [[ $# -ne 1 || ( "$1" != --item-rewards-only && "$1" != --npc-locations-only \
            && "$1" != --autonote-refs-only && "$1" != --site-refs-only && "$1" != --npc-topic-refs-only \
            && "$1" != --monster-spawns-only ) ]]; then
        echo 'Usage: regenerate_lua_events.sh [--item-rewards-only|--npc-locations-only|--autonote-refs-only|--site-refs-only|--npc-topic-refs-only|--monster-spawns-only]' >&2
        exit 2
    fi
    export_args=("$1")
fi

cd "${repo_root}"

# These references also resolve legacy object types. Missing them can silently change gameplay on regeneration.
for world in mm6 mm7 mm8; do
    reference_dir="reference/mmext-scripts/Decompiled_Scripts/${world}/txt"
    if [[ ! -d "${reference_dir}" ]]; then
        echo "Missing required exporter reference: ${reference_dir}" >&2
        exit 1
    fi
done

build_dir="${OPENYAMM_BUILD_DIR:-build}"
cmake --build "${build_dir}" --target openyamm_event_asset_export -j25

exporter="${build_dir}/tools/openyamm_event_asset_export"
if [[ -f "${exporter}.exe" ]]; then
    exporter="${exporter}.exe"
fi

# Global.lua is shared engine content. Generate it exactly once from the canonical
# MM8/default global source, then regenerate each world's map-local scripts only.
"${exporter}" --world mm8 --lua-export-config tools/lua_event_export.mm8.ini --map __none__ "${export_args[@]}"
"${exporter}" --world mm6 --lua-export-config tools/lua_event_export.mm6.ini --skip-global "${export_args[@]}"
"${exporter}" --world mm7 --lua-export-config tools/lua_event_export.mm7.ini --skip-global "${export_args[@]}"
"${exporter}" --world mm8 --lua-export-config tools/lua_event_export.mm8.ini --skip-global "${export_args[@]}"
