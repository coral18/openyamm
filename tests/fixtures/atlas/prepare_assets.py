#!/usr/bin/env python3
"""Prepare a fresh, isolated runtime package view for an Atlas scenario fixture.

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


def prepare(assets, output, fixture_name="hidden-expert-bow"):
    archives = [assets / "engine.zip", *sorted((assets / "worlds").glob("*.zip"))]
    if not archives[0].is_file() or not (assets / "assets.zip").is_file():
        raise ValueError("Expected engine.zip, assets.zip and world packages in the input directory")
    output.mkdir(parents=True, exist_ok=False)
    (output / "worlds").mkdir()
    for archive in archives:
        os.link(archive, output / archive.relative_to(assets))
    shutil.copy2(assets / "assets.zip", output / "assets.zip")
    if fixture_name == "legacy-pure-accuracy":
        target = "worlds/mm8/events/Global_mm8_accuracy.lua"
        fixture = b"-- Test fixture: retain generated legacy Pure Accuracy handlers without the modern supplement.\n"
    elif fixture_name == "remote-companion-contact":
        target = "worlds/mm8/events/maps/d19_zz_companion_contact_fixture.lua"
        fixture = Path(__file__).with_name("d19_zz_companion_contact_fixture.lua").read_bytes()
    elif fixture_name in ("site-spells-mm7", "site-spells-mm8"):
        world = fixture_name.rsplit("-", 1)[1]
        name = ("7out02" if world == "mm7" else "out01") + "_zz_spell_observation_fixture.lua"
        target = f"worlds/{world}/events/maps/{name}"
        fixture = Path(__file__).with_name(name).read_bytes()
    elif fixture_name == "site-items-mm7":
        target = "worlds/mm7/events/maps/7out02_zz_item_observation_fixture.lua"
        fixture = Path(__file__).with_name("7out02_zz_item_observation_fixture.lua").read_bytes()
    elif fixture_name == "inherited-deliveries":
        target = "engine/events/Global_zz_object_delivery_fixture.lua"
        fixture = Path(__file__).with_name("Global_zz_object_delivery_fixture.lua").read_bytes()
    else:
        target = "engine/events/Global_zz_hide_bow.lua"
        fixture = Path(__file__).with_name("Global_zz_hide_bow.lua").read_bytes()
    # A shadow entry also overrides a cooked world archive. Remove old copies of this key first;
    # duplicate ZIP entries have reader-dependent precedence.
    temporary = output / "fixture-overlay.zip"
    with zipfile.ZipFile(output / "assets.zip") as source, \
            zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as package:
        for entry in source.infolist():
            if entry.filename != target:
                package.writestr(entry, source.read(entry))
        package.writestr(target, fixture)
    os.replace(temporary, output / "assets.zip")
    receipt = {"fixture": fixture_name, "source": str(assets), "shadow_path": target,
               "overlay_sha256": hashlib.sha256(fixture).hexdigest()}
    (output / "atlas-fixture.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"Prepared {fixture_name} fixture; the source package contents were not changed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path, help="A new directory on the input filesystem")
    parser.add_argument("--fixture", choices=["hidden-expert-bow", "legacy-pure-accuracy", "remote-companion-contact",
                                              "site-spells-mm7", "site-spells-mm8", "site-items-mm7",
                                              "inherited-deliveries"],
                        default="hidden-expert-bow")
    args = parser.parse_args()
    prepare(args.assets.resolve(), args.output.resolve(), args.fixture)
