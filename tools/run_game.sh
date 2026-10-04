#!/usr/bin/env bash
# Launch the normal development build on the current desktop display.
# --isolated selects reproducible private runs; other arguments go directly to the game.
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
cd -- "$repo_root"
if [[ "${1-}" == "--isolated" ]]; then
    shift
    exec python3 "$script_dir/run_game.py" "$@"
fi
exec ./build/game/openyamm "$@"
