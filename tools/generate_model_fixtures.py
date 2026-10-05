#!/usr/bin/env python3
"""Generate deterministic shared-model fixtures used by runtime and visual checks."""

from __future__ import annotations

import argparse
import json
import struct
import zlib
from pathlib import Path


def align(buffer: bytearray, alignment: int = 4) -> None:
    while len(buffer) % alignment:
        buffer.append(0)


def append_values(buffer: bytearray, values: list[float] | list[int], fmt: str) -> tuple[int, int]:
    align(buffer)
    offset = len(buffer)
    for value in values:
        buffer.extend(struct.pack("<" + fmt, value))
    return offset, len(buffer) - offset


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))


def checker_png() -> bytes:
    width = 4
    height = 4
    rows = bytearray()
    colors = ((240, 112, 28, 255), (36, 112, 240, 128))
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(colors[(x + y) % 2])
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(bytes(rows), level=9))
        + png_chunk(b"IEND", b"")
    )


def cube_vertices() -> tuple[list[float], list[float], list[float], list[int]]:
    faces = (
        ((0, 0, 1), ((-0.5, -0.5, 0.5), (0.5, -0.5, 0.5), (0.5, 0.5, 0.5), (-0.5, 0.5, 0.5))),
        ((0, 0, -1), ((0.5, -0.5, -0.5), (-0.5, -0.5, -0.5), (-0.5, 0.5, -0.5), (0.5, 0.5, -0.5))),
        ((1, 0, 0), ((0.5, -0.5, 0.5), (0.5, -0.5, -0.5), (0.5, 0.5, -0.5), (0.5, 0.5, 0.5))),
        ((-1, 0, 0), ((-0.5, -0.5, -0.5), (-0.5, -0.5, 0.5), (-0.5, 0.5, 0.5), (-0.5, 0.5, -0.5))),
        ((0, 1, 0), ((-0.5, 0.5, 0.5), (0.5, 0.5, 0.5), (0.5, 0.5, -0.5), (-0.5, 0.5, -0.5))),
        ((0, -1, 0), ((-0.5, -0.5, -0.5), (0.5, -0.5, -0.5), (0.5, -0.5, 0.5), (-0.5, -0.5, 0.5))),
    )
    positions: list[float] = []
    normals: list[float] = []
    texcoords: list[float] = []
    indices: list[int] = []
    for normal, corners in faces:
        first = len(positions) // 3
        for corner, uv in zip(corners, ((0, 0), (1, 0), (1, 1), (0, 1)), strict=True):
            positions.extend(corner)
            normals.extend(normal)
            texcoords.extend(uv)
        indices.extend((first, first + 1, first + 2, first, first + 2, first + 3))
    return positions, normals, texcoords, indices


