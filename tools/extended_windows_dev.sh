#!/usr/bin/env bash
# Incremental native Windows builds from WSL. Keeps dependencies, Ninja state and ccache outside the checkout.
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-build}"
if [[ $# -gt 0 ]]; then shift; fi
if [[ "$mode" != build && "$mode" != run ]]; then
    echo 'Usage: extended_windows_dev.sh [build|run] [game arguments...]' >&2
    exit 2
fi
dev_root="${OPENYAMM_WINDOWS_DEV_ROOT:-}"
if [[ -z "$dev_root" ]]; then
    powershell="$(command -v powershell.exe || true)"
    powershell="${powershell:-/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe}"
    profile_windows="$("$powershell" -NoProfile -Command '[Environment]::GetFolderPath("UserProfile")' | tr -d '\r')"
    dev_root="$(wslpath -u "$profile_windows")/openyamm-extended-dev"
fi
native_bash="$dev_root/tools/msys64/usr/bin/bash.exe"
if [[ ! -f "$native_bash" ]]; then
    echo "Missing portable MSYS2/UCRT64 toolchain: $native_bash" >&2
    echo 'Install MSYS2 here, or set OPENYAMM_WINDOWS_DEV_ROOT to the prepared Windows development directory.' >&2
    exit 1
fi
mkdir -p "$repo_root/build/extended-windows" "$dev_root/source"
native_root="$(wslpath -m "$dev_root")"
command_file="$(mktemp "$dev_root/.extended-command-XXXXXXXX.sh")"
trap 'rm -f -- "$command_file"' EXIT
build_jobs="${OPENYAMM_BUILD_JOBS:-25}"
if [[ ! "$build_jobs" =~ ^[1-9][0-9]*$ ]]; then
    echo 'OPENYAMM_BUILD_JOBS must be a positive integer.' >&2
    exit 2
fi
{
    printf '%s\n' 'export MSYSTEM=UCRT64' 'export CHERE_INVOKING=1' 'source /etc/profile' 'set -euo pipefail'
    printf 'export CCACHE_DIR=%q\n' "$native_root/cache/ccache"
    if [[ "$mode" == build ]]; then
        rsync -a --exclude=.git --exclude=build --exclude='build-*' --exclude=assets_cooked \
            --exclude=reference --exclude=re_mm8 --exclude=output --exclude=test_img --exclude=saves \
            "$repo_root/" "$dev_root/source/"
        if [[ ! -f "$dev_root/build/CMakeCache.txt" ]]; then
            printf 'cmake -S %q -B %q -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENYAMM_BUILD_TESTS=OFF -DOPENYAMM_BUILD_EDITOR=OFF -DOPENYAMM_BUILD_TOOLS=OFF -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache\n' \
                "$native_root/source" "$native_root/build"
        fi
        printf 'cmake --build %q --target openyamm -j%s\n' "$native_root/build" "$build_jobs"
    else
        if [[ ! -f "$dev_root/runtime/settings.ini" ]]; then
            echo "Missing isolated runtime settings: $dev_root/runtime/settings.ini" >&2
            exit 1
        fi
        printf 'cd %q\n' "$native_root/runtime"
        printf '%q ' "$native_root/build/game/openyamm.exe" "$@"
        printf '\n'
    fi
} > "$command_file"
log="$repo_root/build/extended-windows/$mode.log"
echo "Windows $mode; log: $log"
start_seconds=$SECONDS
"$native_bash" "$(wslpath -w "$command_file")" > "$log" 2>&1 || { tail -35 "$log"; exit 1; }
# Small source-data overlay: iterate on tables/presentation without recooking multi-GB packages.
if [[ "$mode" == build && -d "$dev_root/runtime/assets" ]]; then
    python3 - "$repo_root" "$dev_root" <<'PY_OVERLAY'
from pathlib import Path
import os, sys, tempfile, zipfile
repo, dev = map(Path, sys.argv[1:])
package = dev / "runtime/assets/assets.zip"
updates = {
    "engine/data_tables/class_multipliers.txt": repo / "assets_dev/engine/data_tables/class_multipliers.txt",
    "engine/world/atlas/mm7.yml": repo / "assets_dev/engine/world/atlas/mm7.yml",
}
fd, temporary = tempfile.mkstemp(prefix=".extended-overlay-", suffix=".zip", dir=package.parent)
os.close(fd)
try:
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as target:
        if package.exists():
            with zipfile.ZipFile(package) as previous:
                for entry in previous.infolist():
                    if entry.filename not in updates:
                        target.writestr(entry, previous.read(entry))
        for virtual, source in updates.items():
            target.writestr(virtual, source.read_bytes())
    os.replace(temporary, package)
finally:
    if os.path.exists(temporary):
        os.unlink(temporary)
PY_OVERLAY
fi
echo "Finished in $((SECONDS - start_seconds))s."
tail -8 "$log"
