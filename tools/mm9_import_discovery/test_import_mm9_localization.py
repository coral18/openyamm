#!/usr/bin/env python3
from __future__ import annotations

import csv
import io
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from extract_rez import RezEntry
from import_mm9_localization import (
    dialogue_pairs, import_movies, item_pairs, l10n, relative_path, select_voices, translate_catalog, voice_path,
    write_file,
)
from mm9_polish_effects import NUMBER, polish_effect_description
from mm9_localization_review import prepare_review


def rude(rows: list[list[str]], encoding: str) -> bytes:
    stream = io.StringIO()
    csv.writer(stream).writerows(rows)
    return stream.getvalue().encode(encoding)


def entry(path: str) -> RezEntry:
    return RezEntry(path, 0, 44, 0, 0, int.from_bytes(b"VAW\0", "little"))


def item_fixture():
    headers = ["Item #", "package_id", "canonical_id", "source_item_id", "Name", "Not identified name", "Notes",
               "use_action"]
    rows = [["10002", "mm9", "mm9:item/sword", "2", "Poor Sword", "Sword", "Old weapon.", ""],
            ["10175", "mm9", "mm9:item/artifact", "175", "Artifact", "Sword", "Story. (Effects: +5 Might)", ""],
            ["10400", "mm9", "mm9:item/manual", "400", "Skill manual", "Book", "Teaches a new skill.", "LearnSkill"],
            ["10245", "mm9", "mm9:item/letter", "245", "Letter", "Letter", "A letter.", "ReadText"]]
    stream = io.StringIO()
    writer = csv.writer(stream, delimiter="\t")
    writer.writerows([headers, *rows])
    originals = []
    for raw, name, desc in (("2", "Miecz", "Stara broń."), ("175", "Artefakt", "Dawne efekty."),
                             ("400", "Inna księga", "Uczy czegoś innego."), ("245", "List", "List.")):
        originals.append([raw, name, "", "", "", "", "", name, desc, "", "", "", "", "", ""])
    local = {"masteritemlist.csv": rude(originals, "cp1250"),
             "book and scroll text.csv": rude([["245", "List do Kiry", "Droga Kiro,\nBjarni"]], "cp1250")}
    books = 'Item#\tmessage text\tLocation\tItem Name\n10245\t"Dear Kira,\nBjarni"\tLetter to Kira\tLetter\n'
    return stream.getvalue(), books, local


