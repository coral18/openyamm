#!/usr/bin/env python3
"""Build a private language overlay from an owned MM9 installation and an installed OpenYAMM asset index.

Only code and documentation belong in Git. The resulting recordings and translations retain their original rights.
Audio keeps its original WAV bytes; English sound ids, dialogue graphs and gameplay tables are never replaced.
"""
from __future__ import annotations

import argparse
import collections
import csv
import hashlib
import io
import json
import re
import shutil
import subprocess
import sys
import tempfile
import wave
import zipfile
from pathlib import Path, PurePosixPath

from extract_rez import read_rez_entries, type_to_extension
from mm9_polish_effects import polish_effect_description

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import l10n
from mm9_localization_review import prepare_review


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
    result = {"audio": set(), "rude": {}, "videos": set(), "release": index.get("release"), "voice_rows": {}}
    for pack in index["packs"]:
        relevant = pack["id"] in ("mm9.core", "engine.core", "engine.languages")
        if not relevant and not pack["id"].startswith(("mm9.audio.", "mm9.videos.")):
            continue
        with zipfile.ZipFile(store / relative_path(pack["file"])) as archive:
            if pack["id"].startswith("mm9.audio."):
                for name in archive.namelist():
                    if name.startswith("voices/") and name.endswith(".wav"):
                        relative_path(name)
                        if name in result["audio"]:
                            raise ValueError(f"duplicate indexed MM9 voice: {name}")
                        result["audio"].add(name)
            elif pack["id"].startswith("mm9.videos."):
                result["videos"].update(name for name in archive.namelist() if name.endswith(".ogv"))
            elif pack["id"] == "mm9.core":
                for name in archive.namelist():
                    if name.startswith("dialogue/rude/") and name.endswith(".rude"):
                        result["rude"][Path(name).name.lower()] = archive.read(name)
                result["sounds"] = archive.read("data_tables/pc_voice_sounds.txt").decode("utf-8")
                result["items"] = archive.read("data_tables/items.txt").decode("utf-8")
                result["readables"] = archive.read("data_tables/readable_items.txt").decode("utf-8")
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


def dialogue_pairs(english: dict[str, bytes], localized: dict[str, bytes], encoding: str,
                   origins: list[dict] | None = None) -> tuple[dict, dict]:
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
        for line, row in enumerate(en_rows, 1):
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
                    if origins is not None:
                        origins.append({"source": l10n._mm9_normalized(source), "translation": translation,
                                        "role": "party" if column == 3 else "npc" if column == 4 else "label",
                                        "file": name, "row": line, "column": column, "ids": row[:3]})
    stats["unique_texts"] = len(pairs)
    return pairs, dict(stats)


def item_pairs(items: str, readables: str, localized: dict[str, bytes], encoding: str,
               language: str = "pl", origins: list[dict] | None = None) -> tuple[dict, dict, set[str]]:
    """Join on explicit retail ids retained by the existing MM9 importer, never on translated names or row order."""
    pairs = collections.defaultdict(set)
    stats = collections.Counter()
    adapted_effects = set()

    def retail_rows(name, width):
        result = {}
        for row in csv.reader(io.StringIO(localized[name].decode(encoding))):
            if not row:
                continue
            if len(row) != width or not row[0].isdigit() or row[0] in result:
                raise ValueError(f"invalid or duplicate original item row in {name}: {row[0]!r}")
            result[row[0]] = row
        return result

    original_items = retail_rows("masteritemlist.csv", 15)
    original_books = retail_rows("book and scroll text.csv", 3)

    def add(source, translation, identifier, field):
        if l10n.is_text(source) and translation.strip():
            pairs[l10n._mm9_normalized(source)].add(translation.strip())
            if origins is not None:
                origins.append({"source": l10n._mm9_normalized(source), "translation": translation.strip(),
                                "role": "item/book", "source_item_id": identifier, "field": field})

    item_ids = {}
    for row in csv.DictReader(io.StringIO(items), delimiter="\t"):
        if row["package_id"] != "mm9" or not row["canonical_id"].startswith("mm9:item/"):
            raise ValueError("MM9 item table has an unexpected owner")
        if row["Item #"] in item_ids:
            raise ValueError(f"duplicate runtime item id: {row['Item #']}")
        raw_id = row["source_item_id"]
        item_ids[row["Item #"]] = raw_id
        original = original_items.get(raw_id)
        if original is None:
            stats["without_retail_item"] += 1
            continue
        # The engine reworks these books to teach its shared skills. Their new names and instructions stay intact.
        if row["use_action"] in ("LearnSkill", "GrantSkillPoints"):
            stats["reworked_learning_items_kept"] += 1
            continue
        add(row["Name"], original[1], raw_id, "name")
        add(row["Not identified name"], original[7], raw_id, "unidentified name")
        # Effects are rewritten from OpenYAMM's final balance values; the retail description would be misleading.
        if "(Effects:" in row["Notes"]:
            translated = polish_effect_description(row["Notes"], original[8]) if language == "pl" else None
            if translated is None:
                stats["effect_descriptions_kept"] += 1
            else:
                add(row["Notes"], translated, raw_id, "adapted effect description")
                adapted_effects.add(l10n._mm9_normalized(row["Notes"]))
                stats["effect_descriptions_adapted"] += 1
        else:
            add(row["Notes"], original[8], raw_id, "description")
        stats["matched_items"] += 1
    for row in csv.DictReader(io.StringIO(readables), delimiter="\t"):
        original = original_books.get(item_ids.get(row["Item#"]))
        if original is None:
            stats["without_retail_book"] += 1
            continue
        add(row["message text"], original[2], original[0], "book body")
        add(row["Location"], original[1], original[0], "book title")
        stats["matched_books"] += 1
    stats["unique_texts"] = len(pairs)
    return pairs, dict(stats), adapted_effects


