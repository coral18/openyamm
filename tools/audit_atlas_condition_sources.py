#!/usr/bin/env python3
"""Triage condition selectors on physical Atlas water/shrine sites against readable original EVT exports.

This compares sets of named operations, not branch order, operands or runtime behavior. An explicit
overlay is reported for manual review; its presence does not certify a repair. Inputs remain read-only.
"""

import argparse
import csv
import json
import re
from pathlib import Path


CONDITIONS = {
    "Cursed", "Weak", "Asleep", "Afraid", "Drunk", "Insane", "PoisonedGreen", "DiseasedGreen",
    "PoisonedYellow", "DiseasedYellow", "PoisonedRed", "DiseasedRed", "Paralyzed", "Unconscious",
    "Dead", "Stoned", "Eradicated", "MainCondition",
}
KINDS = {"well", "drink", "shrine"}


def normalize(name):
    return "MainCondition" if name == "MajorCondition" else name


def original_operations(text, event):
    match = re.search(r"^event\s+" + str(event) + r"\s*\n(.*?)(?=^event\s|\Z)", text, re.M | re.S)
    if not match:
        return None
    return {
        (operation, normalize(name))
        for operation, name in re.findall(r'\b(Cmp|Set|Add|Subtract)\s+\{\s*"([^"]+)"', match[1])
        if normalize(name) in CONDITIONS
    }


def generated_operations(text, event):
    match = re.search(r"RegisterEvent\(" + str(event) + r",.*?(?=\nRegister(?:Map)?Event\(|\Z)", text, re.S)
    if not match:
        return None
    methods = {"IsAtLeast": "Cmp", "SetValue": "Set", "AddValue": "Add", "SubtractValue": "Subtract"}
    return {
        (methods[method], normalize(name))
        for method, name in re.findall(r"\b(IsAtLeast|SetValue|AddValue|SubtractValue)\(\s*(\w+)", match[0])
        if normalize(name) in CONDITIONS
    }


def read_text(path):
    # Legacy files mix encodings. Only ASCII EVT operation and selector names are compared here.
    return path.read_text(encoding="utf-8", errors="replace")


def audit(repo, inventory):
    results = []
    seen = set()
    texts = {}
    for site in inventory:
        if site["world"] not in {"mm7", "mm8"} or site["kind"] not in KINDS:
            continue
        world, name, event = site["world"], site["map"], int(site["event"])
        identity = (world, name, event)
        if identity in seen:
            continue
        seen.add(identity)
        stem = Path(name).stem
        original_stem = stem[1:] if world == "mm7" and stem.startswith("7") else stem
        reference = repo / "reference/mmext-scripts/Decompiled_Scripts" / world / "txt"
        sources = [p for p in reference.iterdir() if p.suffix.lower() == ".txt"
                   and p.stem.lower() == original_stem.lower()]
        entry = {"world": world, "map": name, "event": event}
        results.append(entry)
        if len(sources) != 1:
            entry["status"] = "missing_or_ambiguous_source"
            continue
        source = sources[0]
        if source not in texts:
            texts[source] = read_text(source)
        entry["source"] = source.relative_to(repo).as_posix()
        original = original_operations(texts[source], event)
        if original is None:
            entry["status"] = "missing_source_event"
            continue
        folder = repo / "assets_dev/worlds" / world / "events/maps"
        path = folder / (stem + ".lua")
        if not path.is_file():
            entry["status"] = "missing_generated_program"
            continue
        if path not in texts:
            texts[path] = read_text(path)
        generated = generated_operations(texts[path], event)
        overlays = []
        for overlay in sorted(folder.glob(stem + "_*.lua")):
            if overlay not in texts:
                texts[overlay] = read_text(overlay)
            if re.search(r"ReplaceMapEvent\(\s*" + str(event) + r"\s*[,)]", texts[overlay]):
                overlays.append(overlay.relative_to(repo).as_posix())
        entry.update(original=sorted(original), generated=sorted(generated) if generated is not None else None,
                     overlays=overlays)
        entry["status"] = ("explicit_overlay_requires_review" if overlays else
                           "missing_generated_event" if generated is None else
                           "no_condition_operations" if not original and not generated else
                           "generated_operation_set_matches" if original == generated else
                           "generated_operation_set_differs")
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--inventory", type=Path, required=True, help="Native Atlas interactions.tsv")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    with args.inventory.open(encoding="utf-8", newline="") as stream:
        entries = audit(args.repo.resolve(), csv.DictReader(stream, delimiter="\t"))
    summary = {}
    for entry in entries:
        summary[entry["status"]] = summary.get(entry["status"], 0) + 1
    report = {
        "scope": "Unique physically indexed MM7/MM8 well/drink/shrine callbacks",
        "limitation": "Operation sets are triage only; not active-overlay, branch, operand or runtime verification.",
        "summary": summary,
        "sites": entries,
    }
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, sort_keys=True))
    for entry in entries:
        if entry["status"] == "generated_operation_set_differs":
            print(entry["world"], entry["map"], entry["event"],
                  "original=", entry["original"], "generated=", entry["generated"])


if __name__ == "__main__":
    main()
