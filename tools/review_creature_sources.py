#!/usr/bin/env python3
"""Serve the selected creature source bank and explicitly import saved review adaptations.

The shared editor previews the original reviewed atlases with absolute saved transforms.
Exported/adapted atlases are never used as its baseline. Preparing or serving the editor
does not change adaptations.json or runtime assets. Import creates an immutable snapshot.
"""

import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile


REPO = Path(__file__).resolve().parents[1]
VIEWER = REPO / 'tools/creatures/acceptance_viewer'
DEFAULT_BANK = REPO / 'assets_source/creatures/mm6'
GEOMETRY = {'offset_px': [0, 0], 'scale_factor': 1, 'scale_anchor': 'bottom'}
ANNOTATIONS = ('regenerate', 'note', 'guide_offset_px', 'center_guide_offset_px', 'original_crop_origin_px')


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.tmp')
    with temporary.open('w') as stream:
        json.dump(value, stream, indent=2, ensure_ascii=False, allow_nan=False)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def repo_path(path):
    return (REPO / path).resolve()


def path_label(path):
    path = path.resolve()
    return str(path.relative_to(REPO)) if path.is_relative_to(REPO) else str(path)


def shared_viewer():
    sys.path.insert(0, str(VIEWER))
    import serve
    return serve


def review_paths(manifest):
    paths = {Path('aligned_2x') / f'{name}.png' for name in manifest['frames']}
    paths.update(Path('native_variants') / f'{name}_{palette}.png'
                 for name in manifest['frames'] for palette in manifest['variants'])
    for page in manifest['pages']:
        paths.update(Path(page[field]) for field in ('base', 'mask'))
        paths.update(Path(path) for path in page['variant_previews'].values())
    if any(path.is_absolute() or '..' in path.parts for path in paths):
        raise ValueError('Review artifact paths must stay inside their family package')
    return paths


def copy_checked(source, target, expected=None):
    digest = sha(source)
    if expected and digest != expected:
        raise ValueError(f'Selected source changed: {source}')
    if target.exists():
        if sha(target) != digest:
            raise ValueError(f'Existing bank review input differs: {target}')
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    target.chmod(0o644)


def family_brightness(colors, manifest):
    """The deployed exporter supports one whole-family brightness multiplier."""
    from checkpoints import validate_colors
    validate_colors(colors, manifest)
    values = []
    for palette in manifest['variants']:
        settings = colors.get(palette)
        if not settings or not settings['enabled']:
            values.append(1)
            continue
        # Neutral settings have no effect even when a material region is selected.
        if settings['saturation'] == 1 and settings['brightness'] == 1:
            values.append(1)
            continue
        if settings['region'] != 'all' or settings['saturation'] != 1:
            raise ValueError('Export currently supports Whole sprite brightness with 100% saturation. '
                             'Material-specific or saturation edits need explicit exporter work.')
        values.append(settings['brightness'])
    if len(set(values)) != 1 or not 0 < values[0] <= 2:
        raise ValueError('Set the same positive Whole sprite brightness for every native palette before import')
    return values[0]


def collect_changes(recipe, families, checkpoint):
    """Replace absolute transforms, including resets omitted from sparse checkpoints."""
    if checkpoint.get('schema_version') != 1 or not isinstance(checkpoint.get('families'), dict):
        raise ValueError('Unsupported checkpoint schema')
    if set(recipe['families']) != {family.id for family in families}:
        raise ValueError('Recipe and editor family coverage differ')
    candidate = copy.deepcopy(recipe)
    changes = []
    # Reuse the editor's strict validation, in temporary storage only. No revisions
    # or user files are changed by validation or the dry-run changes command.
    from checkpoints import Checkpoints
    with tempfile.TemporaryDirectory(prefix='creature-review-validation-') as directory:
        validator = Checkpoints(Path(directory) / 'frame_fixups.json')
        for family in families:
            current = recipe['families'][family.id]
            source = current['source']
            if (source['manifest_sha256'] != family.manifest_sha256 or
                    source['artifact_sha256'] != family.artifact_sha256):
                raise ValueError(f'{family.id}: recipe and editor select different art')
            versions = checkpoint['families'].get(family.id, {})
            record = versions.get(family.artifact_sha256)
            if record is None:
                if versions:
                    raise ValueError(f'{family.id}: checkpoint only contains a different selected generation')
                record = {'revision': 0, 'frames': {}, 'colors': {}, 'landmarks': {}, 'guides': None}
            else:
                for field, expected in [('manifest_sha256', family.manifest_sha256),
                                        ('artifact_sha256', family.artifact_sha256),
                                        ('pixels_per_logical_pixel', family.manifest['pixels_per_logical_pixel'])]:
                    if record.get(field) != expected:
                        raise ValueError(f'{family.id}: checkpoint {field} does not match selected art')
            if type(record.get('revision')) is not int or record['revision'] < 0:
                raise ValueError(f'{family.id}: checkpoint revision must be a nonnegative integer')
            validator.save(family, {**record, 'revision': 0})
            if set(current['frames']) != set(family.manifest['frames']):
                raise ValueError(f'{family.id}: recipe frame coverage differs')
            frames = {name: {field: record['frames'].get(name, {}).get(field, default)
                             for field, default in GEOMETRY.items()} for name in family.manifest['frames']}
            annotations = {name: {field: value for field, value in entry.items() if field in ANNOTATIONS}
                           for name, entry in record['frames'].items()}
            brightness = family_brightness(record.get('colors', {}), family.manifest)
            review = {**current['review'], 'revision': record['revision'],
                      'guides': record.get('guides'), 'landmarks': record.get('landmarks', {}),
                      'frame_annotations': annotations}
            changed_frames = [name for name, value in frames.items() if value != current['frames'][name]]
            geometry_changed = bool(changed_frames)
            color_changed = brightness != current['brightness_multiplier']
            review_changed = review != current['review']
            if geometry_changed or color_changed or review_changed:
                candidate['families'][family.id].update(frames=frames, brightness_multiplier=brightness, review=review)
                changes.append({'id': family.id, 'label': family.label, 'revision': record['revision'],
                                'changed_frames': changed_frames, 'geometry_changed': geometry_changed,
                                'brightness_before': current['brightness_multiplier'], 'brightness_after': brightness,
                                'review_changed': review_changed, 'requires_export': geometry_changed or color_changed})
    return candidate, changes


