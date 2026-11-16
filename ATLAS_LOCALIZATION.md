# Atlas localization: English and Polish

The native MM7/MM8 Atlas on the OpenYAMM 2.5 base uses the upstream language-pack system. English remains the
source text and fallback; Polish additions live in `assets_dev/languages/pl/text/engine-atlas.po`.

## Scope and ownership

- Atlas UI, guidebook sections, map observations, notes, training/travel information, item comparison and class
  progression text use the existing `tr`, `trn` and data-localization APIs.
- Item, NPC, monster, map, class, skill, spell, quest and recipe names/descriptions use the active game's catalogs.
  Original tables, canonical IDs, event keys, player names and personal note text stay unchanged.
- Monster spell aliases resolve through the loaded spell table before their display names are translated.
- Searches compare Unicode-lowercased displayed text, preserving discovery filtering. Sorting uses displayed labels.
- Generated event sidecars stay English and remain owned by the exporter. Their complete reading paragraphs are
  translated in PO, without rewriting Lua or adding runtime text-replacement rules.
- Long button labels fit inside one line of their own plate. Polish uses the official upstream font pack.

This addition contains **640 entries / 8,407 English source words**, including **124 unique generated paragraphs**.
The count includes shared skill display terms and item-category labels. The game's much larger existing Polish
catalogs provide names and story content; they are not copied into this file. New or untranslated upstream/mod
content uses the normal English fallback. This does not claim to complete every translation in upstream itself.

English needs no separate translation file. To use Polish, install the official 2.5 Polish language packs and put
this branch's `languages/pl/text/engine-atlas.po` under the user-data directory's `languages/` folder. Select Polish
in Settings or set `[gameplay] language=pl`, then restart the game. English uses `language=en`.

## Checks

```sh
python3 tools/atlas_l10n.py --catalog assets_dev/languages/pl/text/engine-atlas.po
python3 tools/atlas_l10n.py --template output/atlas-language/engine-atlas.pot --json output/atlas-language/inventory.json
```

The inventory extends the upstream extractor with Atlas-owned display sources, generated site paragraphs and the
item-category column actually presented by the Atlas. It reports missing/fuzzy/empty entries and placeholder
mismatches. `openyamm_unit_tests` additionally validates each translation with the engine's actual PO/format parser.

Focused coverage:

- **91 cases / 5,001 assertions**: Atlas regressions, the engine localization parser and five Atlas language tests.
  New tests cover Polish text and plurals, UTF-8 searches, discovery privacy, stable IDs, original item data,
  untranslated future-text fallback and preservation of player-authored names and notes.
- Installed-pack and geometry audits pass for both English and Polish: **138 event programs / 137 installed maps**
  per language, **626 assertions** per audit. The reserved map row remains explicitly excluded.
- Native Windows GPU tours cover MM7/MM8 in English and Polish, including Discovery/Guide, item art and descriptions,
  alchemy, quests, class caps, Polish skill searches in the comparison grid and bestiary spell names. Polish font atlases report no missing glyphs in these runs.
- Android and other languages have not been visually accepted in this localization stage.

## What this measures for the next language

The display hooks, canonical-ID protection, extraction, packaging and language regression tests are shared work.
A new language primarily needs translations/review of the inventory, reuse of that language's upstream game terms,
then a visual/search pass in both worlds. Do not estimate each language as another engine port. The 124 generated
paragraphs repeat a smaller set of interaction patterns; a translator can reuse terminology, but each complete
entry still needs checking. Other scripts and fonts can require more layout work than Polish.