MOVIES = {"Cutscenes/MM9Intro.ogv": "Intro.bik", "Cutscenes/MM9Shipwreck.ogv": "Shipwreck.bik",
          "Cutscenes/MM9LoseGame.ogv": "LoseGame.bik"}


def file_digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def import_movies(installation: Path, targets: set[str], output: Path, cache: Path) -> list[dict]:
    script = Path(__file__).resolve().parents[1] / "transcode_bik.sh"
    version = subprocess.run(["ffmpeg", "-version"], check=True, capture_output=True).stdout.splitlines()[0]
    result = []
    for target, source in MOVIES.items():
        if target not in targets:
            raise ValueError(f"indexed MM9 content does not use the expected movie: {target}")
        path = installation / "Movies" / source
        source_hash = file_digest(path)
        key = hashlib.sha256((source_hash + file_digest(script)).encode("ascii") + version).hexdigest()
        folder = cache / key
        converted = folder / Path(target).name
        checksum = folder / "sha256.txt"
        hit = converted.is_file() and checksum.is_file() and file_digest(converted) == checksum.read_text().strip()
        if not hit:
            folder.mkdir(parents=True, exist_ok=True)
            with tempfile.TemporaryDirectory() as directory:
                renamed = Path(directory) / (Path(target).stem + ".bik")
                shutil.copyfile(path, renamed)
                log = folder / "transcode.log"
                with log.open("wb") as stream:
                    subprocess.run(["bash", str(script), "-i", directory, "-o", str(folder)], check=True,
                                   stdout=stream, stderr=subprocess.STDOUT)
            if not converted.is_file():
                raise ValueError(f"movie conversion did not produce {converted}")
            checksum.write_text(file_digest(converted) + "\n", encoding="ascii")
        relative = Path("worlds/mm9/videos") / relative_path(target)
        changed = write_file(output / relative, converted.read_bytes())
        result.append({"path": relative.as_posix(), "source": source, "source_sha256": source_hash,
                       "sha256": file_digest(converted), "size": converted.stat().st_size, "cache_hit": hit,
                       "written": changed})
    return result


def translate_catalog(catalog: str, pairs: dict, adapted_effects: set[str] | None = None,
                       candidates: dict | None = None, selections: dict | None = None,
                       retained: set | None = None) -> tuple[str, dict]:
    header, entries = l10n.parse_po(catalog)
    stats = collections.Counter()
    for entry in entries:
        if entry.obsolete or entry.plural is not None:
            continue
        key = (entry.context, l10n._mm9_normalized(entry.msgid))
        if retained and key in retained:
            stats["reviewed_kept"] += 1
            continue
        translations = candidates.get(key) if candidates is not None else pairs.get(key[1])
        if selections and key in selections:
            translations = {selections[key]}
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
        if entry.translated() and not machine:
            stats["human_translation_confirmed"] += 1
            if selections and key in selections:
                stats["reviewed_selections"] += 1
            continue
        stats["changed"] += int(entry.msgstr[0] != translation)
        entry.msgstr = [translation]
        if adapted_effects and l10n._mm9_normalized(entry.msgid) in adapted_effects:
            entry.translator = ["Official MM9 retail narrative; current OpenYAMM effects translated into Polish."]
            stats["adapted_effect_entries"] += 1
        elif selections and key in selections:
            entry.translator = ["Official MM9 retail variant selected during contextual review."]
            stats["reviewed_selections"] += 1
        else:
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