def prepare(bank):
    viewer = shared_viewer()
    recipe_path = bank / 'adaptations.json'
    recipe = read(recipe_path)
    collection_path = bank / 'review/collection.json'
    migration_path = bank / 'review/migration.json'
    state_path = bank / 'review_state/frame_fixups.json'
    if collection_path.exists() and migration_path.exists() and state_path.exists():
        families, _ = viewer.load_collection(collection_path)
        _, changes = collect_changes(recipe, families, read(state_path))
        print(f'Existing bank editor checked: {len(families)} families; {len(changes)} pending family records')
        return
    if state_path.exists():
        raise ValueError('Existing editor state has no complete migration record; refusing to replace it')
    old_collection_path = repo_path(recipe['source_collection']['path'])
    if sha(old_collection_path) != recipe['source_collection']['sha256']:
        raise ValueError('Pinned historical selection changed')
    old_collection = read(old_collection_path)
    if {r['creature'] for r in old_collection['families']} != {f['creature'] for f in recipe['families'].values()}:
        raise ValueError('Historical selection and recipe coverage differ')
    records = []
    for record in old_collection['families']:
        key = record['creature'].replace(':', '_')
        selected = read(bank / key / 'selected.json')
        source = (old_collection_path.parent / record['manifest']).resolve()
        if sha(source) != record['manifest_sha256']:
            raise ValueError(f'{key}: historical selected manifest changed')
        manifest = read(source)
        if viewer.artifact_digest(source.parent, source.read_bytes(), manifest) != record['artifact_sha256']:
            raise ValueError(f'{key}: historical selected pixels changed')
        target = bank / key / 'review'
        for relative in sorted(review_paths(manifest)):
            copy_checked(source.parent / relative, target / relative)
        for name in ('manifest.json', 'registration.json'):
            metadata = selected['metadata'][name]
            copy_checked(bank / key / metadata['path'], target / name, metadata['sha256'])
        records.append({**{field: record[field] for field in (
            'creature', 'label', 'frame_count', 'manifest_sha256', 'artifact_sha256', 'source')},
            'manifest': f'../{key}/review/manifest.json', 'pipeline_status': 'reviewed_source',
            'minimum_generated_sampling': record['minimum_generated_sampling']})
        print(f'Prepared {key}', flush=True)
    collection = {'schema_version': 1, 'collection_id': f"{recipe['world']}_creature_source_bank",
                  'title': f"{recipe['world'].upper()} creature source editor", 'family_count': len(records),
                  'frame_count': recipe['frame_count'], 'pixels_per_logical_pixel': 2,
                  'selection_policy': 'One selected unadapted reviewed package per family; hash-pinned bank copies',
                  'original_collection': recipe['source_collection'], 'families': records}
    write(collection_path, collection)
    for source in sorted((old_collection_path.parent / 'landmarks').glob('*.json')):
        if source.stem == 'seeds' or source.stem in recipe['families']:
            copy_checked(source, collection_path.parent / 'landmarks' / source.name)
    families, _ = viewer.load_collection(collection_path)
    old_state_path = repo_path(recipe['reviewed_checkpoint']['path'])
    old_state_bytes = old_state_path.read_bytes()
    old_state = json.loads(old_state_bytes)
    migrated = {**old_state, 'families': {}}
    for family in families:
        record = old_state['families'].get(family.id, {}).get(family.artifact_sha256)
        if record is not None:
            migrated['families'][family.id] = {family.artifact_sha256: {
                **record, 'manifest_path': str(family.root / 'manifest.json')}}
    _, changes = collect_changes(recipe, families, migrated)
    old_decisions_path = old_state_path.parent / 'decisions.json'
    old_decisions = viewer.load_decisions(old_decisions_path)
    decisions = {'schema_version': 1, 'decisions': {}}
    for family in families:
        decision = old_decisions['decisions'].get(family.id, {}).get(family.artifact_sha256)
        if decision:
            decisions['decisions'][family.id] = {family.artifact_sha256: decision}
    # Catch a save occurring during preparation; migration must not discard it.
    if old_state_path.read_bytes() != old_state_bytes:
        raise ValueError('Historical editor was edited during migration; rerun prepare to preserve the latest save')
    write(bank / 'review_state/decisions.json', decisions)
    write(state_path, migrated)
    write(migration_path, {'schema_version': 1, 'created_at': datetime.now(timezone.utc).isoformat(),
                          'recipe_sha256': sha(recipe_path), 'collection_sha256': sha(collection_path),
                          'previous_checkpoint': {'path': path_label(old_state_path),
                                                  'sha256': hashlib.sha256(old_state_bytes).hexdigest()},
                          'checkpoint_sha256': sha(state_path), 'family_count': len(families),
                          'frame_count': recipe['frame_count'], 'identical_selected_artifacts': True,
                          'pending_changes_at_migration': changes})
    print(f'Bank editor ready: {len(families)} families, {recipe["frame_count"]} frames; '
          f'{len(changes)} pending records; adaptations.json unchanged')


