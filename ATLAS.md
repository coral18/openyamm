# Native Interactive Atlas: MM7 and MM8

Reference: [hulululuh's MM7 interactive world-map guide](https://www.reddit.com/r/MightAndMagic/comments/1ww8pq0/i_made_an_interactive_world_map_guide_for_might/).
This is an original native implementation using OpenYAMM's renderer, menus and game data. The reference HTML,
JavaScript, SVG floor plans, translated descriptions and embedded artwork are not bundled. The downloaded guide
did not include a declared reuse license; a future import of its authored material requires an appropriate license.

The later [MM8 community guide](https://www.reddit.com/r/MightAndMagic/comments/1x7x7ok/simon_found_a_map_of_jadame_i_made_it_clickable/)
is also a behavioral reference, not a bundled HTML application. Its downloaded archive contains one HTML file
without a declared reuse license. See [the explicit coverage/completion checklist](ATLAS_COVERAGE.md): neither
community guide's complete content has been integrated yet.

## Working slice

- Optional native UI module provided by `mods/interactive-atlas/mod.yaml`, enabled through the normal mod profile.
  Disabling it removes the Atlas menu entry and preserves shared discoveries and save compatibility.
- Interactive Antagarich overview and all 13 outdoor region images from the existing game packages.
- Separate MM7/MM8 catalog buttons in the native Atlas; opening from gameplay selects its current world.
  Switching the viewed catalog does not travel, change the party, activate another world package or award discoveries.
- Jadame region maps and an original schematic continent overview assembled from existing regional artwork.
  Discovery shows only known locations without full-map thumbnails; Guide shows the reference maps. The elemental
  planes, their dungeon maps and Plane Between Planes are searchable. The developer Tutorial map remains searchable
  in Guide but is not positioned on the playable-world overview.
- Region/dungeon selection, resident NPCs, buildings, mastery trainers, walking routes and transport schedules.
- Sites layer for physical chests, wells/fountains, shrines, obelisks and declared passages/teleports.
  Search and clustered small markers work on regional images and dungeon plans; declared map links open the
  connected Atlas page without moving the party.
- Off-map dungeon plans projected from the engine's actual geometry. Discovery respects saved outline bits,
  seen faces and runtime invisible-face overrides; Guide renders the authored visible outlines.
- Case-insensitive search and typed filters. Trainer names and topics come from the game's NPC/topic tables.
- The default discovery list excludes routes. Use Travel to see routes, labelled with departure direction,
  origin and duration. Distinct map exits remain separate even when they lead to the same destination.
  The continent overview lists discovered regions/dungeons; a selected region lists local discoveries under
  `Discovered here`, rather than implying that it lists the party's entire journey.
- Default Discovery Mode; opt-in Guide Mode is a read-only view of the same index.
- Successful map loads record discoveries; actual opened house/NPC dialogue records encountered services/residents.
- Existing visited-map snapshots reveal locations from older saves. Older saves do not retroactively mark every
  resident as met. Opening the atlas, searching and switching modes never award discoveries.
- Outdoor Discovery maps use the same saved 88x88 full/partial reveal masks as the engine Map Book. Unknown
  areas are opaque black, partial exploration is dimmed; missing masks never reveal an entire region. Guide
  hides this overlay without writing discoveries. Zoom/pan transform both art and fog together.
- Current party position and heading use the same MAPDIR arrow sprites/octants as the minimap. Markers are
  restricted to the active region/dungeon, including Guide Mode. Paused Atlas opens on the current location.
- Outdoor maps display clickable building (`B`), trainer (`T`) and dungeon-entrance (`D`) markers. Nearby doors
  form numbered groups; clicking a group narrows the list, and its heading clears that selection. Filters and
  search apply to both the list and markers. Single dungeon markers open the existing floor-plan view.
  Region badges use 12 logical pixels and continent badges 10; their unchanged 24/28-pixel hit areas retain
  comfortable pointer/touch selection. Marker glyphs use the small reading font; party-arrow size is unchanged.
- Buildings require a real visit and trainers require meeting their resident. Dungeon entrances require the
  exact entrance cell to be fully explored, not merely dimly revealed. Learning an entrance never grants its
  interior map or reveals other entrances. Guide Mode shows authored markers without changing discoveries.
- Paused trainer details reuse the gameplay training evaluator for the selected character, including price,
  class/promotion limits and skill requirements. This inspection never trains a character or spends gold.
- Zoom, drag, search input, native button focus and scroll controls. Entry in main menu and pause menu.

`game/world/WorldDatabase` indexes **references** to `MapStats`, `HouseTable`, `NpcDialogTable` and
`MergedTeacherTopicTable`. It owns neither copies of those records nor an alternative balancing/content database.
Map IDs reuse the canonical IDs already supplied by the engine. Temporary legacy house/NPC IDs use the existing
merged numeric IDs; richer package IDs can replace these as the upstream registries evolve.

`assets_dev/engine/world/atlas/mm7.yml` and `mm8.yml` supply presentation only: world names, overview style, region
anchors and dungeon groupings. Names,
trainer topics, shop metadata, destinations and schedules stay in their authoritative tables. Anchors on the
illustrated continent artwork are approximate; they are not gameplay coordinates or precise points of interest.
Dungeon grouping uses the existing `areaId` plus the presentation's area definitions. Locations without declared
parent metadata remain searchable rather than being assigned a guessed parent.

`WorldMapLocations` derives outdoor door positions from interactive geometry and explicit event context metadata,
then links those sites to the existing house/dungeon records. Shared vertices are deduplicated; distinct models
and decorations remain separate sites. No hand-maintained coordinates or copied guide dataset is introduced.
The Atlas reuses `GameDataLoader::loadEventPrograms`, including world common scripts and sorted overlays, and loads
geometry without render textures. Each known outdoor region is indexed once per Atlas opening; Guide can index
all regions. Missing geometry/metadata reports a content-package error instead of fabricating coordinates.

The canonical house table had stale geography for six MM7 regions after the map registry numbering changed.
The 114 affected rows now use the canonical map IDs; 90 correspond to active outdoor house-entry events.
All 13 MM7 outdoor event files were checked against their house references with no remaining map mismatch.
`Dungeon Ent` transition cards are excluded from the building index. The explicit Harmondale world overlay
`7out02_zz_extended.lua` restores Castle Harmondale's destination metadata after the MMMerge event replacement;
it changes neither the generated event script nor its gameplay handler.

Discoveries use `openyamm.discovery.*` integer keys in the existing session/runtime named-variable store, whose
serialization is already supported by saves. No save version bump or separate atlas save file is introduced.
Unknown services are filtered before search; routes require both endpoint maps to be known in Discovery Mode.

## Remaining guide coverage

This slice is not the complete external guide port. Quest chains, monster groups, site effects/loot/open state,
conditional teleport destinations, Barrows connection graphs, item/ingredient sources, guild availability and
companion recruitment locations remain to be added through shared game/world data. The native guidebook below
now covers reference pages and campaign trackers. Player notes remain editable in the existing Map Book.
Dungeon plans currently project all elevations together; floor
selection and annotations are not yet shown in the Atlas. The index currently describes authored residents and teaching topics;
runtime NPC relocation/topic overrides and information learned from books or rumours require additional shared
discovery/data integration. Conditional or dynamically moved entrances require corresponding runtime metadata.
Sites currently cover explicit map-local event bindings; inherited decoration/global-event bindings need their
runtime resolver exposed to the index. The Android implementation
uses shared native code but this slice has not yet been
built or physically tested on Android/controller hardware.

## Windows iteration

The prepared PC directory is `%USERPROFILE%\openyamm-extended-dev` with `tools/msys64`, `source`, `build`, `cache/ccache`
and an isolated `runtime` directory. Launch manually with `%USERPROFILE%\openyamm-extended-dev\OpenYAMM Extended.cmd`. From WSL:

```bash
tools/extended_windows_dev.sh build
tools/extended_windows_dev.sh run --world mm7
```

Set `OPENYAMM_WINDOWS_DEV_ROOT` to another prepared WSL path if needed. The script does not download or provision
the toolchain/payload. It syncs source changes, reuses CMake/Ninja dependencies and ccache, and runs with isolated
settings/saves. Current toolchain: MSYS2 UCRT64 GCC 16.2, CMake 4.4.4, Ninja and ccache 4.14. No Vulkan build.
Upstream Direct3D shader generation needed two portability fixes: scalar vector initialization through
`vec3_splat`, and explicit skinning attribute arguments because HLSL vertex inputs are local to `main`.
The native menu image loader now recognizes PNG signatures in legacy `.bmp`/`.pcx` filenames, as the shared
engine image decoder already does. MM7 region images in the official package use this convention.

All world packages must be present: MM7 baked lighting references qualified MM8 textures. Keep the large runtime
packages outside Git. After a successful build the script updates only the small local overlay
`runtime/assets/assets.zip`, supplying the Extended Knight table, canonical house geography, Atlas presentation
and explicit Harmondale metadata overlay from source. Other overlay
entries are preserved; the multi-GB base packages are not rewritten. Production packaging must include the source metadata.

### Payload provenance

The public Windows 1.0 ZIP downloaded from the upstream Nextcloud release folder has SHA256
`e7d610ed6dfe8dd5d56ccfd32ab9d606925ae820756818fea7dfe328013a54e8`, while its published checksum sidecar says
`833063565c8c70896b6d43cde6479e332ce48476a954cb26af8aa063f831fb93`. This discrepancy is unresolved; do not call the
archive checksum-verified or redistribute it as a verified release. Its executable/DLLs were not used: the local
executable is compiled from the fork, with runtime DLLs from the installed UCRT64 toolchain.

Before adding the local overlay, ZIP entry size/CRC comparisons against the previously checksum-verified official
Android APK matched 37,845 engine entries, all 4,171 MM7 entries and all 103 Merge entries. The 329 differing engine
entries are exclusively the platform sprite profile/manifests/runtime pages. These comparisons are evidence of
cross-platform content parity, not substitutes for a matching published Windows archive hash.
All 4,439 MM6 and 4,043 MM8 package entries also matched the verified APK.

## Validation

Focused doctest coverage exercises discovery before search, Guide Mode without side effects, canonical-table
references, region grouping, cross-world exclusion, route visibility and invalid/duplicate/cyclic presentation.
It also loads the actual canonical source tables and resolves MM7 Sword trainers. Twelve cases / 205 assertions pass,
including MSB-first dungeon/region reveal bits, partial/full precedence, empty exploration, mirrored projections,
arrow octants, dungeon marker projection, invisible geometry, invalid indices and extreme coordinates. Door tests
cover authored metadata, real world-space positions, repeated triangles, separate sites, idempotent reindexing,
hidden/invalid/cross-world entries, visited services, trainer privacy and exact entrance-cell discovery.

Windows Release compilation succeeded. Engine-owned captures verified the overview, Harmondale map, trainer filter
and `Grand Master Sword` search, plus White Cliff Cave geometry and zoom, using native OpenGL on AMD hardware.
The live Harmondale check passed gameplay -> paused Atlas -> pause menu -> resumed gameplay. A real quicksave
contains the discovery key in the existing save namespaces; a fresh process loaded that save, displayed
only discovered Harmondale in Discovery Mode and resumed gameplay. A live White Cliff Cave session displayed
only the explored entrance chamber in Discovery Mode and resumed the same dungeon. Missing/invalid presentation
packages are reported in the menu and log without aborting the game. Captures/logs live outside Git in the isolated
runtime and ignored build directory. This does not constitute a complete guide-content audit or Android test.

The regional fog/marker check used live Harmondale exploration, zoom, Guide -> Discovery, and a second
position/camera pose facing north, plus live White Cliff Cave in Discovery/Guide with its party arrow.
All native tours resumed gameplay. Fog is uploaded once per selected region; the paused view reads the party
position and exploration snapshot without mutating either. MM7 outdoor maps use their native world-to-image
projection; new worlds with alternate presentation catalogs need corresponding Atlas presentation integration.

The duplicated-discovery regression reproduces Harmondale -> Tularean Forest -> Avlee using canonical travel
tables: each local discovery query returns its region once, the continent returns three distinct regions, and
Travel preserves the two genuine exits with distinct labels. Unknown destinations remain filtered before search.
A native Windows tour loaded a copy of the reported Avlee autosave in a separate working directory, checked
both regional lists, Travel, the continent overview and return to gameplay. The original save's SHA256 was
unchanged; all test output and settings stayed in the separate test directory.

The outdoor-marker check used real Harmondale door interactions to enter Tempered Steel and meet Chadric, then
saved and loaded the resulting discoveries in a fresh native process. The Atlas displayed only the encountered
services, retained the fog, and evaluated Grand Master Sword for the active Cleric. Native captures checked both
Harmondale entrances, clustered markers, Avlee and Tularean Forest geography, and Guide floor plans versus an empty
unvisited interior in Discovery Mode. The tour resumed gameplay. Tests used separate settings/save directories;
no original save was overwritten.
The rebuilt native Windows executable also fully loaded New Sorpigal (MM6) and Dagger Wound Island (MM8),
including their event programs, after the shared loader extraction. Both checks exited successfully.

The compact-marker refinement was compiled and visually checked in native Windows gameplay: Harmondale and Avlee,
region zoom, continent overview, Discovery/Guide and return to gameplay. Clicking outside a badge's visible border
but inside its retained hit area selected the correct group. Eight engine-owned captures remain in the isolated
`atlas-small-markers-20261007` directory outside Git.

The 2026-10-17 MM8 checkpoint adds `WorldAtlasPresentation` and package-qualified region images so Jadame can be
viewed while Antagarich is the active gameplay world, and vice versa. A source-data audit corrected 24 MM6 dungeon
records missing `world_id`; 23 have packaged MM6 geometry and one is a legacy pending entry. These records no longer
pollute the MM8 index. Other table columns were preserved. The Windows overlay now includes the canonical map table.

Thirteen focused tests / 1018 assertions passed, including the actual MM8 catalog, 14 outdoor records (one Tutorial),
47 dungeon records, canonical trainer links, parent links and isolation from MM7 discoveries. Native Windows checks
loaded a fresh saved MM8 campaign and captured Discovery fog/arrow, Guide markers and trainer requirements,
Ravenshore, the Chapel of Eep floor plan, both catalog switches and resumed gameplay. A second native tour loaded
a copy of the user's earlier Avlee save, checked secondary-world art, zero MM8 discoveries and the three distinct
MM7 regions after returning from Guide. Captures remain under `atlas-mm8-20261017` and `atlas-mm7-mm8-cross-20261017`
outside Git. MM6 Castle Alamos and MM7 Harmondale full-load regressions also passed.

Native UI tours support `pause: true` while gameplay is active. This invokes the existing pause-menu entry point
without depending on Windows foreground focus or injecting system keyboard input. Normal game input is unchanged.

### Native guidebook checkpoint, 2026-10-17

The Atlas header opens a native Guidebook with searchable Items, Spells, Classes, Alchemy, Companions and Progress
sections. `WorldGuide` derives read-only entries directly from loaded tables; it does not contain a copied guide
database. Class HP/progression reflects typed mod patches. Mastery links translate the class's mastery enum into
the exact teacher-topic rank before searching the selected world. Normal mastery has no advanced trainer link.
Race overrides and a cross-class chart remain future work.

Discovery filters before search: encountered items, learned spells, current party classes, known recipe autonotes
and recruited roster members. Guide Mode reads all definitions without recording discoveries. Shared merged
definitions deliberately remain the same when browsing either world. Companions currently show starting roster
definitions and biography, not their current position or recruitment quest conditions.

Alchemy uses the runtime reagent, potion-mixing and recipe-note tables. It chooses one minimum-reagent path,
orders intermediate steps, accumulates repeated raw ingredients and shows direct formula alternatives. Unreachable
cycles, self recipes, failure/no-mix outcomes and excessive expansion cannot create an infinite plan. Discovery
can show a learned direct formula even when the preceding reagent recipes are not known.

Progress reads existing Arcomage first-win/totals and four Arena difficulty counters. Tavern rules and geography
come from the same loaded house/map tables as actual play. Discovery hides unexplored taverns; a saved first
victory is also evidence that the party visited that tavern. Counts explicitly distinguish the selected world's
taverns from shared campaign totals. Tavern links select their actual containing region. No tracker state is
written or invented by the Atlas.

Six guidebook tests / 99 assertions passed, including canonical MM7/MM8 tavern rules, privacy, unchanged campaign
knowledge, exact trainer-rank conversion, modded Knight HP and recursive alchemy counts. Windows Release builds and
native tours checked the reference layouts, learned-spell/item filtering, Knight HP/level 6, recipes and ingredient
quantities, roster biographies, Progress, tavern-to-map navigation and return to gameplay. Captures are outside Git
under `atlas-guide-20261017`. The full feature matrix remains in [ATLAS_COVERAGE.md](ATLAS_COVERAGE.md); this checkpoint
does not complete the interactive-guide integration or establish Android/controller validation.

The optional-module checkpoint adds manifest-driven native UI activation and `save_required: false` for
presentation-only mods. Gameplay patches and QBit ownership cannot opt out of save requirements. Seven Mod Loader
tests / 133 assertions pass; Windows tours verified the Atlas menu entry with the module on and its absence with
it off. A faithful new-game scenario saved with both `karol.test` and Atlas enabled, then a separate process with
Atlas disabled loaded that save. Required-content metadata retained `karol.test@0.1.0` and excluded the optional
Atlas module. Original campaigns/settings were not used as test outputs. Android packaging already copies the
mod directory; its verifier now requires the Atlas manifest and MM8 catalog, but no new APK was built for this slice.

### Physical sites checkpoint, 2026-10-17

`WorldInteractionSites` extracts physical event sites from the loaded BLV/ODM geometry. Connected faces sharing
an event form one site; coincident vertex positions across mesh seams are deduplicated. Disconnected objects
using the same event remain separate, including within one outdoor model. Decorations retain their own source
indices. Invisible faces and invalid vertex references are excluded. The same extractor now supplies entrance
pins and the new `WorldMapFeatures` index; there is no copied guide coordinate table.

The Sites filter indexes chests, water sources, shrines, obelisks and connections from the existing Lua context
metadata. Hidden metadata is excluded in both modes. Outdoors, only fully revealed terrain cells reveal sites;
partial fog does not. Indoors, a seen interactive face or a recorded interaction reveals that physical site.
Invisible unseen surfaces stay private. An interaction with one reused event does not discover its other objects.
The shared interaction controller writes namespaced site keys into existing map-local save variables; Atlas
browsing/search/Guide never writes those keys. No save-format change is needed.

Declared connections link to known maps in Discovery and to available catalog maps in Guide. These are metadata
links, not a claim to evaluate every conditional Lua branch or the current state of a teleporter. An empty target
is shown as not mapped rather than guessed to be a local teleport. A Barrow I -> Barrow V page transition was checked
natively, but a complete Barrows graph
and conditional-route evaluation remain on the coverage checklist. Site details currently show type and container
count, not exact loot, refill/open state or the effect of every well.

Sixteen World Database tests / 1062 assertions passed, including the previous discovery/entrance regressions,
connected geometry with split vertex indices, duplicate events at separate objects, invalid/invisible faces,
partial/full exploration and filtering before search. Two faithful runtime scenarios under `tests/scenarios/atlas`
exercise actual MM8 interaction picking, then save/load site knowledge: a Dagger Wound well and an Abandoned
Temple chest, with an untouched chest remaining unknown. Native Windows tours checked MM7/MM8 Sites, filtering,
Obelisk details, dungeon chest/passage pins, connected pages and resumed gameplay. A separate native run loaded the
indoor scenario save, showed the one known chest, switched to the full Guide catalog and restored the single
Discovery entry. Its empty floor plan is expected for that headless fixture, which does not render exploration
outlines. Captures remain outside Git under `atlas-sites-20261017` and `atlas-indoor-sites-20261017`.
