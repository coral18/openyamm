# OpenYAMM Atlas — MM7/MM8 integration handover

This document describes the completed **current-base Atlas v1 handover**, not a port to OpenYAMM 2.2.
Source: [Atlas source branch](https://github.com/coral18/openyamm/tree/extended/atlas-mm8).
The branch descends from upstream `c1bc5434fbf630a96ac2465818ab25474b14b31c` (2026-10-05).
It contains other Extended work as well: do not merge the entire branch as an Atlas-only patch.

## What the implementation provides

- Optional native UI module, enabled by `mods/interactive-atlas/mod.yaml`; enter through the **Map Book sidebar**.
  Disabling the module removes this entry without deleting knowledge or requiring the UI to load a save.
- MM7 Antagarich and MM8 Jadame catalogs: 26 outdoor regions and 110 dungeon plans derived from engine geometry.
  The data audit loads 137 MapStats entries, including the MM8 developer map, which is searchable only in Guide.
- Discovery-first maps, persisted exploration fog, current party position/direction, small clustered pins,
  known height slices and height-aware editable notes. Guide is explicit and does not award discoveries.
- Region/dungeon navigation, search, typed filters, buildings, NPCs, trainers, travel, sites and connections.
  Shared knowledge distinguishes an actual visit, a learned clue, an observation and an authored possibility.
- Eight Guidebook sections: items, spells, classes, alchemy, companions, campaign progress, Bestiary and quests.
  Entries and links use loaded gameplay tables, active event metadata and observed state.
- Progressive Guide indexing (one map per frame, current map first); shared pointer/touch/controller UI paths.
- Mod Loader v1 supplies manifest validation, dependencies/order/conflicts, QBit-range checks, optional UI activation,
  typed class patches and required gameplay-mod save metadata. It is infrastructure, not a graphical mod manager.

These are original native systems. The community guides' HTML/JavaScript, authored prose, SVGs and embedded artwork
are **not included**; their complete content was not imported. Regional artwork comes from existing game packages.
Jadame uses an original schematic overview. See [ATLAS.md](ATLAS.md) and [coverage](ATLAS_COVERAGE.md).

## Code ownership and integration boundaries

| Area | Starting points | Contract to preserve |
| --- | --- | --- |
| Shared world index | `game/world/WorldDatabase.*`, `WorldMapLocations.cpp`, `WorldMapFeatures.cpp` | References to authoritative game registries; no second Atlas content database |
| Knowledge and observation | `game/world/WorldMapKnowledge.*`, `WorldMapExploration.*`, `WorldMapNotes.*` and runtime callers | Observe actual gameplay; browsing must not award knowledge or alter world state |
| Reference pages | `game/world/WorldGuide.*`, `WorldItemSources.*` | Active table/event identities, typed navigation and identification/privacy gates |
| Geometry/presentation | `game/world/WorldFloorPlan.*`, `WorldAtlasPresentation.*`, `assets_dev/engine/world/atlas/` | Game coordinates/reveal masks; presentation metadata only |
| Native UI | `game/ui/screens/AtlasScreen.*`, `AtlasConnections.cpp`, Map Book integration | Optional entry, current-world selection, view-only catalog switches |
| Lua metadata | `game/events/ScriptedEventProgram.*`, exporter and explicit world overlays | Callback-owned references; repair exporter/overlays, not generated Lua |
| Mod infrastructure | `game/content/ModLoader.*`, `mods/interactive-atlas/mod.yaml`, [MODDING.md](MODDING.md) | Explicit profile, validated IDs/ranges, optional presentation vs required gameplay state |
| Verification | `tests/World*Tests.cpp`, `tests/scenarios/atlas/`, `tools/menu_tours/atlas_*` | Unit/data checks plus isolated native UI and post-save checks |

Some investigated source-data defects were repaired in shared tables/explicit overlays, including stale house
locations and original well/drink condition semantics. Review these separately from the UI port against the new
upstream content. [The condition audit](tools/audit_atlas_condition_sources.py) and focused tests retain provenance.
All testing uses isolated settings/saves; no official installation or user campaign is part of the fixtures.

## Final current-base checks, 2026-11-12

The final spell-source fix removes 22 dead links to unused MM7 guild rows. Guide now respects indexed physical
venue visibility even without a party-knowledge pointer. Actual guild links remain navigable in both worlds.

- Fully index all 137 MM7/MM8 maps and resolve **24,623 emitted links** across all eight Guide sections:
  12,692 MM7 and 11,931 MM8. Entry IDs/titles, record/feature destinations and typed reference targets are checked.
  This is navigation integrity, not proof of every conditional campaign state or an exhaustive playthrough.
- Native Windows Release builds successfully; the expanded shared regression selection passes **563 cases /
  281,275 assertions** (one unrelated artwork test explicitly excluded, as explained below).
- Run both fresh-campaign native UI tours on the desktop GPU (OpenGL 3.3 / AMD): **68 steps, 14 fresh captures**.
  Inspect spell sources and destination maps, toggle Guide off and reopen Discovery.
- Inspect four saves, compare four identical before/after Discovery map/list crops, then run **26 post-UI scenario
  steps** including save/load. Party/map/gold/food and cached map keys are preserved; the unvisited target guilds
  and maps remain unknown. Ordinary native rendering can reveal the source party's local terrain.

The reproducible final fixtures are documented in [tests/fixtures/atlas/README.md](tests/fixtures/atlas/README.md).
Build with `tools/extended_windows_dev.sh build` in the prepared Windows environment. For the full Guide-link audit:

```bash
OPENYAMM_REGRESSION_ASSETS_ROOT=/path/to/prepared/runtime/assets \
  tools/extended_windows_dev.sh test \
  '--test-case=all active MM7 and MM8 Guide sections resolve every emitted navigation link'
```

Set `OPENYAMM_ATLAS_INTERACTION_AUDIT` to an existing output directory to export `guide-links.tsv`.
The expanded regression selection includes World, ModLoader, asset mounting, input, save UI, content manifests,
shared dialogue, Lua export, party and mercenary tests. One existing unrelated artwork test,
`dwi temple proprietor portrait uses mm8 overlay icon`, requires a retired original-icon path and is excluded;
this is not an all-repository green-test claim.

## Limits and next integration step

This handover closes the agreed **current-base native map/discovery/navigation implementation and its final fixes**.
It does not promise every fact present in third-party guides, every dynamic script predicate, semantic named storeys
or a complete campaign traversal. Specific remaining reference/content limits are retained in [coverage](ATLAS_COVERAGE.md).
New product ideas remain unimplemented in [ATLAS_DECISIONS.md](ATLAS_DECISIONS.md).

The last physically tested POCO release is **0.2.8 / 111**. The user confirmed MM8 fog/arrow, MM7 fog/direction and
faster Guide loading on the corresponding earlier builds. Later PC fixes are not installed there; Android work is
deferred. Physical Windows pad use in Atlas was confirmed; the later ordinary Map Book D-pad follow-up has automated
coverage but still needs physical acceptance. Do not present these as final-device acceptance of this source revision.

The manifest activates C++ UI compiled into Extended; copying it alone into stock OpenYAMM 2.1 will not install Atlas.
Port on a separate branch against the author's current data/API, with particular attention to:

1. **Save formats:** this branch writes version 84 with Extended's identified-item state. Upstream's version numbers
   evolved independently; an Extended 84 save must not be interpreted as an ordinary upstream 84 save. Provide an
   explicit import/migration if retaining these campaigns, then use the upstream-approved extension format.
2. **Reloaded/localized registries and mounting:** reconstruct index references after table reloads and retain canonical
   world identities. The UI must use current mounted tables rather than stale pointers.
3. **Observations and rendering:** this base's monster knowledge covers sprites. Upstream's model-based actors need
   equivalent observation hooks before claiming the same Discovery behavior there.
4. **Mods and QBits:** reconcile the manifest loader with upstream's override/ruleset mechanisms; include ruleset-owned
   QBit ranges, not just world/mod ranges, in collision checks.
5. **Payloads/builds:** use runtime assets matching the target upstream build. Keep Extended identity/signing/update
   channels distinct if distributing a fork. Do not commit multi-GB runtime payloads.

No upstream 2.1 merge, combined build, save migration or MM9 Atlas is included in this handover.


## Project identity, 2026-11-13

The public project name is **OpenYAMM Atlas**, maintained by **Karol**. Earlier records use the name
OpenYAMM Extended. It is a community fork focused on the Atlas, separate from the official upstream release.
Legacy branch/build-directory names, the Android application ID, signing identity and save namespaces stay
stable across this branding change. Old package filenames and test hashes remain historical evidence.
See [support and development capacity](SUPPORT.md) for the current funding goal.


## Community inspiration and credits

Both reference guides are by **[u/hulululuh](https://www.reddit.com/user/hulululuh/)**:
[MM7 guide announcement](https://www.reddit.com/r/MightAndMagic/comments/1ww8pq0/i_made_an_interactive_world_map_guide_for_might/)
and [MM8 Jadame guide announcement](https://www.reddit.com/r/MightAndMagic/comments/1x7x7ok/simon_found_a_map_of_jadame_i_made_it_clickable/).
OpenYAMM itself is by [pjasicek and contributors](https://github.com/pjasicek/openyamm).
