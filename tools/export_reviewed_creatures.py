#!/usr/bin/env python3
"""Curate selected restoration inputs and export reviewed fractional placement at physical 2x."""
import argparse
import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import shutil
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

REPO = Path(__file__).resolve().parents[1]
HASHES = {}

REVIEWED_RASTER_FRAMES = {
    'mm6_ert': {'ertdya', 'ertdyb', 'ertdyc', 'ertdyd', 'ertdye',
                'ertwaa2', 'ertwab2', 'ertwac2', 'ertwad2', 'ertwae2', 'ertwaf2'},
}


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n')


def digest(path):
    stat = path.stat()
    key = (str(path), stat.st_size, stat.st_mtime_ns)
    if key not in HASHES:
        with path.open('rb') as stream:
            HASHES[key] = hashlib.file_digest(stream, 'sha256').hexdigest()
    return HASHES[key]


def retain(source, bank, category, expected=None):
    source = source.resolve(strict=True)
    actual = digest(source)
    if expected and actual != expected:
        raise ValueError(f'Selected input changed: {source}')
    relative = Path('inputs') / category / (actual[:16] + '_' + source.name)
    target = bank / relative
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        target.chmod(0o644)
    if digest(target) != actual:
        raise ValueError(f'Curated input changed: {target}')
    return dict(path=relative.as_posix(), sha256=actual, original=str(source.relative_to(REPO))
                if source.is_relative_to(REPO) else str(source))


def curate(key, family, root):
    bank = root / key
    recipe_path = bank / 'selected.json'
    if recipe_path.exists():
        result = read(recipe_path)
        if result['selected_manifest_sha256'] != family['source']['manifest_sha256']:
            raise ValueError(f'Different selected generation in existing bank: {key}')
        return result
    path = REPO / family['source']['manifest']
    if digest(path) != family['source']['manifest_sha256']:
        raise ValueError(f'Approved manifest changed: {path}')
    project = path.parent
    manifest, registration = read(path), read(project / 'registration.json')
    result = dict(selected_manifest_sha256=digest(path), selected_manifest=family['source']['manifest'],
                  metadata={}, pages=[], frames={}, generations={}, native={}, palettes={})
    for name in ['manifest.json', 'registration.json', 'sources.json', 'config.json', 'generation.json',
                 'recolor_presets.json', 'recolor_vectors.json', 'calibration.json', 'processing.json',
                 'run.json', 'upscale.json', 'acceptance.json', 'validation.json']:
        if (project / name).is_file():
            result['metadata'][name] = retain(project / name, bank, 'metadata')
    for page in manifest['pages']:
        assets = {k: retain(project / page[k], bank, 'reviewed_atlases') for k in ['base', 'mask']}
        if family.get('baked_native_palettes'):
            assets['variants'] = {p: retain(project/page['variant_previews'][p], bank, 'palette_masters')
                                  for p in family['baked_native_palettes']}
        result['pages'].append(assets)
    sources = read(project / 'sources.json')
    for name, source in sources['sources'].items():
        result['native'][name] = retain(REPO / source['path'], bank, 'native', source['sha256'])
    for name, source in sources['palette_sources'].items():
        result['palettes'][name] = retain(REPO / source['path'], bank, 'palettes', source['sha256'])
    video_jobs = {}
    if (project / 'upscale.json').exists():
        video_jobs = {j['job']: j for j in read(project / 'upscale.json').get('jobs', []) if 'job' in j}
    for name, frame in manifest['frames'].items():
        rec = registration[name]
        clean = REPO / rec['clean_path'] if rec.get('clean_path') else project / 'cleaned' / (name + '.png')
        entry = dict(clean=retain(clean, bank, 'masters', rec.get('clean_sha256')))
        if name in REVIEWED_RASTER_FRAMES.get(key, set()):
            entry['source_mode'] = 'reviewed_raster'
        if rec['batch'] in video_jobs:
            job = video_jobs[rec['batch']]
            entry['video_rgb'] = retain(Path(job['output']), bank, 'video_x3', job['output_sha256'])
        result['frames'][name] = entry
        if rec['batch'] in result['generations']:
            continue
        raw = (REPO / rec['raw_path']).resolve()
        generation = dict(raw=retain(raw, bank, 'returned_sheets', rec['raw_sha256']), references=[])
        # The registered return, rather than an old batch label, identifies the selected prompt and guides.
        candidates = [project] + list(raw.parents[:4])
        for origin in dict.fromkeys(candidates):
            gp = origin / 'generation.json'
            if not gp.is_file():
                continue
            batches = read(gp).get('batches', {})
            selected = next((b for b in batches.values() if b.get('raw', {}).get('sha256') == rec['raw_sha256']), None)
            if selected:
                generation['prompt'] = retain(origin / selected['prompt']['path'], bank, 'prompts', selected['prompt']['sha256'])
                for ref in selected['references']:
                    rp = origin / ref['path']
                    category = 'guides' if 'outline' in rp.name.lower() or 'native' in rp.name.lower() else 'appearance'
                    generation['references'].append(retain(rp, bank, category, ref['sha256']))
                break
        if 'prompt' not in generation:
            for origin in dict.fromkeys(candidates):
                run_path = origin/'run.json'
                if not run_path.is_file():
                    continue
                for job in read(run_path).get('jobs', []):
                    attempts = [a for a in job.get('attempts', []) if a.get('sha256')==rec['raw_sha256']]
                    if not attempts:
                        continue
                    attempt = attempts[0]
                    generation['prompt'] = retain(REPO/attempt['prompt'], bank, 'prompts', attempt.get('prompt_sha256'))
                    for ref in attempt['references']:
                        rp = REPO/ref['path']
                        category = 'guides' if 'outline' in rp.name.lower() or 'native' in rp.name.lower() else 'appearance'
                        generation['references'].append(retain(rp, bank, category, ref['sha256']))
                    break
                if 'prompt' in generation:
                    break
        # Native-video exceptions have no imagegen guide/prompt; record their existing source mapping explicitly.
        generation['method'] = 'packed imagegen return' if 'prompt' in generation else 'retained accepted/native-video source; see registration'
        result['generations'][rec['batch']] = generation
    write(recipe_path, result)
    return result


