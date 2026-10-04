#!/usr/bin/env python3
"""Build the reviewed AI reconstruction of Arrus into an outline TrueType font.

No model calls: generated sheets and their cell maps are versioned inputs.
Requires fonttools, numpy, pillow, potracer and scipy. See dialogue_ai/README.md.
"""

from __future__ import annotations

import hashlib
import json
import shutil
import unicodedata
from pathlib import Path
from functools import partial

import numpy as np
import potrace
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage
from fontTools.pens.recordingPen import RecordingPen

from unpack_xbrz_font import parse_font
from font_pipeline import bounds, transformed, rectangle, source_ink
from font_pipeline import make_font as assemble_font, validate as validate_font


ROOT = Path(__file__).resolve().parent / "dialogue_ai"
SOURCE = Path(__file__).resolve().parents[2] / "assets_dev/engine/fonts/english_text/Arrus.fnt"
ASSET = Path(__file__).resolve().parents[2] / "assets_dev/engine/fonts/truetype/openyamm_dialogue_italic.ttf"
PIXEL = 64
BASELINE = 14
UNITS_PER_EM = 19 * PIXEL
ACCENTS = {
    "\u0300": "`", "\u0301": "´", "\u0302": "^", "\u0303": "~", "\u0308": "¨",
    "\u030a": "˚", "\u030c": "ˇ", "\u0327": "¸",
}



ARRUS_PROFILE = {
    "height": 19, "baseline": 14, "x_height": 8, "cap_height": 11,
    "italic": True, "italic_angle": -12, "caret_slope_run": 213,
    "names": {
        "familyName": "OpenYAMM Dialogue", "styleName": "Italic",
        "uniqueFontIdentifier": "OpenYAMMDialogue-Italic-1.000",
        "fullName": "OpenYAMM Dialogue Italic", "psName": "OpenYAMMDialogue-Italic",
        "version": "Version 1.000",
        "description": "AI-assisted outline reconstruction of the OpenYAMM Arrus dialogue bitmap."
                       " Retains legacy character advances; see accompanying provenance.",
    },
}
make_font = partial(assemble_font, profile=ARRUS_PROFILE)
validate = partial(validate_font, profile=ARRUS_PROFILE)


def fit(outline, box):
    x0, y0, x1, y1 = bounds(outline)
    left, bottom, right, top = box
    sx, sy = (right - left) / (x1 - x0), (top - bottom) / (y1 - y0)
    return transformed(outline, (sx, 0, 0, sy, left - sx * x0, bottom - sy * y0))


def extract_sheet(filename, characters, columns, rows):
    image = np.array(Image.open(filename).convert("L"))
    result = {}
    for index, character in enumerate(characters):
        row, column = divmod(index, columns)
        cell = image[
            round(row * image.shape[0] / rows):round((row + 1) * image.shape[0] / rows),
            round(column * image.shape[1] / columns):round((column + 1) * image.shape[1] / columns),
        ]
        ink = cell < 140
        labels, _ = ndimage.label(ink)
        areas = np.bincount(labels.ravel())
        keep = areas >= 4
        keep[0] = False
        ink = keep[labels]
        ys, xs = np.where(ink)
        if not len(xs):
            raise ValueError(f"Empty cell for {character!r} in {filename}")
        if min(xs) == 0 or min(ys) == 0 or max(xs) == cell.shape[1] - 1 or max(ys) == cell.shape[0] - 1:
            raise ValueError(f"Clipped cell for {character!r} in {filename}")
        result[character] = ink[min(ys):max(ys) + 1, min(xs):max(xs) + 1]
    return result


def trace(ink):
    # Potracer expects False for black and inverts its input internally.
    image = np.pad(~ink, 2, constant_values=True)
    path = potrace.Bitmap(image).trace(turdsize=0, alphamax=1.0, opttolerance=0.15)
    pen = RecordingPen()
    def point(value):
        return value.x, value.y
    for curve in path:
        pen.moveTo(point(curve.start_point))
        for segment in curve:
            if segment.is_corner:
                pen.lineTo(point(segment.c))
                pen.lineTo(point(segment.end_point))
            else:
                pen.curveTo(point(segment.c1), point(segment.c2), point(segment.end_point))
        pen.closePath()
    return transformed(pen, (1, 0, 0, -1, 0, 0))


def native_box(metric):
    x0, y0, x1, y1 = metric["ink_bbox"]
    return ((metric["left"] + x0) * PIXEL, (BASELINE - y1) * PIXEL,
            (metric["left"] + x1) * PIXEL, (BASELINE - y0) * PIXEL)