class Mm9LocalizationImportTests(unittest.TestCase):
    def test_party_and_npc_homonyms_use_the_existing_speaker_context(self) -> None:
        catalog = ('# machine translation\nmsgid "Goodbye"\nmsgstr "Do widzenia"\n\n'
                   '# machine translation\nmsgctxt "said by the party"\nmsgid "Goodbye"\nmsgstr "Do widzenia"\n')
        pairs = {"Goodbye": {"Bywaj", "Bywajcie"}}
        origins = [{"source": "Goodbye", "translation": "Bywaj", "role": "party", "ids": ["1", "0", "1"]},
                   {"source": "Goodbye", "translation": "Bywajcie", "role": "npc", "ids": ["1", "0", "1"]}]
        candidates, selected, retained, cases = prepare_review(catalog, pairs, origins)
        result, _ = translate_catalog(catalog, pairs, candidates=candidates)
        _, entries = l10n.parse_po(result)
        self.assertEqual({e.context: e.msgstr[0] for e in entries},
                         {"": "Bywajcie", "said by the party": "Bywaj"})
        self.assertTrue(all(case["status"] == "context-resolved" for case in cases))
        decision = {"id": cases[0]["id"], "source_digest": cases[0]["source_digest"], "action": "select",
                    "translation": "Bywaj", "reason": "Wrong role must be refused"}
        with self.assertRaises(ValueError):
            prepare_review(catalog, pairs, origins, {"schema": 1, "language": "pl", "decisions": [decision]})

    def test_review_selection_is_bound_to_candidates_existing_text_and_source_provenance(self) -> None:
        catalog = '# machine translation\nmsgid "Hello"\nmsgstr "Cześć"\n'
        pairs = {"Hello": {"Witaj", "Witajcie"}}
        origins = [{"source": "Hello", "translation": "Witaj", "role": "npc", "ids": ["1"]},
                   {"source": "Hello", "translation": "Witajcie", "role": "npc", "ids": ["2"]}]
        _, _, _, cases = prepare_review(catalog, pairs, origins)
        decision = {"id": cases[0]["id"], "source_digest": cases[0]["source_digest"], "action": "select",
                    "translation": "Witajcie", "reason": "Use the party plural"}
        review = {"schema": 1, "language": "pl", "decisions": [decision]}
        candidates, selected, retained, _ = prepare_review(catalog, pairs, origins, review)
        result, stats = translate_catalog(catalog, pairs, candidates=candidates, selections=selected, retained=retained)
        self.assertEqual(l10n.parse_po(result)[1][0].msgstr, ["Witajcie"])
        self.assertEqual(stats["reviewed_selections"], 1)
        for changed_catalog, changed_pairs, changed_origins in (
            (catalog.replace("Cześć", "Hej"), pairs, origins),
            (catalog, {"Hello": {"Witaj", "Witajcie", "Hej"}}, origins),
            (catalog, pairs, [{**origin, "ids": ["3"]} for origin in origins]),
        ):
            with self.assertRaises(ValueError):
                prepare_review(changed_catalog, changed_pairs, changed_origins, review)
        with self.assertRaises(ValueError):
            prepare_review(catalog, pairs, origins, {**review, "decisions": [decision, decision]})
        protected = catalog.replace("# machine translation", "# Author reviewed")
        _, _, _, human_cases = prepare_review(protected, pairs, origins)
        human_decision = {**decision, "source_digest": human_cases[0]["source_digest"]}
        with self.assertRaisesRegex(ValueError, "human translation"):
            prepare_review(protected, pairs, origins, {**review, "decisions": [human_decision]})
        confirmed, _ = translate_catalog(protected, {"Hello": {"Cześć"}})
        self.assertEqual(l10n.parse_po(confirmed)[1][0].translator, ["Author reviewed"])
        for invalid in ([], {"schema": 1, "language": "de", "decisions": []},
                        {**review, "decisions": ["invalid"]}):
            with self.assertRaises(ValueError):
                prepare_review(catalog, pairs, origins, invalid)
        keep = {**decision, "action": "keep", "reason": "Retail alternatives change the required location"}
        candidates, selected, retained, _ = prepare_review(catalog, pairs, origins, {**review, "decisions": [keep]})
        result, _ = translate_catalog(catalog, pairs, candidates=candidates, selections=selected, retained=retained)
        self.assertEqual(l10n.parse_po(result)[1][0].msgstr, ["Cześć"])

    def test_uses_exact_runtime_paths_without_flattening_npc_subfolders(self) -> None:
        self.assertEqual(voice_path("VOICES/PC/HALF ORC LICH FEMALE/HalfOrcLichF01a", "wav"),
                         "voices/pc/halforclichfemale/halforclichf01a.wav")
        self.assertEqual(voice_path("VOICES/CINEMA/WINGAME/NJAM", "wav"), "voices/cinema/wingame/njam.wav")

    def test_rejects_unsafe_paths_and_normalization_collisions(self) -> None:
        for path in ("../escape", "/absolute", "C:/drive", "a\\b"):
            with self.assertRaises(ValueError):
                relative_path(path)
        with self.assertRaises(ValueError):
            select_voices([entry("VOICES/PC/ANGRY FEMALE/X"), entry("VOICES/PC/ANGRYFEMALE/X")], set(), "", {})

    def test_missing_active_speech_fails_instead_of_installing_an_english_fallback(self) -> None:
        target = "voices/pc/angryfemale/angryf01a.wav"
        sounds = target[:-4] + "\t55000\tswap\t0"
        with self.assertRaises(ValueError):
            select_voices([], {target}, sounds, {55000: {"Hello1"}})
        with self.assertRaises(ValueError):
            select_voices([], {target}, sounds, {55000: {"n/uShutUp1", "Hello1"}})
        selected, absent = select_voices([], {target}, sounds, {55000: {"n/uShutUp1"}})
        self.assertFalse(selected)
        self.assertEqual(absent[0]["path"], target)

    def test_pairs_reordered_rows_by_full_gameplay_identity(self) -> None:
        english = [["1", "0", "1", "Ask", "Answer", "2", "99"],
                   ["1", "0", "2", "Other", "Another", "3", "100"]]
        polish = [["1", "0", "2", "Inne", "Jeszcze", "3", "100"],
                  ["1", "0", "1", "Pytaj", "Odpowiedź", "2", "99"]]
        pairs, stats = dialogue_pairs({"npc1.rude": rude(english, "cp1252")},
                                      {"npc1.rude": rude(polish, "cp1250")}, "cp1250")
        self.assertEqual(pairs["Answer"], {"Odpowiedź"})
        self.assertEqual(stats["matched_rows"], 2)
        # A changed gameplay condition must invalidate the row even if ids and row order still match.
        polish[1][-1] = "101"
        pairs, stats = dialogue_pairs({"npc1.rude": rude(english, "cp1252")},
                                      {"npc1.rude": rude(polish, "cp1250")}, "cp1250")
        self.assertNotIn("Answer", pairs)
        self.assertEqual(stats["unmatched_rows"], 1)

    def test_top_blurbs_use_node_identity_instead_of_occurrence_order(self) -> None:
        en = [["1", "0", "First"], ["1", "1", "Second"]]
        pl = [["1", "1", "Drugi"], ["1", "0", "Pierwszy"]]
        pairs, _ = dialogue_pairs({"topblurb.rude": rude(en, "cp1252")},
                                  {"topblurb.rude": rude(pl, "cp1250")}, "cp1250")
        self.assertEqual(pairs["First"], {"Pierwszy"})

    def test_repeated_identity_with_different_local_text_is_not_paired_by_position(self) -> None:
        en = [["1", "0", "1", "Ask", "Answer", "2"]]
        pl = [["1", "0", "1", "Pytaj", "Pierwsza", "2"],
              ["1", "0", "1", "Pytaj", "Druga", "2"]]
        pairs, stats = dialogue_pairs({"npc1.rude": rude(en, "cp1252")},
                                      {"npc1.rude": rude(pl, "cp1250")}, "cp1250")
        self.assertFalse(pairs)
        self.assertEqual(stats["ambiguous_rows"], 1)

    def test_catalog_keeps_context_unmatched_and_human_text_and_refuses_unsafe_or_ambiguous_entries(self) -> None:
        catalog = '''msgid ""
msgstr "Language: pl\\n"

# machine translation
#, fuzzy
msgctxt "world:mm9"
msgid "Hello"
msgstr "Cześć"

# Reviewed translation
msgid "Human"
msgstr "Sprawdzone"

# machine translation
msgid "Missing {name}"
msgstr "Brak {name}"

# machine translation
msgid "Ambiguous"
msgstr "Poprzednie"

msgid "New OpenYAMM content"
msgstr "Nowy tekst"
'''
        result, stats = translate_catalog(catalog, {"Hello": {"Witaj"}, "Human": {"Inne"},
                                                   "Missing {name}": {"Brak"}, "Ambiguous": {"A", "B"}})
        _, entries = l10n.parse_po(result)
        by_id = {e.msgid: e for e in entries}
        self.assertEqual(by_id["Hello"].msgstr, ["Witaj"])
        self.assertEqual(by_id["Hello"].context, "world:mm9")
        self.assertNotIn("fuzzy", by_id["Hello"].flags)
        self.assertEqual(by_id["Human"].msgstr, ["Sprawdzone"])
        self.assertEqual(by_id["Missing {name}"].msgstr, ["Brak {name}"])
        self.assertEqual(by_id["Ambiguous"].msgstr, ["Poprzednie"])
        self.assertEqual(by_id["New OpenYAMM content"].msgstr, ["Nowy tekst"])
        self.assertEqual(stats["changed"], 1)
        self.assertEqual(stats["placeholder_refused"], 1)

    def test_rerun_keeps_identical_files_and_refuses_to_overwrite_existing_user_content(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "language.yml"
            self.assertTrue(write_file(path, b"name: Polski\n"))
            self.assertFalse(write_file(path, b"name: Polski\n"))
            with self.assertRaises(ValueError):
                write_file(path, b"different content")
            self.assertEqual(path.read_bytes(), b"name: Polski\n")

    def test_item_identity_imports_names_and_books_but_keeps_reworked_effects_and_learning_items(self) -> None:
        items, books, local = item_fixture()
        pairs, stats, _ = item_pairs(items, books, local, "cp1250")
        self.assertEqual(pairs["Poor Sword"], {"Miecz"})
        self.assertEqual(pairs["Dear Kira, Bjarni"], {"Droga Kiro,\nBjarni"})
        self.assertEqual(pairs["Letter to Kira"], {"List do Kiry"})
        self.assertNotIn("Story. (Effects: +5 Might)", pairs)
        self.assertNotIn("Skill manual", pairs)
        self.assertEqual(stats["matched_books"], 1)
        self.assertEqual(stats["effect_descriptions_kept"], 1)
        self.assertEqual(stats["reworked_learning_items_kept"], 1)

    def test_item_import_rejects_duplicate_retail_ids_and_unexpected_runtime_ownership(self) -> None:
        items, books, local = item_fixture()
        with self.assertRaises(ValueError):
            item_pairs(items.replace("mm9:item/sword", "mm8:item/sword"), books, local, "cp1250")
        local["masteritemlist.csv"] *= 2
        with self.assertRaises(ValueError):
            item_pairs(items, books, local, "cp1250")

    def test_effect_translation_uses_current_values_and_preserves_signs_conditions_and_set_bonuses(self) -> None:
        current = ("Story. (Effects: +15 Endurance, -5 Accuracy, +50 Luck, "
                   "40% chance to poison the target at Grandmaster)")
        original = "Oficjalna opowieść. (Efekty: -5 Wytrzymałość, +99 Moc)"
        translated = polish_effect_description(current, original)
        self.assertTrue(translated.startswith("Oficjalna opowieść. (Efekty:"))
        self.assertEqual(NUMBER.findall(translated), ["+15", "-5", "+50", "40"])
        self.assertIn("arcymistrza", translated)
        self.assertNotIn("+99", translated)
        for label in ("Premia", "Premie"):
            self.assertIn("pełnym zestawie", polish_effect_description(
                "Story. (Effects: +2 Light; +2 Speed (+20 to Speed with complete set))",
                f"Opis. ({label}: stara premia)"))
        self.assertIsNone(polish_effect_description("Story. (Effects: an unknown new mechanic)", original))
        self.assertIsNone(polish_effect_description(current, "Opis bez oznaczenia efektów."))

    def test_movie_import_uses_local_intro_caches_conversions_and_rebuilds_corrupt_cache(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            movies = root / "retail/Movies"
            movies.mkdir(parents=True)
            for name in ("Intro.bik", "Shipwreck.bik", "LoseGame.bik", "Intro_E.bik"):
                (movies / name).write_bytes(name.encode("ascii"))
            converted_inputs = []

            def run(command, **kwargs):
                import subprocess
                if command[0] == "ffmpeg":
                    return subprocess.CompletedProcess(command, 0, stdout=b"ffmpeg test-version\n")
                source = next(Path(command[command.index("-i") + 1]).glob("*.bik"))
                target = Path(command[command.index("-o") + 1]) / (source.stem + ".ogv")
                converted_inputs.append(source.read_bytes())
                target.write_bytes(b"test-converted-" + source.read_bytes())
                return subprocess.CompletedProcess(command, 0)

            targets = {"Cutscenes/MM9Intro.ogv", "Cutscenes/MM9Shipwreck.ogv", "Cutscenes/MM9LoseGame.ogv"}
            with patch("import_mm9_localization.subprocess.run", side_effect=run):
                first = import_movies(root / "retail", targets, root / "output", root / "cache")
                self.assertEqual(len(converted_inputs), 3)
                self.assertIn(b"Intro.bik", converted_inputs)
                self.assertNotIn(b"Intro_E.bik", converted_inputs)
                again = import_movies(root / "retail", targets, root / "output", root / "cache")
                self.assertEqual(len(converted_inputs), 3)
                self.assertTrue(all(row["cache_hit"] and not row["written"] for row in again))
                cached = next((root / "cache").rglob("MM9Intro.ogv"))
                cached.write_bytes(b"corrupted cache")
                repaired = import_movies(root / "retail", targets, root / "output", root / "cache")
                self.assertEqual(len(converted_inputs), 4)
                self.assertEqual(first[0]["sha256"], repaired[0]["sha256"])


if __name__ == "__main__":
    unittest.main()
