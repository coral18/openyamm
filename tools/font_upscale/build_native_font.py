#!/usr/bin/env python3
"""Rebuild a native FNT restoration from a frozen manifest and editable stroke masters."""
from __future__ import annotations
import argparse
import hashlib
import html
import json
import unicodedata
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.svgLib.path import parse_path

from unpack_xbrz_font import parse_font
from font_pipeline import (bounds, conservative_trace, make_font, rectangle, source_ink,
                           stroke_outline, transformed, validate)

REPO = Path(__file__).resolve().parents[2]


def box(mask):
    ys, xs = np.where(mask)
    return [int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1] if len(xs) else None


def extract(source, profile):
    metrics, slots = {}, []
    for byte in range(256):
        m = source.glyph_metrics[byte]
        active = source.first_char <= byte <= source.last_char
        try:
            character = bytes([byte]).decode('cp1252') if active and byte >= 32 and byte != 127 else None
        except UnicodeDecodeError:
            character = None
        record = {"byte": byte, "left": m.left_spacing, "width": m.width, "right": m.right_spacing,
                  "advance": m.left_spacing + m.width + m.right_spacing}
        if active:
            pixels = np.frombuffer(source.pixels, np.uint8, count=m.width * source.font_height,
                                   offset=source.glyph_offsets[byte]).reshape(source.font_height, m.width)
            black_ink = profile['ink_policy'] == 'pixel 1 is ink, pixel 0 transparent'
            record.update(ink_bbox=box(pixels == 1 if black_ink else pixels > 1),
                          shadow_bbox=None if black_ink else box(pixels == 1))
        record['classification'] = ('space' if character in (' ', '\xa0') else 'visible') if character else 'reserved/control'
        record['unicode'] = ord(character) if character else None
        slots.append(record)
        if character:
            metrics[character] = record
    return metrics, slots


def compose_accent(character, metrics, masks, cores, profile):
    parts = unicodedata.normalize('NFD', character)
    if len(parts) != 2 or parts[0] not in cores:
        return None
    base = parts[0]
    original = masks[base].copy()
    outline = cores[base]
    if base == 'i':
        original[:profile['baseline'] - profile['x_height']] = False
        # Core i outline excludes its separate dot rectangle.
    target = masks[character]
    # Use a shared base only where the native glyph actually contains that base, at the same baseline.
    for dx in range(-4, 5):
        shifted = np.zeros_like(target)
        for y, x in zip(*np.where(original)):
            if 0 <= x + dx < target.shape[1]:
                shifted[y, x + dx] = True
        if shifted.sum() != original.sum() or np.any(shifted & ~target):
            continue
        remainder = target & ~shifted
        if not remainder.any():
            continue
        # Require accent ink to be separate from the base, except a cedilla below it.
        allowed = remainder.copy()
        if parts[1] == '\u0327':
            allowed[:profile['baseline']] = False
        else:
            allowed[box(original)[1]:] = False
        if not np.array_equal(allowed, remainder):
            continue
        result = RecordingPen()
        left_shift = metrics[character]['left'] - metrics[base]['left'] + dx
        transformed(outline, (1, 0, 0, 1, left_shift * 64, 0)).replay(result)
        conservative_trace(remainder, metrics[character]['left'], profile['baseline']).replay(result)
        return result, {'base': base, 'native_dx': dx, 'accent': parts[1]}
    return None


def render_line(text, size, profile, metrics, masks, font_path=None):
    scale = size / profile['height']
    width = sum(metrics[c]['advance'] for c in text)
    image = Image.new('L', (round((width + 12) * scale), round((profile['height'] + 8) * scale)))
    if font_path:
        ImageDraw.Draw(image).text((6 * scale, (profile['baseline'] + 4) * scale), text,
                                  font=ImageFont.truetype(str(font_path), size), anchor='ls', fill=255)
    else:
        native = Image.new('L', (width + 12, profile['height'] + 8))
        x = 6
        for c in text:
            m = metrics[c]
            native.paste(255, (x + m['left'], 4), Image.fromarray(masks[c].astype('uint8') * 255))
            x += m['advance']
        image = native.resize(image.size, Image.Resampling.NEAREST)
    return image


