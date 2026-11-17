# MM6 Atlas on OpenYAMM 2.6

The native Atlas covers Enroth alongside MM7 and MM8. Its world data comes from the installed game packages,
merged registries and active scripts. No external MM6 interactive-map site, HTML, browser or copied guide is needed.

## Coverage

- **15 regions and 40 dungeons**, including the sewers, Oracle, Control Center, Arena and authored secret areas.
  Eleven legacy maps named `pending` are reserved test content and are excluded from player navigation/search.
- Every region has an overview anchor following MM6's `out{column}{row}` world grid. The Enroth overview uses native
  region cards and the game's regional art. The positions are presentation anchors, not gameplay coordinates.
- Every playable dungeon has a verified primary parent. The Control Center is nested under the Oracle; sewer and
  Arena grouping also uses their authored return routes. Grouping does not imply reversible or unlocked travel.
- **93 declared map transitions**, indexed from callback-owned metadata. These are possible script destinations;
  conditional and one-way routes remain distinct. Coach/boat schedules and walking routes use the runtime tables.
- **61 journal entries with 203 relationships** connect givers, objectives, items, returns and the nine price-fixing
  subentries. A location not named by the journal is a Guide hint, not a Discovery fact. This changes no quest state.
- **212 generated sidecars** supply possible NPC/topic assignments, encounters, rewards, autonotes, map connections
  and interaction descriptions. The installed programs retain **46 site descriptions and 90 item-reward events**;
  callbacks replaced by gameplay scripts do not inherit stale legacy annotations.
- Shops, trainers, residents, bestiary, items, alchemy and class information use the existing Atlas/shared data APIs.
  Definitions are not copied into an independent MM6 knowledge database.

Discovery uses the party's observations, exploration masks, encountered trainers/NPCs and disclosed quest information.
Opening Guide does not record a visit, reveal terrain in the campaign or execute gameplay callbacks. Region fog,
indoor outlines, position/direction, height layers, search and personal notes use the shared implementation.

English and Polish are supported. The Atlas display inventory has **682 entries / 10,456 English source words**,
including 42 MM6 descriptions added to the earlier MM7/MM8 catalog. The catalog also has two main-menu toggle labels. Existing game terms use the official language
packs; this does not claim to finish every upstream translation or provide missing dubbed voices.

## Build and installation

Follow [ATLAS_PORT.md](ATLAS_PORT.md). This content needs an executable built with `OPENYAMM_BUILD_ATLAS=ON` on
the 2.6 base. Stage the owned overlays and language supplement into an empty directory:

```sh
python3 tools/stage_atlas_overlay.py --output output/atlas-overlay
```

Copy `mods/` and `languages/` into a separate user-data directory. Keep the official 2.6 runtime packs outside Git,
launch with that directory and set `[updates] check_for_updates=off` for this development executable. Open the
Map Book and choose Atlas. The main-menu on/off button persists `[atlas] enabled`; disabling it hides the entry and retains discoveries and notes. The [Windows release](ATLAS_RELEASE.md) includes the compiled screen and staged overlays.

For Polish, install the official Polish language packs and select `language=pl`; English is `language=en`.
Restart after changing language. See [ATLAS_LOCALIZATION.md](ATLAS_LOCALIZATION.md).

## Reproducible metadata

`tools/regenerate_lua_events.sh --atlas-metadata` selects all MM6/MM7/MM8 maps, with the shared global export owned
by MM8/default. Supply the installed pack root, per-world legacy EVT/STR inputs in the metadata input directory,
and the GrayFace reference folders named by the exporter configs. It does not replace gameplay handlers.

The exporter now applies the runtime's `MapStats::applyWorldOwnership` pass before selecting maps. Raw filename
inference classified unprefixed names such as `sewer`, `sci-fi`, `oracle`, castle maps and `hive` as MM8, silently
omitting their MM6 metadata. Ownership now follows the packages that actually ship the maps. Never hand-edit the
generated sidecars or work around this by copying another world's scripts.

## Verification

The full MM6 coverage was established on 2.5. The installed-map audits, shared regressions and five-map save/load scenario were rerun on 2.6; native 2.6 MM6 checks cover Free Haven. The Control Center/VARN visual tours listed below are the earlier 2.5 checkpoint. See [the current verification table](ATLAS_PORT.md#verified-on-26).

- The installed-content audit loads all **204 MM6/MM7/MM8 map programs** in English and Polish. It verifies
  **203 installed geometries**; MM7's reserved `zddb05` row has no geometry and remains excluded.
- MM6 checks cover all 55 playable maps, all region anchors, all 40 primary dungeon parents, special-map transitions,
  every journal target and Guide-only clue filtering. Reserved test maps cannot leak their residents into search.
- Shared Atlas/localization and world-ownership tests cover original identifiers, player names, notes, typed
  callback ownership, discovery privacy, geometry, item reading and quest links.
- `tests/scenarios/atlas/mm6_save_load.yml` checks new game and save/load in New Sorpigal, Free Haven, the sewers,
  Control Center and Tomb of VARN using isolated saves.
- Native Windows tours cover Free Haven, Control Center and Tomb of VARN in English and Polish: Map Book entry,
  Discovery/Guide, position, indoor fog, height layers, world overview, the Castle Alamos crystal quest and return
  to gameplay. These are visual/function checks, not a hardware performance benchmark.
- Polish Unicode search (`Świątynia`) and the final staged package pass native checks. Guide exposes 15 region
  cards; switching back to Discovery retains only the campaign's visited regions.
- Metadata-only regeneration reproduces all **639 MM6/MM7/MM8 sidecars** byte for byte. All **680 staged overlay
  and catalog files** match their SHA-256 manifest. Tests run again against the final ZIP-pack/overlay installation.

Android/POCO acceptance is deferred for this stage. The implementation stays in shared portable code and data;
there is no native plugin ABI or promise that a stock future executable can load the Atlas screen unchanged.

The earlier three-map checkpoint remains documented in [ATLAS_MM6_PILOT.md](ATLAS_MM6_PILOT.md).
