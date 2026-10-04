#!/usr/bin/env python3
"""Capture a repository or /tmp HTML preview through a reusable headless-Chrome entry point."""
import argparse
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import urllib.parse

REPO = Path(__file__).resolve().parents[1]


def local_path(value):
    path = Path(value).expanduser().resolve()
    if not any(path == root or root in path.parents for root in (REPO, Path('/tmp'))):
        raise argparse.ArgumentTypeError('Preview and output paths must be inside the repository or /tmp')
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('html', type=local_path)
    parser.add_argument('output', type=local_path)
    parser.add_argument('--query', default='', help='Viewer query, e.g. action=walk&view=6&palette=803')
    parser.add_argument('--width', type=int, default=1500)
    parser.add_argument('--height', type=int, default=980)
    parser.add_argument('--wait-ms', type=int, default=2500, help='Chrome virtual-time budget; not real animation time')
    args = parser.parse_args()
    if not args.html.is_file() or args.html.suffix.lower() not in ('.html', '.htm'):
        parser.error('Input must be an existing local HTML file')
    if args.output.suffix.lower() != '.png':
        parser.error('Output must be a PNG path')
    if not 100 <= args.width <= 4096 or not 100 <= args.height <= 4096 or not 100 <= args.wait_ms <= 30000:
        parser.error('Width/height must be 100–4096; wait-ms must be 100–30000')
    chrome = shutil.which('google-chrome') or shutil.which('chromium')
    if not chrome:
        parser.error('Install google-chrome or chromium first')
    query = urllib.parse.urlencode(urllib.parse.parse_qsl(args.query, keep_blank_values=True))
    url = args.html.as_uri() + ('?' + query if query else '')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # A fresh profile and temporary screenshot avoid reusing a user's browser or accepting an old capture on failure.
    with tempfile.TemporaryDirectory(prefix='openyamm-html-capture-', dir='/tmp') as work:
        screenshot = Path(work) / 'capture.png'
        command = [chrome, '--headless', '--no-sandbox', '--disable-gpu', '--disable-dev-shm-usage',
                   '--allow-file-access-from-files', '--no-first-run', '--disable-background-networking',
                   '--user-data-dir=' + str(Path(work) / 'profile'),
                   '--window-size={},{}'.format(args.width, args.height),
                   '--virtual-time-budget=' + str(args.wait_ms), '--screenshot=' + str(screenshot), url]
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=60)
        if result.returncode or not screenshot.is_file():
            raise SystemExit('Chrome capture failed:\n' + result.stderr[-4000:])
        header = screenshot.read_bytes()[:24]
        if header[:8] != b'\x89PNG\r\n\x1a\n' or len(header) < 24:
            raise SystemExit('Chrome did not produce a valid PNG')
        shutil.copy2(screenshot, args.output)
        print('{} ({}x{})'.format(args.output, *struct.unpack('>II', header[16:24])))


if __name__ == '__main__':
    main()
