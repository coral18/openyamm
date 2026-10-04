"""Exercise release profile selection and source-free validation with a real cooked family."""
import json
import importlib.util
from pathlib import Path
import shutil
import subprocess
import struct
import sys
import tempfile
import unittest
import zipfile

REPO = Path(__file__).resolve().parents[1]
COOKER = REPO / 'build/game/openyamm_sprite_atlas_cook'
SPEC = importlib.util.spec_from_file_location('package_runtime_assets', REPO / 'tools/package_runtime_assets.py')
PACKAGER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGER)


class SpriteDeploymentTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='openyamm-sprite-package-test-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.assets = self.root / 'assets_dev'
        self.sprites = self.assets / 'engine/sprites_new'
        shutil.copytree(REPO / 'assets_dev/engine/sprites_new/mm6_gob', self.sprites / 'mm6_gob')

    def verify(self, profile='desktop'):
        return subprocess.run([str(COOKER), '--verify', str(self.sprites), profile],
                              capture_output=True, text=True)

    def android_sprites(self):
        mobile = self.root / 'assets_cooked/android/sprites_new'
        shutil.copytree(REPO / 'assets_cooked/android/sprites_new/mm6_gob', mobile / 'mm6_gob')
        return mobile

    def lighting(self, dependency='_legacy/sprites_original/tree.bmp'):
        header = bytearray(96)
        header[:8] = b'OYMLIT1\0'
        struct.pack_into('<II', header, 8, 3, 96)
        name = dependency.encode()
        extension = struct.pack('<IIIQ', 0, 1, len(name), 0) + name
        struct.pack_into('<I', header, 48, 96)
        struct.pack_into('<II', header, 64, 96, 96 + len(extension))
        struct.pack_into('<I', header, 76, 1)
        path = self.assets / 'worlds/mm6/maps/test.lighting'
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(header + extension)
        return path

    def test_runtime_is_complete_without_authoring_pngs(self):
        self.assertEqual(list(self.sprites.rglob('*.png')), [])
        result = self.verify()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_wrong_profile_and_unshipped_files_are_rejected(self):
        self.assertNotEqual(self.verify('android').returncode, 0)
        preview = self.sprites / 'mm6_gob/preview.png'
        preview.write_bytes(b'not a release asset')
        self.assertNotEqual(self.verify().returncode, 0)
        preview.unlink()
        (self.sprites / 'mm6_gob/runtime/page-0.oysdf').write_bytes(b'obsolete distance data')
        self.assertNotEqual(self.verify().returncode, 0)

    def test_corrupt_payload_and_changed_manifest_are_rejected(self):
        path = self.sprites / 'mm6_gob/runtime/page-0.oyatlas'
        data = path.read_bytes()
        path.write_bytes(data[:-1] + bytes([data[-1] ^ 1]))
        self.assertNotEqual(self.verify().returncode, 0)
        path.write_bytes(data)
        manifest = self.sprites / 'mm6_gob/manifest.json'
        manifest.write_text(manifest.read_text() + '\n')
        self.assertNotEqual(self.verify().returncode, 0)

    def test_android_zip_replaces_desktop_profile_and_stores_gpu_pages_once(self):
        mobile = self.android_sprites()
        self.assertFalse((self.root / 'assets_source').exists())
        archived = self.assets / '_legacy/sprites_original'
        archived.mkdir(parents=True)
        (archived / 'tree.bmp').write_bytes(b'required for the lighting hash contract')
        (archived / 'unused.bmp').write_bytes(b'unreferenced archived art')
        self.lighting()
        output = self.root / 'packages'
        result = subprocess.run([sys.executable, str(REPO / 'tools/package_runtime_assets.py'),
                                 '--assets-root', str(self.assets),
                                 '--profile', 'android', '--output', str(output), '--cooker', str(COOKER),
                                 '--engine-only'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        with zipfile.ZipFile(output / 'engine.zip') as archive:
            marker = json.loads(archive.read('sprite_texture_profile.json'))
            self.assertEqual(marker['texture_profile'], 'android')
            self.assertEqual(archive.read('_legacy/sprites_original/tree.bmp'), (archived / 'tree.bmp').read_bytes())
            self.assertNotIn('_legacy/sprites_original/unused.bmp', archive.namelist())
            manifest = json.loads(archive.read('sprites_new/mm6_gob/manifest.json'))
            self.assertEqual(manifest['texture_profile'], 'android')
            pages = [entry for entry in archive.infolist() if entry.filename.endswith('.oyatlas')]
            self.assertEqual(len(pages), 4)
            self.assertTrue(all(entry.compress_type == zipfile.ZIP_STORED for entry in pages))
            outlines = [entry for entry in archive.infolist() if entry.filename.endswith('.oysdf')]
            self.assertEqual(outlines, [])
            self.assertFalse(any(entry.filename.endswith(('.png', '.stamp')) for entry in archive.infolist()))
            for entry in pages:
                self.assertEqual(archive.read(entry), (mobile / Path(entry.filename).relative_to('sprites_new')).read_bytes())
        (mobile / 'mm6_gob/manifest.json').unlink()
        result = subprocess.run([str(COOKER), '--verify', str(mobile), 'android'], capture_output=True)
        self.assertNotEqual(result.returncode, 0)

    def test_android_metadata_cannot_change_the_reviewed_placement(self):
        mobile = self.android_sprites()
        path = mobile / 'mm6_gob/manifest.json'
        manifest = json.loads(path.read_text())
        frame = next(iter(manifest['frames'].values()))
        frame['crop_origin_px'][0] += 0.5
        path.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, 'animation/placement metadata differs'):
            PACKAGER.verify_sprite_profiles(self.sprites, mobile, 'android', COOKER)

    def test_android_palette_lookups_must_match_desktop(self):
        mobile = self.android_sprites()
        shutil.copytree(REPO / 'assets_dev/engine/sprites_new/mm6_aie1', self.sprites / 'mm6_aie1')
        shutil.copytree(REPO / 'assets_cooked/android/sprites_new/mm6_aie1', mobile / 'mm6_aie1')
        path = next(mobile.rglob('*.rgba32f'))
        palette = bytearray(path.read_bytes())
        struct.pack_into('<f', palette, 0, 0.125)
        path.write_bytes(palette)
        with self.assertRaisesRegex(ValueError, 'palette lookup differs'):
            PACKAGER.verify_sprite_profiles(self.sprites, mobile, 'android', COOKER)

    def test_incomplete_android_family_set_is_rejected(self):
        mobile = self.android_sprites()
        shutil.rmtree(mobile / 'mm6_gob')
        with self.assertRaisesRegex(ValueError, 'does not cover exactly'):
            PACKAGER.verify_sprite_profiles(self.sprites, mobile, 'android', COOKER)

    def test_verification_needs_no_authoring_inputs_but_explicit_cooking_does(self):
        command = [sys.executable, str(REPO / 'tools/cook_sprite_atlases.py'),
                   '--source', str(self.root / 'absent-authoring'), '--output', str(self.sprites),
                   '--profile', 'desktop', '--cooker', str(COOKER)]
        result = subprocess.run(command + ['--verify'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Missing authoring exports', result.stderr)

    def test_missing_and_unsafe_lighting_dependencies_are_rejected(self):
        self.lighting()
        with self.assertRaisesRegex(ValueError, 'Missing archived lighting dependency'):
            PACKAGER.lighting_archive_dependencies(self.assets)
        self.lighting('_legacy/sprites_original/../../private.bmp')
        with self.assertRaisesRegex(ValueError, 'Invalid archived lighting dependency'):
            PACKAGER.lighting_archive_dependencies(self.assets)
        path = self.lighting()
        path.write_bytes(path.read_bytes()[:-1])
        with self.assertRaises(ValueError):
            PACKAGER.lighting_archive_dependencies(self.assets)

    def test_interrupted_deployment_cannot_be_packaged(self):
        shutil.copytree(self.sprites / 'mm6_gob', self.sprites / '.mm6_gob.previous')
        result = subprocess.run([sys.executable, str(REPO / 'tools/package_runtime_assets.py'),
                                 '--assets-root', str(self.assets), '--profile', 'desktop',
                                 '--output', str(self.root / 'packages'), '--cooker', str(COOKER), '--engine-only'],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Finish/recover', result.stderr)
        self.assertFalse((self.root / 'packages/engine.zip').exists())


if __name__ == '__main__':
    unittest.main()
