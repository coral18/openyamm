"""Shared native-font outline assembly and validation; all layout/style values are face-specific."""
from __future__ import annotations
import hashlib
import numpy as np
import pathops
import potrace
from scipy import ndimage
from fontTools.svgLib.path import parse_path
from PIL import ImageFont
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont, newTable

PIXEL = 64

def glyph_name(character):
    return "space" if character == " " else f"uni{ord(character):04X}"


def bounds(outline):
    pen = BoundsPen(None)
    outline.replay(pen)
    return pen.bounds


def transformed(outline, transform):
    pen = RecordingPen()
    outline.replay(TransformPen(pen, transform))
    return pen


def rectangle(left, bottom, right, top):
    pen = RecordingPen()
    pen.moveTo((left, bottom))
    pen.lineTo((right, bottom))
    pen.lineTo((right, top))
    pen.lineTo((left, top))
    pen.closePath()
    return pen


def source_ink(font, code, black_ink=False):
    metric = font.glyph_metrics[code]
    offset = font.glyph_offsets[code]
    pixels = np.frombuffer(font.pixels[offset:offset + metric.width * font.font_height], dtype=np.uint8)
    pixels = pixels.reshape(font.font_height, metric.width)
    return pixels == 1 if black_ink else pixels > 1


def make_font(outlines, metrics, destination, profile):
    units_per_em = profile["height"] * PIXEL
    ascent = profile["baseline"] * PIXEL
    descent = (profile["height"] - profile["baseline"]) * PIXEL
    order = [".notdef"] + [glyph_name(c) for c in outlines]
    glyphs, horizontal = {}, {}
    for character, outline in outlines.items():
        pen = TTGlyphPen(None)
        outline.replay(Cu2QuPen(pen, units_per_em / 1000, reverse_direction=True))
        name = glyph_name(character)
        glyphs[name] = pen.glyph()
        horizontal[name] = (metrics[character]["advance"] * PIXEL, 0)
    missing = RecordingPen()
    rectangle(64, 0, 576, 704).replay(missing)
    # Opposite inner winding leaves an open missing-character box.
    hole = rectangle(128, 64, 512, 640)
    from fontTools.pens.reverseContourPen import ReverseContourPen
    hole.replay(ReverseContourPen(missing))
    pen = TTGlyphPen(None)
    missing.replay(pen)
    glyphs[".notdef"] = pen.glyph()
    horizontal[".notdef"] = (640, 64)
    builder = FontBuilder(units_per_em, isTTF=True)
    builder.setupGlyphOrder(order)
    builder.setupCharacterMap({ord(c): glyph_name(c) for c in outlines})
    builder.setupGlyf(glyphs)
    # TrueType side bearings refer to the stored quadratic control-point bounds,
    # not the tight extrema of the original cubic curves. A difference translates
    # the outline at rasterization, moving curved letters within their advance.
    for name, glyph in glyphs.items():
        horizontal[name] = (horizontal[name][0], getattr(glyph, "xMin", 0))
    builder.setupHorizontalMetrics(horizontal)
    builder.setupHorizontalHeader(ascent=ascent, descent=-descent, lineGap=0, caretSlopeRise=1000,
                                  caretSlopeRun=profile.get("caret_slope_run", 0))
    builder.setupNameTable(profile["names"])
    for glyph in glyphs.values():
        glyph.recalcBounds(builder.font["glyf"])
    win_ascent = max(ascent, max(getattr(glyph, "yMax", 0) for glyph in glyphs.values()))
    win_descent = max(descent, -min(getattr(glyph, "yMin", 0) for glyph in glyphs.values()))
    builder.setupOS2(version=4, sTypoAscender=ascent, sTypoDescender=-descent, sTypoLineGap=0,
                     usWinAscent=win_ascent, usWinDescent=win_descent, sxHeight=profile["x_height"] * PIXEL,
                     sCapHeight=profile["cap_height"] * PIXEL,
                     usWeightClass=400, usWidthClass=5, fsSelection=0x81 if profile.get("italic", False) else 0xC0, fsType=0)
    builder.setupPost(italicAngle=profile.get("italic_angle", 0), underlinePosition=-128, underlineThickness=48)
    builder.setupHead(unitsPerEm=units_per_em, macStyle=2 if profile.get("italic", False) else 0, created=3861172800, modified=3861172800)
    builder.setupMaxp()
    gasp = newTable("gasp")
    gasp.gaspRange = {65535: 0x0A}
    builder.font["gasp"] = gasp
    builder.font.recalcTimestamp = False
    builder.save(destination)


