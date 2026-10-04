#!/usr/bin/env python3

from __future__ import annotations

import argparse
import shutil
import struct
import subprocess
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw


FONT_HEADER_SIZE = 32
MM7_ATLAS_SIZE = 4096
MMX_ATLAS_SIZE = 1280
GLYPH_COUNT = 256
GRID_COLUMNS = 16


@dataclass(frozen=True)
class GlyphMetrics:
    left_spacing: int
    width: int
    right_spacing: int


@dataclass(frozen=True)
class ParsedFont:
    format_name: str
    first_char: int
    last_char: int
    font_height: int
    glyph_metrics: list[GlyphMetrics]
    glyph_offsets: list[int]
    pixels: bytes


def read_u32_le(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def read_i32_le(data: bytes, offset: int) -> int:
    return struct.unpack_from("<i", data, offset)[0]


def validate_font(font: ParsedFont) -> bool:
    if font.first_char < 0 or font.first_char > 255:
        return False
    if font.last_char < 0 or font.last_char > 255:
        return False
    if font.first_char > font.last_char or font.font_height <= 0:
        return False

    for glyph_index in range(font.first_char, font.last_char + 1):
        metrics = font.glyph_metrics[glyph_index]
        if metrics.width < 0 or metrics.width > 1024:
            return False
        if metrics.left_spacing < -512 or metrics.left_spacing > 512:
            return False
        if metrics.right_spacing < -512 or metrics.right_spacing > 512:
            return False

        glyph_size = font.font_height * metrics.width
        glyph_end = font.glyph_offsets[glyph_index] + glyph_size
        if glyph_end > len(font.pixels):
            return False

    return True


def parse_font(data: bytes) -> ParsedFont:
    if len(data) < FONT_HEADER_SIZE + MMX_ATLAS_SIZE:
        raise ValueError("file is too small for a supported MM bitmap font")

    if data[2] != 8 or data[3] != 0 or data[4] != 0 or data[6] != 0 or data[7] != 0:
        raise ValueError("font header does not match the expected MM bitmap font signature")

    first_char = data[0]
    last_char = data[1]
    font_height = data[5]

    if len(data) >= FONT_HEADER_SIZE + MM7_ATLAS_SIZE:
        glyph_metrics: list[GlyphMetrics] = []
        for glyph_index in range(GLYPH_COUNT):
            metric_offset = FONT_HEADER_SIZE + glyph_index * 12
            glyph_metrics.append(
                GlyphMetrics(
                    read_i32_le(data, metric_offset),
                    read_i32_le(data, metric_offset + 4),
                    read_i32_le(data, metric_offset + 8),
                )
            )

        glyph_offsets = [
            read_u32_le(data, FONT_HEADER_SIZE + GLYPH_COUNT * 12 + glyph_index * 4)
            for glyph_index in range(GLYPH_COUNT)
        ]
        font = ParsedFont(
            "mm7",
            first_char,
            last_char,
            font_height,
            glyph_metrics,
            glyph_offsets,
            data[FONT_HEADER_SIZE + MM7_ATLAS_SIZE:],
        )
        if validate_font(font):
            return font

    glyph_metrics = [
        GlyphMetrics(0, data[FONT_HEADER_SIZE + glyph_index], 0)
        for glyph_index in range(GLYPH_COUNT)
    ]
    glyph_offsets = [
        read_u32_le(data, FONT_HEADER_SIZE + GLYPH_COUNT + glyph_index * 4)
        for glyph_index in range(GLYPH_COUNT)
    ]
    font = ParsedFont(
        "mmx",
        first_char,
        last_char,
        font_height,
        glyph_metrics,
        glyph_offsets,
        data[FONT_HEADER_SIZE + MMX_ATLAS_SIZE:],
    )
    if not validate_font(font):
        raise ValueError("font metrics and glyph pixels did not validate")

    return font


def glyph_image(font: ParsedFont, glyph_index: int) -> Image.Image:
    metrics = font.glyph_metrics[glyph_index]
    image = Image.new("RGBA", (max(1, metrics.width), font.font_height), (0, 0, 0, 0))

    if glyph_index < font.first_char or glyph_index > font.last_char or metrics.width <= 0:
        return image

    glyph_offset = font.glyph_offsets[glyph_index]
    pixels = image.load()

    for y in range(font.font_height):
        for x in range(metrics.width):
            pixel_value = font.pixels[glyph_offset + y * metrics.width + x]
            if pixel_value == 0:
                continue
            if pixel_value == 1:
                pixels[x, y] = (0, 0, 0, 255)
            else:
                pixels[x, y] = (255, 255, 255, 255)

    return image


def compose_sheet(
    glyph_images: dict[int, Image.Image],
    font_height: int,
    atlas_cell_width: int,
    scale: int,
) -> Image.Image:
    cell_width = max(1, atlas_cell_width * scale)
    cell_height = max(1, font_height * scale)
    sheet = Image.new("RGBA", (cell_width * GRID_COLUMNS, cell_height * GRID_COLUMNS), (0, 0, 0, 0))

    for glyph_index, image in glyph_images.items():
        cell_x = (glyph_index % GRID_COLUMNS) * cell_width
        cell_y = (glyph_index // GRID_COLUMNS) * cell_height
        sheet.alpha_composite(image, (cell_x, cell_y))

    return sheet


def add_padding(image: Image.Image, padding: int) -> Image.Image:
    padded = Image.new("RGBA", (image.width + padding * 2, image.height + padding * 2), (0, 0, 0, 0))
    padded.alpha_composite(image, (padding, padding))
    return padded


def upscale_glyph_with_xbrz(
    xbrz_path: str,
    glyph_path: Path,
    output_path: Path,
    scale: int,
    padding: int,
) -> None:
    padded_path = output_path.with_name(output_path.stem + "_padded_input.png")
    padded_output_path = output_path.with_name(output_path.stem + "_padded_xbrz.png")

    image = Image.open(glyph_path).convert("RGBA")
    add_padding(image, padding).save(padded_path)

    subprocess.run(
        [xbrz_path, str(padded_path), str(scale), str(padded_output_path)],
        check=True,
        stdout=subprocess.DEVNULL,
    )

    upscaled = Image.open(padded_output_path).convert("RGBA")
    crop = (
        padding * scale,
        padding * scale,
        padding * scale + image.width * scale,
        padding * scale + image.height * scale,
    )
    upscaled.crop(crop).save(output_path)

    padded_path.unlink()
    padded_output_path.unlink()


def write_compare_sheet(original_x4: Image.Image, xbrz_x4: Image.Image, output_path: Path) -> None:
    label_height = 24
    gap = 16
    width = original_x4.width + gap + xbrz_x4.width
    height = label_height + max(original_x4.height, xbrz_x4.height)
    compare = Image.new("RGBA", (width, height), (48, 48, 48, 255))
    draw = ImageDraw.Draw(compare)
    draw.text((0, 4), "nearest x4", fill=(255, 255, 255, 255))
    draw.text((original_x4.width + gap, 4), "xbrz x4", fill=(255, 255, 255, 255))
    compare.alpha_composite(original_x4, (0, label_height))
    compare.alpha_composite(xbrz_x4, (original_x4.width + gap, label_height))
    compare.save(output_path)


def main() -> None:
    parser = argparse.ArgumentParser(description="Unpack an MM .fnt and generate xBRZ comparison sheets.")
    parser.add_argument("font", type=Path, help="Source .fnt file")
    parser.add_argument("output_dir", type=Path, help="Directory for unpacked glyphs and sheets")
    parser.add_argument("--scale", type=int, default=4, choices=(2, 3, 4, 5, 6))
    parser.add_argument("--xbrz", default="xbrz", help="xbrz executable path")
    parser.add_argument("--padding", type=int, default=2, help="Transparent source-pixel padding around each glyph")
    args = parser.parse_args()

    font_path = args.font
    output_dir = args.output_dir
    original_glyph_dir = output_dir / "glyphs" / "original"
    xbrz_glyph_dir = output_dir / "glyphs" / f"x{args.scale}_xbrz"
    original_glyph_dir.mkdir(parents=True, exist_ok=True)
    xbrz_glyph_dir.mkdir(parents=True, exist_ok=True)

    font = parse_font(font_path.read_bytes())
    shutil.copy2(font_path, output_dir / font_path.name)

    atlas_cell_width = max(metrics.width for metrics in font.glyph_metrics)
    original_glyphs: dict[int, Image.Image] = {}
    xbrz_glyphs: dict[int, Image.Image] = {}

    for glyph_index in range(font.first_char, font.last_char + 1):
        image = glyph_image(font, glyph_index)
        if image.width <= 0 or image.height <= 0:
            continue

        original_glyphs[glyph_index] = image
        glyph_name = f"{glyph_index:03d}_0x{glyph_index:02x}.png"
        original_path = original_glyph_dir / glyph_name
        xbrz_path = xbrz_glyph_dir / glyph_name
        image.save(original_path)
        upscale_glyph_with_xbrz(args.xbrz, original_path, xbrz_path, args.scale, args.padding)
        xbrz_glyphs[glyph_index] = Image.open(xbrz_path).convert("RGBA")

    source_sheet = compose_sheet(original_glyphs, font.font_height, atlas_cell_width, 1)
    source_sheet.save(output_dir / f"{font_path.stem}_sheet_original.png")

    nearest_glyphs = {
        glyph_index: image.resize((image.width * args.scale, image.height * args.scale), Image.Resampling.NEAREST)
        for glyph_index, image in original_glyphs.items()
    }
    nearest_sheet = compose_sheet(nearest_glyphs, font.font_height, atlas_cell_width, args.scale)
    xbrz_sheet = compose_sheet(xbrz_glyphs, font.font_height, atlas_cell_width, args.scale)
    nearest_sheet.save(output_dir / f"{font_path.stem}_sheet_x{args.scale}_nearest.png")
    xbrz_sheet.save(output_dir / f"{font_path.stem}_sheet_x{args.scale}_xbrz.png")
    write_compare_sheet(nearest_sheet, xbrz_sheet, output_dir / f"{font_path.stem}_compare_x{args.scale}.png")

    metadata = [
        f"source={font_path}",
        f"format={font.format_name}",
        f"first_char={font.first_char}",
        f"last_char={font.last_char}",
        f"font_height={font.font_height}",
        f"atlas_cell_width={atlas_cell_width}",
        f"scale={args.scale}",
        f"xbrz={shutil.which(args.xbrz) or args.xbrz}",
    ]
    (output_dir / "metadata.txt").write_text("\n".join(metadata) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
