# MM6 Atlas pilot: New Sorpigal

This is a bounded addition to the Atlas on the OpenYAMM 2.5 base. It exposes **New Sorpigal, Goblinwatch and the
Abandoned Temple**. The other MM6 maps are deliberately excluded from navigation and place search until verified.
It is not a complete MM6 Atlas release.

## Content

- Regional artwork comes from the installed MM6 icon package. The Enroth overview uses the existing native region
  cards; no outside interactive-map site, browser, artwork or guide text is required.
- Dungeon plans come from the installed BLV geometry, through the same floor-plan renderer used by MM7/MM8.
- Buildings, trainers, residents, travel schedules and requirements use the active merged tables and map scripts.
  Eleven exporter-owned metadata sidecars describe the three maps without replacing their gameplay handlers.
- The shared journal relationships cover Sulman's letter, the Goblinwatch combination, Andover's candelabra and
  Angela's rescue. Locations named by an accepted quest are marked as journal information; Angela's exact encounter
  is Guide information. These relationships add no quest state or rewards.
- Discovery uses existing party observations and map exploration. Entering New Sorpigal does not reveal its entire
  map, trainers or dungeons. Guide exposes the pilot's authored locations without recording discoveries.
- English and Polish use the existing localization system and official language packs. Three new complete site
  descriptions are included in the Atlas Polish supplement.

The Map Book offers Atlas in MM6, alongside the existing MM7/MM8 support. The three world buttons fit within the
existing header. Excluded maps also hide their residents and services from Atlas search. These changes do not add
gameplay rules, alter canonical IDs, change saves or require another engine.

## Build and stage

Use the build instructions in [ATLAS_PORT.md](ATLAS_PORT.md), then stage with
`python3 tools/stage_atlas_overlay.py --output output/atlas-overlay`.
The overlay includes the MM6 presentation, the eleven map sidecars, shared quest relationships and Polish text.
Install it in an isolated user-data directory with an Atlas-enabled executable; a stock executable does not acquire
the native screen from these files alone.

`tools/regenerate_lua_events.sh --atlas-metadata` regenerates the MM6 pilot together with the existing MM7/MM8
metadata. Its MM6 inputs are the canonical legacy events and
`reference/mmext-scripts/Decompiled_Scripts/mm6/txt`; only `oute3`, `6d01` and `6d02` are selected. The default
non-metadata regeneration remains unchanged. Do not hand-edit generated sidecars.

## Verification

`Atlas MM6 pilot:*` in `openyamm_unit_tests` uses the installed 2.5 packages and the staged overlays, selected with
`OPENYAMM_ATLAS_ASSET_ROOT`, `OPENYAMM_ATLAS_MODS_ROOT`, `OPENYAMM_ATLAS_LANGUAGES_ROOT` and
`OPENYAMM_ATLAS_LANGUAGE`. It checks all three maps against actual geometry and event programs, map hierarchy,
trainers, transport, journal relationships, package-qualified artwork and Discovery filtering.

Native Windows tours cover the regional Discovery/Guide views, a dungeon, trainers, a quest and return to gameplay
in English and Polish. The isolated campaign also passes new game, save and load. MM7/MM8 installed-map audits and
the shared Atlas/localization regression suite are retained. This pilot has not been tested on Android.

The shared regression suite passes 91 cases / 5,021 assertions. The installed-content checks pass two cases per
language: 906 assertions in English and 909 in Polish, covering MM7/MM8 plus the three-map MM6 pilot. The MM6 programs
retain five applicable site descriptions and two item-reward annotations; replaced callbacks do not inherit stale
metadata. All 479 staged overlay/catalog files match their manifest checksums.

## Next regions

Expand the existing MM6 presentation and metadata selection a region at a time. Verify entrances and return routes,
shared-house addresses, trainer bindings and quest disclosures against the active game's content before exposing
each region. No separate MM6 database is needed.
