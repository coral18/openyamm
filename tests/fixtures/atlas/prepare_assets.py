#!/usr/bin/env python3
"""Prepare a fresh, isolated runtime package view for the hidden-trainer scenario.

The output must be on the same filesystem as the input packages. Large archives
are reused by hard link and only read by the game; assets.zip is a private copy.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import zipfile


def prepare(assets, output):
    archives = [assets / "engine.zip", *sorted((assets / "worlds").glob("*.zip"))]
    if not archives[0].is_file() or not (assets / "assets.zip").is_file():
        raise ValueError("Expected engine.zip, assets.zip and world packages in the input directory")
    output.mkdir(parents=True, exist_ok=False)
    (output / "worlds").mkdir()
    for archive in archives:
        os.link(archive, output / archive.relative_to(assets))
    shutil.copy2(assets / "assets.zip", output / "assets.zip")
    fixture = Path(__file__).with_name("Global_zz_hide_bow.lua").read_bytes()
    with zipfile.ZipFile(output / "assets.zip", "a", compression=zipfile.ZIP_DEFLATED) as package:
        package.writestr("engine/events/Global_zz_hide_bow.lua", fixture)
    receipt = {"fixture": "hidden-expert-bow", "source": str(assets),
               "overlay_sha256": hashlib.sha256(fixture).hexdigest()}
    (output / "atlas-fixture.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print("Prepared hidden-trainer fixture; the source package contents were not changed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path, help="A new directory on the input filesystem")
    args = parser.parse_args()
    prepare(args.assets.resolve(), args.output.resolve())
