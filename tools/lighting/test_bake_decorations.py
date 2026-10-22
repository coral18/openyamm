from pathlib import Path
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

    def test_generated_reward_metadata_does_not_change_shadow_cards_or_dependencies(self):
        baseline = self.export()
        metadata = self.world / 'events/maps/test_00_item_rewards.lua'
        header = '-- generated reward metadata from legacy EVT/STR; load after the base handlers\n'
        for declarations in ('', 'SetMapItemRewards(1, {42, 43})\n', 'SetMapItemRewards(2, {44})\n'):
            with self.subTest(declarations=declarations):
                metadata.write_text(header + declarations)
                result = self.export()
                for field in ('cards', 'counts', 'dependencies'):
                    self.assertEqual(result[field], baseline[field])

    def test_reserved_reward_metadata_cannot_hide_executable_visual_changes(self):
        metadata = self.world / 'events/maps/test_00_item_rewards.lua'
        header = '-- generated reward metadata from legacy EVT/STR; load after the base handlers\n'
        for content in ('evt.SetSprite(7, 0, "0")\n',
                        header + 'RegisterEvent(1, nil, function() evt.SetSprite(7, 0) end)\n',
                        header + 'SetMapItemRewards(1, {42}); evt.SetSprite(7, 0)\n'):
            with self.subTest(content=content):
                metadata.write_text(content)
                with self.assertRaisesRegex(ValueError, 'Invalid generated reward metadata overlay'):
                    self.export()

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

    def test_site_references_do_not_change_shadow_cards_or_dependencies(self):
        baseline = self.export()
        for suffix, header in [
                ('_00_site_refs.lua', '-- generated site references from legacy EVT/STR; '
                 'load after the base handlers\n'),
                ('_zz_site_refs.lua', '-- Site reference metadata; load after gameplay overlays\n')]:
            metadata = self.world / ('events/maps/test' + suffix)
            for declaration in ('', 'SetMapSiteReference(1, "")\n',
                                'SetMapSiteReference(1, "Gold +200\\nRequirements: below 199")\n'):
                metadata.write_text(header + declaration)
                result = self.export()
                for field in ('cards', 'counts', 'dependencies'):
                    self.assertEqual(result[field], baseline[field])
            metadata.unlink()

    def test_site_reference_cannot_hide_executable_or_invalid_content(self):
        metadata = self.world / 'events/maps/test_00_site_refs.lua'
        header = '-- generated site references from legacy EVT/STR; load after the base handlers\n'
        for declaration in ('SetMapSiteReference(1, "text"); evt.SetSprite(7, 0)\n',
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