def input_path(bank, record):
    path = bank / record['path']
    if digest(path) != record['sha256']:
        raise ValueError(f'Curated source hash mismatch: {path}')
    return path


def tile(image, frame):
    x, y, w, h = frame['atlas_xywh']
    return image.crop((x, y, x+w, y+h))


def sample(image, box, size):
    # Crop with a Lanczos halo, including transparent space outside the source, then sample once.
    radius = 4
    left, top = math.floor(box[0])-radius, math.floor(box[1])-radius
    right, bottom = math.ceil(box[2])+radius, math.ceil(box[3])+radius
    patch = image.crop((left, top, right, bottom))
    local = (box[0]-left, box[1]-top, box[2]-left, box[3]-top)
    return patch.resize(size, Image.Resampling.LANCZOS, box=local)


def extrude(base, mask):
    rgba = np.array(base)
    values = np.array(mask)
    solid = rgba[..., 3] > 0
    if solid.any():
        distance, nearest = ndimage.distance_transform_edt(~solid, return_indices=True)
        band = (distance <= 3) & ~solid
        rgba[band, :3] = rgba[nearest[0][band], nearest[1][band], :3]
        values[band] = values[nearest[0][band], nearest[1][band]]
    return Image.fromarray(rgba), Image.fromarray(values, mode=mask.mode)


