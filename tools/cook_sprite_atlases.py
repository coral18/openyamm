#!/usr/bin/env python3
"""Deploy accepted schema-1 authoring exports as source-free schema-2 GPU packages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

IDENTIFIER = re.compile(r"[a-z0-9_-]{1,128}\Z")


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def inventory(root):
    result = {}
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"Symlink in sprite package: {path}")
        if path.is_file():
            result[path.relative_to(root).as_posix()] = digest(path)
    return result


def source_files(root, manifest):
    names = {"manifest.json"}
    for page in manifest["pages"]:
        for key in ("base", "mask"):
            name = page[key]
            if not re.fullmatch(r"atlas/[a-z0-9_-]+\.png", name):
                raise ValueError(f"Invalid source atlas path: {name}")
            names.add(name)
    for variant in manifest["variants"].values():
        if "lookup" in variant:
            name = variant["lookup"]
            if not re.fullmatch(r"atlas/[a-z0-9_-]+\.rgba32f", name):
                raise ValueError(f"Invalid palette lookup path: {name}")
            names.add(name)
    for name in names:
        if (root / name).is_symlink() or not (root / name).is_file():
            raise ValueError(f"Missing/linked source file: {root / name}")
    return sorted(names)


def runtime_manifest(source, profile):
    if source.get("schema_version") != 1:
        raise ValueError("Authoring exports must use schema 1; do not use a cooked package as the source")
    # Retain placement, variant and animation metadata. Only texture references change.
    result = dict(source, schema_version=2, texture_profile=profile)
    result["pages"] = [dict(size=page["size"], texture=f"runtime/page-{index}.oyatlas")
                       for index, page in enumerate(source["pages"])]
    return result


def verify_runtime(root, profile, cooker):
    if any(not IDENTIFIER.fullmatch(path.name) for path in root.iterdir()):
        raise ValueError(f"Finish/recover the sprite deployment before verification: {root}")
    subprocess.run([str(cooker), "--verify", str(root), profile], check=True)


def publish(staged, destination):
    """Keep the last working package until its replacement is fully cooked and verified."""
    for path in staged.rglob("*"):
        path.chmod(0o755 if path.is_dir() else 0o644)
    backup = destination.with_name(f".{destination.name}.previous")
    if backup.exists():
        raise ValueError(f"Interrupted deployment requires recovery before continuing: {backup}")
    if destination.exists():
        destination.rename(backup)
    try:
        staged.rename(destination)
    except BaseException:
        if backup.exists():
            backup.rename(destination)
        raise
    if backup.exists():
        shutil.rmtree(backup)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path("assets_source/engine/sprites_new"))
    parser.add_argument("--output", type=Path, help="Defaults to the checked-in runtime root for the selected profile")
    parser.add_argument("--profile", choices=("desktop", "android"), required=True)
    parser.add_argument("--cooker", type=Path, default=Path("build/game/openyamm_sprite_atlas_cook" + (".exe" if os.name == "nt" else "")))
    parser.add_argument("--family", help="Deploy one accepted family; other installed families are retained")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--verify", action="store_true", help="Validate all runtime bytes without source images")
    args = parser.parse_args()
    cooker = args.cooker.resolve(strict=True)
    output = (args.output or Path("assets_cooked/android/sprites_new" if args.profile == "android"
                                 else "assets_dev/engine/sprites_new")).resolve()
    if args.verify:
        verify_runtime(output, args.profile, cooker)
        return
    source = args.source.resolve()
    if source == output or source in output.parents or output in source.parents:
        raise ValueError("Source and runtime roots must be separate, non-nested directories")
    if not source.exists():
        raise ValueError(f"Missing authoring exports: {source}; use --verify to validate prebuilt runtime assets")
    packages = sorted(path.parent for path in source.glob("*/manifest.json"))
    if args.family:
        if not IDENTIFIER.fullmatch(args.family):
            raise ValueError("Invalid family id")
        packages = [path for path in packages if path.name == args.family]
    if not packages:
        raise ValueError(f"No source sprite packages found in {source}")
    output.mkdir(parents=True, exist_ok=True)
    for existing in output.glob("*/manifest.json"):
        installed = json.loads(existing.read_text(encoding="utf-8"))
        if installed.get("schema_version") == 2 and installed.get("texture_profile") != args.profile:
            raise ValueError(f"Refusing to mix texture profiles in {output}; select a separate output root")
    cache = source / ".cook-cache" / args.profile
    cache.mkdir(parents=True, exist_ok=True)
    recipe = {"cooker": digest(cooker), "profile": args.profile}
    cooked = retained = 0
    for package in packages:
        if not IDENTIFIER.fullmatch(package.name) or package.is_symlink():
            raise ValueError(f"Invalid source package: {package}")
        manifest = json.loads((package / "manifest.json").read_text(encoding="utf-8"))
        names = source_files(package, manifest)
        fingerprint = dict(recipe, inputs={name: digest(package / name) for name in names})
        runtime_text = json.dumps(runtime_manifest(manifest, args.profile), ensure_ascii=False, indent=2) + "\n"
        runtime_hash = hashlib.sha256(runtime_text.encode()).hexdigest()
        destination = output / package.name
        receipt_path = cache / f"{package.name}.json"
        receipt = json.loads(receipt_path.read_text(encoding="utf-8")) if receipt_path.exists() else {}
        cached_source = receipt.get("source", {})
        inputs_match = all(cached_source.get(key) == value for key, value in fingerprint.items())
        if not args.force and inputs_match and destination.exists():
            if (receipt.get("output", {}).get("manifest.json") == runtime_hash
                    and receipt.get("output") == inventory(destination)):
                retained += 1
                continue
        # Stage beside the destination to keep publication on one filesystem.
        temporary = Path(tempfile.mkdtemp(prefix=f".{package.name}.cook-", dir=output.parent))
        try:
            temporary.chmod(0o755)
            (temporary / "manifest.json").write_text(runtime_text, encoding="utf-8", newline="\n")
            for name in names:
                if name.endswith(".rgba32f"):
                    (temporary / name).parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(package / name, temporary / name)
            command = [str(cooker), str(package), str(temporary)]
            # Explicit retirement of distance sidecars. Subsequent cooker upgrades recook every payload.
            # Require identical source inputs, manifest, profile and every installed byte before reuse.
            if (not args.force and destination.exists()
                    and any(name.endswith(".oysdf") for name in receipt.get("output", {}))
                    and cached_source.get("inputs") == fingerprint["inputs"]
                    and cached_source.get("profile") == args.profile
                    and receipt.get("output", {}).get("manifest.json") == runtime_hash
                    and receipt.get("output") == inventory(destination)):
                command += ["--reuse-colour", str(destination)]
            subprocess.run(command, check=True)
            published = inventory(temporary)
            publish(temporary, destination)
            receipt_temp = receipt_path.with_suffix(".tmp")
            receipt_temp.write_text(json.dumps({"source": fingerprint, "output": published}, indent=2) + "\n")
            os.replace(receipt_temp, receipt_path)
            cooked += 1
        finally:
            if temporary.exists():
                shutil.rmtree(temporary)
    if not args.family:
        expected = {path.name for path in packages}
        actual = {path.name for path in output.iterdir()}
        if actual != expected:
            raise ValueError(f"Unexpected/stale runtime packages: {sorted(actual - expected)}")
    print(f"Sprite deployment ({args.profile}): {cooked} families cooked, {retained} unchanged", flush=True)


if __name__ == "__main__":
    main()
