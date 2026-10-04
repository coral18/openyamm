"""Native transparency and declared source archives for the equipment reviewer."""
import functools
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]


@functools.lru_cache(maxsize=None)
def archive_records(manifest):
    return json.loads(manifest.read_text())['files'] if manifest.exists() else {}


def native_source(path, archive_manifest):
    path = Path(path)
    key = str(path.relative_to(ROOT)) if path.is_absolute() else str(path)
    record = archive_records(archive_manifest).get(key)
    # A declared migration is authoritative, not an existence-based fallback.
    return ROOT / (record['archive'] if record else key)


PROFILES = {
    'hud': dict(magenta=True, teal=True),
    'hud_black': dict(magenta=True, teal=True, black=True),
    'item': dict(magenta=True, teal=True, palette_zero=True),
    'menu_bmp': dict(magenta=True, teal=True, pink=True),
    'menu_pcx': dict(magenta=True, teal=True, pink=True, blue=True),
    'menu_png': dict(magenta=True, teal=True),
}


def rendered(image, profile):
    pixels = np.array(image.convert('RGBA'))
    r, g, b = (pixels[:, :, i] for i in range(3))
    options = PROFILES[profile]
    mask = np.zeros(pixels.shape[:2], dtype=bool)
    if options.get('magenta'):
        mask |= (r >= 248) & (g <= 8) & (b >= 248)
    if options.get('teal'):
        mask |= (r <= 8) & (g >= 248) & (b >= 248)
    if options.get('black'):
        mask |= (r <= 8) & (g <= 8) & (b <= 8)
    if options.get('pink'):
        mask |= (r >= 248) & (g >= 48) & (g <= 64) & (b >= 248)
    if options.get('blue'):
        mask |= (r <= 8) & (g <= 8) & (b >= 248)
    if options.get('palette_zero') and image.mode == 'P':
        mask |= np.array(image) == 0
    pixels[mask, 3] = 0
    # RGB under alpha zero does not affect rendering, but the source/palette hashes retain it.
    pixels[pixels[:, :, 3] == 0, :3] = 0
    return Image.fromarray(pixels)
