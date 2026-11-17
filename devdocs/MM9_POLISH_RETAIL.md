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
  --report output/mm9-polish/import-report.json --videos
```

`--language pl --encoding cp1250` are the defaults. The importer reads the source archives in place and does not run
the original executables. It selects content through `assets/index.json`, avoiding stale, unindexed pack versions.
An identical rerun keeps existing output files; a different existing file causes an error instead of overwriting it.
`--videos` additionally needs Bash, FFmpeg and ffprobe. Omit it to import only audio and text. Movie conversions use
the existing `tools/transcode_bik.sh`; their cache beside the report is keyed by source content, converter script and
FFmpeg version, and verified by SHA-256 before reuse. Repeated imports do not download assets or re-encode movies.

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
- `VOICES/CINEMA` contains audio for the native MM9 scenes. Optional BIK conversion is described below.

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
| Catalog entries matching official text or an adapted current effect description | 5914 |
| Actual translation changes | 4896 |
| Reviewed ambiguous cases | 297 |
| Selected retail variants after review | 193 |
| Existing translations retained after review | 104 |
| Different human translation kept | 1 |
| Entries without a matching source text | 1110 |

441 original RUDE files were matched, with 5326 matching rows and 57 unmatched rows. One additional runtime dialogue
file has no retail counterpart.

The runtime item table retains explicit `source_item_id` values. Joining these to `Data.rez/MASTERITEMLIST.csv`
provides 368 original item records; joining readable item ids through that table provides all 115 books and letters
from `BOOK AND SCROLL TEXT.csv`. Names and ordinary descriptions are imported without needing a loose English retail
DATA extraction. Runtime item ownership and duplicate ids are checked. The additional names/books import changes 573
translations; adapted effect descriptions add 50 more, for 623 changes relative to the first dialogue-only overlay,
without losing any previously imported official translation.

The importer preserves 99 items without a retail counterpart and 23 reworked skill-learning items. Old MM9 effects
must not replace OpenYAMM's updated mechanics. For Polish, `mm9_polish_effects.py` combines the official narrative with
a translation of the **current** English effect list. Its explicit vocabulary retains all numbers, signs, chances,
Grandmaster conditions, party-wide scope and set bonuses. An unknown phrase or missing retail effect marker is refused.
The numbers and their order must match exactly. Adapted entries have a separate provenance comment in the PO file.

This adapts 50 effect descriptions, including all 47 previously fuzzy and both empty entries, leaving the catalog with
zero fuzzy and zero empty entries. The other 13 effect descriptions retain their existing translations. This is a
coverage result, not a claim that every translation has been manually proofread. The 1110 entries without a matching
retail source generally retain existing translations. Unresolved ambiguous matches retain the shipped text unless a
private review selects a verified variant.

## Contextual review

The existing runtime context `said by the party` distinguishes a party prompt from an NPC response with the same
English text. The importer now applies that distinction to candidate selection too. It changes neither RUDE rows nor
the runtime localization API. All 297 originally ambiguous cases are reported under `review_cases` with their
candidates, current translation and source file/row, speaker role or original item id.

An optional private review file can select a candidate or explicitly keep the existing text:

```sh
python3 tools/mm9_import_discovery/import_mm9_localization.py \
  --installation /path/to/owned-mm9 --assets /path/to/openyamm/assets \
  --output output/mm9-polish/languages/pl --report output/mm9-polish/import-report.json \
  --videos --review /path/to/private-review-pl.json
```

The review JSON uses `schema: 1`, `language: pl` and a `decisions` list. Each decision supplies its case's `id` and
`source_digest` from the audit, an `action` (`select` or `keep`) and a reason; `select` also supplies a `translation`
from that context's candidates. The digest binds the English text, current translation, candidates, protected human
status and source provenance. Changed input, duplicate decisions, wrong speaker context, malformed entries and
placeholder/code changes fail rather than reusing an outdated selection. Human translations and their attribution
are preserved. Review files and reports can contain retail text, so they remain outside Git and the mounted pack.

The local 297-case review chose 193 retail variants and retained 104 current translations, producing 186 further
translation changes. It preserves source-consistent wording where retail variants name a different NPC, omit a
requirement or disagree on directions. Some gendered lines and location-specific directions cannot be represented
perfectly by a single global translation; finer per-NPC contexts remain a separate design decision. The current
review does not invent such an API or claim that every line has been manually proofread by a person.

## Movies

Three paths are declared explicitly, matching the existing runtime packs:

| Retail source | Language overlay path under `worlds/mm9/videos/` |
|---|---|
| `Movies/Intro.bik` | `Cutscenes/MM9Intro.ogv` |
| `Movies/Shipwreck.bik` | `Cutscenes/MM9Shipwreck.ogv` |
| `Movies/LoseGame.bik` | `Cutscenes/MM9LoseGame.ogv` |

The inspected Polish installation also has `Intro_E.bik`. Its audio envelope matches the English runtime intro
(correlation 0.99972); the default `Intro.bik` has a different soundtrack (0.52356). The importer uses the default
localized movie, never the `_E` alternative. Logos are not game cutscene replacements. Movie language still warrants
manual listening; waveform comparison alone does not identify the spoken language.

The output is Theora/Vorbis OGV, retaining source resolution, frame rate, stereo audio and 44100 Hz sample rate.
Inspected durations are 96.733, 40.467 and 25.667 seconds. The three files add about 46.4 MiB to the private overlay;
they are not stored in Git. Source and output hashes are included in the audit.

## Validation and distribution

Run the importer tests with:

```sh
python3 -m unittest discover -s tools/mm9_import_discovery -p test_import_mm9_localization.py
```

The `AssetFileSystemTests.cpp` regression covers same-named local/shipped catalogs, remaining shipped text and English
behavior. Runtime verification should mount the overlay with MM6, MM7, MM8 and MM9 active, since MM9 character voices
can be used across worlds, and decode the actual WAVs with the game's SDL decoder.

Local Windows verification on 2026-11-16/17 passed: 14 importer unit tests, the existing audio-import test, 15 focused
C++ localization/pack cases (204 assertions), decoding all 1777 WAVs through the engine's native SDL library, and
eight real-content mount checks (four worlds, English/Polish). A native OpenGL run reached Bootcamp gameplay and
captured the result without audio decoding errors. Manual listening of the complete voice archive remains outstanding.
The follow-up fully decoded all three converted movies with FFmpeg, started each through the native game video/audio
decoder, verified English/Polish movie selection in all four worlds, and exercised movie-cache reuse and repair.
The review was also checked through the native engine in all eight world/language combinations: 2376 lookups for the
297 reviewed entries, alongside the existing 400 checks of adapted item effect descriptions.
A broader Windows run had 25 failures involving unavailable loose development audio/table fixtures and pre-existing
case-sensitive filesystem assumptions; these are not counted as passing validation.

Public downloads can use OpenYAMM's existing optional language packs: `assets_dev/languages/pl/worlds/mm9/audio/`
becomes `lang-pl.mm9.audio.*`, indexed with `language: pl`. The official release publisher would have to include
the content in its own build and signed manifest. A local folder alone does not register a new download with the
official updater. See [Releases](../packaging/RELEASE.md) and [Incremental updates](../packaging/UPDATER_PLAN.md).

Original retail assets are separate from the OpenYAMM code license. Confirm permission to redistribute the recordings
and translations before hosting them publicly. Publishing this importer lets owners create their own overlay locally.