def validate(font_path, metrics, profile):
    units_per_em = profile["height"] * PIXEL
    font = TTFont(font_path, checkChecksums=2)
    if font["head"].unitsPerEm != units_per_em:
        raise ValueError("Font scale no longer matches native advances")
    cmap = font.getBestCmap()
    checks = {"mapped_characters": len(cmap), "glyphs": len(font.getGlyphOrder()),
              "units_per_em": font["head"].unitsPerEm, "missing": [], "advance_mismatches": [],
              "empty_visible_glyphs": [], "clipped_glyphs": [], "bearing_mismatches": [],
              "rasterization_checks": 0}
    for character, metric in metrics.items():
        code = ord(character)
        if code == 127:
            continue
        if code not in cmap:
            checks["missing"].append(character)
            continue
        name = cmap[code]
        if font["hmtx"][name][0] != metric["advance"] * PIXEL:
            checks["advance_mismatches"].append(character)
        glyph = font["glyf"][name]
        if font["hmtx"][name][1] != getattr(glyph, "xMin", 0):
            checks["bearing_mismatches"].append(character)
        if character not in " \xa0" and glyph.numberOfContours == 0:
            checks["empty_visible_glyphs"].append(character)
        if glyph.numberOfContours and (
            glyph.yMax > font["OS/2"].usWinAscent or glyph.yMin < -font["OS/2"].usWinDescent
        ):
            checks["clipped_glyphs"].append(character)
    for size in (profile["height"], 24, profile["height"] * 2, profile["height"] * 4):
        rasterizer = ImageFont.truetype(str(font_path), size)
        for code in cmap:
            if chr(code) in " \xa0\xad":
                continue
            if not rasterizer.getmask(chr(code)).getbbox():
                raise ValueError(f"Invisible glyph U+{code:04X} at {size}px")
            checks["rasterization_checks"] += 1
    rasterizer = ImageFont.truetype(str(font_path), profile["height"])
    sentences = (
        "Welcome to New Sorpigal. What brings you here?",
        "AVATAR To Wa fi fl 0123456789",
        "ÀÁÂÃÄÅ ÇÈÉÊË ÌÍÎÏ ÑÒÓÔÕÖ Ø ÙÚÛÜ ÝŸ Žž",
    )
    checks["sentence_width_checks"] = []
    for sentence in sentences:
        expected = sum(metrics[c]["advance"] for c in sentence)
        actual = rasterizer.getlength(sentence)
        if abs(expected - actual) > 0.01:
            raise ValueError(f"Sentence advance changed: {expected} vs {actual}")
        checks["sentence_width_checks"].append({"text": sentence, "native_pixels": expected, "ttf_pixels": actual})
    for key in ("missing", "advance_mismatches", "empty_visible_glyphs", "clipped_glyphs", "bearing_mismatches"):
        if checks[key]:
            raise ValueError(f"{key}: {checks[key]}")
    checks["sha256"] = hashlib.sha256(font_path.read_bytes()).hexdigest()
    return checks



def conservative_trace(ink, left, baseline=14):
    """Round pixel steps locally without letting Potrace redesign low-resolution strokes."""
    pen = RecordingPen()
    if not ink.any():
        return pen
    scale, pad = 12, 3
    expanded = np.repeat(np.repeat(np.pad(ink, pad), scale, axis=0), scale, axis=1).astype(float)
    softened = ndimage.gaussian_filter(expanded, .4 * scale) >= .45
    curves = potrace.Bitmap(~softened).trace(turdsize=0, alphamax=1.0, opttolerance=.7)

    def point(p):
        return (left + p.x / scale - pad) * 64, (baseline - p.y / scale + pad) * 64

    for curve in curves:
        pen.moveTo(point(curve.start_point))
        for segment in curve:
            if segment.is_corner:
                pen.lineTo(point(segment.c))
                pen.lineTo(point(segment.end_point))
            else:
                pen.curveTo(point(segment.c1), point(segment.c2), point(segment.end_point))
        pen.closePath()
    return pen


def stroke_outline(commands, width, left, baseline=14, square_caps=False, nib=None):
    centerline = pathops.Path()
    parse_path(commands, centerline.getPen())
    # An elliptical nib retains the thick downstrokes and thin cross-strokes of calligraphy.
    nib_x, nib_y = nib if nib is not None else (1, 1)
    if nib_x <= 0 or nib_y <= 0:
        raise ValueError('Nib dimensions must be positive')
    if nib is not None:
        centerline = centerline.transform(1 / nib_x, 0, 0, 1 / nib_y, 0, 0)
    centerline.stroke(width, pathops.LineCap.SQUARE_CAP if square_caps else pathops.LineCap.ROUND_CAP,
                      pathops.LineJoin.ROUND_JOIN, 4)
    centerline.convertConicsToQuads(.01)
    outline = pathops.simplify(centerline)
    pen = RecordingPen()
    outline.draw(pen)
    return transformed(pen, (64 * nib_x, 0, 0, -64 * nib_y, left * 64, baseline * 64))
