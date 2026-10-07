"""Exercise the release-content gate without downloading or building the game."""

import importlib.util
import io
from pathlib import Path
import unittest
from unittest.mock import patch
import tempfile
import warnings
import zipfile


spec = importlib.util.spec_from_file_location(
    "verify_extended_apk", Path(__file__).resolve().parents[1] / "tools/verify_extended_apk.py")
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)


class RuntimeComparisonTests(unittest.TestCase):
    def setUp(self):
        self.name = "assets/worlds/mm7/maps/7out01.odm"
        self.baseline = {self.name: (123, 200)}

    def test_undeclared_content_loss_fails(self):
        with self.assertRaisesRegex(ValueError, "missing"):
            verifier.compare_runtime(self.baseline, {}, set(), set())

    def test_undeclared_content_change_fails(self):
        with self.assertRaisesRegex(ValueError, "unexpected_changes"):
            verifier.compare_runtime(self.baseline, {self.name: (456, 200)}, set(), set())

    def test_declared_upstream_change_and_deletion_pass(self):
        result = verifier.compare_runtime(self.baseline, {self.name: (456, 200)}, {self.name}, set())
        self.assertEqual(result["changed_entries"], [self.name])
        result = verifier.compare_runtime(self.baseline, {}, set(), {self.name})
        self.assertEqual(result["deleted_entries"], [self.name])

    def test_repacking_does_not_change_index(self):
        indices = []
        for compression in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
            stream = io.BytesIO()
            with zipfile.ZipFile(stream, "w", compression=compression) as archive:
                archive.writestr(self.name, b"map payload")
                archive.writestr("lib/arm64-v8a/libmain.so", b"native code")
            with zipfile.ZipFile(stream) as archive:
                indices.append(verifier.runtime_index(archive))
        self.assertEqual(indices[0], indices[1])
        self.assertEqual(len(indices[0]), 1)

    def test_duplicate_runtime_entry_fails(self):
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, "w") as archive, warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            archive.writestr(self.name, b"first")
            archive.writestr(self.name, b"second")
        with zipfile.ZipFile(stream) as archive:
            with self.assertRaisesRegex(ValueError, "Duplicate"):
                verifier.runtime_index(archive)

    def test_android_cooked_assets_map_into_engine(self):
        self.assertEqual(verifier.asset_path("assets_cooked/android/sprites_new/monster/manifest.json"),
                         "assets/engine/sprites_new/monster/manifest.json")
        self.assertIsNone(verifier.asset_path("game/tables/ClassMultiplierTable.cpp"))
        self.assertEqual(verifier.asset_path("mods/karol-test/mod.yaml"), "assets/mods/karol-test/mod.yaml")

    def test_mod_payload_is_part_of_runtime_inventory(self):
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, "w") as archive:
            archive.writestr("assets/mods/karol-test/mod.yaml", b"id: karol.test\n")
        with zipfile.ZipFile(stream) as archive:
            self.assertIn("assets/mods/karol-test/mod.yaml", verifier.runtime_index(archive))

    def test_apk_gate_checks_base_knight_and_exact_mod_payload(self):
        mod = (Path(__file__).resolve().parents[1] / "mods/karol-test/mod.yaml").read_bytes()
        payload = {
            "lib/arm64-v8a/libmain.so": b"OpenYAMM Extended bootstrap build",
            "lib/arm64-v8a/libSDL3.so": b"fixture",
            "assets/engine/events/Global.lua": b"fixture",
            "assets/worlds/mm7/maps/7out01.odm": b"fixture",
            "assets/settings.ini": b"fixture",
            "assets/engine/sprite_texture_profile.json": b'{"schema_version":2,"texture_profile":"android"}',
            "assets/runtime/shaders/essl/test.bin": b"fixture",
            "assets/mods/karol-test/mod.yaml": mod,
            "assets/engine/data_tables/class_multipliers.txt": b"Knight\t35\t5\t0\t0\tNone\n",
        }
        badging = ("package: name='org.openyamm.extended'\napplication-label:'OpenYAMM Extended'\n"
                   "launchable-activity: name='org.openyamm.android.OpenYammActivity'\n")
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / "fixture.apk"
            for mutation, message in ((None, None), ("mod", "declared source"), ("base", "Base Knight")):
                candidate = dict(payload)
                if mutation == "mod":
                    candidate["assets/mods/karol-test/mod.yaml"] = mod.replace(b"value: 1", b"value: 9")
                if mutation == "base":
                    candidate["assets/engine/data_tables/class_multipliers.txt"] = b"Knight\t35\t6\t0\t0\tNone\n"
                with zipfile.ZipFile(apk, "w") as archive:
                    for name, contents in candidate.items():
                        archive.writestr(name, contents)
                with patch.object(verifier.subprocess, "check_output", return_value=badging):
                    if message:
                        with self.assertRaisesRegex(ValueError, message):
                            verifier.verify(apk, "fixture-aapt")
                    else:
                        self.assertEqual(verifier.verify(apk, "fixture-aapt")["test_mod"]["id"], "karol.test")


if __name__ == "__main__":
    unittest.main()
