import importlib.util
import tempfile
import unittest
from pathlib import Path


spec = importlib.util.spec_from_file_location(
    "atlas_condition_sources", Path(__file__).resolve().parents[1] / "tools/audit_atlas_condition_sources.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class ConditionSourceAuditTests(unittest.TestCase):
    def test_original_isolates_event_and_normalizes_main_condition(self):
        text = ('event 1\n 0: Set {"PoisonedRed", Value = 0}\nend\n'
                'event 2\n 0: Cmp {"MajorCondition", Value = 0}\nend\n')
        self.assertEqual(audit.original_operations(text, 1), {("Set", "PoisonedRed")})
        self.assertEqual(audit.original_operations(text, 2), {("Cmp", "MainCondition")})
        self.assertIsNone(audit.original_operations(text, 3))

    def test_generated_does_not_read_following_event_or_non_condition_variable(self):
        text = ('RegisterEvent(1, nil, function()\nSetValue(BaseEndurance, 35)\nend)\n'
                'RegisterEvent(2, nil, function()\nSetValue(Paralyzed, 0)\nend)\n')
        self.assertEqual(audit.generated_operations(text, 1), set())
        self.assertEqual(audit.generated_operations(text, 2), {("Set", "Paralyzed")})
        self.assertIsNone(audit.generated_operations(text, 3))

    def test_inventory_reports_mismatch_then_overlay_review_without_certifying_it(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            reference = repo / "reference/mmext-scripts/Decompiled_Scripts/mm7/txt"
            source = repo / "assets_dev/worlds/mm7/events/maps"
            reference.mkdir(parents=True)
            source.mkdir(parents=True)
            (reference / "D12.txt").write_text('event 451\n 0: Set {"PoisonedRed", Value = 0}\nend\n')
            (source / "7d12.lua").write_text('RegisterEvent(451, nil, function()\nSetValue(Paralyzed, 0)\nend)')
            site = dict(world="mm7", map="7d12.blv", event="451", kind="drink")
            rows = [site, site, dict(site, kind="enter_dungeon", event="1")]
            result = audit.audit(repo, rows)
            self.assertEqual(len(result), 1)
            self.assertEqual(result[0]["status"], "generated_operation_set_differs")
            # Also detect a condition accidentally introduced by interpreting a non-condition selector.
            (reference / "D12.txt").write_text('event 451\n 0: Set {"BaseEndurance", Value = 35}\nend\n')
            self.assertEqual(audit.audit(repo, rows)[0]["status"], "generated_operation_set_differs")
            (source / "7d12_zz_repair.lua").write_text('ReplaceMapEvent(451, nil, function() end)')
            result = audit.audit(repo, rows)
            self.assertEqual(result[0]["status"], "explicit_overlay_requires_review")

    def test_missing_reference_is_reported_instead_of_counted_as_a_match(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            (repo / "reference/mmext-scripts/Decompiled_Scripts/mm8/txt").mkdir(parents=True)
            result = audit.audit(repo, [dict(world="mm8", map="out02.odm", event="102", kind="well")])
            self.assertEqual(result[0]["status"], "missing_or_ambiguous_source")


if __name__ == "__main__":
    unittest.main()
