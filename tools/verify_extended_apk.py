#!/usr/bin/env python3
"""Verify an Extended bootstrap APK and optionally compare its runtime content with release 1.0."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile


OFFICIAL_SHA256 = "76938005894690ff636be14677b4b0eb9f6f29e91e7237ce4d9bfc36b3662b47"
RUNTIME_PREFIXES = ("assets/engine/", "assets/worlds/")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def runtime_index(archive):
    entries = {}
    for entry in archive.infolist():
        if entry.is_dir() or not entry.filename.startswith(RUNTIME_PREFIXES):
            continue
        if entry.filename in entries:
            raise ValueError(f"Duplicate runtime entry: {entry.filename}")
        entries[entry.filename] = (entry.CRC, entry.file_size)
    return entries


def asset_path(source):
    if source.startswith("assets_cooked/android/sprites_new/"):
        return "assets/engine/sprites_new/" + source.removeprefix("assets_cooked/android/sprites_new/")
    if source.startswith(("assets_dev/engine/", "assets_dev/worlds/")):
        return "assets/" + source.removeprefix("assets_dev/")
    return None


def compare_runtime(baseline, candidate, changed, deleted):
    missing = sorted(set(baseline) - set(candidate) - deleted)
    modified = sorted(name for name in baseline.keys() & candidate.keys() if baseline[name] != candidate[name])
    unexpected = sorted(set(modified) - changed)
    if missing or unexpected:
        raise ValueError(f"Runtime differs from release 1.0: missing={missing}, unexpected_changes={unexpected}")
    return {"baseline_entries": len(baseline), "candidate_entries": len(candidate),
            "changed_entries": modified, "added_entries": sorted(set(candidate) - set(baseline)),
            "deleted_entries": sorted(set(baseline) - set(candidate))}


def declared_changes(repo, baseline_ref):
    output = subprocess.check_output(
        ["git", "diff", "--name-status", "--no-renames", baseline_ref, "--",
         "assets_dev", "assets_cooked/android/sprites_new"], cwd=repo, text=True,
    )
    changed, deleted = set(), set()
    for line in output.splitlines():
        status, source = line.split("\t", 1)
        name = asset_path(source)
        if name:
            (deleted if status == "D" else changed).add(name)
    return changed, deleted


def has_marker(archive, name, marker):
    tail = b""
    with archive.open(name) as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            data = tail + block
            if marker in data:
                return True
            tail = data[-len(marker):]
    return False


def verify(apk_path, aapt, baseline_apk=None, baseline_ref="1.0"):
    badging = subprocess.check_output([aapt, "dump", "badging", str(apk_path)], text=True)
    package = re.search(r"^package: name='([^']+)'", badging, re.MULTILINE)
    if package is None or package.group(1) != "org.openyamm.extended":
        raise ValueError("APK must use the separate org.openyamm.extended application ID")
    if "application-label:'OpenYAMM Extended'" not in badging:
        raise ValueError("APK is missing the OpenYAMM Extended application label")
    activity = re.search(r"^launchable-activity: name='([^']+)'", badging, re.MULTILINE)
    if activity is None or activity.group(1) != "org.openyamm.android.OpenYammActivity":
        raise ValueError("APK launcher must resolve to the existing Java activity")
    report = {"application_id": package.group(1), "activity": activity.group(1),
              "apk_sha256": sha256(apk_path), "apk_bytes": apk_path.stat().st_size}
    with zipfile.ZipFile(apk_path) as apk:
        names = set(apk.namelist())
        abis = sorted({name.split("/")[1] for name in names if name.startswith("lib/")})
        if abis != ["arm64-v8a"]:
            raise ValueError(f"Expected only arm64-v8a, found {abis}")
        required = {"lib/arm64-v8a/libmain.so", "lib/arm64-v8a/libSDL3.so",
                    "assets/engine/data_tables/class_multipliers.txt", "assets/engine/events/Global.lua",
                    "assets/worlds/mm7/maps/7out01.odm", "assets/settings.ini"}
        if not required <= names:
            raise ValueError(f"Missing required APK entries: {sorted(required - names)}")
        profile = json.loads(apk.read("assets/engine/sprite_texture_profile.json"))
        if profile != {"schema_version": 2, "texture_profile": "android"}:
            raise ValueError("APK must use the Android ETC2/EAC sprite profile")
        shaders = sorted(name for name in names if name.startswith("assets/runtime/shaders/essl/")
                         and name.endswith(".bin"))
        if not shaders or any(apk.getinfo(name).file_size == 0 for name in shaders):
            raise ValueError("Missing or empty Android shaders")
        if not has_marker(apk, "lib/arm64-v8a/libmain.so", b"OpenYAMM Extended bootstrap build"):
            raise ValueError("Native library is missing the Extended build marker")
        rows = apk.read("assets/engine/data_tables/class_multipliers.txt").decode().splitlines()
        knight_rows = [row.split("\t") for row in rows if row.startswith("Knight\t")]
        if len(knight_rows) != 1 or knight_rows[0][1:] != ["35", "6", "0", "0", "None"]:
            raise ValueError("Packaged Knight must have base HP 35 and HP per level 6")
        report.update({"abis": abis, "shader_count": len(shaders),
                       "knight": {"base_health": 35, "health_per_level": 6}})
        candidate = runtime_index(apk)
    if baseline_apk:
        if sha256(baseline_apk) != OFFICIAL_SHA256:
            raise ValueError("Official 1.0 baseline SHA256 does not match the verified public release")
        repo = Path(__file__).resolve().parents[1]
        changed, deleted = declared_changes(repo, baseline_ref)
        with zipfile.ZipFile(baseline_apk) as baseline:
            report["runtime_comparison"] = compare_runtime(runtime_index(baseline), candidate, changed, deleted)
        report["official_baseline_sha256"] = OFFICIAL_SHA256
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--aapt", required=True)
    parser.add_argument("--baseline-apk", type=Path)
    parser.add_argument("--baseline-ref", default="1.0")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    report = verify(args.apk, args.aapt, args.baseline_apk, args.baseline_ref)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Verified Extended ARM64 APK: Knight HP/level=6, {report['shader_count']} shaders")
    if "runtime_comparison" in report:
        comparison = report["runtime_comparison"]
        print(f"Release 1.0 parity: {comparison['baseline_entries']} entries checked, "
              f"{len(comparison['changed_entries'])} declared changes")


if __name__ == "__main__":
    main()
