#!/usr/bin/env bash
# Isolated native-game screenshots and scripted camera/input review.
# All arguments belong to the capture helper; no shell commands are accepted.
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
cd -- "$repo_root"
exec xvfb-run -a -s '-screen 0 1600x1200x24' \
    python3 level_generation/projects/brackenford_greywatch/capture_gameplay.py "$@"
