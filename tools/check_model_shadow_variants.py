#!/usr/bin/env python3
"""Check that compiled GLSL world programs exclude mesh shadows unless explicitly enabled."""

import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="compiled GLSL directory, e.g. build/runtime/shaders/glsl")
    args = parser.parse_args()
    names = (
        "fs_outdoor_textured_fog", "fs_outdoor_terrain_fog", "fs_outdoor_bmodel_baked",
        "fs_outdoor_terrain_baked", "fs_terrain_decoration", "fs_terrain_decoration_baked",
    )
    for name in names:
        ordinary = (args.directory / f"{name}.bin").read_bytes()
        shadowed = (args.directory / f"{name}_shadow.bin").read_bytes()
        assert ordinary[:3] == shadowed[:3] == b"FSH", f"{name}: invalid fragment shader"
        assert ordinary[:12] == shadowed[:12], f"{name}: vertex/fragment interfaces differ"
        for binding in (b"u_sunShadowParams", b"u_sunShadowMatrices", b"s_sunShadowNear", b"s_sunShadowFar"):
            assert binding not in ordinary, f"{name}: ordinary scenes still carry {binding!r}"
            assert binding in shadowed, f"{name}: shadow receiver missing {binding!r}"
    print(f"Verified {len(names)} receiver-free/shadowed GLSL pairs with matching interfaces.")


if __name__ == "__main__":
    main()
