#!/usr/bin/env bash
# Reuse the Windows source mirror, SDK, native objects and verified Android runtime packages.
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work_root="${OPENYAMM_ANDROID_DEV_ROOT:?Set OPENYAMM_ANDROID_DEV_ROOT to the prepared local Android directory}"
config="$work_root/build-config.json"
readarray -t paths < <(python3 - "$config" <<'PY'
import json, sys
from pathlib import Path
config = json.loads(Path(sys.argv[1]).read_text())
for key in ('python', 'source'):
    print(config[key])
PY
)
[[ ${#paths[@]} == 2 ]] || exit 1
windows_python="$(wslpath -u "${paths[0]}")"
mirror="$(wslpath -u "${paths[1]}")"
dev_root="$(dirname -- "$mirror")"
if [[ "$mirror" != "$dev_root/source" ]]; then
    echo 'Configured source must be the Windows development source mirror.' >&2
    exit 1
fi
# The Windows build also updates runtime ZIPs; do not touch a running desktop game.
powershell="/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
"$powershell" -NoProfile -Command \
    'if (Get-Process -Name openyamm -ErrorAction SilentlyContinue) { Write-Error "Close OpenYAMM before building"; exit 1 }'
OPENYAMM_WINDOWS_DEV_ROOT="$dev_root" "$repo_root/tools/extended_windows_dev.sh" build
python3 - "$repo_root" "$config" "$mirror" "$work_root/runtime-changes.json" <<'PY'
import hashlib, json, subprocess, sys
from pathlib import Path
repo, config_path, mirror, output = map(Path, sys.argv[1:])
config = json.loads(config_path.read_text())
changes = []
text_types = {'.yml', '.yaml', '.txt', '.lua', '.json', '.ini', '.csv', '.md'}
diff = subprocess.check_output(
    ['git', 'diff', '--name-status', '--no-renames', config['seedRef'], '--', 'assets_dev',
     'assets_cooked/android'], cwd=repo, text=True)
for line in diff.splitlines():
    status, name = line.split('\t', 1)
    if not name.startswith(('assets_dev/engine/', 'assets_dev/worlds/')) or Path(name).suffix not in text_types:
        raise SystemExit('Runtime seed needs an explicit refresh for this asset change: ' + name)
    digest = None
    if status != 'D':
        data = (repo / name).read_bytes()
        if data != (mirror / name).read_bytes():
            raise SystemExit('Stale source mirror: ' + name)
        digest = hashlib.sha256(data).hexdigest()
    changes.append({'virtual': name.removeprefix('assets_dev/'),
                    'source': config['source'].replace('\\', '/') + '/' + name, 'sha256': digest})
output.write_text(json.dumps({'source_ref': config['seedRef'], 'changes': changes}, indent=2) + '\n')
PY
for helper in prepare_runtime.py build.py; do
    "$windows_python" "$(wslpath -w "$repo_root/tools/android_local/$helper")" \
        --work-dir "$(wslpath -w "$work_root")"
done