def proofs(out, profile, metrics, masks, font_path):
    label = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 17)
    glyph_label = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 14)
    for theme, bg, ink in [('dark', '#252727', '#ededdf'), ('paper', '#f2ebd8', '#211d16')]:
        canvas = Image.new('RGB', (1900, 2600), bg)
        draw = ImageDraw.Draw(canvas)
        y = 16
        for size in (profile['height'], 24, profile['height'] * 2, profile['height'] * 4):
            for title, path in [('Original bitmap', None), ('Restored TTF', font_path)]:
                draw.text((20, y), f'{title} / {size}px', font=label, fill=ink)
                y += 25
                for text in profile['samples'][:5] if size < 50 else profile['samples'][:2]:
                    mask = render_line(text, size, profile, metrics, masks, path)
                    canvas.paste(ink, (12, y), mask)
                    y += mask.height
                y += 18
        canvas.crop((0, 0, canvas.width, y + 12)).save(out / f'sentence_comparison_{theme}.png')
    characters = list(metrics)
    for page in range((len(characters) + 47) // 48):
        chars = characters[page * 48:(page + 1) * 48]
        canvas = Image.new('RGB', (1440, ((len(chars) + 7)//8) * 150), '#f2ebd8')
        draw = ImageDraw.Draw(canvas)
        for i, c in enumerate(chars):
            x, y = i % 8 * 180, i // 8 * 150
            draw.text((x + 5, y + 3), f'U+{ord(c):04X}   old / new', font=glyph_label, fill='#555044')
            for offset, path in [(0, None), (86, font_path)]:
                mask = render_line(c, 54, profile, metrics, masks, path)
                canvas.paste('#211d16', (x + offset, y + 25), mask)
        canvas.save(out / f'glyph_comparison_{page + 1}.png')
    sample = html.escape('\n'.join(profile['samples']))
    title = html.escape(profile['names']['familyName'])
    (out / 'preview.html').write_text(f'''<!doctype html><html lang="en"><meta charset="utf-8">
<title>{title}</title><style>
@font-face{{font-family:Restored;src:url({profile['output']})}}body{{background:#242626;color:#eee;margin:30px;font:16px system-ui}}
textarea{{width:95%;height:170px}}#proof{{font:{profile['height'] * 2}px Restored;white-space:pre-wrap;line-height:1.25;padding:20px}}
</style><h1>{title}</h1><p><a href="../README.md">Source, comparisons and runtime review</a></p>
<textarea id="text">{sample}</textarea><p>Size <input id="size" type="range" min="{profile['height']}" max="{profile['height'] * 4}" value="{profile['height'] * 2}">
<label><input id="paper" type="checkbox">Paper background</label></p><div id="proof"></div><script>
const text=document.querySelector('#text'),size=document.querySelector('#size'),proof=document.querySelector('#proof');
function update(){{proof.textContent=text.value;proof.style.fontSize=size.value+'px'}}text.oninput=size.oninput=update;
document.querySelector('#paper').onchange=e=>{{proof.style.background=e.target.checked?'#f2ebd8':'#242626';
proof.style.color=e.target.checked?'#211d16':'#eee'}};update();</script></html>''')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    args = parser.parse_args()
    profile = json.loads(args.manifest.read_text())
    source_path = REPO / profile['source']
    raw = source_path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != profile['source_sha256']:
        raise ValueError('Source changed; review and freeze the new source explicitly')
    if profile['encoding'] != 'windows-1252' or profile['ink_policy'] not in (
            'pixel > 1; pixel 1 is shadow, pixel 0 transparent', 'pixel 1 is ink, pixel 0 transparent'):
        raise ValueError('Unsupported source policy; implement and review it before building')
    source = parse_font(raw)
    if source.font_height != profile['height']:
        raise ValueError('Manifest cell height differs from native source')
    root = args.manifest.parent
    out = root / 'output';out.mkdir(exist_ok=True)
    references = root / 'references';references.mkdir(exist_ok=True)
    (references / source_path.name).write_bytes(raw)
    metrics, slots = extract(source, profile)
    (references / 'metrics.json').write_text(json.dumps({'source_sha256': profile['source_sha256'],
        'height': source.font_height, 'first_byte': source.first_char, 'last_byte': source.last_char,
        'slots': slots, 'metrics': metrics}, indent=2, ensure_ascii=False)+'\n')
    black_ink = profile['ink_policy'] == 'pixel 1 is ink, pixel 0 transparent'
    if black_ink and any(value not in (0, 1) for value in source.pixels):
        raise ValueError('Black-ink source contains unexpected palette values')
    masks = {c: source_ink(source, m['byte'], black_ink) for c, m in metrics.items()}
    masters = json.loads((root / profile['masters']).read_text())
    cores = {c: stroke_outline(commands, masters['stroke_width'], metrics[c]['left'],
                              profile['baseline'], profile.get('square_caps', False), masters.get('nib'))
             for c, commands in masters['glyphs'].items()}
    for c, commands in masters.get('filled_paths', {}).items():
        if c in cores:
            raise ValueError(f'Duplicate stroke and filled master: {c!r}')
        native = RecordingPen()
        parse_path(commands, native)
        cores[c] = transformed(native, (64, 0, 0, -64, metrics[c]['left'] * 64, profile['baseline'] * 64))
    outlines, provenance = {}, {}
    for c, m in metrics.items():
        if c in masters.get('components', {}):
            recipe = masters['components'][c]
            base = recipe['base']
            dx = (m['left'] - metrics[base]['left'] + recipe.get('dx', 0)) * 64
            outline = transformed(cores[base], (1, 0, 0, 1, dx, 0))
            if 'filled_path' in recipe:
                native = RecordingPen()
                parse_path(recipe['filled_path'], native)
                transformed(native, (64, 0, 0, -64, m['left'] * 64, profile['baseline'] * 64)).replay(outline)
            else:
                stroke_outline(recipe['path'], masters['stroke_width'], m['left'], profile['baseline'],
                               profile.get('square_caps', False), masters.get('nib')).replay(outline)
            provenance[c] = {'method': 'explicit shared-base component recipe', **recipe}
        elif c in cores:
            outline = RecordingPen();cores[c].replay(outline)
            for x0,y0,x1,y1 in masters.get('rectangles',{}).get(c,[]):
                rectangle((x0+m['left'])*64,(profile['baseline']-y1)*64,
                          (x1+m['left'])*64,(profile['baseline']-y0)*64).replay(outline)
            provenance[c] = {'method': ('native-coordinate filled Bezier master' if c in masters.get('filled_paths', {})
                                       else 'native-coordinate stroke/Bezier master')}
        elif c in (' ', '\xa0'):
            outline = RecordingPen();provenance[c] = {'method': 'native spacing; no ink'}
        else:
            composition = compose_accent(c, metrics, masks, cores, profile)
            if composition:
                outline, recipe = composition
                provenance[c] = {'method': 'shared master + native accent', **recipe}
            else:
                outline = conservative_trace(masks[c], m['left'], profile['baseline'])
                provenance[c] = {'method': 'conservative native foreground trace'}
        outlines[c] = outline
        extent = bounds(outline)
        if extent and (extent[0] < (m['left']-4)*64 or extent[2] > (m['left']+m['width']+4)*64
                       or extent[1] < (profile['baseline']-profile['height']-4)*64
                       or extent[3] > (profile['baseline']+4)*64):
            raise ValueError(f'Glyph exceeds padded runtime cell: {c!r}')
    svg_root = root / 'outlines';svg_root.mkdir(exist_ok=True)
    for c, outline in outlines.items():
        pen = SVGPathPen(None);outline.replay(pen)
        m = metrics[c]
        (svg_root / f'U{ord(c):04X}.svg').write_text(
            f'<svg xmlns="http://www.w3.org/2000/svg" '
            f'viewBox="{(m["left"] - 4) * 64} {-(profile["baseline"] + 4) * 64} '
            f'{(m["width"] + 8) * 64} {(profile["height"] + 8) * 64}">'
            f'<path transform="scale(1,-1)" d="{pen.getCommands()}"/></svg>\n')
    path = out / profile['output']
    make_font(outlines, metrics, path, profile)
    report = validate(path, metrics, profile)
    report.update(source_sha256=profile['source_sha256'], provenance=provenance)
    (out / 'validation.json').write_text(json.dumps(report, indent=2, ensure_ascii=False)+'\n')
    proofs(out, profile, metrics, masks, path)
    print(json.dumps({k:v for k,v in report.items() if k != 'provenance'}, indent=2))


if __name__ == '__main__':
    main()
