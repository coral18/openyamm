#!/usr/bin/env python3
"""Stage the Atlas-owned overlay for the v2.5 port, without old gameplay scripts or release assets."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='New or empty output directory')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    destination = args.output.resolve()
    if destination.exists() and any(destination.iterdir()):
        parser.error('Output directory must be empty; existing installations are never replaced')
    assets = root / 'assets_dev'
    files = [assets / name for name in (
        'engine/scripts/common/event_support.lua',
        'engine/ui/gameplay/journal.yml',
        'engine/data_tables/quest_locations.txt',
    )]
    files += sorted((assets / 'engine/world/atlas').glob('*.yml'))
    files += sorted((assets / 'engine/events').glob('Global_00_*.lua'))
    for world in ('mm7', 'mm8'):
        maps = assets / 'worlds' / world / 'events/maps'
        files += sorted(maps.glob('*_00_*.lua'))
        files += sorted(maps.glob('*_zz_*.lua'))
    if len(files) < 400 or any(not source.is_file() for source in files):
        parser.error('Atlas sources are incomplete; check out this branch including assets_dev')
    checksums = {}
    for source in files:
        relative = Path('mods') / source.relative_to(assets)
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        checksums[relative.as_posix()] = hashlib.sha256(target.read_bytes()).hexdigest()
    shutil.copyfile(root / 'ATLAS_PORT.md', destination / 'ATLAS_PORT.md')
    (destination / 'atlas-overlay.json').write_text(json.dumps({
        'project': 'OpenYAMM Atlas', 'upstream': 'v2.5', 'files': checksums,
    }, indent=2) + '\n', encoding='utf-8')
    print(f'Staged {len(files)} Atlas files in {destination}')


if __name__ == '__main__':
    main()