def proof(font_path, characters):
    image = Image.new("RGB", (1500, 1520), "#f5efdf")
    draw = ImageDraw.Draw(image)
    label = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 19)
    draw.text((45, 25), "OpenYAMM Dialogue / reconstructed Arrus / actual TTF rendering", font=label, fill="#544c40")
    samples = [
        "Welcome to New Sorpigal, traveller. What brings you here?",
        "The goblins have taken the road. Speak to the captain before you leave.",
        "Bring me 250 gold pieces, and I will teach you what I know.",
    ]
    y = 80
    for size in (19, 24, 38):
        font = ImageFont.truetype(str(font_path), size)
        draw.text((45, y), f"{size} px", font=label, fill="#807363")
        y += 32
        for text in samples:
            line = ""
            for word in text.split():
                candidate = f"{line} {word}".strip()
                if font.getlength(candidate) > 1410:
                    draw.text((45, y), line, font=font, fill="#201b16")
                    y += round(size * 1.3)
                    line = word
                else:
                    line = candidate
            draw.text((45, y), line, font=font, fill="#201b16")
            y += round(size * 1.3)
        y += 30
    font = ImageFont.truetype(str(font_path), 64)
    for text in ("HAMBURGEFontsiv 0123456789", "ÀÁÂÃÄÅ Ç ÈÉÊË ÌÍÎÏ Ñ ÒÓÔÕÖ Ø",
                 "àáâãäå ç èéêë ìíîï ñ òóôõö ø", "ŠŽšž Œœ Ææ Ðð Þþ ß Üü Ýýÿ",
                 "“A clear voice,” she said. ‘Yes!’ — €250"):
        draw.text((45, y), text, font=font, fill="#201b16")
        y += 82
    image.crop((0, 0, 1500, y + 35)).save(ROOT / "output/dialogue_specimen.png")
    columns, cell_width, cell_height = 12, 120, 120
    sheet_height = ((len(characters) + columns - 1) // columns) * cell_height
    sheet = Image.new("RGB", (columns * cell_width, sheet_height), "white")
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype(str(font_path), 58)
    for index, character in enumerate(characters):
        x, y = (index % columns) * cell_width, (index // columns) * cell_height
        draw.text((x + 7, y + 4), f"U+{ord(character):04X}", font=label, fill="#888888")
        draw.text((x + 25, y + 30), character, font=font, fill="black")
    sheet.save(ROOT / "output/glyph_proof.png")

    source = parse_font(SOURCE.read_bytes())
    text = "Welcome to New Sorpigal."
    source_width = sum(source.glyph_metrics[ord(c)].left_spacing + source.glyph_metrics[ord(c)].width
                       + source.glyph_metrics[ord(c)].right_spacing for c in text)
    native = Image.new("L", (source_width + 8, 19), 255)
    x = 4
    for character in text:
        metric = source.glyph_metrics[ord(character)]
        x += metric.left_spacing
        ink = Image.fromarray(np.where(source_ink(source, ord(character)), 0, 255).astype(np.uint8))
        native.paste(0, (x, 0, x + ink.width, ink.height), Image.eval(ink, lambda v: 255-v))
        x += metric.width + metric.right_spacing
    comparison = Image.new("RGB", (max(1150, native.width * 4 + 90), 660), "#f5efdf")
    draw = ImageDraw.Draw(comparison)
    y = 25
    for scale in (1, 2, 4):
        draw.text((40, y), f"Original bitmap / {19 * scale}px", font=label, fill="#544c40")
        y += 30
        raster = native.resize((native.width * scale, 19 * scale), Image.Resampling.NEAREST)
        comparison.paste("#201b16", (40, y, 40 + raster.width, y + raster.height), Image.eval(raster, lambda v: 255-v))
        y += 19 * scale + 12
        draw.text((40, y), f"Reconstructed TTF / {19 * scale}px", font=label, fill="#544c40")
        y += 30
        font = ImageFont.truetype(str(font_path), 19 * scale)
        draw.text((40 + 4 * scale, y + BASELINE * scale), text, font=font, fill="#201b16", anchor="ls")
        y += 19 * scale + 28
    comparison.crop((0, 0, comparison.width, y)).save(ROOT / "output/original_comparison.png")


def main():
    ROOT.joinpath("output").mkdir(exist_ok=True)
    data = json.loads((ROOT / "references/metrics.json").read_text())
    if data["source_sha256"] != hashlib.sha256(SOURCE.read_bytes()).hexdigest():
        raise ValueError("Source font changed; regenerate and review the reference sheets")
    metrics = data["metrics"]
    font = parse_font(SOURCE.read_bytes())
    bitmaps = extract_sheet(ROOT / "generated/core_v1.png", data["chars"], 8, 8)
    for name in ("punctuation", "special"):
        layout = json.loads((ROOT / f"references/{name}.json").read_text())
        for character, ink in extract_sheet(ROOT / f"generated/{name}_v1.png", layout["chars"],
                                            layout["cols"], layout["rows"]).items():
            bitmaps.setdefault(character, ink)
    bitmaps.update(extract_sheet(ROOT / "generated/thorn_v1.png", "Þþ", 2, 1))
    raw = {character: trace(ink) for character, ink in bitmaps.items()}
    outlines = {}
    provenance = {}
    for character, metric in metrics.items():
        if ord(character) == 127:
            continue
        if character in " \xa0":
            outlines[character] = RecordingPen()
            provenance[character] = "spacing only"
        elif character in raw:
            outlines[character] = fit(raw[character], native_box(metric))
            provenance[character] = "AI sheet; native ink box and advance"

    # Straight ASCII quotes are distinct from the generated curly quotation marks.
    for character, count in (("'", 1), ('"', 2)):
        left, bottom, right, top = native_box(metrics[character])
        outline = RecordingPen()
        width = (right - left) / (1 if count == 1 else 3)
        for index in range(count):
            rectangle(left + index * width * 2, bottom, left + index * width * 2 + width, top).replay(outline)
        outlines[character] = outline
        provenance[character] = "straight quote geometry; native ink box and advance"

    # Accented letters reuse the exact same AI base outlines, not separately invented letters.
    labels, _ = ndimage.label(bitmaps["i"])
    areas = np.bincount(labels.ravel())
    areas[0] = 0
    dotless = labels == areas.argmax()
    dotless_outline = trace(dotless)
    dotless_box = list(native_box(metrics["i"]))
    dotless_box[3] = 8 * PIXEL
    dotless_outline = fit(dotless_outline, dotless_box)
    for character, metric in metrics.items():
        if character in outlines or ord(character) == 127:
            continue
        parts = unicodedata.normalize("NFD", character)
        if len(parts) != 2 or parts[1] not in ACCENTS or parts[0] not in outlines:
            raise ValueError(f"No explicit construction for {character!r}")
        base, mark = parts
        outline = RecordingPen()
        dx = (metric["left"] - metrics[base]["left"]) * PIXEL
        transformed(dotless_outline if base == "i" else outlines[base], (1, 0, 0, 1, dx, 0)).replay(outline)
        # These CP1252 slots contain only missing-character boxes in the legacy source.
        # Use the matching S/s caron as the explicit reconstruction guide for Z/z.
        accent_source = {"Ž": "Š", "ž": "š"}.get(character, character)
        accent_metric = metrics[accent_source]
        accent_ink = source_ink(font, accent_metric["byte"]).copy()
        if mark == "\u0327":
            accent_ink[:BASELINE, :] = False
        else:
            cutoff = 6 if base == "i" else metrics[base]["ink_bbox"][1]
            accent_ink[cutoff:, :] = False
        ys, xs = np.where(accent_ink)
        if not len(xs):
            raise ValueError(f"No source accent position for {character!r}")
        box = ((metric["left"] + min(xs)) * PIXEL, (BASELINE - max(ys) - 1) * PIXEL,
               (metric["left"] + max(xs) + 1) * PIXEL, (BASELINE - min(ys)) * PIXEL)
        if accent_source != character:
            base_box = bounds(outline)
            center = (base_box[0] + base_box[2]) / 2 + PIXEL
            half_width = (box[2] - box[0]) / 2
            box = (center - half_width, box[1], center + half_width, box[3])
        fit(raw[ACCENTS[mark]], box).replay(outline)
        outlines[character] = outline
        provenance[character] = f"AI {base} plus AI {ACCENTS[mark]}; {accent_source} accent guide; native advance"

    outlines = dict(sorted(outlines.items(), key=lambda item: ord(item[0])))
    destination = ROOT / "output/OpenYAMMDialogue-Italic.ttf"
    make_font(outlines, metrics, destination)
    checks = validate(destination, metrics)
    checks["source_sha256"] = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    checks["provenance"] = provenance
    (ROOT / "output/validation.json").write_text(json.dumps(checks, indent=2, ensure_ascii=False) + "\n")
    proof(destination, list(outlines))
    ASSET.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(destination, ASSET)
    ASSET.chmod(0o644)
    print(json.dumps({key: value for key, value in checks.items() if key != "provenance"}, indent=2))


if __name__ == "__main__":
    main()
