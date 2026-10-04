#!/usr/bin/env python3
"""Extract foreground-only Arrus references and authoritative legacy metrics."""

import hashlib
import json
import unicodedata
from pathlib import Path

from PIL import Image

from unpack_xbrz_font import parse_font


ROOT = Path(__file__).resolve().parent / "dialogue_ai/references"
SOURCE = Path(__file__).resolve().parents[2] / "assets_dev/engine/fonts/english_text/Arrus.fnt"


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    source_bytes = SOURCE.read_bytes()
    font = parse_font(source_bytes)
    core = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!?"
    metrics = {}
    for code in range(32, 256):
        try:
            character = bytes([code]).decode("cp1252")
        except UnicodeDecodeError:
            continue
        metric = font.glyph_metrics[code]
        ink = [(x, y) for y in range(font.font_height) for x in range(metric.width)
               if font.pixels[font.glyph_offsets[code] + y * metric.width + x] > 1]
        box = [min(x for x, y in ink), min(y for x, y in ink),
               max(x for x, y in ink) + 1, max(y for x, y in ink) + 1] if ink else None
        metrics[character] = {"byte": code, "left": metric.left_spacing, "width": metric.width,
                              "right": metric.right_spacing,
                              "advance": metric.left_spacing + metric.width + metric.right_spacing,
                              "ink_bbox": box}
    metadata = {"height": font.font_height, "chars": core, "metrics": metrics,
                "source_sha256": hashlib.sha256(source_bytes).hexdigest()}
    (ROOT / "metrics.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n")
    punctuation = "".join(chr(c) for c in range(33, 127) if chr(c) not in core) + "´¨¯¸ˇ˚"
    special = "".join(c for c, m in metrics.items() if ord(c) > 127 and c not in punctuation and m["ink_bbox"]
                      and len(unicodedata.normalize("NFD", c)) == 1) + "´¨¯¸"
    for name, characters, columns in (("core", core, 8), ("punctuation", punctuation, 6), ("special", special, 8)):
        rows = (len(characters) + columns - 1) // columns
        sheet = Image.new("L", (columns * 384, rows * 384), 255)
        for index, character in enumerate(characters):
            donor = {"ˇ": "Š", "˚": "Å"}.get(character, character)
            code = donor.encode("cp1252")[0]
            metric = font.glyph_metrics[code]
            start = font.glyph_offsets[code]
            glyph = Image.new("L", (metric.width, font.font_height), 255)
            glyph.putdata([0 if b > 1 else 255 for b in font.pixels[start:start + metric.width * font.font_height]])
            if character in "ˇ˚":
                glyph = glyph.crop((0, 0, glyph.width, 3))
                glyph = glyph.crop(Image.eval(glyph, lambda v: 255-v).getbbox())
            glyph = glyph.resize((glyph.width * 12, glyph.height * 12), Image.Resampling.NEAREST)
            sheet.paste(glyph, ((index % columns) * 384 + (384-glyph.width)//2, (index // columns) * 384 + 70))
        sheet.save(ROOT / f"{name}.png")
        if name != "core":
            layout = {"chars": characters, "cols": columns, "rows": rows}
            (ROOT / f"{name}.json").write_text(json.dumps(layout, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
