#!/usr/bin/env bash
# Reusable approval entry point for creature authoring/export/review scripts.
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ $# != 1 ]]; then
    echo "Usage: tools/run_creature_blender.sh level_generation/creatures/<package>/<script>.py" >&2
    exit 2
fi
script_path="$(realpath -- "$repo_root/$1")"
case "$script_path" in
    "$repo_root"/level_generation/creatures/*.py) ;;
    *) echo "Expected a Python script under level_generation/creatures/." >&2; exit 2 ;;
esac
[[ -f "$script_path" ]] || { echo "Script does not exist: $script_path" >&2; exit 2; }
log_path="/tmp/openyamm-creature-blender-$(basename -- "$script_path" .py).log"
echo "Blender log: $log_path"
cd -- "$repo_root"
exec /snap/bin/blender --background --python "$script_path" > "$log_path" 2>&1