def build_fixture(masked_emissive: bool = False) -> tuple[bytes, bytes]:
    positions, normals, texcoords, opaque_indices = cube_vertices()
    transparent_first = len(positions) // 3
    for position, uv in zip(
        ((-0.8, -0.35, 0.8), (0.8, -0.35, 0.8), (0.8, 0.75, 0.8), (-0.8, 0.75, 0.8)),
        ((0, 0), (1, 0), (1, 1), (0, 1)),
        strict=True,
    ):
        positions.extend(position)
        normals.extend((0, 0, 1))
        texcoords.extend(uv)
    transparent_indices = [
        transparent_first,
        transparent_first + 1,
        transparent_first + 2,
        transparent_first,
        transparent_first + 2,
        transparent_first + 3,
    ]

    buffer = bytearray()
    views: list[dict[str, int]] = []

    def view(values: list[float] | list[int], fmt: str, target: int | None = None) -> int:
        offset, length = append_values(buffer, values, fmt)
        descriptor: dict[str, int] = {"buffer": 0, "byteOffset": offset, "byteLength": length}
        if target is not None:
            descriptor["target"] = target
        views.append(descriptor)
        return len(views) - 1

    position_view = view(positions, "f", 34962)
    normal_view = view(normals, "f", 34962)
    texcoord_view = view(texcoords, "f", 34962)
    opaque_index_view = view(opaque_indices, "H", 34963)
    transparent_index_view = view(transparent_indices, "H", 34963)
    time_view = view([0.0, 1.0, 2.0], "f")
    prop_translation_view = view([0, 0, 0, 0, 0.35, 0, 0, 0, 0], "f")
    prop_rotation_view = view([0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1], "f")
    marker_translation_view = view([0, 0.75, 0, 0.35, 1.0, 0, 0, 0.75, 0], "f")
    marker_time_view = view([0.0, 1.0, 2.0], "f")
    marker_scale_view = view([1, 1, 1, 1.5, 1.5, 1.5, 1, 1, 1], "f")

    accessors = [
        {"bufferView": position_view, "componentType": 5126, "count": 28, "type": "VEC3",
         "min": [-0.8, -0.5, -0.5], "max": [0.8, 0.75, 0.8]},
        {"bufferView": normal_view, "componentType": 5126, "count": 28, "type": "VEC3"},
        {"bufferView": texcoord_view, "componentType": 5126, "count": 28, "type": "VEC2"},
        {"bufferView": opaque_index_view, "componentType": 5123, "count": len(opaque_indices), "type": "SCALAR"},
        {"bufferView": transparent_index_view, "componentType": 5123,
         "count": len(transparent_indices), "type": "SCALAR"},
        {"bufferView": time_view, "componentType": 5126, "count": 3, "type": "SCALAR",
         "min": [0.0], "max": [2.0]},
        {"bufferView": prop_translation_view, "componentType": 5126, "count": 3, "type": "VEC3"},
        {"bufferView": prop_rotation_view, "componentType": 5126, "count": 3, "type": "VEC4"},
        {"bufferView": marker_translation_view, "componentType": 5126, "count": 3, "type": "VEC3"},
        {"bufferView": marker_time_view, "componentType": 5126, "count": 3, "type": "SCALAR",
         "min": [0.0], "max": [2.0]},
        {"bufferView": marker_scale_view, "componentType": 5126, "count": 3, "type": "VEC3"},
    ]

    document = {
        "asset": {"version": "2.0", "generator": "OpenYAMM deterministic model fixture v1"},
        "scene": 0,
        "scenes": [{"name": "fixture", "nodes": [2]}],
        "nodes": [
            {"name": "textured_prop", "mesh": 0, "children": [1]},
            {"name": "animated_attachment", "translation": [0, 0.75, 0]},
            {"name": "matrix_root", "children": [0],
             "matrix": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.25, 0, 0, 1]},
        ],
        "meshes": [{"name": "textured_and_transparent", "primitives": [
            {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0},
            {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 4, "material": 1},
        ]}],
        "materials": [
            {"name": "opaque_checker", "pbrMetallicRoughness": {
                "baseColorFactor": [1, 1, 1, 1], "baseColorTexture": {"index": 0}}},
            {"name": "transparent_checker", "alphaMode": "BLEND", "doubleSided": True,
             "extensions": {"KHR_materials_unlit": {}}, "pbrMetallicRoughness": {
                 "baseColorFactor": [0.25, 0.8, 1, 0.55], "baseColorTexture": {"index": 0}}},
        ],
        "extensionsUsed": ["KHR_materials_unlit"],
        "textures": [{"sampler": 0, "source": 0}],
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}],
        "images": [{"name": "checker", "uri": "checker.png", "mimeType": "image/png"}],
        "animations": [
            {"name": "bob_spin", "samplers": [
                {"input": 5, "output": 6, "interpolation": "LINEAR"},
                {"input": 5, "output": 7, "interpolation": "LINEAR"},
                {"input": 5, "output": 8, "interpolation": "LINEAR"},
            ], "channels": [
                {"sampler": 0, "target": {"node": 0, "path": "translation"}},
                {"sampler": 1, "target": {"node": 0, "path": "rotation"}},
                {"sampler": 2, "target": {"node": 1, "path": "translation"}},
            ]},
            {"name": "marker_step", "samplers": [
                {"input": 9, "output": 10, "interpolation": "STEP"},
            ], "channels": [
                {"sampler": 0, "target": {"node": 1, "path": "scale"}},
            ]},
        ],
        "buffers": [{"byteLength": len(buffer)}],
        "bufferViews": views,
        "accessors": accessors,
    }

    if masked_emissive:
        document["materials"][0].update(
            name="masked_emissive_checker", alphaMode="MASK", alphaCutoff=0.75,
            emissiveFactor=[0.2, 0.025, 0.005],
        )
        document["materials"][0]["pbrMetallicRoughness"].update(metallicFactor=0, roughnessFactor=0.25)

    json_bytes = json.dumps(document, sort_keys=True, separators=(",", ":")).encode("utf-8")
    while len(json_bytes) % 4:
        json_bytes += b" "
    align(buffer)
    total_length = 12 + 8 + len(json_bytes) + 8 + len(buffer)
    glb = bytearray(struct.pack("<III", 0x46546C67, 2, total_length))
    glb.extend(struct.pack("<II", len(json_bytes), 0x4E4F534A))
    glb.extend(json_bytes)
    glb.extend(struct.pack("<II", len(buffer), 0x004E4942))
    glb.extend(buffer)
    return bytes(glb), checker_png()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("assets_dev/engine/models/fixtures"),
        help="Repository-relative output directory.",
    )
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    glb, png = build_fixture()
    (args.output_dir / "shared_model_fixture.glb").write_bytes(glb)
    masked_glb, _ = build_fixture(masked_emissive=True)
    (args.output_dir / "masked_emissive_fixture.glb").write_bytes(masked_glb)
    (args.output_dir / "checker.png").write_bytes(png)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
