from pathlib import Path
import re
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image
import yaml

import bake_decorations


class DecorationBakeTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.assets = self.root / 'assets_dev'
        self.world = self.assets / 'worlds/mm6'
        self.output = self.root / 'masks'
        self.profile = dict(world='mm6', map='test.odm', azimuth=225)
        for directory in ('maps', 'events/maps', 'sprites'):
            (self.world / directory).mkdir(parents=True)
        (self.world / 'maps/test.odm').touch()
        (self.world / 'maps/test.scene.yml').write_text('{}')
        self.script = self.world / 'events/maps/test.lua'
        table = self.assets / 'engine/data_tables/decoration_data.txt'
        table.parent.mkdir(parents=True)
        table.write_text(
            '1\tnull\t-\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t-\n'
            '2\ttree\t-\t0\t0\t0\t0\t0\t0\t0\t0\t0\t1\t-\n'
            '3\tpending\t-\t0\t0\t0\t0\t0\t0\t0\t0\t0\t1\t-\n')
        frames = self.assets / 'engine/rendering/sprite_frame_data_common.yml'
        frames.parent.mkdir(parents=True)
        frames.write_text(yaml.safe_dump(dict(sprites=[dict(
            sprite_id=1, sprite_name='tree', frames=[dict(
                flags=['Image1'], texture_name='tree', palette_id=0, scale=1)])])))
        image = Image.new('P', (4, 8), 1)
        image.putpalette([0, 0, 0, 255, 255, 255] + [0] * 762)
        image.save(self.world / 'sprites/tree.bmp')
        self.entities = [dict(index=0, name='tree', decoration_list_id=1,
                              event_id_primary=7, ai_attributes=0, facing=0,
                              position=dict(x=0, y=0, z=0))]

    def export(self):
        with patch.object(bake_decorations, 'ROOT', self.root), patch.object(
                bake_decorations, 'parse_odm_file', return_value=dict(payload=dict(entities=self.entities))):
            return bake_decorations.export_cards(self.profile, self.output)

    def test_legacy_bytes_in_comments_and_strings_do_not_hide_sprite_calls(self):
        self.script.write_bytes(
            b'-- NPCs \xb913\n'
            b'evt.SetMessage("Hero\x92s message: evt.SetSprite(999, 0)")\n'
            b'--[[ evt.SetSprite(998, 0) \x85 ]]\n'
            b'evt.SetSprite(7, 0, "0")\n')
        result = self.export()
        self.assertEqual(result['counts'], dict(casters=0, skipped=1))
        self.assertEqual(result['skip_reasons'], dict(can_be_hidden=1))
        self.assertIn('worlds/mm6/events/maps/test.lua', result['dependencies'])

    def test_visibility_only_names_keep_native_silhouette(self):
        baseline = self.export()['cards'][0]
        for arguments in ('7, 1', '7, true, ""', '7, 1, "0"'):
            with self.subTest(arguments=arguments):
                self.script.write_text('evt.SetSprite(' + arguments + ')\n')
                result = self.export()
                self.assertEqual(result['counts'], dict(casters=1, skipped=0))
                card = result['cards'][0]
                for field in ('width', 'height', 'mask', 'sources', 'policy'):
                    self.assertEqual(card[field], baseline[field])
                self.assertEqual(card['policy'], 'static')

    def test_empty_initial_name_is_null_regardless_of_raw_descriptor(self):
        self.entities[0]['name'] = ''
        for descriptor in (1, 2, 999):
            with self.subTest(descriptor=descriptor):
                self.entities[0]['decoration_list_id'] = descriptor
                result = self.export()
                self.assertEqual(result['counts'], dict(casters=0, skipped=1))
                self.assertEqual(result['skip_reasons'], dict(marker_or_no_visible_sprite=1))

    def test_unknown_replacement_still_fails(self):
        self.script.write_text('evt.SetSprite(7, 1, "missing_tree")\n')
        with self.assertRaisesRegex(ValueError, 'Unresolved decoration or sprite state: missing_tree'):
            self.export()

    def test_reward_metadata_does_not_change_shadow_cards_or_dependencies(self):
        baseline = self.export()
        formats = [('00', '-- generated reward metadata from legacy EVT/STR; load after the base handlers'),
                   ('zz', '-- Item reward metadata; load after gameplay overlays')]
        for suffix, header in formats:
            metadata = self.world / f'events/maps/test_{suffix}_item_rewards.lua'
            for declarations in ('', 'SetMapItemRewards(1, {42, 43})\n', 'SetMapItemRewards(2, {44})\n'):
                with self.subTest(suffix=suffix, declarations=declarations):
                    metadata.write_text(header + '\n' + declarations)
                    result = self.export()
                    for field in ('cards', 'counts', 'dependencies'):
                        self.assertEqual(result[field], baseline[field])
            metadata.unlink()

    def test_reserved_reward_metadata_cannot_hide_executable_visual_changes(self):
        formats = [('00', '-- generated reward metadata from legacy EVT/STR; load after the base handlers'),
                   ('zz', '-- Item reward metadata; load after gameplay overlays')]
        for suffix, header in formats:
            metadata = self.world / f'events/maps/test_{suffix}_item_rewards.lua'
            for declaration in ('evt.SetSprite(7, 0, "0")',
                                'RegisterEvent(1, nil, function() evt.SetSprite(7, 0) end)',
                                'SetMapItemRewards(1, {42}); evt.SetSprite(7, 0)',
                                'SetMapItemRewards(65536, {42})',
                                'SetMapItemRewards(1, {4294967296})'):
                with self.subTest(suffix=suffix, declaration=declaration):
                    metadata.write_text(header + '\n' + declaration + '\n')
                    with self.assertRaisesRegex(ValueError, 'Invalid reward metadata overlay'):
                        self.export()
            metadata.unlink()

    def test_mm8_late_reward_catalog_matches_the_actual_fixed_gem_exchange_rules(self):
        maps = bake_decorations.ROOT / 'assets_dev/worlds/mm8/events/maps'
        script = (maps / 'out07_mmmerge.lua').read_text()
        table = re.search(r'local GemExchangeEntries = \{(.*?)\n\}', script, re.S)
        self.assertIsNotNone(table)
        row = r'\{\s*([1-9][0-9]*)\s*,\s*(-?[0-9]+)\s*\},?'
        exchanges = re.findall(row, table[1])
        self.assertTrue(exchanges)
        self.assertFalse(re.sub(row, '', table[1]).strip())
        expected = sorted({int(reward) for _, reward in exchanges if int(reward) > 0})
        metadata = maps / 'out07_zz_item_rewards.lua'
        text = metadata.read_text()
        bake_decorations.validate_reward_metadata(text, metadata)
        declarations = re.findall(r'SetMapItemRewards\(455, \{([^}]+)\}\)', text)
        self.assertEqual(len(declarations), 1)
        self.assertEqual([int(value) for value in declarations[0].split(', ')], expected)

    def test_well_context_catalog_is_safe_and_cannot_hide_gameplay(self):
        baseline = self.export()
        metadata = self.world / 'events/maps/test_zz_site_context.lua'
        header = '-- Site context metadata; load after gameplay overlays\n'
        metadata.write_text(header + 'SetMapContextAction(207, {kind = "well", source = "script"})\n')
        result = self.export()
        self.assertEqual(result['dependencies'], baseline['dependencies'])
        for declaration in ('SetMapContextAction(0, {kind = "well", source = "script"})',
                            'SetMapContextAction(65536, {kind = "well", source = "script"})',
                            'SetMapContextAction(1, {kind = "well", source = "script"}); evt.SetSprite(7, 0)',
                            'SetMapContextAction(1, {kind = "well", source = "script", hidden = true})'):
            metadata.write_text(header + declaration + '\n')
            with self.assertRaisesRegex(ValueError, 'Invalid site context metadata overlay'):
                self.export()

    def test_typed_interaction_references_preserve_shadow_cards_and_dependencies(self):
        baseline = self.export()
        metadata = self.world / 'events/maps/test_zz_site_interactions.lua'
        metadata.write_text('-- Site interaction metadata; load after gameplay overlays\n'
                            'SetMapInteractionReference(1, {kind = "enter_dungeon", itemId = 610, '
                            'openedMapFlag = "CrystalOpened", targetMap = "d10.blv"})\n'
                            'SetMapInteractionReference(2, {kind = "stone_to_flesh", itemId = 339, '
                            'spellId = 40, npcId = 42, spriteId = 20, consumesItem = true})\n')
        result = self.export()
        for key in ('cards', 'counts', 'dependencies'):
            self.assertEqual(result[key], baseline[key])

    def test_gate_reference_is_literal_metadata_and_rejects_mixed_effect_fields(self):
        baseline = self.export()
        metadata = self.world / 'events/maps/test_zz_site_interactions.lua'
        header = '-- Site interaction metadata; load after gameplay overlays\n'
        good = 'SetMapInteractionReference(1, {kind = "dimension_door", network = "merge.dimension_doors"})'
        metadata.write_text(header + good + '\n')
        result = self.export()
        for key in ('cards', 'counts', 'dependencies'):
            self.assertEqual(result[key], baseline[key])
        for bad in (good + '; evt.SetSprite(1, 0)', good.replace('})', ', itemId = 1})'),
                    good.replace('merge.dimension_doors', '../network'),
                    good.replace('merge.dimension_doors', 'bad..network')):
            metadata.write_text(header + bad + '\n')
            with self.assertRaisesRegex(ValueError, 'Invalid site interaction metadata overlay'):
                self.export()

    def test_typed_interaction_references_reject_unknown_fields_and_executable_content(self):
        metadata = self.world / 'events/maps/test_zz_site_interactions.lua'
        header = '-- Site interaction metadata; load after gameplay overlays\n'
        declaration = ('SetMapInteractionReference(1, {kind = "enter_dungeon", itemId = 610, '
                       'openedMapFlag = "CrystalOpened", targetMap = "d10.blv"})')
        for line in (declaration + '; evt.SetSprite(7, 0)', declaration.replace('610', 'true'),
                     declaration.replace('610', '1.5'), declaration.replace('610', '2147483648'),
                     declaration.replace('d10.blv', '../d10.blv'), declaration.replace('610', '0'),
                     declaration.replace('})', ', typo = 1})'), declaration.replace('610', 'ItemId()'),
                     declaration.replace('610', '610, itemId = 611'),
                     declaration.replace('CrystalOpened', 'Crystal/Opened')):
            metadata.write_text(header + line + '\n')
            with self.assertRaisesRegex(ValueError, 'Invalid site interaction metadata overlay'):
                self.export()

    def test_passage_groups_do_not_change_shadow_cards_or_dependencies(self):
        baseline = self.export()
        metadata = self.world / 'events/maps/test_zz_site_groups.lua'
        metadata.write_text('-- Site group metadata; load after gameplay overlays\n'
                            'SetMapSiteGroups(1, {{"bmodel/0/surface/1", "bmodel/1/surface/0"}})\n')
        result = self.export()
        for field in ('cards', 'counts', 'dependencies'):
            self.assertEqual(result[field], baseline[field])

    def test_passage_groups_reject_executable_or_ambiguous_metadata(self):
        metadata = self.world / 'events/maps/test_zz_site_groups.lua'
        header = '-- Site group metadata; load after gameplay overlays\n'
        valid = 'SetMapSiteGroups(1, {{"surface/0", "surface/1"}})'
        for line in (valid + '; evt.SetSprite(1, 0)', valid.replace('1,', '65536,'),
                     valid.replace('"surface/1"', '"surface/0"'),
                     valid.replace('"surface/1"', 'SiteId()'),
                     valid.replace('"surface/1"', '"../surface/1"'),
                     valid.replace('"surface/1"', '"surface/01"'),
                     valid.replace('"surface/1"', '"surface/4294967296"'),
                     'SetMapSiteGroups(1, {{"surface/0"}})', valid + '\n' + valid):
            metadata.write_text(header + line + '\n')
            with self.assertRaisesRegex(ValueError, 'Invalid site group metadata overlay'):
                self.export()

    def test_site_references_do_not_change_shadow_cards_or_dependencies(self):
        baseline = self.export()
        for suffix, header in [
                ('_00_site_refs.lua', '-- generated site references from legacy EVT/STR; '
                 'load after the base handlers\n'),
                ('_zz_site_refs.lua', '-- Site reference metadata; load after gameplay overlays\n')]:
            metadata = self.world / ('events/maps/test' + suffix)
            for declaration in ('', 'SetMapConnectionTargets(1, {"a.blv", "b.odm"})\n',
                                'SetMapSiteReference(1, "")\n',
                                'SetMapSiteReference(1, "", "drink")\n',
                                'SetMapSiteReference(1, "Gold +200\\nRequirements: below 199")\n'):
                metadata.write_text(header + declaration)
                result = self.export()
                for field in ('cards', 'counts', 'dependencies'):
                    self.assertEqual(result[field], baseline[field])
            metadata.unlink()

    def test_site_reference_dependencies_are_literal_distinct_bounded_callbacks(self):
        script = Path('test_00_site_refs.lua')
        header = '-- generated site references from legacy EVT/STR; load after the base handlers\n'
        for declaration in ('SetMapSiteReference(10, "Lever", nil, {11, 12})',
                            'SetMapSiteReference(10, "", "drink", {})'):
            bake_decorations.validate_site_reference_metadata(header + declaration, script)
        for declaration in ('SetMapSiteReference(10, "Lever", nil, {10})',
                            'SetMapSiteReference(10, "Lever", nil, {11, 11})',
                            'SetMapSiteReference(10, "Lever", nil, {0})',
                            'SetMapSiteReference(10, "Lever", nil, {65536})',
                            'SetMapSiteReference(10, "Lever", nil, {callback})',
                            'SetMapSiteReference(10, "Lever", nil, {11}); evt.Set(1, 2)',
                            'SetMapSiteReference(10, "Lever", nil, {'
                            + ', '.join(str(value) for value in range(100, 165)) + '})'):
            with self.subTest(declaration=declaration), self.assertRaises(ValueError):
                bake_decorations.validate_site_reference_metadata(header + declaration, script)

    def test_connection_targets_cannot_hide_executable_or_invalid_content(self):
        metadata = self.world / 'events/maps/test_00_site_refs.lua'
        header = '-- generated site references from legacy EVT/STR; load after the base handlers\n'
        for declaration in ('SetMapConnectionTargets(1, {"../a.blv"})',
                            'SetMapConnectionTargets(1, {"A.BLV"})',
                            'SetMapConnectionTargets(1, {"a.blv", "a.blv"})',
                            'SetMapConnectionTargets(65536, {"a.blv"})',
                            'SetMapConnectionTargets(1, {"a.blv"}); evt.MoveToMap(0)',
                            'SetMapConnectionTargets(1, {name})'):
            metadata.write_text(header + declaration + '\n')
            with self.assertRaises(ValueError):
                self.export()

    def test_site_reference_cannot_hide_executable_or_invalid_content(self):
        metadata = self.world / 'events/maps/test_00_site_refs.lua'
        header = '-- generated site references from legacy EVT/STR; load after the base handlers\n'
        for declaration in ('SetMapSiteReference(1, "text"); evt.SetSprite(7, 0)\n',
                            'SetMapSiteReference(1, "text", "drink"); evt.SetSprite(7, 0)\n',
                            'SetMapSiteReference(1, "text", "teleport")\n',
                            'SetMapSiteReference(1, "text", 1)\n',
                            'SetMapSiteReference(65536, "text")\n',
                            'SetMapSiteReference(0, "text")\n',
                            'SetMapSiteReference(1, string.rep("x", 3))\n',
                            'SetMapSiteReference(1, "a" .. "b")\n',
                            'SetMapSiteReference(1, "' + 'x' * 16385 + '")\n',
                            'SetMapSiteReference(1, "bad\\u0000")\n'):
            metadata.write_text(header + declaration)
            with self.assertRaisesRegex(ValueError, 'Invalid site reference metadata overlay'):
                self.export()

    def test_npc_catalogs_do_not_change_shadow_cards_or_dependencies(self):
        baseline = self.export()
        for suffix, header in [
                ('_00_npc_locations.lua', '-- generated possible NPC locations from legacy EVT/STR; '
                 'load after the base handlers\n'),
                ('_zz_npc_locations.lua', '-- NPC location metadata; load after gameplay overlays\n')]:
            metadata = self.world / ('events/maps/test' + suffix)
            for declaration in ('', 'SetMapNpcLocations(1, {{42, 11}, {43, 12}})\n'):
                metadata.write_text(header + declaration)
                result = self.export()
                for field in ('cards', 'counts', 'dependencies'):
                    self.assertEqual(result[field], baseline[field])
            metadata.unlink()

    def test_npc_catalog_cannot_hide_executable_or_invalid_declarations(self):
        metadata = self.world / 'events/maps/test_zz_npc_locations.lua'
        header = '-- NPC location metadata; load after gameplay overlays\n'
        for content in ('evt.SetSprite(7, 0)\n',
                        header + 'SetMapNpcLocations(1, {{42, 11}}); evt.SetSprite(7, 0)\n',
                        header + 'RegisterEvent(1, nil, function() end)\n',
                        header + 'SetMapNpcLocations(1, {{42, 0}})\n',
                        header + 'SetMapNpcLocations(65536, {{42, 11}})\n',
                        header + 'SetMapNpcLocations(1, {{4294967296, 11}})\n'):
            with self.subTest(content=content):
                metadata.write_text(content)
                with self.assertRaisesRegex(ValueError, 'Invalid NPC location metadata overlay'):
                    self.export()

    def test_journal_catalogs_do_not_change_shadow_cards_or_dependencies(self):
        baseline = self.export()
        for suffix, header in [
                ('_00_autonote_refs.lua', '-- generated journal references from legacy EVT/STR; '
                 'load after the base handlers\n'),
                ('_zz_autonote_refs.lua', '-- Journal reference metadata; load after gameplay overlays\n')]:
            metadata = self.world / ('events/maps/test' + suffix)
            for declaration in ('', 'SetMapAutonoteRefs(1, {42, 43})\n', 'SetMapAutonoteRefs(1, {})\n'):
                metadata.write_text(header + declaration)
                result = self.export()
                for field in ('cards', 'counts', 'dependencies'):
                    self.assertEqual(result[field], baseline[field])
            metadata.unlink()

    def test_journal_catalog_cannot_hide_executable_or_invalid_declarations(self):
        metadata = self.world / 'events/maps/test_zz_autonote_refs.lua'
        header = '-- Journal reference metadata; load after gameplay overlays\n'
        for content in ('evt.SetSprite(7, 0)\n',
                        header + 'SetMapAutonoteRefs(1, {42}); evt.SetSprite(7, 0)\n',
                        header + 'RegisterEvent(1, nil, function() end)\n',
                        header + 'SetMapAutonoteRefs(1, {0})\n',
                        header + 'SetMapAutonoteRefs(65536, {42})\n',
                        header + 'SetMapAutonoteRefs(1, {65536})\n'):
            with self.subTest(content=content):
                metadata.write_text(content)
                with self.assertRaisesRegex(ValueError, 'Invalid journal reference metadata overlay'):
                    self.export()


if __name__ == '__main__':
    unittest.main()
