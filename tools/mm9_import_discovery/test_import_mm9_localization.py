#!/usr/bin/env python3
from __future__ import annotations

import csv
import io
import tempfile
import unittest
from pathlib import Path

from extract_rez import RezEntry
from import_mm9_localization import (
    dialogue_pairs, l10n, relative_path, select_voices, translate_catalog, voice_path, write_file,
)


def rude(rows: list[list[str]], encoding: str) -> bytes:
    stream = io.StringIO()
    csv.writer(stream).writerows(rows)
    return stream.getvalue().encode(encoding)


def entry(path: str) -> RezEntry:
    return RezEntry(path, 0, 44, 0, 0, int.from_bytes(b"VAW\0", "little"))


class Mm9LocalizationImportTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
