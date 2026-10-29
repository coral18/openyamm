#!/usr/bin/env python3
"""Prepare isolated native UI tours over the active MM7/MM8 dungeon names; do not run the game."""

import argparse
import json
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[1]


def dungeon_maps(table):
    """Read the merged table's identity columns, matching MapStats/MapIdentity's MM7/MM8 defaults."""
    result = {"mm7": [], "mm8": []}
    for line in table.read_text(encoding="utf-8-sig").splitlines():
        row = line.split("\t")
        if len(row) < 3 or not row[0].isdigit() or not row[2].lower().endswith(".blv"):
            continue
        filename = row[2].lower()
        explicit_world = row[34].strip().lower() if len(row) > 34 else ""
        stem = Path(filename).stem
        world = explicit_world or (
            "mm6" if stem.startswith("6") else "mm7" if stem.startswith("7")
            else "mmmerge" if stem in {"breach", "bralvar", "brbase"} else "mm8"
        )
        if world in result:
            result[world].append({"name": row[1].strip(), "file": filename, "id": int(row[0])})
    for world, entries in result.items():
        names = [entry["name"] for entry in entries]
        if not names or any(not name for name in names) or len({name.lower() for name in names}) != len(names):
            raise ValueError(f"{world}: expected distinct, nonempty dungeon names for native row selection")
        for entry in entries:
            matches = sorted(
                (other for other in entries if entry["name"].lower() in other["name"].lower()),
                key=lambda other: (other["name"].lower(), other["file"]),
            )
            entry["search_rank"] = matches.index(entry)
            if entry["search_rank"] >= 7:
                raise ValueError(f"{world}/{entry['file']}: matching row requires an explicit scrolling tour")
    return result


def documents(world, entries):
    start = "7out02.odm" if world == "mm7" else "out01.odm"
    fixture = {
        "id": f"atlas.dungeon_navigation.{world}", "world": world, "mode": "faithful",
        "steps": [
            {"new_game_flow": {"start_map": start, "party": {
                "gold": 5000, "food": 10, "members": [{
                    "name": "Atlas Tester", "class": "Knight", "level": 20, "race_id": 0,
                    "skills": [], "equipment": {},
                }],
            }}},
            {"save_game": "before"},
        ],
    }
    steps = [
        {"expect": "gameplay"}, {"action": "map_book"}, {"click": [1250, 770]},
        {"click": [170, 190]}, {"click": [644, 126]}, {"pointer_move": [800, 860]},
        {"capture": "discovery-before"}, {"click": [1152, 57]}, {"wait": 3},
    ]
    for entry in entries:
        steps.extend([
            {"click": [170, 190]}, {"click": [885, 126]}, {"click": [275, 126]},
            {"text": entry["name"]}, {"key": "Return"},
            {"click": [1250, 260 + 71.25 * entry["search_rank"]]}, {"wait": 0.2},
            {"pointer_move": [800, 860]}, {"capture": "guide-" + Path(entry["file"]).stem},
        ])
    steps.extend([
        {"click": [1152, 57]}, {"click": [170, 190]}, {"click": [644, 126]},
        {"pointer_move": [800, 860]}, {"capture": "discovery-after"},
        {"key": "Escape"}, {"key": "Escape"}, {"expect": "gameplay"},
        {"key": "F9"}, {"wait": 0.4},
    ])
    verify = {
        "id": f"atlas.dungeon_navigation.{world}.verify", "world": world, "mode": "faithful",
        "steps": [
            {"load_game": {"name": "quicksave", "path": "saves/quicksave.oysav"}},
            {"assert": {"map": start, "gold": 5000, "food": 10}},
        ],
    }
    for entry in entries:
        verify["steps"].append({"assert": {"named_global_var_changed": {
            "name": f"openyamm.discovery.world.{world}.map.{Path(entry['file']).stem}", "value": 0,
        }}})
    return {"fixture": fixture, "tour": {"output_dir": "captures", "exit": True, "steps": steps},
            "verify": verify}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="Private QA directory; never an existing campaign directory")
    parser.add_argument("--map-stats", type=Path,
                        default=ROOT / "assets_dev/engine/data_tables/map_stats.txt")
    args = parser.parse_args()
    maps = dungeon_maps(args.map_stats)
    outputs = {args.output / "maps.json": json.dumps(maps, indent=2) + "\n"}
    for world, entries in maps.items():
        for name, document in documents(world, entries).items():
            outputs[args.output / world / f"{name}.yml"] = yaml.safe_dump(document, sort_keys=False)
    for path in outputs:
        if path.exists():
            raise FileExistsError(f"Refusing to overwrite an existing QA artifact: {path}")
    for path, text in outputs.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    for world, entries in maps.items():
        print(f"{world}: {len(entries)} dungeon names; fixture, native 1600x900 tour and save verifier prepared")


if __name__ == "__main__":
    main()
