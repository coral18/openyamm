"""Regression checks for native-coordinate TrueType construction."""
import tempfile
import unittest
from pathlib import Path

from fontTools.pens.recordingPen import RecordingPen
from fontTools.ttLib import TTFont

from font_pipeline import bounds, make_font, rectangle


class NativeOriginTests(unittest.TestCase):
    def test_quadratic_bearings_preserve_authored_origins(self):
        curve = RecordingPen()
        curve.moveTo((0, 0))
        curve.curveTo((-128, 0), (-128, 640), (0, 640))
        curve.lineTo((256, 640))
        curve.curveTo((384, 640), (384, 0), (256, 0))
        curve.closePath()
        outlines = {"o": curve, "j": rectangle(-128, -192, 64, 640), " ": RecordingPen()}
        metrics = {"o": {"advance": 9}, "j": {"advance": 5}, " ": {"advance": 6}}
        profile = {
            "height": 17, "baseline": 13, "x_height": 7, "cap_height": 9,
            "names": {"familyName": "Origin test", "styleName": "Regular"},
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "origin.ttf"
            make_font(outlines, metrics, path, profile)
            with TTFont(path) as font:
                glyph_set = font.getGlyphSet()
                cmap = font.getBestCmap()
                # Exercise the actual cause: cubic extrema are not glyf bounds.
                self.assertNotEqual(font["glyf"][cmap[ord("o")]].xMin, round(bounds(curve)[0]))
                for character, outline in outlines.items():
                    with self.subTest(character=character):
                        name = cmap[ord(character)]
                        glyph = font["glyf"][name]
                        advance, bearing = font["hmtx"][name]
                        self.assertEqual(advance, metrics[character]["advance"] * 64)
                        self.assertEqual(bearing, getattr(glyph, "xMin", 0))
                        raw, positioned = RecordingPen(), RecordingPen()
                        glyph.draw(raw, font["glyf"])
                        glyph_set[name].draw(positioned)
                        self.assertEqual(raw.value, positioned.value)
                self.assertEqual(font["hmtx"][cmap[ord("j")]][1], -128)
                self.assertEqual(font["hmtx"][cmap[ord(" ")]][1], 0)


if __name__ == "__main__":
    unittest.main()
