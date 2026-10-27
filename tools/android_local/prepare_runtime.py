#!/usr/bin/env python3
"""Reuse the configured local Windows Android toolchain and verified runtime payload."""

from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import shutil
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--work-dir", type=Path, required=True)
args = parser.parse_args()
root = args.work_dir.resolve()
config = json.loads((root / 'build-config.json').read_text())
seed = Path(config['seedApk'])
manifest = json.loads((root / 'runtime-changes.json').read_text())
assert manifest['source_ref'] == config['seedRef']
runtime = root / 'runtime-assets'
runtime.mkdir(exist_ok=True)
groups = ['engine', 'worlds/mm6', 'worlds/mm7', 'worlds/mm8', 'worlds/mmmerge']
receipt = runtime / 'seed.json'
if not receipt.exists():
    digest = hashlib.sha256()
    with seed.open('rb') as source:
        for block in iter(lambda: source.read(8 * 1024 * 1024), b''):
            digest.update(block)
    assert digest.hexdigest() == config['seedSha256']
    with zipfile.ZipFile(seed) as source:
        for group in groups:
            prefix = 'assets/' + group + '/'
            path = runtime / (group + '.zip')
            partial = path.with_suffix('.partial')
            path.parent.mkdir(parents=True, exist_ok=True)
            count = 0
            with zipfile.ZipFile(partial, 'w', compression=zipfile.ZIP_STORED) as output:
                for entry in source.infolist():
                    if not entry.filename.startswith(prefix) or entry.is_dir():
                        continue
                    relative = entry.filename[len(prefix):]
                    assert '..' not in PurePosixPath(relative).parts and not relative.startswith('/')
                    with source.open(entry) as data, output.open(relative, 'w', force_zip64=True) as target:
                        shutil.copyfileobj(data, target, 4 * 1024 * 1024)
                    count += 1
            assert count > 0, group
            partial.replace(path)
            print('Seeded', group, count, 'entries', flush=True)
    receipt.write_text(json.dumps({'sha256': digest.hexdigest(), 'source_ref': config['seedRef']}, indent=2))
else:
    saved = json.loads(receipt.read_text())
    assert saved['sha256'] == config['seedSha256'] and saved['source_ref'] == config['seedRef']

state_path = runtime / 'text-state.json'
old = json.loads(state_path.read_text()) if state_path.exists() else {}
desired = {entry['virtual']: entry['sha256'] for entry in manifest['changes']}
inputs = {entry['virtual']: entry for entry in manifest['changes']}
updates = {}
with zipfile.ZipFile(seed) as source:
    for path in old.keys() | desired.keys():
        if old.get(path) == desired.get(path) and (path in old) == (path in desired):
            continue
        assert '..' not in PurePosixPath(path).parts
        if path in desired:
            data = Path(inputs[path]['source']).read_bytes() if desired[path] is not None else None
            if data is not None:
                assert hashlib.sha256(data).hexdigest() == desired[path], path
        else:
            data = source.read('assets/' + path) if 'assets/' + path in source.namelist() else None
        group = next(g for g in groups if path.startswith(g + '/'))
        updates.setdefault(group, {})[path[len(group) + 1:]] = data

for group, entries in updates.items():
    path = runtime / (group + '.zip')
    partial = path.with_suffix('.partial')
    with zipfile.ZipFile(path) as source, zipfile.ZipFile(partial, 'w', compression=zipfile.ZIP_STORED) as output:
        for entry in source.infolist():
            if entry.filename in entries:
                continue
            with source.open(entry) as data, output.open(entry, 'w', force_zip64=True) as target:
                shutil.copyfileobj(data, target, 4 * 1024 * 1024)
        for name, data in entries.items():
            if data is not None:
                output.writestr(name, data)
    partial.replace(path)
    print('Applied source text updates:', group, len(entries), flush=True)
state_path.write_text(json.dumps(desired, indent=2) + '\n')
with zipfile.ZipFile(runtime / 'engine.zip') as package:
    assert json.loads(package.read('sprite_texture_profile.json')) == {
        'schema_version': 2, 'texture_profile': 'android'}
print('Persistent Android runtime packages ready', flush=True)
