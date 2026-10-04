#!/usr/bin/env python3
"""Build an Arrus outline specimen from native pixels and reviewed stroke masters.

This does not replace the active game font. The remaining characters are conservative
source traces for runtime completeness, not a fully reviewed replacement alphabet.
"""
from __future__ import annotations

import hashlib
import html
import json
from pathlib import Path

import numpy as np
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

import build_dialogue_ttf as arrus
from font_pipeline import conservative_trace, stroke_outline
from build_native_font import compose_accent

ROOT = Path(__file__).resolve().parent / "arrus_faithful"
OUTPUT = ROOT / "output"
SOURCE_HASH = "71b51bb55e2367aa4be9305fe8910db57d8f0e9e04f314b5a4b90bf90f52e982"
SENTENCES = [
    "Hello?",
    "Ah, someone else who seeks a way off this accursed island!",
    "My name is Simon Templar!",
    "I found my way into this temple as I was told it was the way out!",
    "Let's travel together!",
]


def rename_font(path):
    font = TTFont(path, recalcTimestamp=False)
    values = {
        1: "OpenYAMM Arrus Faithful Preview", 2: "Italic",
        3: "OpenYAMM-ArrusFaithfulPreview-0.200", 4: "OpenYAMM Arrus Faithful Preview Italic",
        5: "Version 0.200", 6: "OpenYAMM-ArrusFaithfulPreview-Italic",
        10: "Arrus restoration: refined native stroke paths and shared accented bases."
            " Original advances retained; corrected TrueType side bearings. See per-glyph provenance.",
    }
    for name_id, value in values.items():
        font["name"].removeNames(nameID=name_id)
        font["name"].setName(value, name_id, 3, 1, 0x409)
    font.save(path)


def render_line(source, metrics, text, height, font_path=None):
    """Render fresh font specimens, never modifying the user's reference screenshot."""
    scale = height / 19
    width = sum(metrics[c]["advance"] for c in text)
    mask = Image.new("L", (round((width + 10) * scale), round(29 * scale)), 0)
    if font_path:
        draw = ImageDraw.Draw(mask)
        draw.text((5 * scale, 19 * scale), text, font=ImageFont.truetype(str(font_path), height),
                  fill=255, anchor="ls")
    else:
        native = Image.new("L", (width + 10, 29), 0)
        x = 5
        for character in text:
            metric = metrics[character]
            x += metric["left"]
            ink = Image.fromarray(arrus.source_ink(source, metric["byte"]).astype("uint8") * 255)
            native.paste(255, (x, 5), ink)
            x += metric["width"] + metric["right"]
        mask = native.resize(mask.size, Image.Resampling.NEAREST)
    return mask


