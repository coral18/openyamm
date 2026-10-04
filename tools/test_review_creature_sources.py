"""Check explicit source-bank imports, sparse resets and selected-art protection."""

import contextlib
import copy
import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import review_creature_sources as review


class ReviewImportTests(unittest.TestCase):
    def setUp(self):
        review.shared_viewer()
        self.directory = tempfile.TemporaryDirectory(prefix='creature-import-test-')
        self.addCleanup(self.directory.cleanup)
        self.bank = Path(self.directory.name)
        self.family = SimpleNamespace(
            id='mm6_test', label='Test family', root=self.bank / 'mm6_test/review',
            manifest_sha256='manifest-hash', artifact_sha256='artifact-hash',
            manifest={'frames': {'pose': {'crop_origin_px': [12.5, -8]}},
                      'pixels_per_logical_pixel': 2, 'variants': {'1': {}, '2': {}}})
        self.recipe = {'schema_version': 1, 'world': 'mm6', 'family_count': 1, 'frame_count': 1,
                       'reviewed_checkpoint': {}, 'validation': {'viewer_implementation': 'app.js',
                                                                'viewer_sha256': 'js-hash'},
                       'families': {'mm6_test': {
                           'source': {'manifest_sha256': 'manifest-hash', 'artifact_sha256': 'artifact-hash'},
                           'frames': {'pose': {'offset_px': [8, -4], 'scale_factor': 1.08, 'scale_anchor': 'bottom'}},
                           'brightness_multiplier': 1, 'baked_native_palettes': ['2'],
                           'review': {'revision': 1, 'guides': None, 'landmarks': {}, 'frame_annotations': {}}}}}
        self.record = {'revision': 1, 'manifest_sha256': 'manifest-hash', 'artifact_sha256': 'artifact-hash',
                       'pixels_per_logical_pixel': 2, 'frames': {}, 'colors': {}, 'landmarks': {}, 'guides': None}

    def checkpoint(self):
        return {'schema_version': 1, 'families': {'mm6_test': {'artifact-hash': self.record}}}

    def collect(self):
        return review.collect_changes(self.recipe, [self.family], self.checkpoint())

    def test_sparse_reset_removes_old_geometry_instead_of_preserving_it(self):
        candidate, changes = self.collect()
        self.assertEqual(candidate['families']['mm6_test']['frames']['pose'], review.GEOMETRY)
        self.assertEqual(changes[0]['changed_frames'], ['pose'])
        self.assertTrue(changes[0]['requires_export'])
        self.assertEqual(self.recipe['families']['mm6_test']['frames']['pose']['offset_px'], [8, -4])

    def test_absolute_values_are_replaced_once_and_native_palette_source_is_retained(self):
        self.record['frames']['pose'] = {'offset_px': [-3, 2], 'scale_factor': .975,
                                        'scale_anchor': 'pivot', 'regenerate': False}
        candidate, _ = self.collect()
        family = candidate['families']['mm6_test']
        self.assertEqual(family['frames']['pose'], {'offset_px': [-3, 2], 'scale_factor': .975,
                                                   'scale_anchor': 'pivot'})
        self.assertEqual(family['baked_native_palettes'], ['2'])
        self.assertEqual(family['source'], self.recipe['families']['mm6_test']['source'])

    def test_wrong_selected_art_cannot_be_imported_as_a_neutral_reset(self):
        state = self.checkpoint()
        state['families']['mm6_test']['other-artifact'] = state['families']['mm6_test'].pop('artifact-hash')
        with self.assertRaisesRegex(ValueError, 'different selected generation'):
            review.collect_changes(self.recipe, [self.family], state)
        self.record['manifest_sha256'] = 'different-manifest'
        with self.assertRaisesRegex(ValueError, 'manifest_sha256'):
            self.collect()

    def test_non_exportable_palette_edits_fail_instead_of_being_ignored(self):
        setting = {'region': 'all', 'saturation': 1, 'brightness': .93, 'enabled': True}
        self.record['colors'] = {'1': setting}
        with self.assertRaisesRegex(ValueError, 'same positive'):
            self.collect()
        self.record['colors']['2'] = copy.deepcopy(setting)
        candidate, changes = self.collect()
        self.assertEqual(candidate['families']['mm6_test']['brightness_multiplier'], .93)
        self.assertTrue(changes[0]['requires_export'])
        self.record['colors']['1']['saturation'] = .8
        with self.assertRaisesRegex(ValueError, 'saturation'):
            self.collect()

    def test_annotation_changes_do_not_require_resampling(self):
        self.recipe['families']['mm6_test']['frames']['pose'] = copy.deepcopy(review.GEOMETRY)
        self.record['frames']['pose'] = {**review.GEOMETRY, 'regenerate': False, 'note': 'Check bow grip'}
        _, changes = self.collect()
        self.assertFalse(changes[0]['requires_export'])
        self.assertTrue(changes[0]['review_changed'])

    def test_invalid_geometry_is_rejected_by_shared_editor_validation(self):
        for changes in ({'offset_px': [.5, 0]}, {'scale_factor': float('nan')}, {'scale_factor': 2}):
            with self.subTest(changes=changes):
                self.record['frames']['pose'] = {**review.GEOMETRY, 'regenerate': False, **changes}
                with self.assertRaises(ValueError):
                    self.collect()

    def test_import_freezes_checkpoint_and_is_idempotent(self):
        review.write(self.bank / 'adaptations.json', self.recipe)
        review.write(self.bank / 'review_state/frame_fixups.json', self.checkpoint())
        review.write(self.bank / 'review/collection.json', {})
        original = (self.bank / 'adaptations.json').read_bytes()
        viewer = SimpleNamespace(load_collection=lambda path: ([self.family], {}))
        with patch.object(review, 'shared_viewer', return_value=viewer), contextlib.redirect_stdout(io.StringIO()):
            review.changes_or_import(self.bank, False)
            self.assertEqual((self.bank / 'adaptations.json').read_bytes(), original)
            review.changes_or_import(self.bank, True)
            imported = review.read(self.bank / 'adaptations.json')
            snapshot = Path(imported['reviewed_checkpoint']['path'])
            self.assertTrue(snapshot.is_file())
            self.assertEqual(review.sha(snapshot), imported['reviewed_checkpoint']['sha256'])
            imported_bytes = (self.bank / 'adaptations.json').read_bytes()
            review.changes_or_import(self.bank, True)
            self.assertEqual((self.bank / 'adaptations.json').read_bytes(), imported_bytes)
            self.record['frames']['pose'] = {**review.GEOMETRY, 'offset_px': [1, 0], 'regenerate': False}
            review.write(self.bank / 'review_state/frame_fixups.json', self.checkpoint())
            self.assertEqual(review.sha(snapshot), imported['reviewed_checkpoint']['sha256'])
            self.assertEqual(imported['validation']['status'], 'pending_export_verification')


if __name__ == '__main__':
    unittest.main()
