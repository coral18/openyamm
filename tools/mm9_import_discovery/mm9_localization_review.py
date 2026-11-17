"""Context-aware, source-bound review of ambiguous retail translations. Reports stay outside runtime packs."""
from __future__ import annotations

import collections
import hashlib
import json

import l10n


PARTY_CONTEXT = "said by the party"


def review_key(context: str, english: str) -> str:
    return hashlib.sha256(json.dumps([context, english], ensure_ascii=False).encode("utf-8")).hexdigest()[:24]


def case_digest(case: dict) -> str:
    bound = {name: case[name] for name in ("context", "english", "current", "candidates", "protected_human")}
    bound["sources"] = sorted(case["sources"], key=lambda source: json.dumps(source, sort_keys=True))
    return hashlib.sha256(json.dumps(bound, ensure_ascii=False, sort_keys=True).encode("utf-8")).hexdigest()


def prepare_review(catalog: str, pairs: dict, origins: list[dict], review: dict | None = None,
                   language: str = "pl") -> tuple[dict, dict, set, list[dict]]:
    _, entries = l10n.parse_po(catalog)
    by_source = collections.defaultdict(list)
    for origin in origins:
        by_source[origin["source"]].append(origin)
    party_sources = {l10n._mm9_normalized(e.msgid) for e in entries if e.context == PARTY_CONTEXT and not e.obsolete}
    candidates, cases = {}, []
    for entry in entries:
        if entry.obsolete or entry.plural is not None:
            continue
        source = l10n._mm9_normalized(entry.msgid)
        variants = pairs.get(source, set())
        uses = by_source[source]
        if entry.context == PARTY_CONTEXT:
            uses = [origin for origin in uses if origin["role"] == "party"]
            values = {origin["translation"] for origin in uses}
        elif not entry.context and source in party_sources:
            uses = [origin for origin in uses if origin["role"] != "party"]
            values = {origin["translation"] for origin in uses}
        else:
            values = set(variants)
        candidates[(entry.context, source)] = values
        if len(variants) < 2:
            continue
        case = {"id": review_key(entry.context, entry.msgid), "context": entry.context, "english": entry.msgid,
                "current": entry.msgstr[0] if entry.msgstr else "", "candidates": sorted(values),
                "protected_human": entry.translated() and "machine translation" not in " ".join(entry.translator),
                "sources": uses, "all_variants": sorted(variants),
                "status": "context-resolved" if len(values) == 1 else "unreviewed"}
        case["source_digest"] = case_digest(case)
        cases.append(case)
    by_id = {case["id"]: case for case in cases}
    selections, retained = {}, set()
    if review is not None:
        if not isinstance(review, dict) or review.get("schema") != 1 or review.get("language") != language:
            raise ValueError("review must use schema 1 and match the imported language")
        if not isinstance(review.get("decisions"), list):
            raise ValueError("review decisions must be a list")
        seen = set()
        for decision in review["decisions"]:
            if not isinstance(decision, dict) or not isinstance(decision.get("id"), str):
                raise ValueError("each review decision must be an object with a string id")
            identifier = decision["id"]
            if identifier in seen:
                raise ValueError(f"duplicate review decision: {identifier}")
            seen.add(identifier)
            case = by_id.get(identifier)
            if case is None or case["source_digest"] != decision.get("source_digest"):
                raise ValueError(f"review is stale or names an unknown source: {identifier}; review this case again")
            if not isinstance(decision.get("reason"), str) or not decision["reason"].strip():
                raise ValueError(f"review decision needs a reason: {identifier}")
            key = (case["context"], l10n._mm9_normalized(case["english"]))
            if decision["action"] == "select":
                translation = decision["translation"]
                if translation not in case["candidates"]:
                    raise ValueError(f"selected translation is not a candidate in this speaker context: {identifier}")
                if l10n.mismatch(case["english"], translation):
                    raise ValueError(f"selected translation changes placeholders or codes: {identifier}")
                if case["protected_human"] and translation != case["current"]:
                    raise ValueError(f"review cannot replace an existing human translation: {identifier}; keep it")
                selections[key] = translation
                case["status"] = "reviewed-selection"
                case["selected"] = translation
            elif decision["action"] == "keep":
                retained.add(key)
                case["status"] = "reviewed-kept"
            else:
                raise ValueError(f"unknown review action: {decision['action']}")
            case["reason"] = decision["reason"]
    return candidates, selections, retained, cases
