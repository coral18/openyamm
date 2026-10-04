import copy
from pathlib import Path
import sys
import tempfile
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/creatures'))
from palette_export import export_palette_lookups, normalize_palette_model


class PaletteExportTests(unittest.TestCase):
    def test_inline_tables_preserve_values(self):
        cases = [
            ((256, 3), 'masked_luminance_lut_v1', 'luminance_lut'),
            ((33, 33, 33, 3), 'masked_native_rgb_displacement_lut_v1', 'rgb_displacement_lut'),
        ]
        for shape, expected, key in cases:
            values = np.arange(np.prod(shape), dtype=np.float32).reshape(shape) / np.prod(shape)
            data = {'variants': {
                '1': {'exact_base_bypass': True},
                '2': {'luminance_vector': values.tolist()},
            }}
            model = normalize_palette_model(data, 'masked_luminance_rgb_v1')
            self.assertEqual(model, expected)
            data['recolor_model'] = model
            with tempfile.TemporaryDirectory() as folder:
                target = Path(folder)
                (target / 'atlas').mkdir()
                files = export_palette_lookups(data, target)
                self.assertEqual(len(files), 1)
                actual = np.frombuffer((target / files[0]).read_bytes(), dtype='<f4').reshape(-1, 4)
                np.testing.assert_array_equal(actual[:, :3], values.reshape(-1, 3).astype('<f4'))
                self.assertTrue(np.all(actual[:, 3] == 0))
            self.assertNotIn(key, data['variants']['2'])

    def test_linear_and_installed_lookup_are_unchanged(self):
        cases = [
            ('masked_luminance_rgb_v1', {'luminance_vector': [1, 2, 3]}),
            ('masked_luminance_lut_v1', {'lookup': 'atlas/lut.rgba32f', 'lookup_size': [256, 1]}),
        ]
        for model, variant in cases:
            data = {'variants': {'2': variant}}
            before = copy.deepcopy(data)
            self.assertEqual(normalize_palette_model(data, model), model)
            self.assertEqual(data, before)

    def test_mixed_shapes_fail(self):
        data = {'variants': {
            '1': {'luminance_vector': [1, 2, 3]},
            '2': {'luminance_vector': [[0, 0, 0]] * 256},
        }}
        with self.assertRaises(AssertionError):
            normalize_palette_model(data, 'masked_luminance_rgb_v1')


if __name__ == '__main__':
    unittest.main()