def changes_or_import(bank, apply):
    viewer = shared_viewer()
    recipe_path = bank / 'adaptations.json'
    state_path = bank / 'review_state/frame_fixups.json'
    collection_path = bank / 'review/collection.json'
    recipe_bytes, state_bytes = recipe_path.read_bytes(), state_path.read_bytes()
    families, _ = viewer.load_collection(collection_path)
    candidate, changes = collect_changes(json.loads(recipe_bytes), families, json.loads(state_bytes))
    report = {'changed_family_count': len(changes),
              'export_family_count': sum(c['requires_export'] for c in changes),
              'changed_frame_count': sum(len(c['changed_frames']) for c in changes), 'families': changes}
    if apply and changes:
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')
        snapshot = bank / 'review/imports' / stamp
        if recipe_path.read_bytes() != recipe_bytes or state_path.read_bytes() != state_bytes:
            raise ValueError('Editor or recipe changed during import; retry after its saved status appears')
        snapshot.mkdir(parents=True)
        (snapshot / 'previous_adaptations.json').write_bytes(recipe_bytes)
        (snapshot / 'frame_fixups.json').write_bytes(state_bytes)
        candidate['source_collection'] = {'path': path_label(collection_path), 'sha256': sha(collection_path)}
        candidate['reviewed_checkpoint'].update(path=path_label(snapshot / 'frame_fixups.json'),
                                                sha256=sha(snapshot / 'frame_fixups.json'),
                                                live_path=path_label(state_path))
        candidate['updated_at'] = datetime.now(timezone.utc).isoformat()
        candidate['review_import'] = {'snapshot': path_label(snapshot), 'requires_rebuild': True,
                                    'changed_families': [c['id'] for c in changes if c['requires_export']]}
        candidate['validation'] = {field: candidate['validation'][field]
                                   for field in ('viewer_implementation', 'viewer_sha256')}
        candidate['validation'].update(status='pending_export_verification',
                                       scope='Editor snapshot imported; export, geometry verification '
                                             'and deployment pending')
        write(snapshot / 'adaptations.json', candidate)
        report.update(previous_recipe_sha256=hashlib.sha256(recipe_bytes).hexdigest(),
                      recipe_sha256=sha(snapshot / 'adaptations.json'), snapshot=path_label(snapshot))
        write(snapshot / 'report.json', report)
        # The immutable snapshot is the import authority. A concurrent later save
        # remains in live state as a pending change for the next explicit import.
        write(recipe_path, candidate)
    print(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('prepare', 'serve', 'changes', 'import'))
    parser.add_argument('--bank', type=Path, default=DEFAULT_BANK)
    parser.add_argument('--port', type=int, default=8767)
    args = parser.parse_args()
    bank = args.bank.resolve()
    try:
        if args.command == 'prepare':
            prepare(bank)
        elif args.command == 'serve':
            os.execv(sys.executable, [sys.executable, '-B', str(VIEWER / 'serve.py'),
                                     '--collection', str(bank / 'review/collection.json'),
                                     '--state-dir', str(bank / 'review_state'), '--port', str(args.port)])
        else:
            changes_or_import(bank, args.command == 'import')
    except (ValueError, KeyError, OSError, TypeError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
