#!/usr/bin/env python3
"""Cache authentic MM7/MM8 tables, base Lua and legacy geometry for focused Atlas QA.

This is a data-only view of the supplied runtime packages, not a launchable runtime.
Large graphics/audio payloads remain in their original packages. Reused cache entries
are hash-verified; assets.zip overrides engine.zip exactly as in the packaged game.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile


def wanted(virtual):
    if virtual in {"engine/data_tables/map_stats.txt", "engine/data_tables/house_data.txt",
                   "engine/scripts/common/event_support.lua"}:
        return True
    parts = virtual.split("/")
    if len(parts) < 4 or parts[:1] != ["worlds"] or parts[1] not in {"mm7", "mm8"}:
        return False
    return (parts[2] == "maps" and virtual.endswith((".odm", ".blv"))) or (
        parts[2] == "events" and virtual.endswith(".lua"))


def prepare(assets, output):
    manifest_path = output / "manifest.json"
    if output.exists() and not manifest_path.exists() and any(output.iterdir()):
        raise ValueError("Output must be empty or an existing Atlas audit cache")
    previous = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
    if previous and (previous.get("format") != 1 or previous.get("scope") != "atlas-mm7-mm8-base-locations"):
        raise ValueError("The output directory belongs to a different cache format")
    entries = {}
    packages = {}
    # Specific overlay files win over base packages. World packages keep their namespace.
    sources = [("engine.zip", "engine/"), ("worlds/mm7.zip", "worlds/mm7/"),
               ("worlds/mm8.zip", "worlds/mm8/"), ("assets.zip", "")]
    for relative, prefix in sources:
        archive = assets / relative
        stat = archive.stat()
        packages[relative] = {"size": stat.st_size, "mtime_ns": stat.st_mtime_ns}
        with zipfile.ZipFile(archive) as package:
            for item in package.infolist():
                virtual = prefix + item.filename
                if wanted(virtual):
                    if PurePosixPath(virtual).is_absolute() or ".." in PurePosixPath(virtual).parts or "\\" in virtual:
                        raise ValueError(f"Unsafe package entry: {virtual}")
                    entries[virtual] = (relative, item.filename, item.CRC, item.file_size)
    files = {}
    refreshed = 0
    output.mkdir(parents=True, exist_ok=True)
    # Open each package once; Python buffers its central-directory reads on WSL/NTFS.
    for relative, _ in sources:
        with zipfile.ZipFile(assets / relative) as package:
            for virtual, (owner, member, crc, size) in entries.items():
                if owner != relative:
                    continue
                target = output / virtual
                old = previous.get("files", {}).get(virtual, {})
                valid = (target.is_file() and old.get("crc") == crc and old.get("size") == size
                         and hashlib.sha256(target.read_bytes()).hexdigest() == old.get("sha256"))
                if not valid:
                    data = package.read(member)  # zipfile also verifies the original entry CRC.
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(data)
                    refreshed += 1
                files[virtual] = {"package": owner, "crc": crc, "size": size,
                                  "sha256": hashlib.sha256(target.read_bytes()).hexdigest()}
    # Consumers use only this file list. Removed package entries cannot survive as stale cache inputs.
    manifest = {"format": 1, "scope": "atlas-mm7-mm8-base-locations", "packages": packages, "files": files}
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    total = sum(entry["size"] for entry in files.values()) / 1024 ** 2
    print(f"Atlas audit cache: {len(files)} files, {refreshed} refreshed, {total:.1f} MiB")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", required=True, type=Path, help="Directory containing the runtime ZIP packages")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] / "build/atlas-audit")
    args = parser.parse_args()
    prepare(args.assets.resolve(), args.output.resolve())
