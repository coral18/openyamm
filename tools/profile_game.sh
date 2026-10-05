#!/usr/bin/env bash
# Reusable approval entry point for OpenYAMM userspace CPU profiling.
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ "${1-}" == "--enable-user-perf" ]]; then
    if [[ "$#" != 1 ]]; then
        echo "--enable-user-perf accepts no additional arguments." >&2
        exit 2
    fi
    perf_level=$(</proc/sys/kernel/perf_event_paranoid)
    if (( perf_level > 2 )); then
        # Until reboot; this does not install a sysctl configuration or run the game as root.
        exec sudo -n /usr/sbin/sysctl -w kernel.perf_event_paranoid=2
    fi
    echo "Own-process userspace perf sampling is already enabled (level $perf_level)."
    exit 0
fi
exec python3 "$script_dir/profile_game.py" "$@"
