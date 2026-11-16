#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
cd "${repo_root}"

if [[ $# -gt 1 || ( $# -eq 1 && "$1" != --atlas-metadata ) ]]; then
    echo 'Usage: regenerate_lua_events.sh [--atlas-metadata]' >&2
    exit 2
fi

build_dir="${OPENYAMM_BUILD_DIR:-build}"
cmake --build "${build_dir}" --target openyamm_event_asset_export -j25
exporter="${build_dir}/tools/openyamm_event_asset_export"
if [[ -f "${exporter}.exe" ]]; then exporter="${exporter}.exe"; fi

if [[ ${1:-} == --atlas-metadata ]]; then
    # Read the installed release through the same pack/override mounts as the game. Legacy inputs can be a
    # separate mods-shaped folder; they are not shipped in the Atlas runtime package.
    input_args=()
    if [[ -n ${OPENYAMM_ASSET_ROOT:-} ]]; then input_args+=(--asset-root "$OPENYAMM_ASSET_ROOT"); fi
    if [[ -n ${OPENYAMM_METADATA_MODS_ROOT:-} ]]; then
        input_args+=(--mods-root "$OPENYAMM_METADATA_MODS_ROOT")
    fi
    for world in mm6 mm7 mm8; do
        if [[ ! -d "reference/mmext-scripts/Decompiled_Scripts/${world}/txt" ]]; then
            echo "Missing exporter reference: reference/mmext-scripts/Decompiled_Scripts/${world}/txt" >&2
            exit 1
        fi
    done
    for kind in item-rewards npc-locations npc-topic-refs autonote-refs site-refs monster-spawns; do
        "$exporter" "${input_args[@]}" --world mm8 --lua-export-config tools/lua_event_export.mm8.ini \
            --legacy-events-dir worlds/mm8/_legacy/events --map __none__ "--${kind}-only"
        for world in mm6 mm7 mm8; do
            # MM6 is a deliberately bounded New Sorpigal pilot, including its two local dungeons.
            maps=(__all__)
            if [[ $world == mm6 ]]; then maps=(oute3 6d01 6d02); fi
            for map in "${maps[@]}"; do
                map_args=()
                if [[ $map != __all__ ]]; then map_args=(--map "$map"); fi
                "$exporter" "${input_args[@]}" --world "$world" "${map_args[@]}" \
                    --lua-export-config "tools/lua_event_export.${world}.ini" \
                    --legacy-events-dir "worlds/${world}/_legacy/events" --skip-global "--${kind}-only"
            done
        done
    done
    exit 0
fi

# Global.lua is shared engine content. Generate it exactly once from the canonical
# MM8/default global source, then regenerate each world's map-local scripts only.
"${exporter}" --world mm8 --lua-export-config tools/lua_event_export.mm8.ini --map __none__
"${exporter}" --world mm6 --lua-export-config tools/lua_event_export.mm6.ini --skip-global
"${exporter}" --world mm7 --lua-export-config tools/lua_event_export.mm7.ini --skip-global
"${exporter}" --world mm8 --lua-export-config tools/lua_event_export.mm8.ini --skip-global