def make_proofs(source, metrics, destination, refined):
    label = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 18)
    panels = [("Original bitmap", None), ("Faithful outline specimen", destination),
              ("Existing AI reconstruction", arrus.ASSET)]
    sheet = Image.new("RGB", (1350, 860), "#262524")
    draw = ImageDraw.Draw(sheet)
    y = 18
    for title, path in panels:
        draw.text((20, y), title + " / 38 px", font=label, fill="#e1c99d")
        y += 27
        for sentence in SENTENCES:
            mask = render_line(source, metrics, sentence, 38, path)
            sheet.paste("#ededed", (14, y - 10), mask)
            y += 36
        y += 25
    sheet.crop((0, 0, 1350, y)).save(OUTPUT / "dialogue_comparison.png")

    sizes = (19, 24, 38, 76)
    sheet = Image.new("RGB", (1400, 1400), "#f4efdf")
    draw = ImageDraw.Draw(sheet)
    y = 20
    for size in sizes:
        for title, path in panels:
            draw.text((20, y), f"{title} / {size} px", font=label, fill="#504c43")
            y += 24
            mask = render_line(source, metrics, "Welcome to New Sorpigal.", size, path)
            sheet.paste("#201e19", (20, y), mask)
            y += mask.height + 12
        y += 16
    sheet.crop((0, 0, 1400, y)).save(OUTPUT / "size_comparison.png")

    columns, cell_width, cell_height = 6, 210, 170
    sheet = Image.new("RGB", (columns * cell_width, ((len(refined) + columns - 1) // columns) * cell_height), "#eee9db")
    draw = ImageDraw.Draw(sheet)
    for index, character in enumerate(refined):
        x, y = (index % columns) * cell_width, (index // columns) * cell_height
        draw.text((x + 8, y + 8), character + " / source, refined", font=label, fill="#504c43")
        for offset, path in [(6, None), (100, destination)]:
            mask = render_line(source, metrics, character, 76, path)
            sheet.paste("#201e19", (x + offset, y + 30), mask)
    sheet.save(OUTPUT / "refined_glyph_proof.png")

    specimen = html.escape("\n".join(SENTENCES))
    (OUTPUT / "preview.html").write_text(f'''<!doctype html>
<html lang="en"><meta charset="utf-8"><title>Arrus faithful specimen</title>
<style>
@font-face{{font-family:Faithful;src:url(OpenYAMM-ArrusFaithfulPreview-Italic.ttf)}}
@font-face{{font-family:Previous;src:url(../../dialogue_ai/output/OpenYAMMDialogue-Italic.ttf)}}
body{{margin:32px;background:#242323;color:#eee;font:16px system-ui}}h1{{font-size:24px}}
textarea{{width:95%;height:125px;background:#333;color:#eee;padding:12px;font:16px system-ui}}
.sample{{white-space:pre-wrap;line-height:1.15;padding:20px 0;font-size:38px;overflow-wrap:anywhere}}
.faithful{{font-family:Faithful}}.previous{{font-family:Previous}}
.light{{background:#f4efdf;color:#201e19;padding:20px}}label{{display:inline-block;margin:20px 12px 0 0}}
</style><h1>Arrus: faithful outline specimen</h1>
<p>Editable comparison. Native sizes and actual game captures are linked in the README.</p>
<textarea id="text">{specimen}</textarea>
<label>Size <input id="size" type="range" min="19" max="76" value="38"> <span id="value">38</span> px</label>
<label><input id="light" type="checkbox">Paper background</label>
<main><h2>Faithful specimen</h2><div class="sample faithful"></div>
<h2>Existing AI reconstruction</h2><div class="sample previous"></div></main>
<script>
const text=document.querySelector('#text'),size=document.querySelector('#size');
function update(){{document.querySelectorAll('.sample').forEach(e=>{{e.textContent=text.value;e.style.fontSize=size.value+'px'}});
document.querySelector('#value').textContent=size.value;}}
text.oninput=size.oninput=update;document.querySelector('#light').onchange=e=>document.querySelector('main').classList.toggle('light',e.target.checked);
update();document.fonts.ready.then(()=>document.title+=' — fonts loaded');
</script></html>''')


def main():
    if hashlib.sha256(arrus.SOURCE.read_bytes()).hexdigest() != SOURCE_HASH:
        raise ValueError("Arrus source changed; review source and stroke masters before rebuilding")
    data = json.loads((arrus.ROOT / "references/metrics.json").read_text())
    if data["source_sha256"] != SOURCE_HASH:
        raise ValueError("Arrus reference metrics do not match the frozen source")
    metrics = data["metrics"]
    source = arrus.parse_font(arrus.SOURCE.read_bytes())
    masters = json.loads((ROOT / "strokes.json").read_text())
    masks = {c: arrus.source_ink(source, metric["byte"]) for c, metric in metrics.items()}
    cores = {c: stroke_outline(commands, masters["stroke_width"], metrics[c]["left"])
             for c, commands in masters["glyphs"].items()}
    accent_cores = dict(cores)
    for c, commands in masters.get("accent_bases", {}).items():
        accent_cores[c] = stroke_outline(commands, masters["stroke_width"], metrics[c]["left"])
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (ROOT / "outlines").mkdir(exist_ok=True)
    outlines, provenance = {}, {}
    for character, metric in metrics.items():
        if ord(character) == 127:
            continue
        if character in masters["glyphs"]:
            outline = cores[character]
            provenance[character] = "manually placed native-coordinate stroke/Bezier master"
        elif character in (" ", "\xa0"):
            outline = RecordingPen()
            provenance[character] = "native spacing; no ink"
        else:
            composition = compose_accent(character, metrics, masks, accent_cores, arrus.ARRUS_PROFILE)
            if composition:
                outline, recipe = composition
                provenance[character] = {"method": "shared master + native accent", **recipe}
            else:
                outline = conservative_trace(masks[character], metric["left"])
                provenance[character] = "native foreground trace; not individually refined"
        outlines[character] = outline
        svg_pen = SVGPathPen(None)
        outline.replay(svg_pen)
        bounds = arrus.bounds(outline)
        commands = svg_pen.getCommands()
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="-256 -1216 1536 1792">'
               f'<path transform="scale(1,-1)" d="{commands}"/></svg>\n')
        (ROOT / "outlines" / f"U{ord(character):04X}.svg").write_text(svg)
        if bounds and (bounds[0] < (metric["left"] - 4) * 64 or bounds[2] > (metric["left"] + metric["width"] + 4) * 64
                       or bounds[1] < (14 - 19 - 4) * 64 or bounds[3] > (14 + 4) * 64):
            raise ValueError(f"Outline exceeds runtime padding: {character!r}")
    destination = OUTPUT / "OpenYAMM-ArrusFaithfulPreview-Italic.ttf"
    arrus.make_font(outlines, metrics, destination)
    rename_font(destination)
    checks = arrus.validate(destination, metrics)
    checks.update({"source_sha256": SOURCE_HASH, "refined_glyph_count": len(masters["glyphs"]),
                   "refined_glyphs": "".join(masters["glyphs"]),
                   "shared_accent_count": sum(isinstance(p, dict) for p in provenance.values()),
                   "status": "refined alphabet, digits and accents; other native traces retained",
                   "provenance": provenance})
    (OUTPUT / "validation.json").write_text(json.dumps(checks, indent=2, ensure_ascii=False) + "\n")
    make_proofs(source, metrics, destination, list(masters["glyphs"]))
    print(json.dumps({k: v for k, v in checks.items() if k != "provenance"}, indent=2))


if __name__ == "__main__":
    main()