def palette_helpers():
    path = REPO / 'tools/creatures/palette_export.py'
    spec = importlib.util.spec_from_file_location('existing_palette_export', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def export(key, family, bank, selected, output, recipe_hash):
    manifest = read(input_path(bank, selected['metadata']['manifest.json']))
    registration = read(input_path(bank, selected['metadata']['registration.json']))
    original_pages = [{k: Image.open(input_path(bank, page[k])).copy() for k in ['base', 'mask']}
                      for page in selected['pages']]
    package = 'mm6_gob' if key == 'mm6_goblin' else key
    target = output / package
    (target / 'atlas').mkdir(parents=True, exist_ok=True)
    runtime = {k: copy.deepcopy(manifest[k]) for k in ['creature', 'pixels_per_logical_pixel', 'logical_canvas',
                                                      'logical_pivot', 'variants']}
    baked_palettes = family.get('baked_native_palettes', [])
    if baked_palettes:
        runtime['variants'] = {p: dict(exact_base_bypass=True, name=v.get('name',p)) for p,v in manifest['variants'].items()}
        runtime['palette_frame_aliases'] = {p:{} for p in baked_palettes}
    runtime.update(schema_version=1, recolor_model=(manifest['recolor_model'] if 'recolor_model' in manifest else manifest['recolor']['model']),
                   brightness_multiplier=family['brightness_multiplier'], frames={}, pages=[],
                   adaptations_sha256=recipe_hash)
    # Actors use a bottom origin; centered native Cactus art still keeps its reviewed crop coordinates.
    runtime['logical_pivot'][1] = manifest['logical_canvas'][1]
    helpers = palette_helpers()
    runtime['recolor_model'] = helpers.normalize_palette_model(runtime, runtime['recolor_model'])
    sources = read(input_path(bank, selected['metadata']['sources.json']))
    for source in sources['sources'].values():
        if 'fixed_palette' in source:
            runtime['variants'].setdefault(str(source['fixed_palette']), dict(exact_base_bypass=True))
    overrides = {}
    presets = {} if baked_palettes else manifest.get('recolor_presets', {})
    next_variant = 32767
    for preset, data in presets.get('presets', {}).items():
        ids = {}
        for palette, vectors in data['vectors'].items():
            if runtime['variants'][palette].get('exact_base_bypass'):
                continue
            while str(next_variant) in runtime['variants']:
                next_variant -= 1
            shape = np.asarray(vectors).shape
            field = {'multi_mask_luminance_lut_v1': 'region_luminance_luts',
                     'multi_mask_luminance_rgb_v1': 'region_luminance_vectors',
                     'masked_luminance_lut_v1': 'luminance_lut',
                     'masked_luminance_rgb_v1': 'luminance_vector'}[runtime['recolor_model']]
            appearance = {field: vectors}
            if runtime['recolor_model'] == 'masked_luminance_lut_v1' and shape == (3,):
                appearance = dict(recolor_model='masked_luminance_rgb_v1', luminance_vector=vectors)
            runtime['variants'][str(next_variant)] = appearance
            ids[palette] = next_variant
            next_variant -= 1
        overrides[preset] = ids
    lookup_export = dict(runtime, variants={p:v for p,v in runtime['variants'].items() if 'recolor_model' not in v})
    helpers.export_palette_lookups(lookup_export, target)
    entries = []
    audit = dict(family=key, frames=len(manifest['frames']), high_resolution_frames=0, video_frames=0,
                 source_reconstruction_failures=[], maximum_float_geometry_error_px=0,
                 geometry={}, explicit_reviewed_raster_frames=[])
    video_cache = (None, None)
    for name, frame in manifest['frames'].items():
        rec = registration[name]
        reviewed = tile(original_pages[frame['page']]['base'].convert('RGBA'), frame)
        mask = tile(original_pages[frame['page']]['mask'], frame)
        pixels = np.array(reviewed)
        yy, xx = np.where(pixels[..., 3] >= 128)
        bounds = [int(xx.min()), int(yy.min()), int(xx.max()+1), int(yy.max()+1)] if len(xx) else [0, 0, *reviewed.size]
        fix = family['frames'][name]
        origin = np.array(frame['crop_origin_px'], float)
        size = np.array(reviewed.size, float)
        anchor = (np.array(manifest['logical_pivot'])*manifest['pixels_per_logical_pixel'] if fix['scale_anchor']=='pivot'
                  else origin + [(bounds[0]+bounds[2])/2, bounds[1 if fix['scale_anchor']=='top' else 3]])
        transformed_origin = anchor + fix['scale_factor']*(origin-anchor) + fix['offset_px']
        draw_size = size*fix['scale_factor']
        destination = tuple(max(1, round(v)) for v in draw_size)
        entry = selected['frames'][name]
        clean = Image.open(input_path(bank, entry['clean'])).convert('RGBA')
        registered_size = np.array([max(1, round(v*rec['scale'])) for v in clean.size])
        raw_scale = registered_size / clean.size
        raw_origin = (np.array(rec['translate']) - rec['canvas_origin_px'] + frame['aligned_canvas_origin_px'])
        box = np.r_[(origin-raw_origin)/raw_scale, (origin+size-raw_origin)/raw_scale]
        # Reconstruct the frozen reviewed raster independently before trusting its master mapping.
        reconstructed = clean.convert('RGBa').resize(tuple(registered_size), Image.Resampling.LANCZOS).convert('RGBA')
        reconstructed = reconstructed.crop(tuple(int(v) for v in np.r_[origin-raw_origin, origin+size-raw_origin]))
        rp = np.array(reconstructed)
        visible = pixels[..., 3] >= 128
        if 'video_rgb' in entry:
            video_path = input_path(bank, entry['video_rgb'])
            if video_cache[0] != video_path:
                video_cache = (video_path, Image.open(video_path).convert('RGB'))
            rgb_sheet = video_cache[1]
            rr = np.array(rec['raw_rect'])*3
            rgb_master = rgb_sheet.crop(tuple(rr))
            rgb_ref = rgb_master.resize(tuple(registered_size), Image.Resampling.LANCZOS).crop(
                tuple(int(v) for v in np.r_[origin-raw_origin, origin+size-raw_origin]))
            rgb_ok = np.array_equal(np.array(rgb_ref)[visible], pixels[..., :3][visible])
            alpha_ok = np.array_equal(rp[..., 3], pixels[..., 3])
            if not rgb_ok or not alpha_ok:
                raise ValueError(f'Video master does not reconstruct approved raster: {key}/{name}')
            restored = sample(rgb_master, tuple(box*3), destination).convert('RGBA')
            restored.putalpha(reviewed.getchannel('A').resize(destination, Image.Resampling.LANCZOS))
            audit['video_frames'] += 1
        elif np.array_equal(rp[..., 3], pixels[..., 3]) and np.array_equal(rp[..., :3][visible], pixels[..., :3][visible]):
            restored = sample(clean.convert('RGBa'), tuple(box), destination).convert('RGBA')
            audit['high_resolution_frames'] += 1
        else:
            # Some accepted per-frame warps/clipping cleanups are authored only in the retained physical master.
            # Preserve that reviewed artwork explicitly; never substitute an unverified earlier generation.
            if entry.get('source_mode') != 'reviewed_raster' or name not in REVIEWED_RASTER_FRAMES.get(key, set()):
                raise ValueError(f'Master does not reconstruct reviewed art: {key}/{name}')
            restored = reviewed.convert('RGBa').resize(destination, Image.Resampling.LANCZOS).convert('RGBA')
            audit['explicit_reviewed_raster_frames'].append(name)
        resized_mask = Image.merge(mask.mode, [b.resize(destination, Image.Resampling.LANCZOS) for b in mask.split()])
        restored, resized_mask = extrude(restored, resized_mask)
        fields = dict(crop_origin_px=transformed_origin.tolist(), draw_size_px=draw_size.tolist())
        if name in presets.get('frames', {}):
            fields['palette_overrides'] = overrides[presets['frames'][name]]
        runtime['frames'][name] = fields
        float_error = max(np.max(np.abs(np.float32(transformed_origin)-transformed_origin)),
                          np.max(np.abs(np.float32(draw_size)-draw_size)))
        audit['maximum_float_geometry_error_px'] = max(audit['maximum_float_geometry_error_px'], float(float_error))
        audit['geometry'][name] = dict(origin=transformed_origin.tolist(), size=draw_size.tolist(), bounds=bounds)
        entries.append((name, restored, resized_mask))
        for palette in baked_palettes:
            reference = selected['pages'][frame['page']]['variants'][palette]
            alternate = tile(Image.open(input_path(bank, reference)).convert('RGBA'), frame)
            if alternate.getchannel('A').tobytes() != reviewed.getchannel('A').tobytes():
                raise ValueError(f'Native palette changes reviewed alpha: {key}/{name}/{palette}')
            alternate = alternate.convert('RGBa').resize(destination, Image.Resampling.LANCZOS).convert('RGBA')
            alternate, alternate_mask = extrude(alternate, resized_mask)
            alias = name[:-1]+'__palette_'+palette+name[-1] if name[-1].isdigit() else name+'__palette_'+palette
            runtime['palette_frame_aliases'][palette][name] = alias
            runtime['frames'][alias] = copy.deepcopy(fields)
            entries.append((alias, alternate, alternate_mask))
    # Stable shelves with explicit gutters. The GPU cook adds isolated mip padding afterwards.
    limit, gutter = 2048, 4
    pages, page_index, cursor_x, cursor_y, row_height = [], 0, 2, 2, 0
    mode = entries[0][2].mode
    for name, base, mask in sorted(entries, key=lambda e: (-e[1].height, -e[1].width, e[0])):
        if base.width+4>limit or base.height+4>limit:
            raise ValueError(f'Export tile exceeds atlas size: {key}/{name}')
        if cursor_x+base.width+2>limit:
            cursor_x, cursor_y, row_height = 2, cursor_y+row_height+gutter, 0
        if cursor_y+base.height+2>limit:
            page_index += 1
            cursor_x, cursor_y, row_height = 2, 2, 0
        if page_index == len(pages):
            pages.append([Image.new('RGBA', (limit,limit)), Image.new(mode, (limit,limit)), 0, 0])
        page = pages[page_index]
        page[0].paste(base, (cursor_x,cursor_y));page[1].paste(mask, (cursor_x,cursor_y))
        runtime['frames'][name].update(page=page_index, atlas_xywh=[cursor_x,cursor_y,*base.size])
        page[2], page[3] = max(page[2], cursor_x+base.width+2), max(page[3], cursor_y+base.height+2)
        cursor_x += base.width+gutter
        row_height = max(row_height, base.height)
    for index, (base, mask, width, height) in enumerate(pages):
        page = dict(base=f'atlas/base_{index}.png', mask=f'atlas/mask_{index}.png', size=[width,height])
        base.crop((0,0,width,height)).save(target/page['base']);mask.crop((0,0,width,height)).save(target/page['mask'])
        runtime['pages'].append(page)
    for k in ['animations', 'animations_by_palette', 'native_group_bindings', 'native_pixel_aliases']:
        if k in manifest:
            runtime[k] = copy.deepcopy(manifest[k])
    write(target/'manifest.json', runtime)
    audit['baked_palette_frames'] = sum(len(v) for v in runtime.get('palette_frame_aliases', {}).values())
    audit['files'] = {str(p.relative_to(target)): digest(p) for p in target.rglob('*') if p.is_file()}
    write(bank/'export_validation.json', audit)
    return audit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--recipe', type=Path, default=REPO/'assets_source/creatures/mm6/adaptations.json')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--family', action='append')
    args = parser.parse_args()
    recipe = read(args.recipe)
    root = args.recipe.parent
    reports = []
    for key, family in recipe['families'].items():
        if args.family and key not in args.family:
            continue
        selected = curate(key, family, root)
        report = export(key, family, root/key, selected, args.output, digest(args.recipe))
        reports.append(report)
        print(key, report['frames'], 'masters', report['high_resolution_frames'], 'video', report['video_frames'],
              'reviewed raster', len(report['explicit_reviewed_raster_frames']), flush=True)
    write(args.output.parent/'export_validation.json', dict(recipe_sha256=digest(args.recipe), families=[
        {k:v for k,v in r.items() if k!='geometry'} for r in reports]))


if __name__ == '__main__':
    main()
