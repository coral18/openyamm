#!/usr/bin/env python3
"""Keep an authored GLB intact and transcode embedded JPEG maps to runtime PNGs."""
import argparse
import io
import json
import struct
from pathlib import Path

from PIL import Image


def prepare(source: Path, destination: Path):
    data = source.read_bytes()
    magic, version, length = struct.unpack_from("<III", data)
    if magic != 0x46546C67 or version != 2 or length != len(data):
        raise ValueError("Expected a complete glTF 2 GLB")
    json_length, json_kind = struct.unpack_from("<II", data, 12)
    if json_kind != 0x4E4F534A:
        raise ValueError("Expected a JSON chunk")
    document = json.loads(data[20:20 + json_length])
    bin_length, bin_kind = struct.unpack_from("<II", data, 20 + json_length)
    if bin_kind != 0x004E4942 or 28 + json_length + bin_length != len(data):
        raise ValueError("Expected one embedded BIN chunk")
    original = data[28 + json_length:]
    payload = bytearray(original)
    for image in document.get("images", []):
        if image.get("mimeType") != "image/jpeg":
            continue
        view = document["bufferViews"][image["bufferView"]]
        offset = view.get("byteOffset", 0)
        with Image.open(io.BytesIO(original[offset:offset + view["byteLength"]])) as pixels:
            encoded = io.BytesIO()
            pixels.save(encoded, format="PNG")
        png = encoded.getvalue()
        payload.extend(b"\0" * (-len(payload) % 4))
        image["bufferView"] = len(document["bufferViews"])
        image["mimeType"] = "image/png"
        document["bufferViews"].append({"buffer": 0, "byteOffset": len(payload), "byteLength": len(png)})
        payload.extend(png)
    document["buffers"][0]["byteLength"] = len(payload)
    payload.extend(b"\0" * (-len(payload) % 4))
    encoded = json.dumps(document, separators=(",", ":")).encode()
    encoded += b" " * (-len(encoded) % 4)
    result = (struct.pack("<III", magic, version, 28 + len(encoded) + len(payload))
              + struct.pack("<II", len(encoded), json_kind) + encoded
              + struct.pack("<II", len(payload), bin_kind) + payload)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(result)
    print(f"{destination}: {len(result)} bytes; geometry, UVs, weights and animation accessors unchanged")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    arguments = parser.parse_args()
    prepare(arguments.source, arguments.destination)
