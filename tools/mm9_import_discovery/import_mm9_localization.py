#!/usr/bin/env python3
"""Build a private language overlay from an owned MM9 installation and an installed OpenYAMM asset index.

Only code and documentation belong in Git. The resulting recordings and translations retain their original rights.
Audio keeps its original WAV bytes; English sound ids, dialogue graphs and gameplay tables are never replaced.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import io
import json
import re
import sys
import wave
import zipfile
from pathlib import Path, PurePosixPath

from extract_rez import read_rez_entries, type_to_extension

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import l10n


def relative_path(value: str) -> Path:
    path = PurePosixPath(value)
    if not value or path.is_absolute() or "\\" in value or ":" in value or ".." in path.parts:
        raise ValueError(f"unsafe content path: {value!r}")
    return Path(*path.parts)


def voice_path(source: str, extension: str) -> str:
    parts = list(relative_path(source.replace("\\", "/").lower()).parts)
    if len(parts) < 3 or parts[0] != "voices" or parts[1] not in ("pc", "npc", "cinema"):
        raise ValueError(f"unexpected voice path: {source}")
    if parts[1] == "pc":
        if len(parts) != 4:
            raise ValueError(f"unexpected PC voice path: {source}")
        parts[2] = parts[2].replace(" ", "")  # The existing PC voice importer uses this exact package path.
    path = PurePosixPath(*parts)
    return str(path if path.suffix else path.with_suffix("." + extension))


def read_runtime(assets: Path, language: str) -> dict:
    index = json.loads((assets / "index.json").read_text(encoding="utf-8"))
    store = assets / relative_path(index["store"])
    result = {"audio": set(), "rude": {}, "release": index.get("release"), "voice_rows": {}}
    for pack in index["packs"]:
        relevant = pack["id"] in ("mm9.core", "engine.core", "engine.languages")
        if not relevant and not pack["id"].startswith("mm9.audio."):
            continue
        with zipfile.ZipFile(store / relative_path(pack["file"])) as archive:
            if pack["id"].startswith("mm9.audio."):
                for name in archive.namelist():
                    if name.startswith("voices/") and name.endswith(".wav"):
                        relative_path(name)
                        if name in result["audio"]:
                            raise ValueError(f"duplicate indexed MM9 voice: {name}")
                        result["audio"].add(name)
            elif pack["id"] == "mm9.core":
                for name in archive.namelist():
                    if name.startswith("dialogue/rude/") and name.endswith(".rude"):
                        result["rude"][Path(name).name.lower()] = archive.read(name)
                result["sounds"] = archive.read("data_tables/pc_voice_sounds.txt").decode("utf-8")
            elif pack["id"] == "engine.core":
                for line in archive.read("data_tables/character_voices.txt").decode("utf-8").splitlines():
                    cells = line.split("\t")
                    for cell in cells[1:]:
                        if cell.isdigit() and int(cell):
                            result["voice_rows"].setdefault(int(cell), set()).add(cells[0])
            else:
                result["language_yml"] = archive.read(f"{language}/language.yml")
                result["catalog"] = archive.read(f"{language}/text/mm9.po").decode("utf-8-sig")
    if not result["audio"] or not result["rude"] or "catalog" not in result or "sounds" not in result:
        raise ValueError("the indexed installation must include MM9 audio, core and language catalogs")
    return result


def read_resource(stream, entry) -> bytes:
    stream.seek(entry.offset)
    data = stream.read(entry.size)
    if len(data) != entry.size:
        raise ValueError(f"truncated REZ resource: {entry.path}")
    return data


def select_voices(entries, targets: set[str], sounds: str, voice_rows: dict) -> tuple[dict, list[dict]]:
    sources = {}
    for entry in entries:
        if type_to_extension(entry.resource_type) != "wav":
            continue
        name = voice_path(entry.path, "wav")
        if name in sources:
            raise ValueError(f"voice paths collide after normalization: {name}")
        sources[name] = entry
    sound_ids = {}
    for line in sounds.splitlines():
        cells = line.split("\t")
        if len(cells) > 1 and not line.startswith("/") and cells[1].isdigit():
            sound_ids[cells[0] + ".wav"] = int(cells[1])
    unavailable = []
    for target in sorted(targets - sources.keys()):
        rows = voice_rows.get(sound_ids.get(target), set())
        # An absent active speech is an error. A genuinely unused slot is recorded, never filled with another line.
        if not rows or any(not row.startswith("n/u") for row in rows):
            raise ValueError(f"missing active localized voice: {target} (rows: {sorted(rows)})")
        unavailable.append({"path": target, "rows": sorted(rows), "reason": "not shipped by this localization"})
    return {target: sources[target] for target in sorted(targets & sources.keys())}, unavailable


def dialogue_pairs(english: dict[str, bytes], localized: dict[str, bytes], encoding: str) -> tuple[dict, dict]:
    import csv

    pairs = collections.defaultdict(set)
    stats = collections.Counter()
    for name, data in sorted(english.items()):
        if name not in localized:
            stats["missing_files"] += 1
            continue
        stats["files"] += 1
        en_rows = list(csv.reader(io.StringIO(data.decode("cp1252"))))
        pl_rows = list(csv.reader(io.StringIO(localized[name].decode(encoding))))
        columns = (1,) if name == "npcname.rude" else (2,) if name == "topblurb.rude" else (3, 4)

        def key(row):
            return tuple((i, value) for i, value in enumerate(row) if i not in columns)

        keyed = collections.defaultdict(list)
        for row in pl_rows:
            if row:
                keyed[key(row)].append(row)
        seen = collections.Counter()
        for row in en_rows:
            if not row:
                continue
            identity = key(row)
            occurrence = seen[identity]
            seen[identity] += 1
            matches = keyed[identity]
            if len({tuple(match) for match in matches}) > 1:
                stats["ambiguous_rows"] += 1
                continue
            if occurrence >= len(matches):
                stats["unmatched_rows"] += 1
                continue
            other = matches[occurrence]
            stats["matched_rows"] += 1
            for column in columns:
                if column >= len(row) or column >= len(other):
                    continue
                source, translation = row[column].strip(), other[column].strip()
                if l10n.is_text(source) and translation:
                    pairs[l10n._mm9_normalized(source)].add(translation)
    stats["unique_texts"] = len(pairs)
    return pairs, dict(stats)


def translate_catalog(catalog: str, pairs: dict) -> tuple[str, dict]:
    header, entries = l10n.parse_po(catalog)
    stats = collections.Counter()
    for entry in entries:
        if entry.obsolete or entry.plural is not None:
            continue
        translations = pairs.get(l10n._mm9_normalized(entry.msgid))
        if not translations:
            stats["without_official_match"] += 1
            continue
        if len(translations) != 1:
            stats["ambiguous_kept"] += 1
            continue
        translation = next(iter(translations))
        if l10n.mismatch(entry.msgid, translation):
            stats["placeholder_refused"] += 1
            continue
        machine = "machine translation" in " ".join(entry.translator)
        if entry.translated() and not machine and entry.msgstr[0] != translation:
            stats["human_translation_kept"] += 1
            continue
        stats["official_matches"] += 1
        stats["changed"] += int(entry.msgstr[0] != translation)
        entry.msgstr = [translation]
        entry.translator = ["Official MM9 retail localization, imported from the user's own installation."]
        entry.flags.discard("fuzzy")
    # Use the existing PO serializer; keep every original entry, context and fallback not imported above.
    import tempfile
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "mm9.po"
        l10n.write_po(path, header, entries)
        return path.read_text(encoding="utf-8"), dict(stats)


def write_file(path: Path, data: bytes) -> bool:
    if path.exists():
        if path.read_bytes() != data:
            raise ValueError(f"output already contains a different file; choose a fresh output directory: {path}")
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return True


def import_localization(installation: Path, assets: Path, output: Path, language: str, encoding: str) -> dict:
    if not re.fullmatch(r"[a-z]{2}(?:-[a-z0-9]+)?", language) or language == "en":
        raise ValueError("choose a non-English language id, for example pl")
    if installation.resolve() == output.resolve() or installation.resolve() in output.resolve().parents:
        raise ValueError("output must be outside the original installation")
    runtime = read_runtime(assets, language)
    data_root = installation / "Data"
    voices = data_root / "Voices.rez"
    rude = data_root / "Rude.rez"
    selected, unavailable = select_voices(
        read_rez_entries(voices), runtime["audio"], runtime["sounds"], runtime["voice_rows"])
    localized = {}
    with rude.open("rb") as stream:
        for entry in read_rez_entries(rude):
            if type_to_extension(entry.resource_type) == "rude":
                name = Path(entry.path.replace("\\", "/")).name.lower() + ".rude"
                localized[name] = read_resource(stream, entry)
    pairs, dialogue_stats = dialogue_pairs(runtime["rude"], localized, encoding)
    if not pairs:
        raise ValueError("no original dialogue rows match the indexed MM9 content")
    catalog, text_stats = translate_catalog(runtime["catalog"], pairs)
    report = {"schema": 1, "language": language, "release": runtime["release"], "voices": [],
              "unavailable_unused_voices": unavailable, "empty_original_recordings": [],
              "dialogue": dialogue_stats, "text": text_stats,
              "written": 0, "unchanged": 0, "source_archives": {}}
    for archive in (voices, rude):
        digest = hashlib.sha256()
        with archive.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
        report["source_archives"][archive.name] = digest.hexdigest()
    with voices.open("rb") as stream:
        for target, entry in selected.items():
            data = read_resource(stream, entry)
            # Preserve lossless retail PCM, supported by the engine's SDL_LoadWAV_IO decoder.
            with wave.open(io.BytesIO(data), "rb") as sound:
                if sound.getsampwidth() not in (1, 2, 3, 4):
                    raise ValueError(f"invalid PCM recording: {entry.path}")
                if not sound.getnframes():
                    # The Polish CD has empty NPC_012B and NPC_099B files. Record the gap, leaving the base asset alone.
                    report["empty_original_recordings"].append({"path": target, "source": entry.path})
                    continue
                expected = sound.getnframes() * sound.getnchannels() * sound.getsampwidth()
                if len(sound.readframes(sound.getnframes())) != expected:
                    raise ValueError(f"truncated PCM recording: {entry.path}")
            relative = Path("worlds/mm9/audio") / relative_path(target)
            changed = write_file(output / relative, data)
            report["written" if changed else "unchanged"] += 1
            report["voices"].append({"path": relative.as_posix(), "source": entry.path,
                                     "size": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    for relative, data in (("language.yml", runtime["language_yml"]), ("text/mm9.po", catalog.encode("utf-8"))):
        changed = write_file(output / relative, data)
        report["written" if changed else "unchanged"] += 1
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--installation", type=Path, required=True, help="owned MM9 installation, containing Data/")
    parser.add_argument("--assets", type=Path, required=True, help="OpenYAMM assets folder, containing index.json")
    parser.add_argument("--output", type=Path, required=True, help="fresh destination for languages/<id>/ contents")
    parser.add_argument("--report", type=Path, required=True, help="JSON audit outside the language folder")
    parser.add_argument("--language", default="pl")
    parser.add_argument("--encoding", default="cp1250")
    args = parser.parse_args()
    try:
        report_path = args.report.resolve()
        for source in (args.installation.resolve(), args.assets.resolve()):
            for destination in (args.output.resolve(), report_path):
                if destination == source or source in destination.parents:
                    raise ValueError("output and report must be outside the original installation and runtime assets")
        if report_path == args.output.resolve() or args.output.resolve() in report_path.parents:
            raise ValueError("put the audit report outside the mounted language folder")
        report = import_localization(args.installation, args.assets, args.output, args.language, args.encoding)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, KeyError, wave.Error, zipfile.BadZipFile) as error:
        print(f"MM9 localization: {error}", file=sys.stderr)
        return 1
    print(f"MM9 {args.language}: {len(report['voices'])} original WAVs; "
          f"{len(report['unavailable_unused_voices'])} unavailable unused slots; "
          f"{len(report['empty_original_recordings'])} empty retail recordings; "
          f"{report['text'].get('changed', 0)} changed translations; "
          f"written={report['written']} unchanged={report['unchanged']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
