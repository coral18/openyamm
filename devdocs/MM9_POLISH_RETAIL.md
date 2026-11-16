# Polish MM9 retail localization

`tools/mm9_import_discovery/import_mm9_localization.py` builds a local language overlay from an owned installation of
MM9. It uses the existing [language pack mechanism](LOCALIZATION.md), without replacing English gameplay tables,
RUDE graphs, sound ids or base content ZIPs. No retail recordings or translated catalogs are included in this repository.

## Import

Requires Python 3.10+ and an installed desktop OpenYAMM content index with MM9 and Polish text catalogs. Run from the
repository root, choosing a fresh output folder outside the original game installation:

```sh
python3 tools/mm9_import_discovery/import_mm9_localization.py \
  --installation '/mnt/c/Program Files (x86)/3DO/Might and Magic IX' \
  --assets /path/to/openyamm/assets \
  --output output/mm9-polish/languages/pl \
  --report output/mm9-polish/import-report.json
```

`--language pl --encoding cp1250` are the defaults. The importer reads the source archives in place and does not run
the original executables. It selects content through `assets/index.json`, avoiding stale, unindexed pack versions.
An identical rerun keeps existing output files; a different existing file causes an error instead of overwriting it.

Install the generated `languages/pl/` folder under the OpenYAMM **user data directory**, alongside `settings.ini`.
Select Polish and restart. With an ordinary portable release the user data directory is the game directory; a launch
using `--user-data` must use that directory instead. Existing `languages/pl/` files should be backed up and checked
before merging. Remove the added files to revert to the shipped localization. Saves do not need conversion.

The player's catalogs must override equally named shipped catalogs. The accompanying `AssetFileSystem` correction
prioritizes the explicit `languages/` namespace ahead of the package search; older binaries without that correction
may mount local audio correctly while continuing to use the shipped text. This does not change other package searches.

## Audio

- Exact paths from the indexed MM9 audio packs determine which original recordings to install. PC voice directory
  names follow the existing PC voice importer, including removal of spaces. NPC and cinema subfolders retain their
  identity. Colliding normalized paths fail.
- Original PCM WAV bytes are preserved, with no lossy conversion or FFmpeg dependency. Each output has its source
  entry, size and SHA-256 recorded in the external audit. Source archive hashes are recorded too.
- A missing active voice fails the import. Missing assets referenced exclusively by `n/u` speech rows are reported
  separately; the importer does not substitute unrelated dialogue.
- An empty original recording is reported and omitted. The base English asset remains available for that gap.
- `VOICES/CINEMA` contains audio for the native MM9 scenes. Original BIK movie files are not converted by this tool.

Checked against the Polish installation supplied on 2026-11-16 and the local OpenYAMM 2.5 asset index:

| Content | Result |
|---|---:|
| Indexed MM9 recordings | 1790 |
| Nonempty original localized recordings installed | 1777 |
| Unavailable unused `ShutUp` slots | 11 |
| Empty original NPC recordings, left to English base assets | 2 |

The empty files are `NPC_012B` and `NPC_099B`. These are limitations of the inspected retail assets, not inferred
Polish dialogue. One Angry Female greeting was independently identified by the user as Polish speech; the remaining
recordings originate from the same localized archive but have not all been manually listened to.

## Text

The release retains the original English RUDE data. The importer pairs it with the localized `Rude.rez`, using every
non-text field as row identity, including choice slots, destinations, conditions and actions. Row order alone never
determines a match. NPC names and top blurbs use their own text columns and identities.

Only unambiguous translations preserving the original placeholders/control codes replace machine drafts. Existing
different human translations are kept. The complete base catalog remains in the output, preserving contexts,
unmatched entries, new OpenYAMM content and English fallback. Gameplay graphs remain untouched.

For the inspected 2.5 installation:

| Text result | Entries |
|---|---:|
| Catalog entries matching an unambiguous official translation | 4982 |
| Actual translation changes | 4087 |
| Ambiguous matches kept as shipped | 285 |
| Different human translation kept | 1 |
| Entries without a matching source text | 1861 |

441 original RUDE files were matched, with 5326 matching rows and 57 unmatched rows. One additional runtime dialogue
file has no retail counterpart. The 47 fuzzy entries in the shipped catalog remain fuzzy. This is an official-dialogue
supplement, not a claim that every MM9 text has been replaced: original `Data.rez` tables/books need an original
English reference before they can be imported safely.

## Validation and distribution

Run the importer tests with:

```sh
python3 -m unittest discover -s tools/mm9_import_discovery -p test_import_mm9_localization.py
```

The `AssetFileSystemTests.cpp` regression covers same-named local/shipped catalogs, remaining shipped text and English
behavior. Runtime verification should mount the overlay with MM6, MM7, MM8 and MM9 active, since MM9 character voices
can be used across worlds, and decode the actual WAVs with the game's SDL decoder.

Local Windows verification on 2026-11-16 passed: eight importer unit tests, the existing audio-import test, 15 focused
C++ localization/pack cases (204 assertions), decoding all 1777 WAVs through the engine's native SDL library, and
eight real-content mount checks (four worlds, English/Polish). A native OpenGL run reached Bootcamp gameplay and
captured the result without audio decoding errors. Manual listening of the complete voice archive remains outstanding.
A broader Windows run had 25 failures involving unavailable loose development audio/table fixtures and pre-existing
case-sensitive filesystem assumptions; these are not counted as passing validation.

Public downloads can use OpenYAMM's existing optional language packs: `assets_dev/languages/pl/worlds/mm9/audio/`
becomes `lang-pl.mm9.audio.*`, indexed with `language: pl`. The official release publisher would have to include
the content in its own build and signed manifest. A local folder alone does not register a new download with the
official updater. See [Releases](../packaging/RELEASE.md) and [Incremental updates](../packaging/UPDATER_PLAN.md).

Original retail assets are separate from the OpenYAMM code license. Confirm permission to redistribute the recordings
and translations before hosting them publicly. Publishing this importer lets owners create their own overlay locally.
