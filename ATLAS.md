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
- Sites layer for physical chests, wells/fountains, shrines, obelisks, declared passages/teleports,
  authored encounter areas, last-observed creatures and saved map notes.
  Search and clustered small markers work on regional images and dungeon plans; declared map links open the
  connected Atlas page without moving the party.
- Player notes can be placed on a visited map, edited or deleted in the native Atlas. They reuse the Map Book
  note state and remain independent of fog. Automatic trainer notes are indexed as read-only records.
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
`MergedTeacherTopicTable`. It owns neither copies of those base records nor an alternative balancing/content database.
Observed NPC/transport values are explicit saved knowledge; they do not replace the authoritative gameplay tables.
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

This slice is not the complete external guide port. Complete campaign chains, placed/scripted monsters, site effects/loot/open state,
conditional teleport destinations, Barrows connection graphs, item/ingredient sources, guild availability and
companion recruitment locations remain to be added through shared game/world data. The native guidebook below
now covers reference pages and campaign trackers. Player notes are editable in the Atlas and existing Map Book.
Dungeon plans currently project all elevations together; floor
selection and floor-specific annotations are not yet shown in the Atlas. The index uses current or last-observed
resident locations/topics, but conditional/shared venues, generated/roaming NPCs and more information learned from
books or rumours require additional shared integration. Dynamically moved entrances require runtime metadata.
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
tools/extended_windows_dev.sh test '--source-file=*WorldDatabaseTests.cpp,*WorldGuideTests.cpp'
```

Set `OPENYAMM_WINDOWS_DEV_ROOT` to another prepared WSL path if needed. The script does not download or provision
the toolchain/payload. It syncs source changes, reuses CMake/Ninja dependencies and ccache, and runs with isolated
settings/saves. Current toolchain: MSYS2 UCRT64 GCC 16.2, CMake 4.4.4, Ninja and ccache 4.14. No Vulkan build.
The `test` action enables and builds the standard CMake `openyamm_unit_tests` target in the same native cache,
then forwards the supplied doctest arguments. It does not launch gameplay or update the runtime overlay.
The Windows test target links the existing NPC/transport knowledge implementations and FFmpeg's required bcrypt
library. Only focused test selections have been run; compiling the target does not establish full-suite success.
An unchanged focused Atlas run took about one second with the prepared native cache.

For packaged-data checks, explicitly set `OPENYAMM_REGRESSION_ASSETS_ROOT` to the isolated `runtime/assets`
directory and `OPENYAMM_ATLAS_AUDIT_ROOT` to a data-only cache prepared by `tools/prepare_atlas_audit.py`.
The wrapper converts WSL paths for native tests. Without the regression override, tests retain their normal
`assets_dev` source. Neither variable silently substitutes a different payload when data is missing.
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

### Monster knowledge checkpoint, 2026-10-17

Sites now includes monster encounter areas derived from native map spawn points and the loaded MapStats and
MonsterTable definitions. The engine and Atlas share encounter-slot decoding, picture-base selection and tier
weights; the runtime keeps its existing random rolls and spawn counts. Invalid/empty groups are omitted, fixed tiers
remain fixed, and difficulty-zero groups expose only their possible A tier. Guide labels are reference count ranges,
not live population counts. This covers authored spawn points, not all placed/scripted or individually named actors.

Discovery never reveals an unknown encounter group merely because terrain was explored. Actual actor picks during
hover, interaction or attack record stable actor ID, base creature definition, last observed coordinates, alive/dead
state and game time in existing map-local save variables. Cached hover does not track hidden movement. Last-known
pins read those saved observations, never the current positions of unseen actors. This first implementation observes
picked/targeted actors; merely rendering a distant actor does not yet create an observation.

The Guidebook has an observed-only Bestiary in Discovery and the loaded monster catalog in Guide. Entries show base
HP/AC/experience, attacks, spells, resistances and loot parameters, with an explicit distinction from runtime scaling
and effects. Monster sites link to their first creature definition. Variant navigation, individually named overrides,
portraits, comprehensive placed/scripted actor sources and broader content auditing remain on the coverage checklist.
Guide browsing does not change the party's knowledge. The native filter highlight and footer now follow the active
Sites/search selection, including when a previous selection falls outside the new search.

Nineteen World Database cases / 1125 assertions passed. Seven Guidebook cases / 108 assertions also passed, including Bestiary privacy.
The faithful `tests/scenarios/atlas/mm8_monster_observation.yml` picks a real Dagger Wound actor, verifies exact known
coordinates and an untouched actor's unknown state, saves, changes map to verify namespace isolation, reloads and
checks the original observations. Existing faithful map-lifecycle scenarios passed for MM6, MM7 and MM8 after the
encounter refactor. Windows Release native GPU tours cover observed-only sites, creature details, unknown dragons
remaining hidden after Guide browsing, authored encounters in both worlds and return to gameplay. Captures stay
outside Git in `atlas-monster-ui-20261017`. This checkpoint does not establish Android/controller validation or
complete the full Atlas feature matrix.

### Personal map annotations, 2026-10-17

`WorldMapNotes` reads and edits the existing `EventRuntimeState::runtimeMapNotes`; there is no second annotation
save format. Both the Map Book and Atlas consume those records. Active automatic notes, including trainer notes,
are read-only in the Atlas. New user notes get an unused ID in the upper uint32 half and a namespaced map-local
ownership key, `openyamm.map-notes.user/<id>`. Allocation skips occupied IDs and ownership keys. Edits/deletes
validate ownership and map scope before changing state; empty, oversized or NUL-containing text is rejected.
A delete removes both the note and its ownership key. The UTF-8 input buffer is bounded to 512 bytes.

The owning live map or saved snapshot supplies map identity explicitly. `EventRuntimeState::mapFileName` is runtime
context, deliberately absent from serialized snapshots, so it cannot authorize historical-note edits. Current-map
edits update its live event state; historical-map edits update that map's existing authoritative snapshot.
`findMapSnapshot` centralizes the existing canonical-first, legacy-filename lookup used by actual map loading.
Atlas knowledge collection uses that same owner for notes, observations, fog and floor plans, rather than displaying
or editing an alias while gameplay restores a different canonical record. It never merges conflicting snapshots. Adding notes never
creates a visited-map snapshot, reveals terrain, marks a service as visited or changes gameplay content. Unvisited
maps and the campaign-free main-menu Guide have no note editor. Known maps can be annotated in either mode.

The native Add note action selects a point on the displayed regional image or dungeon floor plan. World coordinates
are calculated through the same projection used to render markers, including inverse dungeon projection. A native
text editor supports save, cancel and delete. The Sites view labels note pins with N, searches their text and opens
user-owned records for editing. Non-note indexing preserves the independently owned note layer. Guide browsing alone
still writes no campaign discoveries. Marker style choices, richer text editing, Android/controller validation and
broader device/content auditing remain future work.

Map refills retain map annotations and namespaced Core discoveries/observations, including last-known creature
positions; they do not restore gameplay map variables, opened chests or trap state. The same preservation hook is
used for outdoor refills and indoor/one-visit resets. Last-observed creature pins remain historical knowledge until
the party actually observes those actors again.

The native review also exposed an exception-lifetime problem: application GPU resources could outlive the bgfx
context if a frame callback threw. The engine now runs its shutdown callback before context destruction on that
path as well as normal exit. Active-screen rendering also balances its deferred-switch scope when rendering throws.
Native input-tour step logs flush immediately so the original failing action is visible in captured process logs.

Twenty-six World Database cases / 1201 assertions passed with AddressSanitizer and UndefinedBehaviorSanitizer,
covering note ownership and collision rejection, map scope without serialized runtime context, conflicting legacy
aliases, independent site-layer indexing, inverse dungeon projection and knowledge retention without gameplay-state
retention on refill. The Windows Release target built successfully. Faithful map-lifecycle regressions passed for
MM6, MM7 and MM8. Native Windows tours exercised create/edit/delete/cancel in MM8, notes on current and historical
MM7/MM8 maps, quicksave and Map Book sharing. A fresh process loaded a note-bearing save with Atlas disabled,
showed the same text in Map Book and showed a pause menu without Atlas. Original campaigns were not used as outputs.

Additional faithful processes loaded the note-bearing MM7/MM8 saves, actually entered each annotated historical map,
then saved/reloaded that visit and verified ownership persistence. A controlled MM8 scenario advanced 700 days and
verified that both the indoor and outdoor maps truly refilled (`map_refill_count` changed from 0 to 1) while both
notes survived. The read-only scenario assertion observes the runtime's processed refill count. Deleted/canceled
text was absent from the resulting saves. A deliberately invalid native tour with Atlas still open returned the
original error and exit code 1 after orderly resource cleanup, rather than the previous shutdown access violation.
All native captures/logs and generated fixture saves stay outside Git under `atlas-notes-*-20261017`,
`atlas-notes-restore-20261017` and `atlas-exception-cleanup-20261017`. Android/device checks remain outstanding.

Fresh native UI processes also reopened the canonical note-bearing saves in both worlds, displayed the saved region
and historical-map text, edited the historical note and saved it again. This validates Atlas reads/edits against the
same canonical snapshot that actual map entry restores, including saves where the legacy filename alias differs.

### Shared quest relationships checkpoint, 2026-10-18

The shared `JournalQuestTable` now accepts language-neutral relationships from
`engine/data_tables/quest_locations.txt`: QBit, owning world, giver/objective/return role, typed target and disclosure.
The initial reviewed dataset covers 19 quests and 70 relationships in MM7/MM8; this is partial content coverage.
Target names and descriptions come from current map, NPC, house and item tables. Canonical map IDs and merged
registry IDs are validated at startup; duplicate relationships, invalid roles/visibility, conflicting quest worlds,
unknown QBits and unresolved references fail instead of yielding dead links. Metadata loading is transactional.

The native Guidebook has a Quests tab. Discovery lists active or recorded quests only; Guide lists the relationships
available for the selected world. Journal-disclosed objectives can make a place known without visiting it or revealing
its terrain/floor plan. Guide-only source hints stay hidden in Discovery. Viewing Guide never awards discoveries.
NPC contacts use the current registry's names, but no location is inferred from a static NPC residence: event-driven
relocations require a separate location relationship until the shared NPC location index supports current overrides.
Quest item names do not unlock their full reference descriptions before ownership/encounter.

Actual event QBit changes record `openyamm.discovery.quest/<qbit>` in existing saved global state. Conditional journal
rows reuse the journal visibility predicate on both sides of a changed bit. A removed journal entry is labelled
"Recorded earlier", not automatically "Completed". Opening Atlas does not write knowledge; old saves infer currently
active quests but do not invent past quests. No save-format change or generated Lua edit is required.

Upstream compatibility was separately assessed without updating the working engine; see
[the 2026-10-18 audit](UPSTREAM_COMPATIBILITY.md). Atlas completion remains governed by [coverage](ATLAS_COVERAGE.md).

Validation: Windows Release compiled successfully. Focused ASan/UBSan tests passed 38 cases / 1,447 assertions
across the shared World Database and Guidebook. The image-decoder translation unit excludes the external bx
allocator's vptr instrumentation because the linked third-party allocator has no instrumented RTTI; Atlas, quest
and database logic retain both sanitizers. Native faithful scenarios accepted Dadeross' letter, saved/reloaded it,
returned it to Elgar Fellmoon, then saved/reloaded its recorded history; MM7 accepted the Lantern of Light quest
and saved/reloaded knowledge through actual NPC dialogue. An MM6 regression also accepted Potbello's letter
through dialogue and preserved QBit 1106 / quest knowledge after save/load; its local fixture uses the normal
new-game party because the older smoke scenario's named party fixture is unsupported by the current runner.
Native AMD/OpenGL captures checked both worlds, the
active/history labels, quest navigation to still-fully-fogged Ravenshore and Guide-to-Discovery privacy. Captures and
isolated campaigns are under the local Windows development directory `atlas-quests-20261018/`; they are not bundled
in Git. Scenarios are retained under `tests/scenarios/atlas/*_quest_knowledge.yml`. Android remains unverified for
this checkpoint.

### Current and observed NPC locations, 2026-10-18

The shared World Database now accepts a derived `WorldNpcState` instead of assuming that every NPC still lives
in its base-table residence. Guide uses canonical Party NPC overrides plus current-map presentation state;
it applies changed residence/topic slots/name and unavailable status. Its records keep pointers to the loaded
registries and copy presentation overrides by value, so moving/copying an index does not leave dangling NPC pointers.
Sparse topic resolution is shared with ordinary dialogue and does not allocate/walk up to the largest slot index.

Discovery uses actual dialogue observations: `openyamm.discovery.npc/<id>/house` and
`openyamm.discovery.npc/<id>/teacher/<topic>`. Only offers presented in the NPC dialogue are recorded. Observation
uses the resolved dialogue context, covering automatic single-resident house dialogue as well as selected occupants.
Opening a training confirmation does not refresh the main dialogue's observed offers.
Automatic Map Book trainer notes now use the same visible offers, closing a path that could disclose a hidden
trainer even when the Atlas list correctly hid it. The old double copy of the full event state per NPC dialogue
was removed along with that ungated note-registration path. Zero-valued retired offer keys
propagate through the existing global-state merge. A remote/actor dialogue with no host house records no residence.
No save-format change or generated Lua edit is required; knowledge is recorded even when the optional Atlas UI is off.

The Discovery index keeps the last encountered residence/offers when the NPC secretly moves or changes topics.
The shared query layer independently checks observed house and teaching offer, so passing a current index to a
Discovery query cannot disclose a new residence or a hidden new training topic. Guide rebuilds from current state;
switching back restores the observed index without writing discoveries. Quest contact links use the selected index
and require knowledge of the residence; a named quest contact alone does not award a current NPC address.

Existing saves remain readable. Their older boolean NPC discovery flags remain intact, but cannot prove where an
NPC was met: meet the contact again to record a location/offer. Existing Map Book trainer notes remain available.
Historical runtime-generated names and roaming/generated NPC placements are not yet a complete knowledge system.

Indoor house/service doors now use the same physical-site index as outdoor buildings. Their markers project through
the dungeon floor-plan transform, including NPC and trainer occupants. Existing connection features keep their own
exploration gating; this does not introduce a shortcut around dungeon fog or conditional travel.

#### Canonical physical-location repairs

82 `house_data.txt` map references were corrected against both packaged base event metadata and actual interactive
geometry. Only the map-ID column changed; other fields, IDs, stock rules, schedules and original TSV formatting were
preserved. Repairs include Stone City (18), Celeste (21), the Pit (21), Castle Harmondale's throne room and MM8 interior
venues in the Merchant House, Smuggler's Cove, faction headquarters and elemental prisons. These are shared content
repairs, not Atlas-only aliases. The Castle Harmondale throne room previously pointed at Emerald Island.

The repeatable content audit checks 137 supplied MM7/MM8 maps and 673 physical house IDs using the actual text-table,
Lua-metadata and geometry readers. Legacy `Dungeon Ent` transition/movie cards are not treated as usable venues.
One shared Adventurer's Inn service (1607, used on both Dagger Wound and Ravenshore) still needs multiple physical
placements; it is reported rather than assigned an arbitrary new map. Conditional services whose exported metadata
selects a construction/movie-card branch need richer event metadata and remain part of the completion work.

To prepare the data-only cache from a full runtime payload, without duplicating graphics/audio:

```bash
python3 tools/prepare_atlas_audit.py --assets /path/to/runtime/assets
OPENYAMM_ATLAS_AUDIT_ROOT="$PWD/build/atlas-audit" \
  ./build/tests/openyamm_unit_tests --source-file='*WorldDatabaseTests.cpp,*WorldGuideTests.cpp'
```

The cache tool respects package overlay priority, verifies reused file hashes and records package/entry provenance.
It extracted 322 files / 102.6 MiB locally. A repeat refreshed zero files and took less than one second; the manifest
also prevents removed package entries from surviving as stale audit inputs. This cache is not a launchable runtime.
The audit covers packaged base metadata tied to physical sites, not all conditional outcomes or unprovided maps.

#### Validation

Windows Release compiled successfully. Focused ASan/UBSan coverage passed 46 cases / 3,450 assertions, including the
packaged-content audit, high sparse topic slots, index ownership, full-range house IDs, legacy knowledge behavior,
current/observed privacy, quest links and indoor service positions. The existing external image-decoder vptr exclusion
in the focused sanitizer harness remains as described in the quest checkpoint.

Native scenarios under `tests/scenarios/atlas/` visited the actual Clan Leader's Hall and trainer house in MM8,
recorded visible training offers and verified save/load. A separate Talimere encounter verified automatic NPC
selection. The MM7 relocation scenario deliberately supplies scavenger/combat prerequisite flags as a controlled
hybrid fixture; it then uses the real castle entrance, tavern door, Butler reward event and Party relocation state.
It verifies the move from house 240 to 1169, keeps the last-met address at 240, and checks save/load and map changes.
It does not claim to test the full scavenger quest or castle combat.

Native AMD/OpenGL captures verified the tavern in Discovery, the throne-room marker in Guide, the return to the
remembered tavern, quest contact links and observed-only MM8 Bow training. Reloading a quicksave made after Atlas
browsing preserved both addresses and did not discover the Dwarf King. MM6/MM7/MM8 map lifecycle regressions passed.
Isolated campaigns/captures live in the local Windows development directory `atlas-npcs-20261018/`; user campaigns
and official installation data were not used as test-write targets. Android/controller validation remains pending.

The additional paired trainer-note regression uses the QA-only fixture under `tests/fixtures/atlas/`: a real
`CanShowTopic(315)` overlay hides Expert Bow. The real house/teacher interaction records no offer and creates no
map note 1105; the unchanged-runtime control records both. Both checks include save/load. The fixture preparer
creates an isolated package view with hard-linked unchanged large archives and a private mutable overlay ZIP;
original package contents remain unchanged. Native MM6/MM7/MM8 quest-dialogue save/load regressions also passed.

### Expanded campaign relationships, 2026-10-18

The shared quest relationship registry now covers 61 quests (26 MM7, 35 MM8), with 231 typed relationships.
The added content includes MM7 promotion destinations and MM8 local requests, delivery recipients, named items
and disclosed regional clues. [The content audit](ATLAS_QUEST_AUDIT.md) identifies the source handlers and
remaining gaps. This expands the initial 19-quest checkpoint; it is not full campaign coverage.

Relationships use merged IDs and the active mounted scripts instead of importing a separate community-guide
database. For example, the Rust potion request/reward resolves to Pole, while Hobert handles an individual
delivery; the journal's obsolete contact name remains visible as the original text. The MM7 Lich quest points
at Walls of Mist as specified by the actual dialogue, not an invented map named Proving Grounds. Named items
resolve to their current merged definitions, including MM7's own empty Lich Jar and Dragon Egg.

The Troll Homeland's precise map and the Ninja cipher's hidden tomb remain Guide-only. Accepted requests expose
their disclosed regions and contacts, but do not infer NPC addresses, reveal terrain or open unknown item-reference
descriptions. Conditional updates after solving the cipher, detailed delivery sites and other staged relationships
remain part of the full coverage work.

Focused ASan/UBSan coverage passed 47 cases / 3,563 assertions, including validation of all relationship targets
and real-dataset disclosure checks. Isolated native Windows scenarios accepted Languid's and Pascella's requests
in MM8 and Anthony's pilgrimage in MM7 through authored dialogue and preserved knowledge across save/load.
Native AMD/OpenGL captures checked the new quest details, Guide-to-Discovery transitions and navigation to
fully fogged Tatalia / the unexplored Smuggler's Cove plan in both worlds.
Records live under the local Windows development directory `atlas-quest-content-20261018/`; user saves were not
test targets. Android/controller checks and full campaign coverage remain pending.

### Conditional clues and retained quest knowledge, 2026-10-18

Quest relationships now accept optional required/forbidden QBit lists through the shared JournalQuestTable.
Six-column rows remain valid; extended eight-column rows add both condition fields. Incomplete rows and contradictory
conditions reject the metadata transactionally. Condition QBit parsing uses bounded unsigned conversion consistently on
Windows and Linux. [MODDING.md](MODDING.md#shared-quest-relationships-and-learned-clues) documents the format.

Actual EventRuntime QBit changes evaluate both the old and new journal/relationship predicates. Learned facts
are stored in the existing named-global save namespace using semantic quest/role/kind/target keys. Clearing a
quest or changing a stage does not erase a previously learned destination. This tracking also runs with the
optional Atlas mod disabled; Atlas queries remain read-only. Legacy active quests can infer presently available
clues, but an old history flag does not manufacture unrecorded later clues.

Discovery filters conditional rows using that knowledge. Guide labels unlearned conditional rows and does not
mark them informed. Current content uses QBit 569 to reveal the Ninja quest's Hidden Tomb after the Erathian
obelisk puzzle, and QBit 69 for the Troll Homeland after its canonical discovery flag. Neither relationship
reveals explored terrain or an NPC's address. These two stages extend the previous campaign-data checkpoint;
complete campaign relationships and remaining conditions still need content work.

Focused ASan/UBSan tests passed 49 cases / 3,612 assertions, including conjunctions, exclusions, both sides of a
change, historical retention, unsigned parsing, transactional failures and both real conditional datasets.
Windows Release compiled successfully. The explicitly hybrid Ninja scenario supplies controlled promotion
prerequisites, then uses actual Stephan dialogue and all five physical Erathian puzzle-button interactions.
The exact tomb fact is absent before solving and present afterward, including save/load. Controlled chest and
removed-puzzle flags isolate the actual promotion reward and preservation after the quest disappears; this does
not claim to test its prerequisite quests, tomb combat or chests.

Native AMD/OpenGL captures verified Discovery before learning, after learning and after quest completion with
the original stage flag cleared; Guide toggles preserve each view's knowledge. Native MM6/MM7/MM8 quest-dialogue
save/load regressions also passed. Isolated records are under the local Windows development directory
`atlas-clues-20261018/`. No generated scripts, save-format version or user campaign files changed. Android and
controller verification remains pending.

### Directed walking routes and destination navigation, 2026-10-18

The continent overview's Travel filter now draws directional walking links from the active MapStats edge
definitions. Distinct exits remain distinct: Harmondale's north/east paths to Tularean Forest keep their own
durations and arrows. Reverse connections are evaluated independently. Selected routes are highlighted while
the overview stays open; `View destination` opens the corresponding native Atlas map without travelling there.
The same destination navigation is available on existing scheduled-transport reference cards.

WorldDatabase supplies a read-only walking graph from canonical map records and existing presentation anchors.
Both endpoints and their anchors must be known in Discovery. Missing anchors do not create invented coordinates;
the route remains available in the list. No regional terrain, resident or service knowledge is awarded by browsing
the graph or following its destination link. Guide-to-Discovery switches preserve these boundaries.

Walking quest eligibility now uses one shared TravelRuntime predicate in both the actual outdoor boundary
transition and the World Database. The existing ANY-of-QBits behavior is preserved. Locked walking links remain
Guide-only, with the selected route reporting its quest requirement. Avlee's Shoals entry and the separate return
exits retain their canonical asymmetry. Surface requirements, including water access, remain separately displayed
and enforced by the existing travel runtime; a quest-unlocked line is not a claim that every surface condition
is satisfied at the party's current position.

This graph covers walking transitions. Boats/coaches still use the earlier reference index; scripted destination
overrides, observed schedules, prices, route graphics and dynamic availability need further integration. It does
not yet replace that remaining work with an inferred or simulated transport timetable. Teleports, Barrows and
Regna graphs retain their separate remaining requirements in the coverage matrix.

Focused ASan/UBSan coverage passed 51 cases / 3,680 assertions. The canonical MM7/MM8 datasets check endpoint
privacy, distinct directional exits, the three alternative Shoals unlock bits, unrelated flags, missing anchors
and world separation. Windows Release compiled successfully. A faithful native scenario walked into the actual
north Harmondale boundary, opened its travel confirmation and reached Tularean Forest through the normal handler.
The visit/save fixtures used for Atlas UI capture do not claim to perform those journeys themselves.

Native AMD/OpenGL captures checked both worlds' known/Guide graphs, selected directions, destination navigation,
retained regional fog and return to gameplay. Isolated runs are retained in the local Windows development
directory `atlas-travel-20261018/`. Android/controller validation and complete Atlas coverage remain pending.

### Observed transport offers and current local routing, 2026-10-18

Discovery now indexes the actual offers presented by a transport agent: destination, scheduled weekdays,
quoted fare and personal travel duration. The typed `TransportOffer` comes from the existing house-service
calculation, including reputation, Merchant skill and follower travel reductions. The Atlas does not parse
translated button labels or maintain a second pricing formula. This shared observation runs even when the
optional Atlas module is disabled.

Each house/route slot keeps its last offered values. An unobserved story change cannot silently replace them;
revisiting the agent updates only the slots actually presented. An offer learns the destination's existence,
not its terrain, buildings, residents or visited status. Unvisited destinations stay black and cannot receive
Atlas notes. Older saves must revisit agents to learn their offers; a recorded house visit alone does not
invent prices or schedules.

Guide uses live route overrides for houses on the party's current map. Other maps remain explicitly labelled
`Authored timetable`; no remote Lua is executed and old map snapshots are not treated as current predictions.
`Current timetable` and authored rows show base duration, while `Last offered` rows show the actual quoted
duration and gold. Reading Guide or switching catalogs does not record these offers.

State reuses the existing session/runtime named globals, without a save-format bump:
`openyamm.discovery.transport/<house>/<slot>/` owns `seen`, `map/<canonical-id>`, `days`, `week`, `required`,
`quoted_days` and `gold`. Replacing a slot explicitly zeros its previous destination keys so the existing
state merge retires them. Independently learned geography remains known. Missing, ambiguous or invalid
saved offer payloads are reported and excluded instead of falling back to an authored schedule.

The native Windows Release build and focused transport tests passed. The MM7 hybrid scenario
`tests/scenarios/atlas/mm7_transport_observation.yml` opens Royal Steeds through its real door, saves the Emerald
Island quote, applies a controlled scavenger-completion fixture, reloads through the authored on-load reroute,
and proves knowledge changes to Bracada only after the next visit. It does not test the scavenger quest chain.
The faithful MM8 `mm8_transport_observation.yml` opens Windling's real ship dialogue, learns Ravenshore and
preserves the four-day, 16-gold fixture quote through save/load. The MM7 fixture quote is six days/eight gold;
these are party-adjusted test outcomes, not universal prices.

Native AMD/OpenGL captures verify remembered versus current schedules, Guide/Discovery switching, quoted
prices, fully fogged unvisited destinations and return to gameplay. Records are isolated under the local
Windows development directory `atlas-transport-20261018/`. Existing MM6/MM7/MM8 quest-dialogue save/load scenarios
also passed after the shared observation hook. Focused booking tests compare the structured quote with gold
actually spent and game time advanced, including follower reductions. Android/controller verification, drawn
boat/coach routes, remote conditional timetable references, arrival points and full guide coverage remain pending.

Final focused Atlas validation passed 54 cases / 3,795 assertions on native Windows and Linux ASan/UBSan.
It includes atomic rejection of invalid offers, invalid saved schedules, canonical IDs, unsigned QBit round trips,
and the existing map/content audits. The separate house-transport selection passed eight cases / 110 assertions.
These are focused selections, not a full-suite or complete-campaign result.

### Coach/boat graph and departure markers, 2026-10-18

The continent Travel view now draws walking, coach and boat connections through one shared `travelLinks` query.
Solid lines denote walking, dashed lines coaches and dotted lines boats; arrows retain departure direction.
Parallel services and opposite directions retain distinct route IDs/lanes. The selected list row highlights its
own connection. Lines follow the same search and Discovery/Guide rules as the list; unknown endpoints or missing
presentation anchors do not receive invented lines. The region anchors indicate broad connections, not exact
roads, sea lanes or points of arrival.
The complete network stays subdued over the continent art; an inspected connection is drawn last with a dark
edge so its direction remains readable on bright terrain.

Discovery draws only saved, last-offered services from the transport checkpoint above. Guide uses the explicitly
current/authored schedule index, including required-QBit styling. A remembered quote is not coloured as a live
availability forecast. Cross-world services stay outside a local-world graph.

Regional Travel views now expose departure pins through the same event-backed physical house locations as
buildings: `C` for coaches and `B` for boats, retaining the compact badge and larger hit area. Selecting a
transport pin narrows the service list. Other filters retain their previous house/resident marker behaviour;
a visited building does not disclose unknown transport routes.

The focused native Windows and ASan/UBSan suites pass 55 cases / 3,873 assertions. Added canonical MM7/MM8 checks
cover coach/boat identity, known endpoints, missing anchors, search, distinct historical slots, departure-record
selection, Guide quest locks and world separation. Native Windows Release compilation passed; graphical checks
use isolated copies of the previously exercised MM7 Royal Steeds and MM8 Windling saves under
`atlas-transport-graph-20261018/`. Android/controller checks and the remaining coverage matrix remain open.