def import_localization(installation: Path, assets: Path, output: Path, language: str, encoding: str,
                        video_cache: Path | None = None, review: dict | None = None) -> dict:
    if not re.fullmatch(r"[a-z]{2}(?:-[a-z0-9]+)?", language) or language == "en":
        raise ValueError("choose a non-English language id, for example pl")
    if installation.resolve() == output.resolve() or installation.resolve() in output.resolve().parents:
        raise ValueError("output must be outside the original installation")
    runtime = read_runtime(assets, language)
    data_root = installation / "Data"
    voices = data_root / "Voices.rez"
    rude = data_root / "Rude.rez"
    data_archive = data_root / "Data.rez"
    selected, unavailable = select_voices(
        read_rez_entries(voices), runtime["audio"], runtime["sounds"], runtime["voice_rows"])
    localized = {}
    with rude.open("rb") as stream:
        for entry in read_rez_entries(rude):
            if type_to_extension(entry.resource_type) == "rude":
                name = Path(entry.path.replace("\\", "/")).name.lower() + ".rude"
                localized[name] = read_resource(stream, entry)
    origins = []
    pairs, dialogue_stats = dialogue_pairs(runtime["rude"], localized, encoding, origins)
    if not pairs:
        raise ValueError("no original dialogue rows match the indexed MM9 content")
    tables = {}
    with data_archive.open("rb") as stream:
        for entry in read_rez_entries(data_archive):
            name = Path(entry.path.replace("\\", "/")).name.lower() + "." + type_to_extension(entry.resource_type)
            if name in ("masteritemlist.csv", "book and scroll text.csv"):
                tables[name] = read_resource(stream, entry)
    table_pairs, table_stats, adapted_effects = item_pairs(
        runtime["items"], runtime["readables"], tables, encoding, language, origins)
    for key, translations in table_pairs.items():
        pairs[key].update(translations)
    candidates, selections, retained, cases = prepare_review(runtime["catalog"], pairs, origins, review, language)
    catalog, text_stats = translate_catalog(
        runtime["catalog"], pairs, adapted_effects, candidates, selections, retained)
    report = {"schema": 1, "language": language, "release": runtime["release"], "voices": [],
              "unavailable_unused_voices": unavailable, "empty_original_recordings": [],
              "dialogue": dialogue_stats, "items_and_books": table_stats, "text": text_stats,
              "review_cases": cases,
              "written": 0, "unchanged": 0, "source_archives": {}}
    for archive in (voices, rude, data_archive):
        report["source_archives"][archive.name] = file_digest(archive)
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
    report["videos"] = import_movies(installation, runtime["videos"], output, video_cache) if video_cache else []
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--installation", type=Path, required=True, help="owned MM9 installation, containing Data/")
    parser.add_argument("--assets", type=Path, required=True, help="OpenYAMM assets folder, containing index.json")
    parser.add_argument("--output", type=Path, required=True, help="fresh destination for languages/<id>/ contents")
    parser.add_argument("--report", type=Path, required=True, help="JSON audit outside the language folder")
    parser.add_argument("--language", default="pl")
    parser.add_argument("--encoding", default="cp1250")
    parser.add_argument("--videos", action="store_true", help="convert the three localized BIK cutscenes; needs FFmpeg")
    parser.add_argument("--review", type=Path, help="private, source-bound decisions for ambiguous translations")
    args = parser.parse_args()
    try:
        report_path = args.report.resolve()
        for source in (args.installation.resolve(), args.assets.resolve()):
            for destination in (args.output.resolve(), report_path):
                if destination == source or source in destination.parents:
                    raise ValueError("output and report must be outside the original installation and runtime assets")
        if report_path == args.output.resolve() or args.output.resolve() in report_path.parents:
            raise ValueError("put the audit report outside the mounted language folder")
        cache = args.report.parent / "video-cache" if args.videos else None
        review = json.loads(args.review.read_text(encoding="utf-8")) if args.review else None
        report = import_localization(
            args.installation, args.assets, args.output, args.language, args.encoding, cache, review)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, KeyError, wave.Error, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        print(f"MM9 localization: {error}", file=sys.stderr)
        return 1
    print(f"MM9 {args.language}: {len(report['voices'])} original WAVs; "
          f"{len(report['unavailable_unused_voices'])} unavailable unused slots; "
          f"{len(report['empty_original_recordings'])} empty retail recordings; "
          f"{len(report['videos'])} movies; "
          f"{report['text'].get('changed', 0)} changed translations; "
          f"written={report['written']} unchanged={report['unchanged']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
