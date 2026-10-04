"""Export lossless native palette lookup data for the sprite cooker."""
import numpy as np


def normalize_palette_model(runtime, model):
    """Convert authored inline tables to their explicit runtime lookup schema."""
    variants = [v for v in runtime['variants'].values() if not v.get('exact_base_bypass')]
    if model == 'masked_luminance_rgb_v1' and variants:
        shapes = {np.asarray(v.get('luminance_vector', [])).shape for v in variants}
        if shapes == {(256, 3)}:
            model = 'masked_luminance_lut_v1'
            for variant in variants:
                variant['luminance_lut'] = variant.pop('luminance_vector')
        elif shapes == {(33, 33, 33, 3)}:
            model = 'masked_native_rgb_displacement_lut_v1'
            for variant in variants:
                variant['rgb_displacement_lut'] = variant.pop('luminance_vector')
        else:
            assert shapes == {(3,)}, ('Mixed/invalid inline palette shapes', shapes)
    if model == 'multi_mask_luminance_rgb_v1':
        nonlinear = [v for v in runtime['variants'].values() if not v.get('exact_base_bypass')
                     and len(v.get('region_luminance_vectors', [[]])[0]) == 256]
        if nonlinear:
            assert len(nonlinear) == sum(not v.get('exact_base_bypass') for v in runtime['variants'].values())
            model = 'multi_mask_luminance_lut_v1'
            for variant in nonlinear:
                variant['region_luminance_luts'] = variant.pop('region_luminance_vectors')
    return model


def export_palette_lookups(runtime, target):
    """Write the shader's float lookup textures, preserving authored values."""
    model = runtime['recolor_model']
    files = []
    for palette, variant in runtime['variants'].items():
        if variant.get('exact_base_bypass'):
            continue
        key = {'masked_luminance_lut_v1': 'luminance_lut',
               'multi_mask_luminance_lut_v1': 'region_luminance_luts',
               'masked_native_rgb_displacement_lut_v1': 'rgb_displacement_lut'}.get(model)
        if key is None:
            continue
        values = np.array(variant.pop(key), dtype='<f4')
        shape = {'luminance_lut': (256, 3), 'region_luminance_luts': (4, 256, 3),
                 'rgb_displacement_lut': (33, 33, 33, 3)}[key]
        assert values.shape == shape and np.isfinite(values).all() and np.abs(values).max() <= 64
        size = [1089, 33] if key == 'rgb_displacement_lut' else [256, 4 if key == 'region_luminance_luts' else 1]
        rgba = np.zeros((size[1], size[0], 4), dtype='<f4')
        rgba[:, :, :3] = values.reshape(size[1], size[0], 3)
        relative = f'atlas/palette_{palette}.rgba32f'
        (target / relative).write_bytes(rgba.tobytes())
        variant.update(lookup=relative, lookup_size=size)
        files.append(relative)
    return files
