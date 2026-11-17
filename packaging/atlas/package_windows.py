#!/usr/bin/env python3
"""Package an existing Atlas-enabled Windows build without runtime packs or private localization media."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--overlay", type=Path, required=True)
    parser.add_argument("--content-index", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--code-commit", required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    folder = args.output / f"OpenYAMM-Atlas-{args.version}-windows-x64"
    if folder.exists():
        raise SystemExit(f"Refusing to overwrite existing package directory: {folder}")
    folder.mkdir()
    executable = args.runtime / "OpenYAMMAtlas.exe"
    if not executable.is_file():
        executable = args.runtime / "openyamm.exe"
    shutil.copy2(executable, folder / "OpenYAMMAtlas.exe")
    for name in ["libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"]:
        shutil.copy2(args.runtime / name, folder / name)
    shutil.copytree(args.runtime / "runtime/shaders/glsl", folder / "runtime/shaders/glsl")
    shutil.copytree(args.overlay / "mods", folder / "mods")
    catalog = folder / "languages/pl/text/engine-atlas.po"
    catalog.parent.mkdir(parents=True)
    shutil.copy2(args.overlay / "languages/pl/text/engine-atlas.po", catalog)
    index = json.loads(args.content_index.read_text(encoding="utf-8"))
    if index.get("release") != "2.6" or index.get("content_format") != 1:
        raise SystemExit("Expected a verified OpenYAMM 2.6 desktop content index")
    index["store"] = "packs"
    (folder / "assets/packs").mkdir(parents=True)
    (folder / "assets/index.json").write_text(json.dumps(index, indent=2) + "\n", encoding="utf-8")
    scripts = args.source_root / "packaging/atlas"
    for name in ["Setup-Content.ps1", "Play Atlas.cmd"]:
        content = (scripts / name).read_text(encoding="utf-8")
        (folder / name).write_bytes(content.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8"))
    for name in ["LICENSE", "COPYRIGHT", "ATLAS_RELEASE.md"]:
        shutil.copy2(args.source_root / name, folder / name)
    (folder / "settings.ini").write_text(
        "[profile]\nname=desktop\nversion=5\n[gameplay]\nlanguage=en\n"
        "[atlas]\nenabled=true\n[updates]\ncheck_for_updates=false\n"
        "[startup]\nstart_in_main_menu=true\nsave_file=\n",
        encoding="utf-8",
    )
    metadata = {"product": "OpenYAMM Atlas", "version": args.version, "upstream": "2.6",
                "code_commit": args.code_commit, "platform": "windows-x64",
                "private_mm9_media_included": False}
    (folder / "atlas-release.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    files = sorted(path for path in folder.rglob("*") if path.is_file())
    manifest = "".join(hashlib.sha256(path.read_bytes()).hexdigest() + "  "
                       + path.relative_to(folder).as_posix() + "\n" for path in files)
    (folder / "FILES.sha256").write_text(manifest, encoding="utf-8")
    archive = args.output / (folder.name + ".zip")
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
        for path in sorted(folder.rglob("*")):
            if path.is_file():
                bundle.write(path, path.relative_to(folder.parent))
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (args.output / "SHA256SUMS").write_text(digest + "  " + archive.name + "\n", encoding="utf-8")
    print(f"Created {archive.name}: {archive.stat().st_size / 1024 / 1024:.1f} MiB; SHA-256 {digest}")


if __name__ == "__main__":
    main()
