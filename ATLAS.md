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
  comfortable pointer/touch selection. Marker glyphs use the small reading font; the party arrow uses 14 logical pixels.
- Buildings require a real visit and trainers require meeting their resident. Dungeon entrances require the
  exact entrance cell to be fully explored, not merely dimly revealed. Learning an entrance never grants its
  interior map or reveals other entrances. Guide Mode shows authored markers without changing discoveries.
- Paused trainer details reuse the gameplay training evaluator for the selected character, including price,
  class/promotion limits and skill requirements. This inspection never trains a character or spends gold.
- Zoom, drag, search input, native button focus and scroll controls. Click the gameplay minimap to open the Map Book,
  then use its sidebar's Atlas button. Closing the Atlas returns to that Map Book view.
- Guidebook pages for items/equipment, alchemy, spells, classes/races, quest relationships and companions use
  the active shared registries and discovery history. They do not install a copied community-guide database.
- Dungeon controls select known height bands, all heights or the party height, with matching site/arrow filtering
  and optional note elevations. Known overlapping horizontal surfaces refine their own band into smaller slices.
  These remain height filters; complex slopes and mechanism-driven geometry still need wider audit.

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

This is not yet the complete external guide equivalent. The current scopes and outstanding content cases are
recorded in [ATLAS_COVERAGE.md](ATLAS_COVERAGE.md); optional extensions require a decision under
[ATLAS_DECISIONS.md](ATLAS_DECISIONS.md). Existing campaign relationships, Barrows/Regna graphs, observed
site/chest states, item/ingredient sources, guild observations and companion contacts have substantial native
coverage. Scripted monster sources, remaining conditional/handwritten declarations and wider content checks
are still incomplete. The guidebook also covers reference pages and campaign trackers. Player notes are editable
in the Atlas and existing Map Book.
Dungeon plans provide an all-height overview and filtered known height bands with explicit note elevations.
The index uses current or last-observed
resident locations/topics, but conditional/shared venues, generated/roaming NPCs and more information learned from
books or rumours require additional shared integration. Dynamically moved entrances require runtime metadata.
Sites cover explicit map-local event bindings and inherited decoration/global-event bindings through the shared
runtime binding rules. Conditional effects and source semantics still have the gaps listed in ATLAS_COVERAGE.md.
The shared native code has an ARM64 Android Release build. Version 0.2.0 is installed and running on the test
phone with both mods loaded. Atlas-specific physical acceptance remains separate from that startup proof;
see [Android QA](ATLAS_ANDROID_QA.md). Controller acceptance is still open.

The dated checkpoints below retain their historical validation scope. Use this overview and the coverage/QA
documents for current status; an old checkpoint's list of missing functions is not a new completion requirement.

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
A sparse checkout must also materialize the small upstream courtyard binding overlay before `build`:

```bash
git restore --source HEAD --worktree --ignore-skip-worktree-bits \
  assets_dev/worlds/mm7/maps/7d29_dimension_travel.scene.yml
```

This restores that tracked data file only; it does not check out the multi-GB maps directories. The Windows build
overlay includes it because the bootstrap public payload lacks this upstream scene supplement. Its six water-face
bindings are required for the Castle Harmondale dimensional fountain and its Atlas site.

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

### Spellbooks and eligible guild sources, 2026-10-18

Items and Spells now link in both directions through each book's actual spell encoding. Discovery includes a
spell reference when the party learned the spell or has encountered its book; reference navigation never teaches
the spell. Unknown books are not listed merely because the spell is known. Spell schools use the shared casting-skill
resolver: the TXT `Res` field describes damage/resistance type, not a spell school. In particular, Torch Light
belongs to Fire Magic rather than a school named `none`.

Spell pages link to eligible guilds within the viewed world. Discovery requires a real recorded guild visit and
known containing geography, rather than revealing every guild on an explored continent. Guide can inspect the
world's stock-rule references without recording discoveries. Links select the same shared house/map records used
by the main Atlas; no guild locations or item definitions are duplicated.

The existing stock generator's school ranges and value/treasure-weight selection now live in the shared
`GuildSpellbookRules` evaluator. Both gameplay stock generation and the Atlas use this evaluator, including
quality limits and `NoShop` exclusions. Deterministic vendor pages bypass the random guild pool. Spell encodings
must be complete unsigned IDs, so malformed suffixes or overflowing values cannot masquerade as another spell.

These are explicitly possible stock sources, not predictions of current shelves, membership/access or the
selected character's ability to learn a spell. Reading the Atlas does not generate, refresh or reserve stock,
advance the RNG, spend gold or grant membership. Observed shelf contents, live access/learning evaluation and
conditional/unused venue audit remain in the full coverage work.

Focused ASan/UBSan validation passed 57 cases / 3,997 assertions. Native Windows focused validation passed
59 cases / 4,038 assertions, including the existing real guild stock generation/purchase regression and agreement
between generated books and the shared candidate evaluator. Canonical MM7/MM8 tests cover world isolation,
unvisited-guild privacy, book/spell links, known-book discovery and unchanged exploration/learned-spell state.
Faithful native scenarios `mm7_guild_discovery.yml` and `mm8_guild_discovery.yml` enter the actual Harmondale Fire
Guild and Cures and Curses doors and preserve their discoveries through save/load. They do not test gaining
membership or acquiring a book. Separate owned-book fixtures are explicitly hybrid UI setup, not evidence of
performing those acquisition quests. Isolated records are retained under `atlas-guilds-20261018/` in the Windows
development directory. Android/controller testing and complete campaign coverage remain pending.

Native AMD/OpenGL checks cover book-to-spell navigation with the selected list row visible, visited/unvisited
guild source lists, Guide-to-Discovery privacy, the reverse book link, guild-map navigation with retained fog
and return to gameplay. Known-book fixtures remain isolated copies; original campaigns are not test targets.

### Race mastery and promotion comparisons, 2026-10-18

Class reference pages now compare race-adjusted mastery limits with the base class through the existing
`ClassSkillTable::getEffectiveCap` evaluator. Race IDs/names come from the loaded character-selection registry;
the read-only enumeration supports sparse mod IDs rather than assuming a built-in race count or contiguous IDs.
A changed cap gets an exact teacher-rank search link. These links search the existing trainer index, not grant a
rank or assert that a teacher for every racial ability exists.

Discovery uses only classes and races represented in the current party. Guide includes the full loaded registry.
Inspecting an unfamiliar class/race or following a Guide link does not add it to Discovery, change party members
or modify their skills. Persistent history of earlier classes/races remains future work; old saves still receive
the same current-party view.

The class registry exposes its existing metadata-only next-tier query. Earlier-tier links invert those same
relationships, while next-tier links compare base HP/SP and per-level growth using the current, potentially
patched class definitions. The query does not invent a legacy named promotion if metadata is absent, and the
maximum unsigned progression step cannot wrap into a false edge to a base class. Promotion references are family
relationships, not permission to promote: campaign quests, branch choices, racial transformations and character
creation restrictions still apply and are not inferred from the reference.

Native class links use the shared reference navigation and scroll the selected target into view. The global
merged registry is the same while browsing MM7 or MM8; this does not replace its mechanics with vanilla campaign
rules. At this checkpoint a full comparison grid, starting choices and links to actual promotion quest conditions
remained open; the following checkpoint adds the grid.

Focused ASan/UBSan validation passed 60 cases / 4,061 assertions. Native Windows focused tests passed 64 cases /
4,142 assertions, including the existing race-cap, class-kind exception, warrior/mana-mode exception and character
creation-choice regressions. Added tests use a custom class family and race ID 97, verify discovery privacy,
exact teacher-topic ranks, promotion growth after typed HP patching, reverse family links and overflow boundaries.
Canonical table checks cover the Knight/Cavalier branches and the actual Human Learning modifier. Native GPU
records use isolated copies of the previous MM7/MM8 fixture saves under `atlas-classes-20261018/`; Android/controller
checks and complete Atlas coverage remain pending.

Native AMD/OpenGL tours in both worlds verified the Human Cleric modifier, exact-rank trainer search, navigation
Cleric -> Priest -> Priest of the Light, selected-row scrolling, and Guide -> Discovery returning to the sole
current Cleric entry. Both tours resumed the same gameplay session; they do not perform promotion quests or
change the character's class.


### Native class comparison grid, 2026-10-18

`Guidebook -> Classes -> select class -> Compare` opens a native table derived from the same active class and
skill registries. The selected class remains the first column, alongside up to three related classes per page;
additional branches remain available on later pages. Column headings open the normal class detail page. The
metadata family is traversed in both directions, without named legacy fallbacks or unrelated class families.
Discovery filters columns after traversal, allowing two known tiers to be compared without disclosing an unknown
intermediate class. Changing Guide/Discovery, world or reference section discards the comparison.

The race arrows cycle through Base class and the permitted race names from the actual registry, including sparse
mod IDs. Human ID zero is distinct from the Base option. Discovery offers only current-party races and classes;
Guide can compare the full family and every registered race. A request for an unknown race or hidden anchor is
rejected, rather than silently showing base caps or leaking options. Reading or changing the table does not award
discoveries, grant a skill, modify the party or change save data.

Rows show base HP/SP, HP/SP per level and mastery caps from `getClassCap` or `getEffectiveCap`. Numeric growth
differences use the pinned class as baseline with widened subtraction; caps use the same shared gameplay evaluator.
Skills unavailable to all compared columns are omitted. The search field filters skill names, and the body uses
the shared native wheel/touch/scrollbar behavior in logical units. Definitions reflect loaded mods and the merged
game, not a separate vanilla database. Choosing a race is an inspection option: creation, transformation and
promotion quest/branch eligibility are not inferred or bypassed.

Focused ASan/UBSan checks passed 61 cases / 4,121 assertions. Native Windows focused tests passed 65 cases /
4,202 assertions, including the existing racial-cap and creation-choice regressions. Added coverage checks a
six-class custom family, all branches, a known later tier behind a hidden intermediate, sparse race 97,
base-versus-Human-zero semantics, invalid inputs, modded HP growth and read-only Discovery behavior.

Native AMD/OpenGL tours for MM7 and MM8 use isolated one-Human-Cleric fixtures under
`atlas-class-grid-20261018/` in the Windows development directory. The fixtures are explicitly preseeded UI test
campaigns, not proof of completing promotion quests. Captures cover base and Human caps, Learning filtering,
both Cleric-family pages, race changes, the bottom of the skill table, heading navigation to Priest of the Light,
return to Discovery with only the current Cleric, and resumed gameplay. Android/controller checks and the rest of
the complete Atlas coverage matrix remain pending.


### Item sources and retained observations, 2026-10-19

Item pages now link to a shared `WorldDatabase` item-source index. The Guide's **Scan sources** action reads the
viewed world's mounted maps incrementally, one map per frame, through the normal headless asset loader. The
existing event/geometry index supplies physical chest links. Placed items and explicitly carried actor items get
native `I` source markers; fixed chest entries link to a known physical chest when available, otherwise to its
containing map. Source links return to the normal item page through **Item details**. Source coordinates distinguish
multiple placements of identically named items. The reference retains a selected ID across mode changes only if
that ID remains visible, avoiding an accidental switch to a different Merge item with the same display name.

The reader inspects authored item payloads and fixed positive chest IDs. It excludes negative random-treasure
placeholders, invalid item IDs, gold, hidden policy objects and loot-container sprites. It does not materialize
chests, generate artifact rolls, advance campaign RNG or modify the party. This catalog describes authored source
possibilities, not remaining stock, scripted eligibility, a respawn promise or the current position of an actor.
Scripted rewards/drops, general semantic source policies and their conditional availability remain separate
coverage work; this is not a claim of every possible source for every item.

Discovery receives no authored-source entitlement from merely visiting or exploring a map. Successful pickups
record the actual item's position immediately before removal through the shared interaction controller. Indoor/outdoor hooks
supply that position before removal, including inventory and held-item destinations. Opening a real chest records
only its visible materialized slots, excluding gold and hidden overflow. A chest record is historical evidence
of seeing an item there, not a live inventory or a claim that the party took it. The last pickup per item/map and
previously seen chest items remain available even after an item leaves the inventory.

The existing named-map state stores facts under `openyamm.discovery.item/<kind>/<source>/<item>/...`. Canonical
numeric keys, complete pickup coordinates, finite bounded positions and nonnegative timestamps are required.
There is no save-format change; the existing shared knowledge preservation also carries these facts over a map
refill. An old save receives no invented historical sources. Recording remains active when the optional Atlas UI
module is disabled. Guide inspection does not write observations or reveal terrain. The reference list/detail
viewports now consistently use the native scroll helper's logical units, avoiding an empty overscroll area beneath
long source lists.

An indoor map can frame known pickups, notes, observations and the current party before its first wall outline
has been revealed. Only visible features contribute points; undiscovered geometry supplies neither lines nor
bounds. A lone known point uses a small blank coordinate frame. Partly explored maps also include known points
outside the revealed wall bounds. This keeps a recorded source selectable without exposing the dungeon layout.
Invalid positions are ignored, and drawing the view leaves exploration state unchanged.

Focused ASan/UBSan checks passed 64 cases / 4,200 assertions; the native Windows focused suite passed 68 cases /
4,281 assertions. Added coverage checks fixed versus random chest entries, duplicate slots, hidden objects,
sparse/corrupt saved keys, invalid coordinates/times, visible versus overflow items, refill retention, source
re-indexing, Discovery filtering, native feature links, known-point dungeon framing and read-only queries. These are focused suites, not a full
project test run.

The faithful scenarios `tests/scenarios/atlas/mm7_item_sources.yml` and `mm8_item_sources.yml` use actual gameplay
interactions: a Harmondale Phirna Root pickup, an Abandoned Temple diamond pickup and opening chest 19. The latter
observes the Idol of the Snake. Both verify their source facts after save/load; they do not grant test items or
populate observations directly. An isolated repeat with `openyamm.interactive-atlas` disabled also passed the MM7
pickup/save/load flow. Scenario starting parties remain harness fixtures, not a claim of a vanilla playthrough.

Native AMD/OpenGL captures and disposable campaign copies are retained under `atlas-items-20261019/` in the
Windows development directory. They cover the source map with retained fog, return to the item, complete source
scans for the viewed MM7/MM8 map registries, the last rows of long source lists, known-ID mode switching, observed
chest-item references and resumed gameplay. Android/controller checks and complete Atlas coverage remain pending.

### Scripted reward sources and confirmed receipts, 2026-10-19

Guide now indexes declared possible fixed rewards from the actual loaded local and shared Lua programs.
Map callbacks link to their physical interaction sites where available, otherwise to their map. Shared callbacks
link to NPCs whose current resolved topic slots actually reference them. Current relocation, availability and
sparse topic overrides remain authoritative. Conditional possibilities do not assert quest eligibility or a
remaining reward. Replacing or removing a callback invalidates its previous declaration by callback identity;
appended/replacement callbacks must explicitly declare their own complete reward possibilities.

The exporter emits 320 declarations: 125 in MM7 map programs, 20 in MM8, 95 in MM6, and 80 in the shared global
program. These are declarations before runtime overlays, not 320 guaranteed active rewards. It follows reachable
fixed Add/Set/GiveItem operations and separates synthetic timer/on-load continuations from direct interaction
handlers. Random rolls are not generated or advertised as fixed rewards. Handwritten overlays without declarations,
actor drops, general semantic source policies and conditional availability remain coverage work.

Confirmed receipt tracking is independent of these declarations. Lua inventory additions, fixed/random GiveItem,
SetHeldItem and direct GivePartyMemberItem carry the executing event's identity. Queued requests save temporary
origin records under `openyamm.item-grant/`; they become `openyamm.discovery.item/map-reward/` or `shared-reward/`
facts only after successful inventory or held-item delivery. A full inventory, an unexecuted branch or merely
viewing the Guide grants no discovery. Direct successful inventory grants record immediately. The source is the
map/event; historical knowledge never guesses which topic-sharing NPC delivered an item. Old queues without
provenance receive no fabricated origin. Existing save serialization retains pending origins and received facts;
refill retains only discoveries. Tracking also works with the Atlas UI module disabled.

Regeneration uses `tools/regenerate_lua_events.sh`, with an optional existing native `OPENYAMM_BUILD_DIR`.
Its reference-directory checks and explicit MM8 configuration prevent silently dropping required decompiled
object-type resolutions. The exporter now produces separate `_00_item_rewards.lua` overlays, ordered after the
base callbacks and before gameplay supplements. `--item-rewards-only` refreshes these declarations without
rewriting any callback, string table annotation or lighting dependency. Full regeneration still exports both
callbacks and their reward metadata. Empty generated overlays replace obsolete generated declarations.

The packaged bakes hash their source callback files. Initial inline annotations correctly triggered that
validation; the final separate-overlay design preserves the original callback bytes and all existing baked
lighting checks. No bake hash was rewritten, no lighting check was disabled, and no generated callback was
hand-edited. Unrelated STR labels and entrance-card differences found during full regeneration were left out
of this reward-only installation. They require their own content regeneration and validation.

Native validation exposed an existing package-precedence bug: mounting the active world ahead of `assets.zip`
hid updated world scripts, even at canonical paths. The package mount now preserves the explicit overlay above
all world ZIPs; active-world selection still follows namespace expansion. A regression test fails on the old
mount order across MM6 -> MM7 -> MM6 and verifies both active and inactive namespaces. The Windows development
overlay now includes current event programs and their common Lua support, without rewriting multi-GB world ZIPs.

The lighting producer validates the reserved generated reward-only format and excludes it from future shadow
bake inputs. Its tests prove that reward changes leave shadow cards/dependencies identical and reject executable
visual changes hidden in the reserved filename. Ordinary gameplay scripts retain their complete dependency hashes.
Callback origin guards restore the outer reward source after nested map/global calls, including caught Lua errors.
The loader explicitly prioritizes base reward declarations before every gameplay overlay; `.fixup.lua` and numeric
prefixes cannot attach stale base rewards to replacement callbacks. A real filesystem/loader test covers removal
and explicit redeclaration for both map and global scopes.


Checkpoint validation used the native Windows Release build: 193 focused cases / 6,876 assertions passed across
reward export/metadata, World Database/Guide, party/save state, filesystem mounts, content manifests and Mod Loader.
Linux ASan/UBSan coverage passed 66 cases / 4,263 assertions; the isolated filesystem suite passed 31 / 265.
All 18 lighting-tool unit tests passed without executing a bake. Invalid scope registries are rejected without a
Lua panic, and pending origins retain the full uint32 item-ID range.

The broader scripted-map suite is not fully green: 88 of 95 cases passed, with seven failures / 134 failed
assertions concerning Breach content, dimensional-travel geometry, sprite/palette fixtures, absent MM9 payload
and a legacy-only d06 companion. Repeating those seven cases with original common Lua support and no generated
reward overlays reproduced the same seven failures / 134 assertions. This is a data-baseline control using the
current binary, not an old-C++ baseline, and does not establish that every existing failure predates this work.
The regression table/map fixtures now share the process-wide PhysicsFS owner and honor the supplied runtime root.

Reward-only regeneration reproduced all 205 metadata overlays exactly after LF normalization and left all 278
existing callback/supplement files byte-for-byte unchanged. The native Release target rebuilt successfully, and
both callback-origin runtime tests passed (2 cases / 36 assertions). Faithful MM7 fruit-picking and MM8 Dadeross
letter scenarios passed their delivery and save/load assertions on that executable. The isolated Atlas-disabled
MM7 repeat and MM6 quest-knowledge regression also passed during this checkpoint.

Native AMD/OpenGL tours under `atlas-rewards-20261019/` in the Windows development directory cover both games:
confirmed reward in Discovery, possible sources in Guide, native source-map navigation, return to the item and
Discovery after Guide inspection. The runs require all five captures to be newly written. Source inspection
neither records new reward history nor grants discovery. Android/controller testing and the remaining coverage
matrix are still open; this checkpoint is not completion of either whole Atlas.

A final native scan found one separate development-payload mismatch: the installed Ravenshore bake still hashed
an older `out02_mmmerge.lua`, while the source supplement already added dimensional travel. The matching upstream
`out02.lighting` and `out02.bake.json` from the current checkout are now included in the development overlay.
The native geometry hash and all 256 baked dependencies match that source set. These are existing tracked upstream
assets, not newly generated or modified binaries; no bake hashes or validation rules were changed. Sparse Windows
development checkouts must hydrate these two files before running the build helper.

After synchronizing the Ravenshore bake/recipe, the final MM7 and MM8 native tours both completed their fresh
captures with no map-marker indexing failures, no item-source scan failures and no stale-lighting rejection.
MM8 now reports `Sources ready`. Repeating the broader scripted-map suite with this matching payload still
reports 88/95 passing and the same seven cases / 134 failed assertions listed above.


### Creature item history and handwritten reward declarations, 2026-10-20

Actual item removal from a creature now records shared `openyamm.discovery.item/carried/<actor>/<item>/`
coordinates and time. The shared recorder reads the current runtime actor position, not the authored spawn or
party position. Both indoor and outdoor corpse-item removal and successful inventory delivery after stealing
use that recorder. Failed theft, a blocked pickup, viewing a creature, gold and untaken contents add no item fact.
Invalid/unresolvable actor indices, nonfinite positions and legacy separate-summon views cannot invent a map
actor origin. The existing named-map serializer and refill knowledge retention preserve these historical facts;
old campaigns receive no reconstructed loot history. The optional Atlas UI remains independent of recording.

Discovery item details link these facts to a native item-source feature and retain the source when returning
from the map. The label is `Taken from creature`, covering both corpse loot and successful theft. This history
is neither a guaranteed random drop nor a current inventory/availability check. Loot is generated only by the
normal gameplay interaction. Corpse auto-loot also captures its source index before taking the final item, so
its debug trace no longer reads a corpse view that the take operation may have destroyed.

Handwritten MM7 script owners now declare five replacement/appended callback rewards explicitly: Colony Zod's
key (376/1463), the Haunted Mansion portrait (376/1423), Malwick's wand (769/947), and the Master Archer and
Sniper promotion bows (816 and 820 / 1345). Generated EVT callbacks and their lighting hashes are unchanged.
Other handwritten sources, guaranteed scripted drops, semantic item policies and conditional-source completeness
remain in the coverage matrix; these five declarations are not a claim of complete reward coverage.

Native Windows validation passed 193 focused Atlas/loader cases / 6,881 assertions and seven targeted creature,
handwritten-reward and callback-origin cases / 729 assertions. Linux Atlas ASan/UBSan checks passed 66 / 4,268.
The expanded selection including GameplayRuleRegressionTests is not fully green: 384 of 395 cases passed,
with eleven fixture/setup failures from missing loose scene/geometry inputs or a second PhysicsFS initialization.
The full report is retained as `build/atlas-reference/quests/actor-sources-native-tests-full.log`; no whole-project
pass is claimed. The previously documented broader scripted-map limitations remain open.

The faithful `mm7_creature_item_source.yml` and `mm8_creature_item_source.yml` scenarios take an authored Longbow
in the Temple of the Moon and Sword of Whistlebone in Garrote Gorge, then verify the recorded position after
save/load. Their CombatLoot action applies deterministic test damage to an existing authored actor and uses the
normal corpse interaction; it does not grant the expected item or write discovery facts. The recorded runtime X
positions are -5586 and -17050, respectively, differing from the static spawn coordinates. These are
isolated interaction fixtures, not a claim of a balanced vanilla playthrough. Repeating MM7 with the Atlas UI
module disabled also passes (only `karol.test` enabled).

Native AMD/OpenGL captures under `atlas-actors-20261020/` in the Windows development directory show the Discovery
item, its source map with retained fog/party arrow, and return to the same item in both games. All three images
per game are checked for fresh writes and were visually inspected. Android/controller checks and complete Atlas
coverage remain pending.


### Dungeon elevation controls, 2026-10-20

Dungeon maps now offer Lower, Higher, All and Party height controls. Layers are 512-world-unit height bands
centred at multiples of 512, using the same native `IndoorOutline.z` elevation reference as the dungeon minimap.
They are height filters, not authored or inferred semantic floor names. All heights retains the previous overview.
The selected band filters outlines, physical service pins, located service records, feature pins and the feature
list. A reference link to a known positioned feature selects its band; selecting a venue restores All so that
navigation does not land on a hidden pin. Existing two-dimensional map notes remain visible on all bands.
The later checkpoint below adds explicit note heights; more complex overlapping-floor separation remains coverage work.

Discovery derives available bands solely from revealed/non-hidden outlines, visible known features and the live
party's known height. Unrevealed geometry supplies neither bands nor bounds. Guide uses the full available plan;
returning to Discovery resets the band and rebuilds its available heights from party knowledge. Raster bounds
stay anchored to all already-known outlines/positions while changing a band, so adjacent layers share the same
projection. Reading or changing the filter does not write exploration state, quest state or item observations.

The live Atlas party projection now carries optional foot Z. The party arrow appears only on its selected band
(or All), and Party height selects that band without revealing any nearby geometry. Source/party points can still
frame an otherwise blank plan. The controls occupy their own bottom rail, clear of the map and its markers;
additional raster padding keeps the party arrow inside the map. Layers are local screen state. Neither the save
schema nor gameplay movement changed, and the optional Atlas module remains independent of shared discovery.

Native visual checks exposed zero-ID authored item sources: an empty carried-item field was accepted because
merged table row zero has a placeholder name. The source builder and common source index now reject ID zero,
and the item reference also excludes that sentinel. Regression coverage supplies an empty actor beside a real
carried-item source; this fixes the authoritative derived index rather than hiding a misleading `0` label.

Windows Release validation passed 169 focused cases / 6,543 assertions across World Database/Guide, party/save,
filesystem mounts, content manifests and Mod Loader. Linux Atlas ASan/UBSan passed 68 cases / 4,296 assertions.
Height-specific tests cover negative heights/band boundaries, invalid inputs, stacked outlines, stable projection,
strict hidden-layer filtering, unchanged discovery bits and physical service Z. These are focused suites; previous
full-regression fixture limitations remain open.

The reusable native input tour is `tools/menu_tours/atlas_height_layers.yml`, with shared design-canvas coordinates
and output under `output/atlas-height-layers` in the isolated runtime. Load an isolated dungeon save and set
`debug.menu_input_tour_path` to that file as described in `tools/RUN_GAME.md`. Native AMD/OpenGL runs under
`atlas-heights-20261020/` in the Windows development directory use the Temple of the Moon (MM7) and Abandoned
Temple (MM8), covering known overview, party height, higher height, full Guide, two Guide bands, Guide party
height and return to Discovery. Each run requires all eight captures to be newly written. The inspected images
confirm that geometry changes by height, the party arrow follows its band, and leaving Guide restores the hidden
plan. Android/controller checks and the complete Atlas coverage matrix remain pending.


### Height-aware dungeon notes, 2026-10-20

User map notes can now carry an optional absolute world Z through the shared `WorldMapNotes` API. Core stores
it in the existing named-map state as `openyamm.map-notes.user/<id>/z`, alongside the ownership marker and normal
`RuntimeMapNote`. The serializer and refill knowledge retention need no new binary save version. Only owned
user notes read this metadata. Missing elevation means All heights, preserving old and script-owned notes without
inventing a floor. Zero is a valid assigned height; deletion clears the ownership and elevation keys, and creation
reserves IDs with orphaned elevation metadata rather than accidentally inheriting it.

Creating a dungeon note on a selected band assigns a representative height for that band. Creating one in All
leaves it unassigned. Editing preserves its exact saved elevation when changing text. The editor offers lower/higher
known bands, All, and the party's actual foot height when the party is in that dungeon. These choices use only
known bands in Discovery; they neither reveal geometry nor change gameplay position. Saving a height-assigned
note selects its band. Pins, list rows and note-link navigation use its saved height; notes without one remain
visible on every band. Dungeon note subtitles identify their band or All heights, including overlapping XY notes.

Windows Release compilation and 172 focused cases / 6,602 assertions passed. Linux Atlas ASan/UBSan passed
70 cases / 4,342 assertions. Coverage includes actual save serialization, old flat notes, script ownership,
explicit zero/reset, reserved metadata IDs, refill retention, shared feature filtering and extreme band conversion.
These are focused checks; the previously documented broader regression fixture limitations remain open.

The reusable native tour is `tools/menu_tours/atlas_note_heights.yml`, using isolated dungeon saves and ordinary
F9 quicksave / F10 quickload. Native Windows evidence is retained under `atlas-note-heights-20261020/` in the
development directory. Both 95-step AMD/OpenGL tours completed with 15 freshly written captures and a fresh
isolated quicksave per world. Inspected captures show the initial lower-band note hidden at party height,
text/height retained after reload, an All-heights note visible at party height, reassignment to actual party
height retained after another reload, and deletion still absent after a final reload. The MM7 fixture uses the
Temple of the Moon (initial band -1, party band 1); MM8 uses Abandoned Temple (initial band -2, party band -1).
The original user saves/settings were not modified. Android/controller checks and full Atlas coverage remain pending.


### Observed event connections and arrival points, 2026-10-20

Actual successful map-local event travel now records the arrival map, actual runtime XY/foot Z and game minute.
The shared recorder runs after loading/teleporting and before normal travel autosave; same-map teleports use it
as well. Loading failure, cancellation, a declared destination and debug map loading add no arrival. Runtime
`PendingMapMove::sourceMapEventId` and `sourceMapFileName` identify the executing map handler independently of
diagnostic trace fields. Capturing the source before queueing also preserves on-leave/deferred event ownership.
Global and local handlers with identical numeric IDs remain separate. The existing scoped handler guard also
handles nested calls and restores the caller after caught Lua errors; its fields now describe general handler
identity rather than only item rewards.

Destination map-local state stores `openyamm.discovery.arrival/<canonical-source-map>/event/<event-id>/`
with `seen`, integer `x/y/z` and elapsed `time`. Coordinates round to world units; time keeps completed minutes.
Each source-event/destination pair retains its latest arrival, so a conditional event can retain several known
destinations without turning one observation into a current destination guarantee. The normal serializer and
refill knowledge preservation retain these facts. Reads use the owning snapshot's map filename explicitly:
the runtime state's temporary map filename is not serialized. Old saves and restored queued moves without a
runtime handler origin receive no invented history. There is no binary save-version change or generated Lua edit.

World Database derives small `A` arrival markers and links source passages to their observed destinations in
latest-first order. Atlas offers View arrival, View origin and a next-destination control when several are known.
Arrival navigation selects the recorded feature and its height, including same-map teleports. An arrival record
does not establish a return passage. View origin restores the known overview, so a previous arrival-height filter
does not hide the origin on same-map links. Discovery retains its normal terrain fog; these facts frame known positions
without revealing nearby map geometry. Guide/Discovery viewing does not write observations. The recorder operates
with the optional Atlas module disabled.

Scheduled coach/boat arrival history, cross-world presentation, full conditional/Gate content audit and dedicated
Barrows/Regna graph views remain separate completion work. Broader Atlas and Android/controller work remains
listed in the coverage matrix.

Final verification on 2026-10-21 passed 175 focused Windows cases / 6,658 assertions, plus three Lua handler/reward
origin cases / 53 assertions. The latter distinguish same-number global/local callbacks, nested local calls and
caught Lua errors without regressing reward origins. The final shared data logic also passed Linux ASan/UBSan
coverage (72 cases / 4,387 assertions). These focused checks do not close the broader regression fixture limitations
documented above.

Faithful native scenarios `mm7_arrival.yml` and `mm8_arrival.yml` use normal exit interaction and confirmation in
the Temple of the Moon and Abandoned Temple, retaining actual arrival X values 15,816 and 21,519 after save/load.
The MM8 scenario also passes with the Atlas module disabled. `mm8_teleport_arrival.yml` activates the authored
Prison of the Lord of Earth portal (event 451), remains in `d39.blv`, records arrival X 47 and retains it after
save/load. These scenarios reposition an isolated party for interaction; they do not grant discovery state.

Native AMD/OpenGL evidence is retained under `atlas-arrivals-20261020/` in the Windows development directory.
`tools/menu_tours/atlas_arrivals.yml` covers the arrival record, origin map, source-to-arrival link and return from
Guide to Discovery in both games. `tools/menu_tours/atlas_teleport_arrival.yml` covers the same-map height change
and origin overview. Tours require fresh captures and isolated settings/saves. Android/controller checks remain open.
The two cross-map tours completed all 20 steps / four captures each. The same-map tour completed 17 steps /
four captures: its inspected images show the known source on band -7, the selected arrival on band 0 and
View origin restoring All heights. All twelve captures were freshly written; the inspected source/arrival and
Discovery images retain the current party's knowledge. These are AMD/OpenGL native checks, not Android evidence.

### Placed creatures and Bestiary map sources, 2026-10-21

Guide Sites now indexes individually placed actors from fresh package `.ddm`/`.dlv` companion data, alongside
encounter areas. Their names and monster definitions use the shared actor resolver; the existing import-ID rule was
moved from MapAssetLoader into that resolver without changing its precedence. Unknown positive stats identities
are not silently remapped to another valid monster. Pins retain the stable package actor index and original XYZ,
and explicitly describe an authored placement, not a living population or a promised current spawn.
The current-map fast path never passes its potentially mutated delta as authored data.

Discovery remains observation-only, even on fully explored terrain. Indoor and outdoor actor runtime states now
expose the actual unique-name index. Actual player picks/targets record that optional identity with the position,
base definition, alive/dead flag and time. Older observations without it remain unnamed; later unnamed observations
clear old names. Save/load and map-refill retention use the existing named-map variables, with no binary save-version
change. No package index or Guide operation creates discovery records.

Bestiary entries link back to visible placed actors, encounter areas and last observations. Source labels identify
the provenance, map and XY; links use the native feature selection and dungeon-height controls. Reference search
also checks visible source labels, so searching an observed name such as Jeric finds its base creature entry without
revealing unknown actors. Known observations
from previous maps are indexed directly from the party's map history, so their links do not depend on reopening the
map or loading its geometry. Guide's existing incremental item-source scan is now one shared map-source scan,
available from Items or Bestiary; it never rolls loot or generates encounters.

Validation on isolated settings and saves:

- Windows Release main build passed. The six focused World Database/Guide, Party/save, AssetFileSystem,
  ContentManifest and ModLoader suites passed 180 cases / 6732 assertions.
- Linux ASan/UBSan shared Atlas suites passed 76 cases / 4449 assertions, including authored-versus-observed privacy,
  actor-ID precedence, unique names, repeat indexing and cross-world history isolation.
- The existing faithful MM8 monster-observation scenario passed. The new
  `tests/scenarios/atlas/mm8_named_monster_observation.yml` contacted the actual Garrote Gorge actor 3, observed
  Jeric Whistlebone's identity, saved, changed map and reloaded it. It passed with the Atlas enabled and disabled.
- Native AMD OpenGL 3.3 tours produced 18 fresh final captures across MM7/MM8. Visual checks covered named
  observations, authored placements, Bestiary-to-map navigation, an MM7 dungeon link and returning from Guide
  to Discovery without disclosing Guide-only sources. A focused lookup tour also verified a named observation
  can be found directly in Bestiary before visiting Sites. Both full map-source scans reached Sources ready without
  marker/source-index failures. Reusable tours are `tools/menu_tours/atlas_monster_sources_mm7.yml` and
  `tools/menu_tours/atlas_monster_sources_mm8.yml`, plus `tools/menu_tours/atlas_monster_lookup.yml`;
  private logs/captures remain outside Git under
  `atlas-monsters-20261021` in the Windows development directory.

This is another checkpoint, not completion of the Atlas objective. Declarative scripted spawn sources, automatic
visibility discovery, variant navigation, broader campaign data auditing and Android/controller checks remain open.

### Bestiary variant navigation, 2026-10-21

Bestiary entries now link to the other explicit A/B/C picture tiers selected by the runtime's MonsterTable lookup.
IDs do not have to be adjacent. Noncanonical reused pictures and records without an explicit tier do not invent
families. Discovery offers a link only when the party already knows that exact target definition; Guide can expose
the other loaded tiers without changing knowledge. The native Monster link opens the target Bestiary entry and
scrolls its list into view. Variant links remain distinct from physical map-source links.

Windows Release built successfully; the six focused suites passed 181 cases / 6766 assertions. Linux ASan/UBSan
passed 77 cases / 4483 assertions. New coverage uses nonadjacent IDs, mixed-case pictures, reused-picture aliases,
untiered records and known/unknown variants. The native AMD OpenGL 3.3 tour produced five fresh final captures:
Discovery hid unknown Dragon Hunter/Crusader variants, Guide navigated from Dragonslayer to Crusader, and returning
to Discovery retained only the actual Dragonslayer observation. See `tools/menu_tours/atlas_monster_variants.yml`.
Captures and logs are private under the Windows development directory's `atlas-monsters-20261021`.

Scripted spawn declarations, automatic visibility discovery and a wider source/content audit still remain open.
This does not establish Android/controller validation or finish the complete Atlas objective.

### Last-inspected chest state, 2026-10-21

Core now records an actual chest view on opening, committed view changes and closing. The shared
`recordWorldChestView` keeps visible-item source history and separately records chest ID, visible slot count,
empty status and game time. A view with hidden overflow is never reported as empty; hidden item identities remain
unknown. Closing after taking the last item replaces the earlier inspection with the observed empty view. Failed
openings do not create a new view or inspection. No current population, chest materialization or random treasure
is read by the Atlas to fabricate this state.

Atlas collects these observations from the same owning map snapshots as other discoveries. A physical chest site
matches only its declared container IDs. Its footer reports the last inspection, or No recorded inspection; it does
not claim unseen contents or imply that an old observation is still current. Guide can display an unvisited chest
without creating an inspection record. Old saves without the new optional named-map fields remain unknown, even
if an older item-source history exists. Existing save serialization and discovery retention through refill apply.

Validation:

- Windows Release built. The six focused Atlas/save/package/mod suites passed 185 cases / 6816 assertions.
  ChestRegression passed another 17 cases / 198 assertions in a separate process. These fixtures must be separate:
  the existing cached RegressionGameData retains PhysFS initialization and otherwise makes seven ContentManifest
  initializations fail when ChestRegression and ContentManifest run in one process. No production workaround or
  test suppression was added.
- Linux ASan/UBSan passed 80 cases / 4525 assertions, including empty/overflow/restocked observations, strict keys,
  retained item history, read-only derivation and map/container ownership. Normal binary serialization of an empty
  observation is covered by the Windows Party suite.
- Faithful scenarios opened actual outdoor chests in Harmondale and Dagger Wound, closed them and retained their
  inspections through save/load. The extended MM8 item-source scenario opened Abandoned Temple chest 19,
  took its actual item 630 into the ordinary held-item state, closed the chest, verified it was observed empty,
  saved and reloaded. This passed with Atlas enabled and disabled; the historical item source remained recorded.
- Three native AMD OpenGL 3.3 tours produced twelve fresh captures of MM7 and MM8 indoor/outdoor observations,
  unknown Guide containers and return to Discovery. Four more MM8 captures verified the final unknown-state wording
  without making claims about old saves. Tours are `tools/menu_tours/atlas_chest_observations_mm7.yml` and
  `tools/menu_tours/atlas_chest_observations_mm8.yml`. Private logs, settings, saves and captures remain under the
  Windows development directory's `atlas-chests-20261021`.

Site effects, inherited decoration events, remaining source/content audits and Android/controller validation remain
on the coverage checklist. The complete MM7/MM8 Atlas objective is still open.

### Creature loot and alchemy source navigation, 2026-10-21

Bestiary and Items now have bidirectional creature-loot references derived from the active MonsterTable death-drop
rules and WorldDatabase carried-item sources. Guide labels configured percentage chances, authored carried items
and actually recovered loot separately, using current registry names rather than stale descriptive columns. A rule
has no invented physical map position and is not a guarantee of a drop from every creature. No RNG or treasure
materialization is involved in building the references.

Core records the actual base creature definition with successful corpse-item receipts. The optional source field
is preserved in existing map-local named variables and normal saves/refill history. Old records without it remain
unattributed; they are not joined to unseen actors or to monster-observation IDs, which can differ from actor indices.
An actual typed loot receipt can establish knowledge of the creature. Discovery links only recovered relationships,
not a configured rule inferred merely from knowing an item and monster independently. Guide browsing never creates
these discoveries. Loose death-drop items collected from the ground still lack creature provenance and remain on
the completion checklist.

Alchemy direct formulas now expose their alternative ingredient links too, not only the one minimum-reagent plan.
The native chain Cure Wounds -> Wolf's Eye -> Dire Wolf -> Wolf's Eye works using existing Item/Monster references;
Guide displays the active 20% rules while Discovery retains its separate knowledge gates.

Validation:

- Windows Release built. Six focused Atlas/save/package/mod suites passed 188 cases / 6874 assertions.
- Linux ASan/UBSan passed 82 cases / 4576 assertions. Coverage includes optional/invalid saved identities, shared
  actor-ID precedence, declaration-versus-receipt privacy, real loaded item labels, preserved 20%/100% configured
  chances, no rule-created physical features, duplicate relationship elimination and alternative ingredient links.
- Faithful MM7 and MM8 creature-item scenarios passed after taking their actual authored loot. The MM8 scenario
  verifies monster45 alongside item540 before and after save/load, with Atlas enabled and disabled. The normal
  serializer/refill retention is also covered by the Windows Party suite.
- Native AMD OpenGL 3.3 tours verified Longbow <-> Guard in MM7 and Sword of Whistlebone <-> Dragonslayer in MM8.
  The extended MM8 tour navigated from the Cure Wounds alternative ingredient to Wolf's Eye, to the actual Dire Wolf
  definition and its configured loot, then back. Returning to Discovery left the unlearned reagent hidden. Nineteen
  final/staged cross-world captures cover these paths; reusable tours are
  `tools/menu_tours/atlas_creature_loot_mm7.yml` and `tools/menu_tours/atlas_creature_loot_mm8.yml`.
  Private logs, settings, saves and captures remain under `atlas-creature-loot-20261021` in the Windows dev directory.

This remains a checkpoint toward the full objective. Ground-drop provenance, remaining scripted sources, the other
coverage matrix gaps and Android/controller validation still require work.


## Ground creature-loot checkpoint (2026-10-21)

Actual monster-death drops now retain their creature identity on the ground, including special carried items from
creatures that leave no corpse. The shared pickup descriptor carries that identity through the successful inventory
or held-item receipt. Discovery records the real pickup position and links Item <-> Bestiary only after acquisition;
spawning, saving or inspecting a ground item does not reveal that relationship. Ordinary player drops carry no
creature tag, and different species can establish independent sources of the same item.

The optional `openyamm.world-item-origins` save trailer preserves instance provenance in current and cached indoor
and outdoor snapshots. It leaves the upstream version-83 core payload unchanged, validates scene/map/slot/object
identity, and rejects malformed or unsupported extension data. Old saves remain unattributed. A legacy engine can
load the unchanged core; its resave discards the extension, so later loads cannot reuse stale slot identities.
Already recorded semantic discoveries use the ordinary named-map save state. See [MODDING.md](MODDING.md).

The scenario runner's recorded world-item activation now goes through the same shared interaction controller as
keyboard pickup. This avoids a test-only direct world-runtime route that skipped receipt observations.

Validation:

- Windows Release built. Six focused Atlas/save/package/mod suites passed 190 cases / 6925 assertions.
- Linux ASan/UBSan passed 83 cases / 4595 assertions.
- Save tests cover active/cached indoor/outdoor origins, untagged controls, a byte-identical legacy core prefix,
  truncated/malformed trailers, unsupported schemas and invalid scene/slot/object/creature values.
- Nine isolated faithful scenarios passed: MM7/MM8 actual ground-drop creation, receipt after save/load, cached
  inactive-map save/load and return, Atlas-disabled receipt, a legacy-engine resave, and a real pre-extension save
  with an ordinary dropped item. Runtime receipts preserve actual coordinates and species in semantic discoveries.
- Ground creation used a private deterministic death-drop table overlay (item201 at 100%) for authored MM7 actor3
  (creature404, Temple of the Moon) and MM8 actor3 (creature45, Garrote Gorge). These are test rules, not gameplay
  changes. Production data, probabilities, runtime packages and original user campaigns remain untouched.
- The retained pre-extension Windows executable loaded a tagged save and wrote an ordinary save with no trailer.
  The current engine then picked up its item as an ordinary source without inventing creature provenance.
- Native AMD OpenGL 3.3 checks show recovered ground-source location and creature links in both worlds, using the
  ordinary active game tables after receipt. The reusable reference tour is
  `tools/menu_tours/atlas_ground_loot_reference.yml`; native captures and isolated fixtures are under the local
  Windows development root's `atlas-ground-loot-20261021/` directory.

This completes loose ground-drop attribution. Campaign-content coverage, automatic visibility discovery, remaining
UI/content work and Android/controller checks still belong to the active complete-atlas objective.


## Passive creature-sight checkpoint (2026-10-21)

Living creature sprites can now establish discoveries without clicking, attacking or hovering over each one. The
indoor/outdoor main-view paths submit their current art/frustum/sector-qualified candidates to one shared sight
tracker. An independent geometry LOS check must succeed before Core writes the ordinary map-local observation.
There is no omniscient population scan, and this does not reveal carried items or configured drops.

The transient tracker rotates through current candidates fairly, with at most two expensive checks per 16ms and
one retry per actor per second. Unchanged known coordinates/names require no repeat LOS or saved-state write.
Changed coordinates or identity must pass LOS again: unseen movement cannot move an Atlas marker. The scheduling
state is not serialized. Existing semantic observation keys, normal saves and map-refill history remain authoritative.

Only main gameplay sprites participate. Reflection passes, preview actors, fullscreen non-gameplay UI, invisible
actors, corpses and replacement FX models do not create passive sightings. Direct targeting, inspection and actual
loot observations remain available. Passive corpse/model visibility and broader map/device checks remain work for
the complete-atlas objective; this checkpoint does not claim those paths are finished.

Validation:

- Windows Release built. Six focused Atlas/save/package/mod suites passed 192 cases / 7161 assertions.
- Linux ASan/UBSan passed 84 cases / 4613 assertions.
- Shared tests reject in-frustum candidates without LOS, invisible/dead/invalid actors, and actors not submitted by
  the view. They preserve historical positions while a moving actor is occluded, update after renewed LOS, cover
  rate limits and retry/map/clock resets, and prove a visible actor after 300 occluded candidates is not starved.
- Four faithful MM7/MM8 scenarios passed: two clean sight fixtures and both existing actual creature-item receipt
  regressions. Headless fixtures intentionally do not render or establish passive sightings by themselves.
- Native AMD OpenGL 3.3 tours inspect the rendered scene, quicksave before opening Atlas and then inspect Discovery
  Bestiary. The Temple of the Moon view records actor2/creature403 and actor3/creature404 out of 21 loaded actors;
  a separate authored-entrance wall view records none of them. The prior executable records no creatures from the
  matching positive indoor view without targeting.
- Dagger Wound Island's matching positive view records 14 actual actors/five definitions versus just the hovered
  actor in the prior executable. Facing away records none. The Atlas-disabled profile records the same 14 actors.
  These are observed-instance counts, not a requirement to expose the full map population.
- Private settings, fixtures, old-build controls, logs and fresh captures live under the local Windows development
  root's `atlas-passive-20261021/`. Reusable sources are `tests/scenarios/atlas/mm{7,8}_passive_sight_fixture.yml`
  and `tools/menu_tours/atlas_passive_sight.yml`. Start native runs from their exported unobserved saves; inspect
  the quicksave as well as the images. All original user saves and the official installation remain untouched.

Android/controller acceptance, complete campaign relationships, remaining site/loot semantics and the other gaps
in [ATLAS_COVERAGE.md](ATLAS_COVERAGE.md) remain part of the active complete MM7/MM8 Atlas objective.


## Corpse sight and visual geometry checkpoint (2026-10-21)

Passive sight now records visible corpses and can update a previously living observation to defeated. It uses the
actual main camera and a point inside an opaque pixel of the current sprite, accounting for scale, offsets and
mirroring. A transparent frame contributes no candidate; a standing collision-height estimate or an empty image
center cannot stand in for the corpse silhouette. The opacity anchor is derived during existing mask loading,
without adding a per-frame pixel scan or changing packaged mask formats.

Core exposes `IGameplayWorldRuntime::hasWorldLineOfSight` separately from combat target facts. Indoor sight reuses
portal traversal and face occlusion without the AI detection radius/portal limit. Outdoor sight includes walkable
occluders and uses zero terrain height slack for low sprites; existing combat defaults and dead-target rejection
remain unchanged. The shared candidate rotation, query budget and retry throttling continue to apply. Seeing a
corpse does not open it, reveal its carried contents or grant items. Save keys and binary formats do not change.

Validation:

- Windows Release built. Eight focused Atlas/save/package/mod/bitmap suites passed 209 cases / 7306 assertions.
- Linux ASan/UBSan Atlas and billboard geometry suites passed 87 cases / 4639 assertions.
- Windows indoor runtime detection/LOS suite passed six cases / 736 assertions using the complete external runtime
  packages. The added geometry test covers a low corpse point across clear connected rooms versus an occluding
  wall beyond the AI detection range, actual death-animation completion, preserved combat rejection and NaN input.
- The runtime test target needed its missing `RegressionMapLoader.cpp` dependency. Detection fixtures now share
  the existing package-backed loader rather than assume the sparse checkout contains a complete payload.
- Faithful fixtures kill actual authored actors with deterministic test damage and `loot_corpse: false`, which
  leaves the real corpse untouched. The default remains true; existing MM7/MM8 creature-item receipt regressions
  still pass. Additional faithful checks round-trip the native observations and confirm no carried-item receipt
  was fabricated merely by sight.
- Native AMD OpenGL 3.3 checks record the Temple of the Moon Guard (actor3/creature404), Jeric Whistlebone in
  Garrote Gorge (actor3/creature45/name2), and an open-field Dagger Wound corpse (actor0/creature3) as defeated.
  Images and the pre-Atlas quicksaves were inspected. Bestiary links reach their historical map markers, whose
  footer displays `Last observed: defeated`. MM8 facing-away records no corpse; Atlas-disabled still records it.
- Matching previous-build controls with the corpse outside the hover ray do not create these passive corpse
  discoveries. Direct hovering remains a valid existing observation path and is not a negative control.
- Reusable inputs: `tests/scenarios/atlas/mm7_corpse_sight_fixture.yml`, `mm8_corpse_sight_fixture.yml`,
  `mm8_open_corpse_sight_fixture.yml` and `tools/menu_tours/atlas_corpse_mm{7,8}.yml`. Private settings, logs, controls
  and fresh captures are under the local Windows development root's `atlas-corpse-sight-20261021/` directory.

Sight uses one opaque silhouette sample and the current world geometry, not GPU pixel-occlusion readback. Partial
cover, decorative-sprite overlap and replacement models still need broader visibility audits; direct inspection
remains available. Android/controller checks and the remaining campaign/site/reference coverage remain part of
the active complete MM7/MM8 Atlas objective.

### MM8 campaign quest relationships, 2026-10-21

The shared registry now covers 94 quests (26 MM7, 68 MM8), with 364 typed relationships. This checkpoint adds
33 MM8 quest chains and 133 relationships: faction alliances, four elemental Hearts, the Crystal Gateway and
elemental lords, remaining promotion requests, potion contacts, One-Eye and the distinct blackmail return stage.
Arion's house is corrected from a return location to a delivery objective. No engine API, campaign script,
save format, generated event or large asset package was changed.

Disclosure follows actual requests: Quixote names Zog's fortress while Deftclaw does not; Leane names Shadowspire
without naming the laboratory, while Vetrinus explicitly identifies Khel's library and the jar supplier.
Exact undisclosed Palace/prison locations remain Guide hints. Conditional return contacts include the separate
Quixote/Deftclaw council records; residence navigation still uses the current or last-observed NPC index.

Windows Release uses the updated small source-data overlay. Six focused suites passed 193 cases / 7330 assertions;
Linux ASan/UBSan passed 88 / 4797. Native authored Champion/Lich requests survive save/load. The separate blackmail
test uses strict assertions and an explicit inventory-placement fixture, then actual Arion/Fellmoon handoff/reward
handlers and persistence checks. Cursor stowing, travel and combat are outside that fixture's scope.
Native AMD OpenGL captures confirm the different Discovery/Guide source lists; reusable inputs are
`tests/scenarios/atlas/mm8_promotion_requests_knowledge.yml`, `mm8_blackmail_return_knowledge.yml` and
`tools/menu_tours/atlas_mm8_campaign_quests.yml`. Private logs/captures/settings live under the local Windows
development root's `atlas-mm8-campaign-20261021/` directory.

[The quest audit](ATLAS_QUEST_AUDIT.md) records three unverified/broken MM8 journal rows (5, 172 and 132), including
Galvinus' mismatched Accuracy/Blazen flags. They require separate event/state review, not invented Atlas facts.
MM7 campaign coverage, remaining conditional clues/site/reference content and Android/controller checks remain
part of the incomplete full objective; these counts do not measure overall Atlas completion.

### MM7 campaign quest relationships, 2026-10-21

The shared registry now contains 144 quests and 578 relationships: 76 MM7 and 68 MM8 entries. This expansion adds
50 MM7 quests / 214 relationships covering the initial hunt, remaining promotions, both political paths, the
main finale and side requests. All nonempty, non-placeholder MM7 campaign journal entries now have relationships
except the transient QBit 757 `Congratulations` notification. This does not establish complete stage, site or
campaign behavior coverage; [the content audit](ATLAS_QUEST_AUDIT.md) lists the handlers and remaining limitations.

The Tapestry request discloses Castle Lambent on Light and Castle Gloaming on Dark. Its known clue persists after
quest completion; Guide offers both branches without recording new knowledge. Unknown contestant proof, exact
meditation destination, assassin proof and Arcomage evidence remain Guide hints. All merged item/NPC/map IDs are
resolved from the current registries; no generated event, gameplay rule, save format or engine API changed.

Validation: 194 focused Windows cases / 7477 assertions and 89 Linux ASan/UBSan cases / 4944 assertions pass.
Strict native scenarios verify the initial hunt assignment and actual Markham/Rogue/Initiate requests, then both
Tapestry request/reward branches and persistence. The latter use explicit faction/Malwick and inventory fixtures;
they do not test the faction story, theft, castle access, travel or combat. Native AMD OpenGL captures show only
the correct castle in Discovery and both alternatives in Guide. Following the clue to the unvisited castle
shows `No explored floor plan yet`, and the saved physical discovery flag remains zero after browsing and reload.

Reusable inputs are `mm7_campaign_requests_knowledge.yml`, `mm7_tapestry_{light,dark}_knowledge.yml` under
`tests/scenarios/atlas/` and `tools/menu_tours/atlas_mm7_campaign_quests.yml`. Private settings, saves, logs and
captures are under the Windows development root's `atlas-mm7-campaign-20261021/` directory. Android/controller
checks, remaining conditional/reward stages, MM8's broken Accuracy journal and wider site/reference coverage
remain part of the active complete-Atlas objective.


### Pure Accuracy journal and save migration, 2026-10-22

The shared quest registry now covers 145 entries / 581 relationships (76 MM7, 69 MM8). Galvinus' Pure Accuracy
request uses active bit 132 and completion bit 133; it no longer writes Blazen bit 134. A world-local supplement
replaces the authored handlers while preserving recipe groups, partial-recipe refusal, one potion and 5000 XP.
Its fixed reward is declared for the shared item-source index. Generated Lua and the binary save schema are unchanged.

A versioned import recognizes the verified legacy NPC/flag patterns, restores active or remembered historical
quest state, records provenance and leaves unclassified state intact. It never grants rewards or erases ambiguous
old Blazen flags. Core supplies generic NPC-topic and remembered-quest APIs; Atlas contains no quest-state workaround.
[The content audit](ATLAS_QUEST_AUDIT.md) defines the import cases and compatibility boundary.

Strict Windows checks cover original legacy active/completed saves with and without Blazen's actual Cure,
repeated save switching, unchanged XP, fresh failure/success/reload and Atlas-disabled operation. Native AMD
OpenGL captures show active and imported historical quest entries. The fixture preparer supports a private
legacy-script shadow view for reproducing old saves without committing binary saves or large packages.

Targeted validation: Atlas/save/package/mod 194 cases / 7479 assertions; Lua/NPC 73 / 2198; curated Linux
ASan/UBSan 89 / 4946. The optional wider GameplayRuleRegression sweep has 11 missing-source-fixture failures
in this sparse checkout, so these are scoped results. Private logs/settings/saves/captures are in
`atlas-accuracy-20261022/` under the Windows development root. Public inputs are the fresh scenario,
legacy/import fixtures in `tests/fixtures/atlas/` and `tools/menu_tours/atlas_mm8_accuracy_quest.yml`.
Android/controller and the remaining conditional/site/reference checks are still part of the incomplete objective.

### Possible NPC residences and quest contact navigation, 2026-10-22

The shared World Database distinguishes current/last-observed residents from authored possible residences.
Guide includes the loaded base home when it differs from the current home, plus declared scripted destinations.
These records are labelled `possible location`, link to their real building/map and carry no inferred future
training offers, names or availability. Current residences are deduplicated; removing an NPC to house zero creates
no location. Discovery rejects possible records even if an informed-record key happens to name one.

Quest contact entries expose these alternatives only in Guide. Discovery retains the last encountered address
and does not follow a secretly relocated NPC. The Quests tab now offers the existing `Scan sources` action to
collect map-local declarations alongside global contacts; Guide scanning and navigation record no discoveries.
Global/current-map metadata is reused when rebuilding or switching the viewed world. Map indexing retains stable
registry pointers while adding records, avoiding invalidation during the region scan.

`SetMapNpcLocations`/`SetGlobalNpcLocations` bind typed NPC/house possibilities to the registered event callback.
Replacement/removal invalidates old declarations. The legacy exporter derives reachable destinations, keeping
synthetic trigger continuations separate and excluding removals/unreachable code. Seventeen nonempty generated
sidecars cover the canonical global script and MM6/MM7/MM8 local scripts. Explicit supplements cover overridden
MM7 promotion/judge/Butler/Archibald helpers, MM8 lord placements and Regna's timer. Verdant's shared declaration
uses the same configured meeting places as the actual placement helper.

Regenerate with `tools/regenerate_lua_events.sh --npc-locations-only`. Map supplement catalogs live separately in
`*_zz_npc_locations.lua`; base catalogs use `*_00_npc_locations.lua`. The lighting exporter validates their exact
declarative format before excluding them from shadow dependencies. Original generated callbacks, map gameplay
overlays, installed lighting and its hashes remain unchanged; no stale-bake check was disabled or rewritten.
The shared NPC registry and save format remain authoritative. This adds no mod-loader Lua entrypoint.

Verification: 138 Windows database/guide/content/package/mod/save-UI regression cases / 15708 assertions,
28 exporter cases / 420 assertions, curated Linux ASan/UBSan 90 / 4983, and eight lighting-export tests pass.
The focused metadata checks also validate callback ordering and resolve mounted MM6/MM7/MM8 source declarations
against shared NPC/house registries. Strict runtime checks execute Butler relocation with controlled quest
prerequisites and real Talimere-house observation, including persistence; they do not test the castle combat.
Native AMD OpenGL tours verify MM7 current/possible/last-encountered views and MM8's complete source scan,
Quixote's alternative contacts, map navigation and Guide-to-Discovery privacy. Post-UI saves retain observed
addresses and keep unvisited MM8 locations unknown through another save/load. Their private records are in
`atlas-npc-locations-20261022/` under the Windows development root. Public tours are
`atlas_mm{7,8}_possible_npc_locations.yml`; matching save verifiers are documented in
[the fixture guide](tests/fixtures/atlas/README.md#possible-npc-locations-and-guide-privacy).
Android/controller acceptance, conditional service/recruitment
eligibility, generated/roaming NPCs and the other coverage-matrix gaps remain open.

### Journal-backed site descriptions and party-wide notes, 2026-10-22

Event-backed Sites now link to the shared `JournalAutonoteTable`. A well, shrine or obelisk can show its
associated note in the footer and open a native, scrollable **Read notes** page. Discovery requires both
knowledge of the site and the actual party note flag; observing a well does not reveal its effect. Guide can
read the reference without executing the event or awarding knowledge. Switching back to Discovery re-evaluates
those gates, including an already-open reading page. These are the mounted game's journal descriptions,
not copied community-guide prose or a second Atlas database of effects.

`SetMapAutonoteRefs(eventId, {noteIds})` and `SetGlobalAutonoteRefs` declare callback-bound references. Replacing
or removing the handler invalidates its old references; an overlay may declare its new references afterward,
or explicitly suppress them with `{}`. IDs are positive uint16 values and the loader rejects malformed arrays.
The exporter emits reachable nonzero Add/Set AutoNote declarations, including synthetic continuations, only
when the shared journal has meaningful text. Internal flags with blank/placeholder text remain gameplay state,
not invented descriptions. Regenerate references with:

```sh
tools/regenerate_lua_events.sh --autonote-refs-only
```

The 48 catalogs comprise one base Global catalog and 17/16/14 MM6/MM7/MM8 map catalogs. They declare
231/62/43/34 handler-to-note relationships respectively. Catalogs load after base handlers and before gameplay
overlays, without rewriting those handlers. Lighting dependency validation recognizes only their restricted,
validated metadata syntax; gameplay script hashes and existing baked lighting remain authoritative.

The cross-map runtime check also exposed the old map-local storage of journal flags. The shared Core now
stores positive indexed E1/DF AutoNote variables in `openyamm.journal.note/<id>` named global state. Event reads,
writes, trainer/guild notes, ordinary journal display, awards and Atlas knowledge use this common ownership.
Notes normalize to boolean flags; their numeric ID is not a character reward. Unindexed E1 remains the Intellect
comparison. This works with the Atlas module disabled and uses the existing save encoding.

Loading a legacy save performs `openyamm.migration.journal-notes-v1` once, collecting raw flags from the active
map and cached indoor/outdoor snapshots even if unrelated named globals already exist. Canonical global values
win, including explicit zero clears. Active-map legacy values override cached conflicts; otherwise cached
positive evidence is retained. Within one snapshot E1 wins over its DF alias. Imported raw aliases are removed
from restored runtime snapshots. The migration records an imported count and neither executes map events nor
adds stat bonuses. A persisted zero and the version marker prevent stale snapshots from resurrecting a clear.

Validation used an isolated native Windows Release build, ordinary OpenGL on the AMD GPU, and private saves:

- 173 focused Windows unit cases passed; 25 additional real event/trainer/guild regressions passed.
- 91 focused ASan/UBSan cases passed, including the refreshed packaged-map audit; lighting's 10 Python cases passed.
- Strict MM7 Harmondale and MM8 Dagger Wound well interactions retain their real Might +10 / Intellect +15 bonus,
  refuse repeated bonuses, preserve the note through map changes and save/load, and pass with Atlas disabled in MM8.
- Actual saves retained from the previous map-local implementation migrate and retain notes after leaving the map.
- Three native UI tours cover learned notes in both worlds and an observed-but-unused MM8 well. Fresh captures
  were inspected: Guide shows the reference; Discovery hides its text after switching back. Post-UI save checks
  confirm unchanged note knowledge and character bonuses, including the zero/zero unlearned case.

Reusable scenarios are `tests/scenarios/atlas/mm{7,8}_well_journal.yml`; migration/privacy verifiers and setup
instructions are under `tests/fixtures/atlas/`. Native tours are `tools/menu_tours/atlas_mm*_site_journal_*.yml`.
Original campaigns and the official installation were not used for writes. This checkpoint covers journal-backed
site descriptions; complete effect predicates, sites without notes, remaining loot semantics, inherited
scripted decoration events and Android/controller acceptance remain open in `ATLAS_COVERAGE.md`.

### Guide effects and requirements for sites without journal notes, 2026-10-22

Guide now offers **Read details** for supported site callbacks, including the unnoted Harmondale gold well.
The reference follows each supported branch and describes stat/resource effects, numerical requirements,
HP/SP fullness and local use gates. It is generated from the same decoded EVT as the callback, rather than
inferred from community-guide prose or executing an event while browsing. Guide search includes this reference;
Discovery search and reading continue to expose only actual party knowledge and learned journal descriptions.
The displayed reference describes rules, not a forecast that a reward is presently available.

`SetMapSiteReference(eventId, text)` binds to the currently registered callback. Replacement/removal invalidates
it; mods can redeclare it after their callback, or suppress it with empty text. Invalid event IDs, non-string
values, NULs and text over 16 KiB fail loading. Generated `_00_site_refs.lua` catalogs precede every callback
overlay. Regenerate just these declarations with `tools/regenerate_lua_events.sh --site-refs-only`.

The initial set has 40 callback references in 26 MM6/MM7/MM8 maps (12/11/17 callbacks, 8/9/9 maps). This projection
covers supported complete callbacks, not partial guesses: reachable random flow, unsupported effects/predicates,
character-selection changes, unknown instructions, loops and conditions after state mutations omit the entire
callback. Unreachable actions do not contaminate the reference. Boolean use flags are not treated as multi-use
counters; writes to calculated Actual* attributes are not described as base-stat increases. The exporter also
now terminates when its existing comparison-ladder/guard recognizers encounter backward edges, preserving the
normal labelled callback representation. The synthetic-cycle test validates finite output and Lua loading.

Data checks exposed two unhinted Mount Nighon wells misclassified as generic events. The explicit
`out10_zz_site_context.lua` overlay restores their existing category without rewriting generated Lua. Shared
journal entries 280/281 now correctly report 25 HP/SP instead of 50, matching the actual callbacks. Lighting
validation recognizes only restricted literal site-reference/context declarations; gameplay script dependencies
and baked lighting remain protected.

[The site-reference audit](ATLAS_SITE_REFERENCE_AUDIT.md) inventories MM7/MM8 callbacks against packaged base
geometry. Six declarations have no visible base interaction site and receive no invented marker. Their live,
scene and inherited-decoration bindings require further investigation. The audit also retains the pre-existing
Castle Harmondale dimensional-fountain geometry regression; this checkpoint does not certify that separate gate.

Validation includes focused Windows unit/data tests, real gold/bank/use-limit boundary execution, actual Nighon
HP/SP effects and note grants, Linux ASan/UBSan world logic, strict lighting tests, and native Windows Release
UI checks. The MM7 scenario uses an explicit controlled zero-gold party and a real physical well interaction;
it awards +200 once and preserves it through reload. MM7/MM8 Guide-to-Discovery captures were inspected, and
post-UI save verifiers confirm unchanged gold/use state and learned-note/stat state. All settings, campaigns
and captures are private QA data; no original player campaign or official installation was written.

Accepted checks for this slice: 176 focused Windows cases / 68,589 assertions, three executed-rule cases /
63 assertions, 91 ASan/UBSan cases / 5,014 assertions and 13 lighting cases passed. The final Release build,
two fresh native UI tours, their post-UI save verifiers and the Atlas-disabled MM8 well scenario passed.
This is a checkpoint within the full MM7/MM8 Atlas goal, not completion of its remaining coverage matrix.


### Actual observed site outcomes, 2026-10-22

Using a visible well, shrine or obelisk now records actual synchronous before/after changes through shared gameplay
interaction code, including when the Atlas module is disabled. Discovery's **Read details** and search use these
records. An unnoted well can therefore teach its effect without exposing unvisited sites, untouched callback
branches, hidden requirements or current reward availability. Merely browsing Guide creates no observations.

The first supported set includes gold/banked gold/food, HP/SP, base and temporary attributes, skill points,
level/age/armour bonus, the six displayed resistance families and visible conditions applied or removed.
Health uses the actual clamped result: healing from 95 to 100 records +5 even if the callback requested +25.
Character changes are anonymous observed ranges, with no invented attribution or extrapolation between endpoints.
Party resources remain separate. Replacing/reordering party members during an event discards character comparisons.
Repeated use without a change preserves historical effects and reports that no additional changes were recorded.
Deferred changes, spell-buff state, items, experience and other unsupported effects are not inferred from silence.

Core state uses `openyamm.discovery.site-effects.v1/<physical-site>/event/<id>/` in existing named map variables.
Stable attribute symbols, rather than numeric legacy EVT aliases, identify effect endpoints. This requires no save
format bump; the ordinary snapshot/save/refill paths retain the history. The Atlas indexes new history by canonical
world/map ID, never an ambiguous filename alias. Older saves acquire observations through new actual interactions;
the feature does not reconstruct rewards from a journal description. Incomplete saved outcome records are reported
explicitly and are not overwritten with guessed defaults.

`EventAttributeInfo` shares labels between actual observations and the existing supported Guide projection.
Observation-only attributes do not silently extend the exporter’s accepted reference predicates. Failed, hidden,
non-site and map-changing activations do not create outcome records. Other site discovery behavior is unchanged.

The extended `mm7_unnoted_gold_well.yml` and `mm8_well_journal.yml` scenarios exercise physical wells, their actual
+200 gold / +15 temporary Intellect, repeated use, save/load and cross-map retention. These are controlled base
campaign fixtures, not universal rewards. Reusable native tours and paired privacy verifiers are documented in
`tests/fixtures/atlas/README.md`. Full Atlas coverage and Android/controller acceptance remain open.

Accepted checks for the observed-outcome slice: Windows Release build, 141 focused unit cases / 5,803 assertions
(including the 137-map packaged-house audit), and one shared-controller runtime case / 19 assertions passed.
The two extended actual-well scenarios, MM8 Atlas-off variant, four native AMD/OpenGL tours and paired post-UI
save/privacy verifiers passed. Captures were inspected, including both unseen sites' actual reference readers
and the empty Discovery reader after returning from Guide. Private results are under the local Windows
`atlas-site-observations-20261022/` directory. This is focused acceptance, not a full suite or Android result.


### Starting-skill profiles by class and race, 2026-10-22

Class details now show **Starting skills by race**, separating automatically granted skills from optional choices.
The profiles query `ClassSkillTable::getEffectiveStartingSkillAvailability`, the same evaluator used by New Game,
including racial grants, restrictions and the loaded starting-skill table. A higher mastery cap alone does not
invent an extra starting choice. Identical profiles share one list of race names to keep the reading page compact.

Race/class combinations follow the shared selection registry; Discovery uses only known party classes and races.
Guide can read all registered profiles, and switching back does not teach a race or class. Replacing the starting
skill table updates the reference without changing class caps. These profiles describe skill rules; portrait
availability, preset optional selections and starting attribute allocation remain separate creation data.
The existing scrollable class view and mastery comparison continue to work alongside the new text.

The custom-class regression includes sparse race ID 97, a race-only Perception choice, an increased Learning cap
which is not a starting choice, identical Human/Twin profiles, a race excluded from that class and changed data
mod defaults. A canonical Knight check uses the mounted source tables to distinguish granted Sword/Leather Armor
from the Human Learning mastery modifier. Reusable controlled Human Cleric fixtures and native tour/privacy checks
are documented in `tests/fixtures/atlas/README.md`.

Accepted checks for starting-skill profiles: the Windows Release build, 35 focused Guide cases / 1,278 assertions,
four existing race-skill cases / 81 assertions, both native AMD/OpenGL tours and both 31-step post-UI save/privacy
verifiers passed. Fresh captures confirm the Human-only Discovery profile, complete registered Guide profiles,
scrolling, the existing comparison grid and return to gameplay. Tests use controlled private campaigns under
`atlas-class-starting-20261022/` in the Windows development directory. This is focused PC acceptance; full
character-creation, original-source asset gates, Android/controller acceptance and complete Atlas coverage remain open.


### Item icons and pinned base-definition comparison, 2026-10-22

Item references now display the mounted inventory artwork at its original aspect ratio. **Pin item**, select
another catalog entry and **Compare** opens two independently scrollable columns. Either column heading returns
to its normal item reference; **Details** returns to the selected item. Guide can compare the complete loaded
catalog, while Discovery requires both type IDs in existing party item knowledge. Changing mode, world or reference
section clears the pin and comparison so Guide-only items cannot remain in Discovery's comparison or footer.

Both catalog and comparison use the same base-definition formatter. Weapon attack/damage, armor, reagent power
and wand defaults share `ItemInspectionText` with the actual inventory inspection overlay. The shared instance
path retains broken status, actual potion power and current/maximum wand charges, and refuses unidentified
instance details. Negative bonuses now retain their sign instead of dropping a damage penalty or printing `+-`;
armor text uses widened arithmetic. The reference also displays actual rarity and declared or legacy class/race
restrictions from `ItemRuntime`. Numeric import material codes are no longer presented as material names.

These are item-definition comparisons, not a simulation of the inspected hero or an enchanted/broken inventory
instance. The columns use loaded values, requirements and authored notes; they do not equip an item, roll loot,
spend gold or forecast current shop prices. Existing source links stay in the normal reading view.

The menu renderer's item-art path uses the existing engine image decoder with palette-zero transparency, as
inventory icons do. It keeps at most 16 item textures per screen, releases them on screen destruction and leaves
the shared menu-art decoder/cache unchanged. This avoids retaining every viewed item in the global menu cache.

This checkpoint initially followed the existing encountered/ever-owned type-knowledge policy, which did not
prove identification. The identification-aware checkpoint below adds shared history and gates catalog, comparison,
map/search, spellbook and loot references. A known base type still does not establish an unidentified instance's
enchantments. Wider content/observation audits and full Atlas completion remain open.

Accepted checks for this slice: Windows Release build, 72 focused Guide/inspection/inventory cases /
1,907 assertions, five image-decoder cases / 174 assertions, four native AMD/OpenGL tours and four 43-step
post-UI save verifiers passed. Fresh captures were inspected in both worlds, including real overflow of Elven
Chainmail's description while the other column remains stationary, mode changes and disabled controls for an
empty search result. Empty filtered results now say that no known records match the query instead of implying
that all party discoveries are gone. Private records live under `atlas-item-comparison-20261022/` in the Windows
development directory. These checks do not establish complete-campaign, identification-history or Android acceptance.

The inventory regression fixture for custom skill scrolls also no longer initializes a second global PhysFS
instance while the shared regression data owns one. Its data-only synthetic item table needs no separate mount;
all original scroll/equipment assertions remain intact and the entire selected inventory file passes.


## Identification-aware item references (2026-10-23)

Discovery separates encountering a type from identifying it. An encountered unidentified item keeps its normal
unidentified name and artwork, source links and an identification notice. It does not disclose the true name,
price, base statistics, restrictions, spellbook contents or hidden description, and cannot be pinned or compared.
Map-item captions, sorting and search apply the same distinction. Recorded creature-loot links remain available
with the unidentified name. Explicit quest clues and learned alchemy notes may still name their own subjects;
hearing a name does not unlock the full item reference.

Guide reads the complete active definitions. Switching to Guide never identifies an instance or adds knowledge.
Comparison remains a comparison of base definitions, without revealing an unidentified instance's enchantments.

The authoritative history is a typed Party set, independent of the optional Atlas UI. Successful identification,
repair actions that identify an item, created alchemy results, identified acquisitions and actual joining members
record it. Visible identified chest slots also teach a type without granting ownership; hidden slots do not.
The history survives losing an item and uses save format 84. Older saves remain readable. Migration uses actual
identified inventory/equipment flags, never historical possession alone, and never the unvisited inn's stock.
An older engine that only understands format 83 cannot read a newly written format-84 save.

Focused Windows checks cover possession versus identification, failed/actual identification, repair paths, current
inventory/equipment migration, hidden chest slots, history replacement/reset and serialization. Frozen unmodified
MM7/MM8 format-83 fixtures additionally verify the real previous reader layout. The optional fixture tests use
`OPENYAMM_LEGACY_IDENTIFICATION_FIXTURES` pointing to a private directory containing
`mm7-identified-current-v83.oysav` and `mm8-identified-current-v83.oysav`; the explicit controlled-fixture test
exports separate unidentified/identified format-84 snapshots there for native review.
`tools/menu_tours/atlas_item_identification.yml` checks the unidentified page, disabled comparison, true-name
search, Guide reference and return to Discovery. The existing item-comparison tour checks both identified swords.
`OPENYAMM_ATLAS_IDENTIFICATION_REVIEW` enables the post-tour save check for private `mm7/mm8-known/unknown`
case directories, preserving instance flags, history, equipment, class/race, skills, experience, resources and QBits.
Original player campaigns and the official installation are not used as test outputs.


Validation: Windows Release build; 193 focused unit cases / 5948 assertions; two previous-save/controlled-export
cases / 55 assertions; one post-tour verifier covering four private campaigns / 121 assertions. Native AMD/OpenGL
captures checked MM7 and MM8 with both identified and unidentified equipment, exact true-name searches, Guide
and return to Discovery. The four chest-observation/scripted-item-source headless scenarios also passed on the
current executable. These checks do not establish Android/controller acceptance or complete the remaining
content audit in `ATLAS_COVERAGE.md`.


## Late handwritten reward sources (2026-10-23)

The loaded-handler registry now includes possible rewards from these existing authored callbacks:

| Scope | Callback | Declared item source |
| --- | --- | --- |
| MM7 Harmondale | Timer 65003 | Scavenger hunt advertisement (774) |
| MM7 Erathia | Timer 65004 | Scavenger hunt advertisement (774) |
| MM7 Tularean Forest | On-load 401 | Message from Erathia (1502) |
| MM7 Land of the Giants | On-load 65012 | Archibald's Blaster handoff (866 in the active merged tables) |
| Shared Verdant topic | Global 1788 | Charged connector stone, read from `CrossContinents.Config` |
| MM8 Murmurwoods tree | Map 455 | Fixed rewards from the current gem exchange rules |

Separate `_zz_item_rewards.lua` overlays bind to the final gameplay callbacks. They preserve the original
callback/common-script bytes and existing baked-lighting dependencies. The explicit MM8 catalog is checked
against the actual `GemExchangeEntries` table: positive fixed item rewards are included once, while gold and
random-item branches are excluded. Replacing a callback still invalidates its prior declaration unless the
replacement explicitly supplies new metadata.

The lighting producer now validates the late literal reward-only format as well as generated `_00` catalogs.
It rejects executable statements and invalid event/item ranges before excluding metadata from shadow inputs.
No baked hash was rewritten and runtime validation remains enabled. Timer/on-load gifts retain map-level links
without invented positions; the actual MM8 tree has geometry-backed source pins. Guide's existing `Scan sources`
action indexes these possibilities. Discovery still requires a recorded successful delivery or observation.

These declarations do not complete all handwritten/world-state sources. Shared automatic CrossContinents
callbacks, inherited crystal decorations and other conditional source semantics still need their own audit.


Validation: 32 focused native Windows unit cases / 1,220 assertions, including real geometry bindings across
five maps and the existing MM8 fixed-item/gold exchange behavior; 26 lighting-tool tests; Windows Release package
refresh; MM7/MM8 native AMD/OpenGL source scans and inspected captures. Two controlled fixture scenarios and two
16-step post-browsing save scenarios passed. The latter verify unchanged equipment, gold and food, absence of the
possible reward before/after Guide, and a second save/load round trip. Native source scans reached Sources ready
without indexing or stale-lighting errors. Original campaigns remain outside test writes. Android/controller and
the remaining content matrix are not established by this checkpoint.


## Inherited interactive objects (2026-10-23)

The native Sites layer now indexes inherited barrels, campfires, cauldrons, crystals and other shared decorations
from actual indoor/outdoor entities and the gameplay decoration registry. It uses the same binding rules,
deterministic initial variants and authored decor variables as the runtime. Outdoor bindings retain their existing
container set and save-slot ordering; indoor-only container rules are not silently enabled outdoors. Direct map
callbacks remain distinct. No geometry, caster bake, existing gameplay callback or save schema was rewritten.

Each inherited object has a map-scoped identity based on its existing saved decoration slot. Successful actual
shared interactions record its event variant and synchronous numeric/condition effects in the existing Core
knowledge namespaces, including when the optional Atlas UI is disabled. A failed handler or an unrelated global
NPC callback does not discover an object. Reusing an empty container records that use without awarding another
bonus. Initial Guide information is explicitly separate from recorded findings and is not a live contents check.
Discovery connects journal notes only to the variants actually observed at that object; knowing another barrel's
note elsewhere does not expose its effect here.

The exporter produces callback-owned `Global_00_site_refs.lua` through `tools/regenerate_lua_events.sh --site-refs-only`.
`SetGlobalSiteReference` shares the map-reference metadata validation and replacement ownership rules. Unsupported
branches remain unprojected rather than being asserted as unconditional effects. Crystal positions are indexed,
but their fixed loot links and script-owned container effect references still need separate work. The six missing
physical bindings, special connections, wider content audit and Android/controller checks remain open.

Validation: Windows Release build; 48 focused native unit cases / 1,382 assertions; 14 lighting producer tests.
The real-map regression includes MM7/MM8 outdoor maps, negative controls for script-owned indoor maps, and a positive
MM6 indoor binding regression against the full mounted packages. Native AMD/OpenGL tours use the ordinary Trigger
action: the Harmondale contest grants exactly five skill points, and a Dagger Wound red barrel changes Might 15 to
17 exactly once. Inspected native Atlas captures show their recorded effects, the barrel's initial Guide reference,
and Discovery after returning from Guide. The two post-tour scenarios (20 and 22 steps) validate previously absent
facts, unchanged party resources, exact object identities, an undiscovered neighboring barrel and another save/load
round trip. Preparation/verification scenarios are under `tests/scenarios/atlas/`; reproducible tours are under
`tools/menu_tours/atlas_mm{7,8}_inherited_decoration.yml`. A further native MM8 run with Atlas disabled
passes the same 22-step persistence checks. Sprite picking requires the real renderer; the headless preparation
scenarios do not verify that input path.


## Object reward provenance and the crystal-source audit (2026-10-23)

Inherited decorations now contribute declared fixed rewards to the shared item-source index. Each possible source
retains the map, binding slot, initial global callback and authored coordinates. Reindexing ordinary loot or NPC
rewards preserves these object sources; replacing a callback invalidates its old declaration. The Atlas derives
clickable Guide links through the existing native item-feature path. Depleted authored objects do not claim an
initial reward. This is a generic binding and declaration mechanism, with no crystal item IDs in the Core logic.
The existing handwritten crystal callback table owns its seven reward declarations without duplicating the item
mapping in Atlas data.

Actual successful delivery records a separate object-reward receipt with the slot, callback and activation point.
The saved pending grant origin retains that information after the active decoration context ends or changes its
remaining event. Merely meeting the object, failing its requirement or failing to receive an item does not create
an item receipt. Full-inventory and save/load unit controls cover deferred delivery, while distinct identical
objects, malformed origins, callback replacement and NPC-source isolation have explicit checks. This extends the
existing named-map state and does not change the save format.

The mounted-package census loaded all registered MM7 and MM8 maps and found no inherited crystal bindings of
these seven shared types in either campaign. Their absence is not replaced with invented points. Positive
shared-engine controls in MM6 Snergle's Caverns and Gharik's Forge contain 17 such bindings; this is not a full MM6
campaign audit. Conditional/script-owned crystal interactions and other reward semantics remain part of the wider
content audit.

Validation: Windows Release build and 54 focused native unit cases / 2,550 assertions. A separate mounted-map
census invocation passed with 190 assertions; it expands the same catalog test, so it is not another distinct
unit case. Native AMD/OpenGL controls with Atlas disabled use real sprite interaction at Perception 6 and 7:
the former retains the crystal and records no item receipt, while the latter gives one Amethyst, clears that
object and records its exact coordinates. The neighboring identical crystal remains unknown. Native captures
were inspected, and the 14/15-step receipt scenarios validate delivery and another save/load round trip.
MM7/MM8 native Atlas regression tours and their 20/22-step post-browsing save scenarios also passed.

The shared-engine controls live under `tests/scenarios/atlas/mm6_{low,high}_crystal_{fixture,verify}.yml`.
Use isolated settings/saves, copy the exported `before-crystal.oysav` to `saves/before.oysav`, and set
`startup.save_file=saves/before.oysav`. Set `debug.menu_input_tour_path` to
`tools/menu_tours/atlas_crystal_receipt.yml` and `debug.screenshot_tour_path` to
`tools/screenshot_tours/atlas_crystal_receipt_pose.yml` using absolute paths in that isolated directory.
The existing screenshot tour restores the explicit indoor camera pose before ordinary Trigger actions; a prepared
indoor save alone does not retain that fixture's pitch. Inspect the captures, then run the matching verifier
against its isolated `saves/quicksave.oysav`. These controls do not add an MM6 Atlas or establish Android acceptance.


## Campaign interaction inventory (2026-10-23)

A package-backed regression now loads the actual local programs and physical interaction sites of all 76 MM7
and 61 MM8 registered maps. It validates 6,695 physical site identities and finite coordinates, and can export
`maps.tsv` and `interactions.tsv` to an explicitly selected QA directory. The export records the active context
kind, hidden state, native Sites inclusion and presence of reference/item/autonote declarations. These are event
sites, often multiple surfaces for one interaction; neither their count nor a passing inventory proves complete
Atlas coverage. Hint-only and unregistered sites require interpretation rather than automatic guessed markers.

The current inventory has no local `boost` callbacks. The remaining content work is more specific than a generic
"script-owned barrels" task: conditional Drink/Take a Drink events, Murmurwoods' Stone to Flesh statues, opaque
conditional entrances such as Escaton's Crystal and the Prison of the Water Lord, and additional bookcase/ore-vein
semantics. Existing item-source pins already cover many fixed bookcase and ore rewards; an unsupported generic
context kind does not mean all information about that object is missing. These candidates must be checked against
actual callbacks and the reference behavior before assigning a native feature or describing its effect.

The focused native regression passes 27,192 assertions without export and 27,194 with both output streams checked.
Run it after building `openyamm_unit_tests`, with the full runtime packages selected through
`OPENYAMM_REGRESSION_ASSETS_ROOT`. For the Windows iteration wrapper, set `OPENYAMM_ATLAS_INTERACTION_AUDIT` to an
existing output directory and select `--test-case='Atlas campaign interaction inventory*'`. The wrapper forwards
and converts the WSL directory path for the native test process. This inventory does not change gameplay or saves.


### Conditional drink sites, 2026-10-23

The native Sites layer now includes ten formerly generic MM7/MM8 drinking callbacks, representing twelve physical
locations in the mounted campaign maps. Callback-owned `"drink"` metadata supplements generic/title-derived
context without overriding explicit script decisions. The shared Core interaction hook records actual numeric
and condition changes; Guide reads generated descriptions independently. Visiting a map or browsing Guide alone
does not discover a drink or copy possible effects into party knowledge.

The exporter now handles condition presence/application/removal, per-character use flags, bounded random branches
and the audited numeric resistance families. Random paths describe possibilities without rolling gameplay RNG
or claiming probabilities. A fully inspected feedback-only callback explicitly has no attribute/condition effect.
Unsupported reachable operations withhold the effect description while retaining the drink category. Existing
global object references also gain supported random food and resistance descriptions through regeneration.

Metadata was regenerated through `tools/regenerate_lua_events.sh --site-refs-only`. No generated gameplay callback,
baked-lighting file/hash or save format changed. Forty map metadata catalogs pass the strict literal validator,
including its optional exact `"drink"` argument; executable or arbitrary-kind additions remain rejected.

Validation: 50 selected native unit cases / 28,434 assertions passed, including all ten mounted drink callbacks
and Discovery/Guide gates. The separate export of the same 137-map inventory passed / 27,194 assertions; it is a
repeat of an included case, not another distinct coverage claim. Fourteen lighting-tool tests passed. Windows
Release built successfully. Five headless interaction scenarios passed, including two-character personal-use
checks and actual geometry picking; five native UI tours passed with fresh captures visually reviewed. Their
post-UI verifiers passed 24, 22, 84, 24 and 22 steps for conditional, resistant, personal, resistance-bonus and MM8
feedback-only cases. They preserve real effects, individual flags, gold/food and unvisited neighboring sites
through another save/load cycle. Evidence is private under `atlas-drinks-20261023` in the Windows dev directory.

The conditional native fixture has a second resistant companion so applying paralysis to its first member does
not enter the all-incapacitated game flow before the Atlas can be inspected. Its actual callback applies Paralysis,
which differs from the GrayFace D12 reference; no gameplay condition remapping is attempted here. The resistance
bonus pool is in The Maze (d02), not Stone City (7d24), whose drinks are feedback-only in the active scripts.

A broader 86-case selection initially hit six unrelated fixture failures: two missing loose geometry inputs and
four attempts to initialize PhysicsFS while the package-backed regression fixture already owned it. The accepted
50-case selection covers the affected site/reference systems; this is not a claim that the full suite passed.
Android/controller validation, special statues/connections and the remaining full-Atlas matrix stay open.


### Special rescues, keyed entrances and canonical object discoveries, 2026-10-24

Seventeen callback-owned typed interaction references now cover seven Red Dwarf Mines rescues, five Murmurwoods
statues, Escaton's Crystal and the four elemental prisons. The mounted maps contain twelve statue sites and
twenty-two entrance components for these definitions. Item, spell, NPC and destination labels resolve through
active game tables. Generated EVT alone would describe stale behavior: the active Merge overlays add rescued dwarf
followers, actual spell targeting and source-map flags that allow already-opened entrances to be reused.

`SetMapInteractionReference` attaches a small validated descriptor to the current handler. Replacement/removal
invalidates it; explicit context overrides keep precedence. Literal handwritten catalogs live in reserved
`*_zz_site_interactions.lua` sidecars. The lighting producer validates their fields and syntax before excluding
these metadata-only declarations from shadow dependencies. No generated gameplay callback, existing map supplement,
baked-lighting payload/hash or save version changed.

The shared physical-site observer now handles input and Stone to Flesh world casts. It records a declared object's
actual visible-to-hidden transition in the existing discovery namespace and retains that historical removal after
later visits. Guide descriptions remain separate: unsuccessful interactions and browsing do not learn a possible
rescue, item requirement or NPC identity. Existing observed effects keep their previous numeric/condition meanings.

Native validation exposed a separate outdoor identity defect: decoration activation indexes a compact renderer
list, while Atlas sites index original map entities. `GameplayEventTargetHit::sourceEntityIndex` now carries that
canonical identity from the world hook. Activation still uses its renderer index. Observation and pointer-release
matching cannot confuse different entities reusing one renderer slot. Indoor entity identity remains unchanged.
Unknown source bindings do not fabricate observations; historical records with incorrect indexes are not guessed
into different objects.

Validation: 58 selected native unit cases / 29,064 assertions passed, including callback ownership/schema errors,
all seventeen real definitions, physical entity keys, current table names, target connections, discovery privacy,
observed object transitions and canonical identity. The same inventory/catalog cases were also exported separately:
two cases / 27,654 assertions passed across all 137 maps. These repeated cases are not additional distinct coverage.
Sixteen lighting-tool tests and all four strict interaction catalogs passed validation. Windows Release built.

Seven isolated native controls passed with fresh captures inspected: MM7 rescue/refusal, MM8 item rescue/refusal,
a real Stone to Flesh cast without an item, a locked entrance's Guide/Discovery views and actual keyed dungeon
entry. Two existing MM7/MM8 drink tours also passed after the shared observer change, with fresh findings captures
and 24/22-step post-UI save checks. The cast used ordinary spellbook quick-spell assignment and targeting with
unlimited mana disabled; SP fell from 21 to 11. An additional native cast with Atlas disabled (`Loaded 1 mod`) produced the same Core receipt.
Eight post-UI verifiers passed 22/22/22/22/24/15/16/24 steps, checking individual object history, untouched neighboring
objects, quest bits, item retention/consumption, gold/food, actual arrival and source-map unlock persistence through
save/load. Gate input also passed two actual headless mesh-picking scenarios with and without the key.

Controlled fixture exports and native input tours are retained under `tests/scenarios/atlas/*statue*`,
`*crystal*` and `tools/menu_tours/atlas_mm{7,8}_*`. Native snapshots/captures are private under
`atlas-interaction-references-20261023` in the Windows dev directory; its folder date marks the start of this run.
Restore the matching `tools/screenshot_tours/atlas_mm{7,8}_*pose.yml` before native input, since indoor saves do not
retain camera pitch.
The no-item outdoor control uses turn mode to prevent unrelated hostile attacks from ending the test. The spell
control uses normal real-time casting and a real item-free target; it does not inject an event or spell ID into Lua.

This checkpoint does not complete Atlas content, component grouping, special connection graphs or device coverage.
Android/controller checks and the remaining feature matrix stay open.

### Explicit passage groups, 2026-10-24

The four elemental prison entrances in Plane Between Planes now each have one native Sites entry and one
location entry. Previously their disconnected geometry produced 21 components (8/5/4/4 for Air/Fire/Water/Earth).
An explicit callback-owned `SetMapSiteGroups` catalog maps those real components to four passages; matching
names, callback IDs or proximity alone never merge sites. Unlisted components and unrelated reused events stay
independent. This shared World Database projection also supports MM7 and mod-authored connections.

The first declared component anchors a stable ID/position. Exploration or interaction at any real member reveals
the passage; existing saved face/component keys remain authoritative. No synthetic group discovery, copied Guide
knowledge or save migration is introduced. Component observations and indoor height filtering use the same members.
Invalid, overlapping, sparse or stale bindings are rejected; replacing the callback invalidates its declarations.
The strict metadata-only lighting exclusion preserves existing shadow dependencies and payloads.

Validation: 64 selected native unit cases / 33,367 assertions passed, including schema/ownership errors, reused
callbacks at separate objects, indoor floor/visibility checks, actual member bindings in the mounted MM8 map and
every one of the 21 component discoveries. Eighteen lighting-tool tests and the strict mounted catalog passed.
Windows Release built successfully. One isolated native UI tour passed; inspected captures show four Guide rows,
one explored Water entrance in Discovery, correct Ring of Keys requirements and unchanged fog after leaving Guide.
A 25-step post-UI verifier checked untouched unlock/discovery keys, gold/food and a further save/load roundtrip.
A separate 26-step headless control activated the real Water prison face (bmodel 24, face 4) without a key.
Its raw component discovery survived save/load; the group anchor, sibling component and other prisons stayed
undiscovered and locked. The input used an actual mesh pick at 213 units, without event replay or forced IDs.

Reproduction inputs are `tests/scenarios/atlas/mm8_prison_groups_{fixture,interaction,verify}.yml` and
`tools/menu_tours/atlas_mm8_prison_groups.yml`. Restore the controlled fixture as `saves/before.oysav` before
running the native tour. Captures and receipts stay private under `atlas-passages-20261024` in the Windows dev root.
These checks do not certify every multipart entrance, conditional connection graph, Android or controller UX;
the remaining full-Atlas matrix is still open.


### Castle Harmondale courtyard payload repair, 2026-10-24

The previously failing dimensional-fountain gate is repaired by packaging the existing upstream scene supplement.
The public bootstrap's base BLV/scene match source, but its `7d29_dimension_travel.scene.yml` file was absent.
The Windows iteration wrapper now mounts that exact tracked file in the small runtime overlay. It adds the six
intended water-face bindings; existing callbacks and runtime handling supply the behavior. No geometry binary,
generated script, bake, Core gameplay rule or save version changed. Other source scene-overlay filenames were
checked against the mounted packages; none was missing after this correction.

Before correction, the existing fountain test failed 18 of 71 assertions. After correction, 66 selected native
unit cases / 33,489 assertions pass, including its full original 71 assertions, both restoration states and the
Atlas projection of all six water faces as one physical site. Merely visiting the map does not discover it, and
an unrelated face cannot reveal it. The Windows Release target passed; the asset-only rebuild took two seconds.

An actual headless mesh pick activates face 1668/event 65031 at 372 units without forced event replay. A native
Windows AMD/OpenGL control confirms first-contact dialogue with Verdant, one discovered Atlas passage, its Guide
view and the return to the unchanged discovered floor plan. A 25-step post-UI save/load verifier preserves Town
Portal discovery, restoration state, resources and the one actually touched face without seeding its siblings. This is the first-contact path, not proof that every
cross-world destination/transition is integrated into the Atlas. Controlled input/pose and save verifiers live in
`tests/scenarios/atlas/mm7_castle_fountain_*.yml`, `tools/menu_tours/atlas_mm7_castle_fountain.yml` and
`tools/screenshot_tours/atlas_mm7_castle_fountain_pose.yml`. Evidence stays private under `atlas-fountains-20261024`.
The remaining full-Atlas matrix, six well declarations, Gate destination semantics and device checks stay open.


### Shared dimensional-gateway destinations and cross-world history, 2026-10-24

The native dimensional selector and Atlas now share `engine/world/gates.yml` and `WorldGateCatalog`.
The catalog preserves the existing MMerge landing policy, Verdant unlock variable, QBits 4/519, original
Town Portal labels and icon placement. It replaces the duplicated C++ landing overrides; it adds no new
campaign requirements or automatic return passage. Unknown maps, malformed/ambiguous branches and unknown
fields fail loading. `evt.OpenDimensionDoor()` uses the default network; an optional network ID selects another
catalog entry. Replacing/cancelling a selector clears its previous physical origin.

Five physical MM7/MM8 sources have callback-owned `dimension_door` references. Explicit site groups describe the
multipart MM7 desert guild gate and MM8 Ravenshore fountain. Their actual raw component/face discoveries remain
independent. Atlas Guide reads current authored landing choices; Discovery retains the destinations actually
shown by a source gateway, with observation dates and an availability warning. These offers inform destination
names without granting a visit, terrain, remote sites or party arrow. Only successful native moves record actual
arrivals, with canonical source-map identity propagated through the existing move/spell path. Source maps retain
both offered choices and travelled arrivals; matching arrivals enrich a choice instead of hiding its siblings.

MM7/MM8 Atlas navigation can switch to an offered destination, an actual arrival or the recorded source in the
other world. A world without an Atlas catalog, such as MM6 here, has descriptive information and disabled map
navigation. The existing serializer/refill retention preserves offers and arrivals without a new save version.
The Core records these observations with the Atlas UI mod disabled too; Guide browsing creates none.

Validation for this checkpoint:

- Windows Release build passed. The current focused native unit selection passed 72 cases / 33,779 assertions,
  including all five physical source bindings, strict catalog/Lua validation, progression changes, malformed
  historical offers, discovery privacy and cross-world arrival projection. Three runtime UI cases / 147 assertions
  passed for original artwork/hover geometry, party-dependent selector caching and replacement/cancellation.
- All five literal gateway sidecars passed the lighting metadata validator. The Windows overlay now includes the
  required catalog and four unchanged upstream dimensional-selector bitmaps missing from its bootstrap payload.
  Full source Android packaging already includes these engine assets; no APK acceptance is claimed here.
- Native Windows AMD/OpenGL tours passed for locked first contact, actual offers plus undiscovered Jadame,
  MM7 Castle Harmondale to MM8 Dagger Wound Island, and the same transfer with the Atlas mod disabled. Inspected
  captures verify selector art, offer/Guide separation, preserved black undiscovered terrain, source/destination
  navigation and all offered choices remaining available after one was travelled.
- A reverse native transfer passed from MM8 Ravenshore to MM7 Emerald Island. Its physical mesh pick hit
  model 84/face 31/event 104 at 173 units, rather than replaying an event ID. The source is a grouped fountain,
  while the actual observation retains face 31. Ordinary Escape dismisses the existing first-continent intro
  movies in the travel tours; no debug cutscene bypass or forced quest unlock is used.
- Post-UI faithful save/load verifiers passed: 17 steps for offers, 17 for locked Guide browsing, 28 for the
  forward transfer, 28 for the Atlas-disabled transfer and 30 for reverse travel. They preserve canonical arrival
  keys, source offers, the actually touched MM8 face, unchanged mana (21), gold (500) and food (10), including
  another save/load and a controlled source-map revisit. All campaigns/settings/captures are isolated.

Reproduction inputs are `tests/scenarios/atlas/mm7_gate_*`, `mm8_gate_return_*`,
`tools/menu_tours/atlas_mm7_gate_*`, `atlas_mm8_gate_return.yml` and the matching screenshot poses. Use the existing
Castle Harmondale pose for MM7. The shared MM7 travel fixture starts a controlled new game and accepts Verdant's
quest through four real dialogue actions; copy its exported save as `saves/before.oysav` for the travel/offer tours.
For locked first contact use the previous Castle fountain fixture. The reverse fixture's `saves/seed.oysav` is the
successful forward tour's quicksave; its controlled Ravenshore setup is not itself claimed as a travelled route.
Restore each fixture before rerunning its tour, then run its verifier against the fresh quicksave. The Atlas-off
control needs its own one-mod fixture/profile because a save requiring Atlas cannot be silently stripped of it.
Evidence stays private under `atlas-gates-20261024` in the Windows development root.

This checkpoint does not complete the Atlas objective. The full conditional/proximity Gate audit, complex
Barrows/Regna graphs, six unresolved well bindings, remaining quest/service/item content, localization and
Android/controller checks remain open in `ATLAS_COVERAGE.md`. Arbitrary community-world map-filename collisions
still require runtime routing work; the verified travel paths use the current official mounted maps.


### Conditional dungeon destination references, 2026-10-24

The previous single-target metadata selected the first `MoveToMap` in an event. In Barrow VII that described
Barrow IX even though the same exit leads to Barrow X after the lever changes state. The exporter now reads
all reachable destination possibilities, attaching callback-owned `SetMapConnectionTargets` annotations to the
existing generated site-reference sidecars. The shared program/World Database exposes all branches; multiple
possibilities clear the misleading single-target/name fields. Guide can cycle the possible destinations, enriched
with actual arrival observations, while Discovery retains only the recorded trips. Possibilities are explicitly
not an evaluation of the current lever/quest condition. No gameplay callback, quest state, RNG, geometry or save version
was changed to obtain these references.

The source catalogs contain 375 base event declarations / 403 possible targets across MM6–MM8, including
28 multi-target events in MM7. They were regenerated with `tools/regenerate_lua_events.sh --site-refs-only`.
Current callback replacement still invalidates stale base annotations. Literal post-overlay references describe
the two existing MM8 submarine replacements, including the Small Sub Pen return trip that does not require a key.
The exporter now preserves LF output on Windows; generated text is not manually patched or normalized.
All 191 generated map site catalogs and both submarine sidecars pass the strict lighting metadata validator.

Validation for this checkpoint:

- Windows Release compiled successfully. After mounting the regenerated sidecars, 67 selected unit cases /
  34,740 assertions passed. They include strict callback ownership, malformed declarations, an actual Barrow
  callback executed with both lever settings, read-only Guide/Discovery queries, canonical arrival history and
  an audit resolving every active connection declaration against its owning world's MM6/MM7/MM8 map registry.
  The focused Python lighting suite passed 20 tests, including rejection of executable/invalid connection data.
- Actual headless mesh picks exercised Barrow VII's face 250/event 502 in both states: before the lever it reaches
  Barrow IX; using lever face 356/event 451 switches MapVar 2 to 2 and the same exit reaches Barrow X. These are
  real interactions, not event-ID replay or injected map-variable changes. The independent submarine pick hits
  Small Sub Pen face 524/event 451, confirms the native transition prompt and reaches Pirate Outpost without
  setting the pirate-key quest bit.
- Three native Windows AMD/OpenGL tours passed for both Barrow states and the submarine return. Inspected
  captures show Guide's two Barrow choices, its matching travelled destination, Discovery's one actually travelled
  route in each state and the current submarine destination. Source-map navigation preserves discovered plans;
  Guide neither grants the other trip nor replaces the actual route with its first authored possibility.
- Faithful post-UI verifiers passed 22 steps for each Barrow state and 19 for the submarine. Another save/load
  retains canonical arrivals, the actual source faces, the lever state and gold 500 / food 10 / spell points 21.
  Controlled revisits of the untravelled Barrow branch find no fabricated arrival; the submarine control retains
  QBit 214 clear. All saves, profiles, settings and captures are isolated.

Reproduction files are `tests/scenarios/atlas/mm7_barrow_{a,b}_{fixture,probe,verify}.yml`,
`mm8_regna_{fixture,probe,verify}.yml`, and matching `tools/menu_tours/atlas_mm7_barrow_*` /
`atlas_mm8_regna.yml` and screenshot poses. Copy each exported `before-connection.oysav` fixture to
`saves/before.oysav`, restore its one-shot pose, run the native tour, then verify its fresh quicksave. The submarine
needs ordinary transition confirmation (Return in the native tour, `map_transition_confirm` in the headless probe).
Evidence stays private under `atlas-connections-20261024` in the Windows development root.

This completes the conditional destination-data correction and native choice navigation, not the full Atlas
objective or dungeon graph presentation. Barrows/Regna graph views, current-condition explanations/eligibility,
remaining handwritten connection declarations, the full content matrix, localization and device checks stay open.


### Native dungeon connection graphs, 2026-10-24

The map header now opens a native Connections view backed by the shared World Database. It derives directed
source/event/destination edges from active callback-owned connection declarations and canonical actual-arrival
history. Discovery shows only actual journeys, even when a different possible destination was visited by another
route. Guide adds dashed possibilities; solid arrows retain observed travel dates. Replacing/removing declarations
does not erase independently observed trips, and an arrow never asserts a return route or current availability.

The graph expands connected indoor maps and stops at outdoor boundary nodes. Focus reroots at the selected node,
including an outdoor endpoint when its adjacent network is wanted. The Barrows catalog contains all fifteen
barrows, Zokarr's Tomb and the Barrow Downs (17 nodes); focusing Regna exposes an eight-node local network including
the submarine/Pirate Outpost route. These are derived from active programs, not hardcoded graph map lists.
Cross-world Gate navigation remains in the existing offer/arrival details, outside this local-world graph.

Nodes and incident routes have separate pagination. Selecting a node emphasizes its routes, with subdued unrelated
edges and separate lanes for parallel/opposite directions. Routes can be searched and open either endpoint's map
plan. Departure navigation selects genuine local connection sites and retains the selected conditional destination,
including Barrow X rather than silently resetting to Barrow IX. Unrelated inherited objects sharing a callback ID
are excluded. Multiple genuine source sites remain selectable; missing source geometry is not replaced by invented
coordinates. Destination navigation highlights an actual arrival when the selected trip has one.

The Guide connection scan reads one map event program per frame while the view is open. It does not load remote
dungeon geometry or falsely complete the independent item-source scan; failures leave an explicit partial catalog.
The graph adds no save namespace/version and browsing it changes neither knowledge nor gameplay state.

Validation for this checkpoint:

- Windows Release compiled and linked successfully. Four focused graph unit cases passed 638 assertions, including
  active MM7/MM8 program indexing without map geometry, Barrows/Regna topology, directional/outdoor boundaries,
  malformed ownership, Discovery privacy and replacement/removal without erasing observed history.
- Two native Windows AMD/OpenGL tours passed with fresh screenshots inspected. They cover Discovery's two-node
  actual journey, Guide's 17-node Barrows network, Regna Focus/eight nodes/route page two, source/destination plans,
  the untravelled Barrow X branch retained in source details and returning to Discovery after Guide browsing.
- Faithful post-UI scenarios passed 25 MM7 and 24 MM8 steps. They compare unvisited destination discovery bits
  before/after browsing, preserve the actual current map and canonical arrival, reject raw-alias/fabricated trips,
  and retain gold 500, food 10 and spell points 21 through another save/load. Source-face discoveries, the unchanged
  Barrow lever and clear pirate-key QBit 214 also survive. All settings, saves and captures are isolated.

Reproduction uses `tools/menu_tours/atlas_mm7_barrow_graph.yml`, `atlas_mm8_regna_graph.yml` and
`tests/scenarios/atlas/mm7_barrow_graph_verify.yml`, `mm8_regna_graph_verify.yml`. Start from the accepted native
journey quicksaves produced by `atlas_mm7_barrow_a.yml` and `atlas_mm8_regna.yml`, copied to `saves/before.oysav`.
Launch directly from that save with only the graph input tour (no pose/screenshot tour), then run the verifier
against its fresh quicksave. Private evidence is under `atlas-graphs-20261024` in the Windows development root.

This completes the initial graph presentation, not the full Atlas objective. Current-condition explanations,
remaining handwritten/conditional Gate and entrance references, complex floor separation, six unresolved well
bindings, the remaining quest/service/item matrix, localization and Android/controller validation remain open.


### Script-derived routing requirements, 2026-10-24

Guide's existing Read details panel now explains selected conditional passages using site references generated
from the actual legacy event control flow. It follows both comparison branches and explicit jumps, retaining
positive and negative campaign records, current/all-member item ownership and party/banked-gold thresholds.
Repeated contradictory predicates are pruned. Paths to an identical landing are grouped as alternatives, while
flags that merely record first entry are omitted when every path reaches the same landing. Landing identity
includes the full move parameters, not just the destination map's display name.

Nine current base declarations are exported: five shared MM6 references, three MM7 references (Harmondale's
altar, Celeste and The Pit) and the MM8 Plane Between Planes flute passage. They reuse callback-owned
`SetMapSiteReference`; overlays replacing the callback invalidate the base explanation. Names and QBit remaps
come from the current exporter lookups. Generated Lua was regenerated, not manually edited. No gameplay callback,
geometry, save format or party knowledge was changed to supply these references.

The analysis requires every reachable path to be supported. Unknown predicates/actions, cycles, multiple moves
on one path, missing labels and comparisons after state mutation suppress the whole explanation. A requirement
list is a description of the interaction's routing branches, not a live eligibility check, and explicitly excludes
other gameplay effects. Raw event/quest IDs and unlabelled local variables are not substituted into player text.
Lever/local-state routing, random paths, other predicate families and handwritten Lua remain open work.

Validation for this checkpoint:

- The Windows exporter compiled and regenerated the catalogs. All 191 generated map sidecars passed the strict
  literal metadata validator, and the Python lighting suite passed 20 tests. The Windows Release runtime overlay
  was refreshed with no game process running.
- 31 selected Windows unit cases passed 6,969 assertions. New cases cover positive/negative paths, alternative
  grouping, contradictory predicates, first-entry bookkeeping, unsupported paths, a gold threshold's exact boundary,
  canonical QBit remaps and actual execution with the required item on a different party member. Mounted MM7/MM8
  geometry-backed passage references remain Guide-only even after their physical sites are discovered.
- Two native Windows AMD/OpenGL tours passed with fresh captures inspected. The Pit shows its Light/Dark
  alternatives; the MM8 giant's-sword passage shows the selected character's Flute requirement. Returning to
  Discovery hides the unvisited source/reference and does not convert Guide browsing into exploration.
- Faithful post-UI scenarios passed 33 MM7 and 28 MM8 steps, retaining actual current maps, canonical arrival history,
  gold 500 / food 10 / spell points 21 through save/load. Unvisited source/destination discovery bits remain clear;
  MM7 Light/Dark bits remain clear, along with the previous source lever/face and MM8 pirate-key invariants.

Reproduction files are `tools/menu_tours/atlas_mm{7,8}_routing_requirements.yml` and
`tests/scenarios/atlas/mm{7,8}_routing_requirements_verify.yml`. Use the same before-UI saves as the graph
checkpoint (MM7 Barrow IX and MM8 Pirate Outpost), with only the menu input tour enabled. The tours navigate from
the continent overview to the Guide source; they never travel there. Private evidence is under
`atlas-requirements-20261024` in the Windows development root.

This adds initial routing explanations. It does not complete current eligibility, lever-state explanations,
handwritten references, the full Atlas content matrix, localization or Android/controller validation.


### Complete Barrow toggle references and controller ownership, 2026-10-24

Guide's routing details now explain the first/second lever positions for 27 authored passages across the fifteen
MM7 Barrows. The exporter matches the complete two-position callback, including both assignments, its threshold
and matching door actions. A competing base writer, unknown instruction, partial toggle or unsupported controller
suppresses inference. The references contain no raw local-variable IDs and do not claim the current position.
The remaining multi-target exit in this network, Zokarr's Tomb, is explicitly described as choosing between
Barrow IV and Barrow VI at random; reading its possibilities does not roll the game RNG or assert probabilities.

The optional fourth argument of `SetMapSiteReference(eventId, text, nil, {controllerIds...})` captures additional
map callbacks. An explanation is valid only while both its owner and every captured dependency remain registered
with the same function identity. A mod replacing/removing a lever therefore discards the old description even
when the exit itself is unchanged. Unrelated callback changes leave it intact, and explicit redeclaration can
supply new text. Dependency arrays are dense, distinct, bounded at 64 and exclude the owner; malformed values or
unregistered callbacks fail loading. Metadata remains presentation-only, with no save-format or gameplay change.

All sidecars were regenerated through the exporter. The 28 additional references use shared callback metadata;
there is no handwritten Atlas destination/control list. Runtime geometry and existing observed journeys still
own map placement and Discovery navigation. More complex controls, live local/remote eligibility and other
handwritten event semantics remain outside this checkpoint.

Validation for this checkpoint:

- Windows Release, the exporter and focused tests compiled. All 191 generated map sidecars passed the strict
  literal validator, and the Python lighting suite passed 21 tests, including rejection of invalid dependencies
  and executable additions. Runtime overlays were refreshed only with no game process running.
- 35 selected Windows unit cases passed 7,374 assertions. New checks cover dependency replacement/removal,
  malformed references, actual Barrow source control flow, threshold positions 0/1/2/255, competing state writers
  and random reference generation without executing callbacks. The mounted-content audit verifies all 27 physical
  exit bindings and 27 lever bindings across the fifteen Barrows. Geometry-backed Barrow and Zokarr descriptions,
  plus existing MM7/MM8 passages, remain Guide-only after discovery.
- Four native Windows AMD/OpenGL tours passed with inspected screenshots: Barrow VII routing details from the
  two accepted pre/post-lever journey saves, Zokarr's random exit details, and an MM8 Flute-reference regression.
  The tours browse the Atlas only; no movement, lever use or quest-state injection is used to read the descriptions.
- Faithful post-UI verifiers passed 25/26/35/28 steps respectively. They preserve actual map/arrival history and
  gold 500 / food 10 / spell points 21 through another save/load. The original Barrow lever settings and physical
  exit discoveries remain intact, as does the actually used lever face in the second setting. Unvisited alternative
  maps and Zokarr source/destinations remain undiscovered, and the MM8 pirate-key bit stays clear.

Reproduction uses `tools/menu_tours/atlas_mm7_barrow_controls_{a,b}.yml`, `atlas_mm7_zokarr_routing.yml` and matching
`tests/scenarios/atlas/mm7_barrow_controls_{a,b}_verify.yml`, `mm7_zokarr_routing_verify.yml`. The two control seeds
are accepted actual-journey quicksaves from the earlier Barrow a/b native tours, copied as `saves/before.oysav`;
Zokarr uses the same before-UI seed as the MM7 routing-requirements tour. No pose tour is needed. MM8 reuses
`atlas_mm8_routing_requirements.yml` and its verifier. Private evidence is under `atlas-controls-20261024` in the
Windows development root.

This completes the initial Barrow control and random-exit explanations. The full Atlas goal still includes the
remaining condition/content matrix, complex floors, localization and Android/controller validation.


## Companion campaign state and recruitment contacts (2026-10-25)

The Companions reference now distinguishes the active party from the Adventurer's Inn roster. Discovery retains
Inn members instead of dropping them when they leave the active party. Current campaign name, class and level
are separate from the explicitly labelled starting profile. Unowned companions remain hidden in Discovery;
seeing an invitation alone does not disclose their full profile. Guide explains an unowned companion's existing
Inn-unlock flag when the shared roster declares one; it does not equate that flag with every recruitment condition.

Contacts are joined by the shared NPC-topic/roster resolver, not by matching names or assuming that an unplaced
roster-proxy NPC is a map location. Guide reads configured/current topics and callback-owned possible topic
assignments. A declared global callback that can grant a companion's Inn-unlock flag supplies a possible unlock
contact. Existing physical interaction references connect such contacts to rescue/statue sites. In particular,
Fredrick links to his actual dialogue NPC and Cauri links to the Murmurwoods statue, whose native details explain
both item and spell alternatives. The Guide source scan is available in Companions to index remote physical sites.

Discovery contact history records only actual displayed roster invitations/confirmation offers in the shared
dialogue controller, even with the Atlas mod disabled. Links still require known containing geography and observed
NPC locations; invitations do not invent a residence. Named global discovery state persists through ordinary saves,
without a save-format change. Existing saves keep party/Inn status but do not invent historical invitations.
Possible metadata remains Guide-only and cannot establish that a campaign actually completed a recruitment.

Validation:

- Windows Release built. The focused guide/dialogue/metadata suites passed 40 cases / 1563 assertions; the mounted
  Fredrick/Cauri content regression passed 1 case / 22 assertions. Lighting metadata validation passed 22 tests. A separate two-case mounted-content/connection audit passed
  596 assertions and loaded every MM7/MM8 map program with the new metadata.
- Generated one shared global catalog and 134 map catalogs (43 MM6, 53 MM7, 38 MM8) from the canonical exporter.
  All map catalogs passed the strict literal validator. Callback replacement/removal invalidates stale references,
  including early-named overlays; the handwritten MM8 promotion replacements explicitly redeclare their sources.
- The faithful 16-step dialogue/save roundtrip passed with Atlas enabled and disabled. It directly opens existing
  roster proxy NPCs as a test fixture, not as evidence of their physical placement. It recruits Rohani into the
  fifth slot and sends Fredrick to the Inn when the party is full.
- Nine native Windows OpenGL captures were inspected for party/Inn state, Fredrick's contacts, Cauri's possible
  contact, navigation to her statue and its item/spell requirements, and return to Discovery with only the two
  owned profiles. The 13-step post-UI verifier preserved invitation history, gold, current map and the clear Cauri
  unlock flag across another save/load; Murmurwoods and Cauri remained undiscovered.

Reproduction uses `tests/scenarios/atlas/mm8_companion_contacts.yml` for the isolated seed, copied from the
scenario temporary save root to `saves/before.oysav`, then `tools/menu_tours/atlas_mm8_companion_contacts.yml` and
`tests/scenarios/atlas/mm8_companion_contacts_verify.yml`. Private captures, fixture scripts and logs are under
`atlas-companions-20261025` in the Windows development root. Original user campaigns remain untouched.

This implements live roster state and initial recruitment sources. At this checkpoint remote map-topic indexing
was still open; the follow-up below adds it. Full dialogue eligibility, Inn venue navigation and the remaining
Atlas coverage matrix remain open.


## Remote companion topic declarations (2026-10-25)

World Database now retains Guide-only possible NPC-topic assignments from scanned map programs. Each canonical
map owns one replaceable snapshot: reindexing clears removed/replaced declarations without erasing another map's
sources or accumulating stale unions. Snapshot values are independent of the temporary loaded program's lifetime.
Foreign-world programs and observed-only databases do not add these references. A declaration's owner map is
provenance, never an inferred NPC position. The Guide still resolves topics through the shared roster resolver
and links only independently authored/current/possible NPC residences or physical interaction references.

The native source scan and current-map indexing populate this same database. Declared future invitations are
labelled **Possible scripted contact**, distinct from configured topics or actually observed invitations. The
Atlas constructor also now keeps current-map gate indexing inside its existing optional-current-map guard.

Validation:

- Windows Release built. Focused World Database/companion/mounted-program tests passed 32 cases / 1966 assertions,
  including stale snapshot replacement, independent owners, copies, wrong-world filtering, observed-only privacy,
  actual Fredrick/Cauri content and loading every MM7/MM8 map program. The existing opt-in packaged-house geometry
  audit requires its separate audit root and was not run as part of this focused command.
- Four inspected native AMD/OpenGL captures use a separate synthetic asset fixture: a remote dungeon callback
  can assign a Cauri invitation to Fredrick. Before scanning, that reference is absent; after scanning it appears
  beside Cauri's ordinary rescue source. The link navigates to Fredrick's independently authored Dagger Wound
  residence, not the remote dungeon. Discovery still contains only the two owned companions.
- A 15-step faithful post-UI verifier passed. It keeps the remote dungeon, Dagger Wound and Murmurwoods unknown,
  NPC 28 unseen, gold 500/current map/unlock flag unchanged through another save/load. Opening Fredrick's actual
  dialogue afterwards still shows topics 10/11, proving the reference scan did not execute the fixture's topic-630
  assignment. The normal asset overlay has no QA script; the private copy differs by exactly that one declaration.

Reproduction: `tests/fixtures/atlas/prepare_assets.py --fixture remote-companion-contact`,
`tools/menu_tours/atlas_mm8_remote_companion_contacts.yml` and
`tests/scenarios/atlas/mm8_remote_companion_contacts_verify.yml`; setup and the synthetic-content limitation are
explained in `tests/fixtures/atlas/README.md`. Private evidence is under `atlas-remote-contacts-20261025` in the
Windows development root. This completes remote declaration indexing, not full recruitment eligibility or the
full MM7/MM8 Atlas objective.


## Recorded spell effects at interaction sites (2026-10-25)

The shared Core site observer now snapshots actual party/character buffs around a successful synchronous
interaction, including inherited-decoration callbacks. It records whether identified spell effects were applied,
updated or removed. Snapshot game time excludes ordinary timer decay/expiry from site attribution; roster changes
exclude character-buff attribution. A source spell can own several buff components, so the text describes changes
to effects from that spell, not receipt/removal of the complete composite enchantment. Duration, power and mastery
are compared to detect a change but are not presented as a future outcome or saved as an exact reward forecast.

The native Atlas resolves spell names through the active shared SpellTable for both findings and search. Knowing
an unrelated active buff alone does not discover an object's effects. Unknown source ID zero stays unidentified
and cannot suppress otherwise valid numeric findings. Empty/no-change visits retain earlier spell observations.
The existing named map namespace preserves history through save/load and map refill without a binary save change;
old records without spell fields still load. Invalid new fields produce an explicit invalid-record result and
cannot be silently overwritten by another observation. Recording remains active when the optional Atlas is off.

Validation:

- Windows Release built. Focused site/World Database suites passed 41 cases / 1499 assertions. They cover actual
  event buff application, party/character separation, active table names and search, unchanged visits, refresh,
  removal, natural expiry, roster replacement, partial composite removal, unidentified sources, corrupt keys and
  atomic rejection. The existing opt-in packaged-house geometry audit was outside this focused run.
- Explicit private MM7/MM8 fixtures replace one existing well callback with Stone Skin/Bless. Both faithful
  12-step scenarios used the real physical target, recorded initially absent spell receipts and survived save/load.
  The MM8 scenario also passed with Atlas disabled. These are controlled API tests, not ordinary well reward edits.
- Two native AMD/OpenGL tours passed and produced eight captures. Inspected findings and Discovery-after-Guide
  captures show both named spells in both worlds; searching Stone Skin selects the actual observed site. Both
  nine-step post-UI verifiers preserve map, gold 500 and exact action flags through another save/load.
- Each private package copy differs from the normal assets overlay by exactly its one QA script. No fixture was
  installed in the normal runtime or original user campaign.

Reproduction uses `tests/fixtures/atlas/prepare_assets.py --fixture site-spells-mm7` / `site-spells-mm8`,
`tests/scenarios/atlas/mm*_site_spell_effects_{fixture,verify}.yml` and
`tools/menu_tours/atlas_mm*_site_spell_effects.yml`. Setup is in the fixture README; private evidence is under
`atlas-site-spells-20261025` in the Windows development root.

Deferred effects after the synchronous activation, unidentified spell sources, broader item/quest/site semantics,
the six unresolved original well bindings and the remaining Atlas coverage matrix are still open. This does not
add Guide predictions for spell-casting callbacks or establish Android/controller acceptance.


## Journal bookkeeping in well references and real armor bonuses (2026-10-26)

The Guide now explains 21 more authored callbacks (20 MM7 and one MM8) whose rewards were previously suppressed
by journal-note checks. The exporter resolves only identical-effect branches with complementary/redundant
requirements. Annotation bookkeeping disappears; genuine named journal gates remain. Private/blank remaining
gates and unrepresentable post-write requirements still reject the reference. Stable contradictory branches are
pruned before their unreachable actions are inspected. No handler executes while generating or reading text.
Existing generated declarations are retained; regeneration also normalizes requirement order in older references.

Every new callback binds a real Water feature in the mounted maps. Mount Nighon's out10:203/204/206 had physical
ODM surfaces and authored journal notes but generic context categories; the existing explicit context overlay now
identifies the two wells and central fountain. The six previously audited missing base-site bindings remain open.
Murmurwoods out07:101 correctly describes applying severe poison, not curing it.

The Tatalia armor well exposed a shared Core error: Add/Subtract ArmorClassBonus wrote a generic event variable
instead of the character's signed armor modifier. Read/write/arithmetic now use the actual modifier, with bounded
64-bit intermediates. Existing save fields are unchanged. Old saves whose personal-use flag was already consumed
are not automatically refunded from the ambiguous generic variable; any repair needs an explicit migration.

Validation:

- Windows Release built. The final focused selection passed 31 cases / 1560 assertions, including all 21 original
  EVT/STR callbacks with learned/unlearned note states, selected/other-character separation and repeated use;
  every mounted physical binding/category and Guide privacy; signed armor endpoints and snapshot restore;
  existing conditional drinks, site/spell observations, journal references and Barrow/Zokarr routing guards.
- Two faithful 15-step scenarios use ordinary authored targets and normal assets. Tatalia grants armor +20 only
  to the selected member; Murmurwoods applies severe poison only to that member. Both preserve journal/actual
  outcome records through reload and avoid additional effects on repeated interaction. Parties/resources are
  explicit controlled fixtures; these are not full campaign playthroughs or tests of every reset schedule.
- Four native AMD/OpenGL tours produced 14 current captures. Inspected known/unused reading pages show the real
  effect in Guide and historical-only findings in Discovery. Each tour returns to gameplay and writes a fresh
  private quicksave. Unused-site captures show no learned findings even after consulting Guide.
- All four 15-step post-UI verifiers passed. They preserve selected/other-member values, learned/unlearned note,
  actual observation flag/endpoint, map and gold 500 through another save/load; unused saves retain no reward
  or effect receipt after browsing. The existing opt-in packaged-house audit was outside this focused run.

Reusable inputs are `tests/scenarios/atlas/mm*_journal_well_fixture.yml`, the paired known/unseen verifiers and
`tools/menu_tours/atlas_mm*_journal_well_*.yml`. Setup is documented in `tests/fixtures/atlas/README.md`.
Private evidence is under `atlas-journal-wells-20261025` in the Windows development root. Original user campaigns
and the official installation are untouched. This advances the coverage matrix; it does not complete all Atlas
content, old-save repair, localization or Android/controller acceptance.


## Actual item gains and costs at interaction sites (2026-10-26)

The shared site observer now captures net quantities of carried items before/after a successful interaction.
It uses the Party's active packs, equipment and full cursor-stack quantity. Moving an item between those locations
or identifying it does not create a receipt. Reordering the same members is supported; a changed roster cannot
attribute new/departing companions' packs to an object's rewards. History stores observed nonzero min/max deltas,
not promises of future rewards, totals earned across all visits or a transaction log of changes that cancelled.

Item IDs refer to the existing active ItemTable. WorldItemSources retains reward origin/provenance ownership;
no item names, definitions, enchantments or separate loot database are copied into site state. Atlas findings and
search use party identification knowledge, including while reading actual history in Guide. Thus an unknown
Stone to Flesh scroll remains Scroll in its receipt; the separate Guide requirement can still name its authored
spell. Discovering an object alone does not create an item receipt.

The existing site-effects namespace stores exact signed 64-bit net ranges as four signed 32-bit named fields
per item. It survives refill and ordinary saves without changing the binary format. Older absent item fields
remain valid, no-change visits preserve prior findings, and malformed/incomplete new fields reject writes
atomically instead of overwriting history. The Core collector remains independent of the optional Atlas module.

Validation:

- Windows Release built. The focused site/World Database/mounted-category selection passed 18 cases / 558
  assertions, including real shared activation, pack/cursor/equipment transfer neutrality, unidentified/active
  table names and search, recruitment filtering, signed 64-bit endpoints, refill and malformed-record rejection.
- Two faithful normal-assets scenarios use the actual Tatalia altar (22 steps) and Murmurwoods rescue statue
  (21 steps). Controlled quest/item setup precedes actual interaction: statuette/scroll counts drop by one,
  corresponding quest flags change, receipts persist after reload. MM8 also passed with Atlas disabled.
- A separate private QA-only well callback queues two unidentified item-1 deliveries. Its 16-step faithful
  scenario confirms receipt +2, one item in the pack and one actually held after reload. Ordinary Harmondale rewards remain unchanged.
- Three native AMD/OpenGL tours produced 13 captures. Inspected reading pages show Eagle Statuette -1,
  Scroll -1 and Longsword +2; actual unidentified names are retained after Guide browsing. The native reward
  tour performs the delivery itself before browsing, exercising one item in the pack and one on the cursor.
- Three 21-step post-UI verifiers passed. The reward verifier checks both the pack count and actual held-item
  identity before/after another save/load. Others preserve item costs and quest state; all retain exact receipt
  endpoints, gold 500, food 10 and the original map. No original campaigns or official installation were used.

Reusable inputs are `tests/scenarios/atlas/mm*_site_item_effects_{fixture,verify}.yml`,
`mm7_site_item_gains_{fixture,verify}.yml` and matching `tools/menu_tours/atlas_mm*_site_item_*.yml`.
The fixture README distinguishes normal assets from `prepare_assets.py --fixture site-items-mm7`; private logs,
captures and hashes are under `atlas-site-items-20261026` in the Windows development root.

Queued delivery after an inherited callback's observation has ended, deferred effects, instance transformations,
remaining Guide effect/predicate families and the rest of `ATLAS_COVERAGE.md` remain open. The focused run did
not execute the existing opt-in packaged-house audit or establish Android/controller acceptance.


## Confirmed inherited-object deliveries on the site page (2026-10-26)

Site reading/search now joins the existing confirmed WorldItemSources receipts to the exact inherited-decoration
slot in the same map. This covers item delivery that completes after the shared callback's immediate snapshot.
Pending grants and possible reward declarations never enter this view. A different slot, map or reward kind
cannot supply its entries. Different callback variants that delivered the same item type are condensed to the
latest available recorded origin; historical delivery remains attached to the object after depletion or variant changes.

Names come from the current ItemTable and existing party identification knowledge in both Discovery and Guide.
The new Items received here section shows item type and the existing recorded origin day. Existing receipts do not retain
quantity, so no count, per-use delta or accumulated campaign total is inferred. Immediate net-change ranges
remain separate. When deliveries are present the UI omits the snapshot-only unchanged-use claim, preserving
the actual saved lastChanged flag rather than changing gameplay/history to make the label appear consistent.
Independent site-discovery gates remain in force. No grant handler runs while searching, reading or scanning.

Validation:

- Windows Release built. Focused site/source/World Database tests passed 23 cases / 390 assertions: pending versus
  confirmed receipts, exact slot/map/kind, observed-only filtering, current table identification, repeat-type
  deduplication and last date, independent discovery gates, unchanged snapshot preservation, existing signed
  net-item ranges and corrupt/pending decoration origins. The opt-in packaged-house audit was not run.
- Two private three-step scenarios prepare unused objects/parties only. Native AMD/OpenGL input performs the actual
  activation on MM7's Harmondale beacon and MM8's Dagger Wound barrel, then opens the physical object's reading
  page and toggles modes. Ten fresh captures were produced; inspected details retain Longsword as unidentified.
- The private QA-only global supplement deliberately depletes each object before queuing two unidentified
  item-1 grants. These are controlled modded callbacks, not new rewards installed in ordinary campaigns. The
  confirmed origin survives clearing the callback's active contents; one item is stored and one remains held.
- Both 27-step faithful post-UI verifiers passed: exact origin coordinates/slot/event, depleted object state,
  immediate changed=0, original Might/skill points, one item in the pack and one actually held, gold 500,
  food 10 and map survive another save/load. Browsing adds no grants. Pending receipt ownership
  and the immediate snapshot are unchanged; only the derived reading/search view is extended.
- A separate unchanged-runtime MM8 control reused the ordinary inherited-decoration fixture/tour/verifier.
  The actual barrel still grants exactly Base Might +2 once, records its learned journal description and
  empty interaction, and preserves Might 17 and unrelated object discoveries through save/load. This normal
  five-step preparation, native tour and 22-step verifier are independent of the private gift callback.

The native preparation/input split is deliberate: the inherited sprite ray in the headless trial did not activate
the beacon. Preparation is not activation evidence; real native input/captures plus the post-UI verifier are the
acceptance path. Reproduction uses `prepare_assets.py --fixture inherited-deliveries`,
`tests/scenarios/atlas/mm*_inherited_delivery_{fixture,verify}.yml` and matching `tools/menu_tours/` files.
The fixture README explains the specific MM7 result selection. Private evidence is under
`atlas-object-deliveries-20261026` in the Windows development root. Original campaigns remain untouched.

Exact queued-delivery quantities, other deferred effect attribution, instance transformations and the remaining
coverage matrix are still open. This derived view adds no save format, separate reward history or item database.


### Depleted decoration initialization state

The post-UI check exposed a separate Core bug: when every decoration variable became zero, both indoor/outdoor
startup heuristics could interpret the depleted map as never initialized and seed rewards again. Initialization
now writes `openyamm.decorations.initialized.v1` only after bindings are available and initialization completes.
The shared policy preserves initialized all-zero states on later view creation and save/load. Existing nonzero
runtime/authored states adopt the marker without changing values; invalid markers fail explicitly. The non-discovery
marker is not retained by the refill knowledge transfer, allowing a fresh refill to seed its new population.

Pure policy coverage checks delayed initialization, depleted reload, nonzero legacy/authored states, refill and
corrupt markers. The native MM7 one-beacon fixture catches the former all-zero case; post-UI checks require zero
contents and marker=1 through another load. No binary save format changes. Historical all-zero saves without an
initialization marker remain ambiguous; this fix does not infer or refund past consumed/reseeded rewards.


## MM8 individual delivery contacts (2026-10-26)

The shared quest registry now contains 145 quests / 595 relationships. Aislen's Cure Disease request and
Pole's Fire Resistance request each reference the requested item and six actual recipient NPCs. Their
individual handoff flags gate Disclosure; the generic shared quest-history mechanism remembers learned
contacts after turn-in and save/load. Guide browsing neither executes deliveries nor records the other five
contacts. Residences remain references to the World Database; a learned name does not invent a known address.

The English journal's Rust return name is corrected from Hobert to Pole in the authoritative quest table.
Hobert is still one of the recipients. The incremental Windows overlay mounts the English table alongside
quest relationships, so the running game's journal and Atlas share the correction. Original TXT encoding,
line endings and every other row/column are preserved. Generated event files are unchanged.

Acceptance on Windows Release:

- Eleven focused quest/condition/campaign tests passed 810 assertions, including the full shared-content
  relationship validator, independent recipient order, undisclosed contacts and historical retention.
- Two faithful actual-dialogue scenarios passed 100 and 98 steps: acceptance, six separate handoffs, repeated
  conversation, refusal without supplies, real return/reward and persistent contact keys after reload.
  Explicit NPC-ID access bypasses travel; the extra three supplies are controlled fixtures. The authored
  cursor item is explicitly placed into inventory with the real placement helper, not inferred absent.
- Two native AMD/OpenGL tours produced eight fresh inspected captures. Partial Discovery displays only Zevah
  Poised / Celia Stone respectively; Guide displays all six actual recipients, and Discovery remains restricted
  after toggling back. The Rust title, quest prose and return contact all use Pole. Native SDL tour input does
  not inject operating-system mouse input or terminal control sequences.
- Both post-UI verifiers passed 28 steps each, loading before/native saves and another round trip. XP remains
  250 / 1000, gold 500, exactly the first delivery is complete, the five remaining delivery/completion flags
  stay clear, and exactly one recipient-history key remains learned.
- Runtime ZIP entries for both edited tables were compared byte-for-byte with source. Private settings,
  scenarios, logs, screenshots and artifact hashes are retained under `atlas-quest-deliveries-20261026/`
  in the Windows development root. User campaigns and official-installation data were not test-write targets.

Reproduction inputs and fixture limitations are in `tests/fixtures/atlas/README.md` and
`ATLAS_QUEST_AUDIT.md`. No save schema, duplicated NPC/house database, reward rebalance or new checklist UI
is introduced. This closes these two delivery-contact gaps; broader quest stages, site/reference families,
complex floors, localization and Android/controller acceptance remain open in `ATLAS_COVERAGE.md`.


## Passage item scope and campaign-persistent unlock references (2026-10-26)

The active Lincoln exit (MM7 `7d23:501`) and submarine entrance (MM8 `d06:451`) now have callback-owned
native references. Their previously unclassified physical surfaces become Atlas passages. Names and destinations
resolve through the current item/map tables. These declarations follow the active Merge supplements rather
than restoring stale explanations for replaced base callbacks.

The shared typed passage schema supports party-wide, selected-member and every-member possession checks,
plus a global `openedQBit` alternative to the existing source-map `openedMapFlag`. Unlock scopes are mutually
exclusive; no unlock field means the item must be present on every use. Existing keyed entrances retain their
source-map semantics. The same schema describes dungeon entrances and exits, with strict C++ loading and the
lighting producer's literal metadata validation. Replacement/removal still invalidates the declaration by
callback identity. No gameplay callback, reward, geometry or save schema is changed.

Lincoln's active callback checks every current member, not a fixed four/five-character roster. It checks
possession, including inventory/equipment. A successful call opens the normal transition confirmation rather
than immediately setting `pendingMapMove`; the test verifies that actual pending transition. This is not a
claim that unequipped suits permit safe exploration of Shoals: its independent survival rules require equipped
suits, and the passage reference does not interpret the destination's timers/hooks. The Guide connection
reading footer now distinguishes possible passage interactions from current availability/destination conditions.

The submarine accepts a key held by any party member, or retained campaign bit 214 after an earlier unlock.
The reference therefore does not claim that a source-map reset closes it or that the key is consumed. Raw
quest-bit numbers and local variable names are not displayed as player-facing requirements.

Validation:

- Windows Release built. Ten focused cases passed 2079 assertions, including schema types/invalid mixed fields,
  callback invalidation, canonical physical bindings/Guide-only disclosure, all active MM6/MM7/MM8 connection
  targets, existing keyed entrances/prison groups/Barrow rules and execution of the two actual supplements.
- Actual callback checks cover one-, two- and five-member parties. One missing suit blocks the Lincoln exit even
  with duplicate suits on another member; a key on the last member opens the submarine, and its campaign unlock
  permits reuse without any key. Inventory totals are unchanged. These unit checks verify queued/confirmed
  transition intent, not complete sea travel or destination survival.
- Twenty-three lighting producer tests passed; all nine current literal interaction-reference overlays validate.
  Neither new metadata file changes geometry cards or lighting dependency identities.
- Public three-step unused-site fixtures and native input tours are under `tests/scenarios/atlas/` and
  `tools/menu_tours/atlas_mm{7,8}_passage_scope.yml`. Post-UI save/load checks, captures and remaining limitations
  are described in the fixture README. Private evidence is retained under `atlas-passage-scopes-20261026/`
  in the Windows development root. Original campaigns are not test-write targets.

Broader handwritten passages, eligibility predicates, destination-specific rules and the full coverage matrix
remain open. This extends the shared content contract rather than adding an Atlas-only gameplay database.


Both final native AMD/OpenGL tours passed; six fresh captures were inspected. Guide shows the actual two
physical passage markers and their item/unlock references. Returning to Discovery removes the unlearned
reference; Lincoln's already visible exit may retain an empty findings page rather than fabricate learned
requirements. Both 25-step post-UI verifiers pass: the exact site interaction key and destination discovery
remain clear, both XP totals stay zero, gold/food remain 500/10, required items stay absent and global unlock
214 stays unset across before/native saves and another round trip. Runtime metadata ZIP entries match source;
`accepted.json` retains executable/overlay/capture/save hashes. Native tours inject only SDL-local input.


## Observed guild shelves and permanent encountered-book knowledge (2026-10-26)

Core now records identifiable spellbooks when the actual Buy Spellbooks shelf overlay is displayed. A building
visit or generated shop state is not treated as an observation. The shared input path records the visible
remaining stock after purchases/stealing, independently of the optional Atlas UI module. Names, spell identity,
shops and geography continue to come from the current shared registries.

The guide distinguishes Last seen (actual shelf plus recorded day) from Possible stock (authored random pool).
Observations describe a past visit, not current availability. Last-seen book IDs are replaced on a new shelf
observation; encountered-book knowledge persists after purchases or rotation. Viewing the shelf may unlock an
item/spell reference without owning its book or learning the spell. Unidentified books with a real identification
requirement are omitted. Another world's houses remain filtered, and a known guild observation cannot invent
knowledge of its containing map. Older generated stock does not retroactively become an observed offer.

`WorldGuildStockKnowledge` uses the existing named-global save mechanism under
`openyamm.discovery.guild-stock.v1/`, with sorted unique book IDs, a bounded count, a timestamp and permanent
known-book markers. Explicit zeroes retire old slots through session merges. Invalid types, counts, order or
missing snapshot fields fail explicitly. No binary save format or second gameplay/item/house database is added.
The Spell guide reads each candidate guild observation once per build rather than once per spell.

Windows Release acceptance:

- Four focused observation/pool cases passed 119 assertions: independent guilds, deduplication, repeated frames,
  latest stock replacement/empty shelves, retained titles, corrupt data/time bounds, both worlds' actual-versus-
  possible links, map-address privacy, and the repaired shared spell prose. The existing guild/book navigation
  case separately passed 101 assertions. Tests use separate processes for independent asset-filesystem owners.
- A broader preliminary guild/spellbook selector included the audio integration test and encountered its
  process-global PhysFS ownership conflict. This is not reported as a green full-suite run. Focused results and
  real native audio/shop runs are the acceptance evidence for this change.
- Two explicit six-step fixtures position a Knight at real guild doors and set shop RNG seeds before any stock
  exists. Seeds 5500129/5500182 make the stock assertions reproducible; they do not inject stock or knowledge.
  Native input enters the guild and opens the real shelf. Each observed shelf contains twelve distinct book IDs.
- Native Discovery shows Torch Light in Harmondale / Stone to Flesh on Dagger Wound as encountered books, with
  Last seen links to the actual guilds. Guide adds separately labelled possible sources; returning to Discovery
  preserves only its known information. Inspected native captures verify the actual reading pages and dates.
- Both 170-step post-Atlas verifiers passed. All twelve IDs/history keys, absent book inventory, gold 5000,
  food 10 and zero XP survive before/native loads, another save round trip, and explicit fixture map changes.
  Independent read-only inspection of the retained saves confirms unchanged observation timestamps (541).
- Real native purchases remove Fire Resistance item 1904 in MM7 (900 gold) / Feather Fall 1916 in MM8 (400 gold).
  Both 41-step verifiers require eleven observed books, a zero retired slot, retained title knowledge, exactly
  one purchased item, gold 4100/4600 and unchanged food/XP after another load. The initial unaffordable Inferno
  attempt was correctly refused and is not counted as a successful purchase.
- A separate MM8 run with only karol.test enabled records twelve books while the Atlas UI module is disabled.
  Its nine-step verifier preserves observation/count/gold through another load. Preliminary door-only native
  controls recorded no shelf observation merely from the root guild dialogue.
- Native tests use AMD/OpenGL and SDL-local scripted input, isolated settings/saves and normal runtime assets.
  Final scenario inputs, native tours and fixture limitations are documented in the fixture README. Private
  evidence is under `atlas-guild-shelves-20261026/` in the Windows development root. No user campaign or official
  installation data is a test-write target.

The visual review also exposed six damaged quote pairs around point of no return in the authoritative shared
spell table. Those exact byte sequences are replaced by ASCII quotes; all other bytes/line endings are retained.
The Windows overlay now mounts spells.txt so runtime descriptions match source. The loaded seven affected/
neighboring curative descriptions are tested, and the native Stone to Flesh reading page was inspected again.
No spell behavior, costs, skill caps or balance changed.

Conditional guild access/learning eligibility, a general equipment-shop observation browser, broader content
coverage and Android/controller acceptance remain separate work in the coverage matrix. This checkpoint does
not complete the full MM7/MM8 Atlas objective.

### Discovered multipart passage positions and heights, 2026-10-26

Grouped passages previously became visible after any physical component was discovered, but their marker,
map bounds and initial height still used the first authored component. The floor filter also accepted unknown
components. This could place a known passage at an unseen entrance or expose an unrelated height.

World Database now supplies individually visible components, using the existing interaction, exploration and
face-visibility rules. The Atlas derives pin positions, framing positions, available heights and floor-filtered
site lists from those components. Ordinary entrance pins use the same projection. Each group retains its
existing stable ID and one list entry; on a selected height its pin uses a known component at that height.
Guide may show authored components, while returning to Discovery rebuilds the selection from actual knowledge.
Legacy notes without elevation remain visible at all heights; explicit note heights retain their meaning.
No discovery flags, event handlers, gameplay tables or save format are changed by this fix.

Regression checks cover an indoor passage with only its upper component discovered, an outdoor entrance whose
known component is far from its authored anchor, and every individual component of the four actual MM8 elemental
prison groups. The public current-package house audit is also run with a freshly verified data cache.
The final focused Windows run passes 43 cases / 3,795 assertions, including that opt-in package audit.

Native Windows Release AMD/OpenGL tours replay `atlas_height_layers.yml` on the Temple of the Moon (MM7) and
Abandoned Temple (MM8), writing sixteen fresh captures. Inspected views retain the known-only geometry, party
arrow and Guide/Discovery separation. The MM8 fixture has recorded positions without rendered exploration;
its blank discovered plan is expected and does not prove general dungeon exploration coverage.

A separate actual mesh interaction with Water prison bmodel 24/face 4/event 504 passes all 26 scenario steps.
Load its `after-prison.oysav` as `saves/before.oysav`, then run `atlas_mm8_prison_groups.yml`: Discovery shows the
Water entrance, Guide shows all four prisons, and Discovery again shows only Water. The 27-step
`tests/scenarios/atlas/mm8_prison_groups_known_component_verify.yml` checks the resulting quicksave, untouched
unlock/other-component flags, gold/food and another save/load roundtrip. Private captures, logs and retained
saves are under `atlas-known-components-20261026` in the Windows development root. These targeted checks leave
the broader content/floor audit and Android/controller acceptance open; they do not complete the Atlas objective.

### Atlas list units and shared touch scrolling, 2026-10-26

The region/record and Sites lists passed pixel-scaled content heights to a logical-unit scroll helper, then
subtracted the logical offset as pixels. A wheel step therefore moved a fraction of a row at larger UI scales,
and the bottom of a long list left a large empty area. Both lists now use logical heights and offsets, matching
the shared reference/settings/save-list contract. One wheel step moves one row; the last row stays at the bottom.

The shared menu viewport also accepts recognized touch drags on the list body. Capture remains with the starting
pane when the finger leaves it, includes the final release displacement, and reverses immediately at a clamp.
Scrollbar handling remains available. A recognized touch drag cancels pointer-button activation so releasing a
short swipe over the same row cannot open a region or select a site. Tap, mouse and keyboard paths retain their
existing input rules. The gesture state belongs to the screen and contains no world/save data.

Six focused Windows cases / 100 assertions pass, including three pure gesture cases for scale, pane ownership,
between-frame drags, clamping, new gestures and unrelated pointer input. Windows Release builds pass. A native
before/after comparison reproduces and removes the empty-list overscroll; both MM7 and MM8 wheel tours pass.
The recognized-touch tour confirms one-row scrolling and that a short drag does not open a region or select a
site. Twenty fresh after-captures were generated; inspected reading/list outcomes match those checks.

Reproduction tours are `tools/menu_tours/atlas_list_scroll_mm7.yml`, `atlas_list_scroll_mm8.yml` and
`atlas_touch_lists_mm7.yml`. The latter uses MenuInputTour's `touch_drag_start`, `touch_drag_move` and
`touch_drag_end` to replay shared UI frames after gesture recognition. It does not prove Android OS touch
delivery. Private logs/captures are under `atlas-touch-scroll-20261026` in the Windows development root.
One baseline harness attempt omitted its one-shot tour directive and timed out; it is excluded. The corrected
harness supplies that directive on every launch and checks fresh capture timestamps.

These fixes target Android `0.2.1-atlas` / `104`, with one default version in `android/gradle.properties`.
The phone currently runs `0.2.0-atlas` / `103`; its first-launch proof is recorded in `ATLAS_ANDROID_QA.md`.
Physical list-gesture checks on the updated APK and the wider Atlas acceptance matrix remain open.

### Map drags starting on pins, 2026-10-26

A native Windows reproduction confirmed that a recognized touch drag starting on a site pin did not pan the
map, although dragging bare terrain worked. Map panning now captures the recognized gesture's original position,
including starts on pins, and consumes its final release position. The existing shared button handling cancels
the corresponding pin tap. Gestures beginning in the list, zoom buttons or dungeon height controls cannot capture
the map. Ordinary mouse pin clicks retain their existing behavior.

Windows Release builds pass. The MM7 Barrow Downs and MM8 Dagger Wound tours each produce five fresh captures.
They exercise a two-axis 130-pixel drag (including 30 pixels delivered only on release), a reverse drag and
gestures starting outside the map or on zoom controls. The terrain follows the expected offsets; the last two
gestures leave the map viewport pixel-identical. The initial MM7 baseline separately reproduces the stationary
pin-start drag. Captures and logs are retained privately under `atlas-map-pan-20261026` in the Windows development
root. Reproduction tours are `tools/menu_tours/atlas_touch_map_mm7.yml` and `atlas_touch_map_mm8.yml`.

These tours replay recognized shared-input frames, not Android OS events. This later source fix is not included
in the already-built `0.2.1-atlas` APK. Physical gesture checks and the wider MM7/MM8 coverage matrix remain open.

### Incremental Guide region indexing, 2026-10-26

After the user reported a long phone delay, a Windows diagnosis reproduced 3.2-second MM7 and 3.1-second MM8
Guide entry frames. Each toggle discarded the index, and the same render callback synchronously loaded all
13/14 outdoor maps, including scenes, baked lighting and event metadata. Repeat entry incurred the same delay.

The user then authorized a small optimization. The existing indexer now processes one map per frame and gives
the selected location priority. A remaining-map count identifies incomplete results. Work continues when the
user opens the Guidebook or a site reading; Guidebook references refresh once the regional index finishes.
The separate explicit full-source scan retains its existing behavior. Dungeon geometry remains on demand.
This does not introduce threads or a second world database, and it does not retain the index across toggles.

Windows Release builds pass. Equivalent MM7/MM8 tours measured entry frames around 0.35/0.36 seconds; the
worst region-indexing frames were 0.36/0.38 seconds. All 13/14 outdoor maps were still indexed on each Guide
entry. Additional native tours enter the Guidebook while loading, reach the completed index and return to
Discovery. The Discovery map viewports are pixel-identical to their pre-Guide captures in both worlds.
These are Windows measurements, not POCO performance claims. Private logs/captures are retained under
`atlas-guide-verification-20261026`; reproducible Guidebook tours are `atlas_guide_incremental_mm7.yml` and
`atlas_guide_incremental_mm8.yml` in `tools/menu_tours/`.

This change and the pin-start touch-pan fix target Android `0.2.2-atlas` / `105`. The installed phone version
remains `0.2.0-atlas` until an updated package is installed and verified. The full Atlas objective remains open.

### Map Book entry and smaller party arrow, 2026-10-27

The user confirmed Discovery fog and arrow direction on POCO, then requested a smaller arrow and an Atlas
button in the sidebar reached by clicking the minimap. The shared Map Book now offers that button only when
the Atlas module is enabled. The main/pause menu entry is removed. Closing the Atlas resumes the existing Map
Book, preserving its view and zoom; holding Escape does not also close that returning Map Book. Atlas party
arrows are 14 logical pixels wide instead of 20 in both regional and dungeon views. Other map arrows retain their size.

Windows Release builds pass. Native MM7/MM8 tours click the actual minimap, enter the Atlas and return to the
same Map Book. The Map Book viewports remain pixel-identical, including a separate non-default-zoom check.
A disabled-module tour confirms the button is absent and its hidden hit area is inert. An updated existing
Guidebook tour passes through the new route. Twenty-five fresh captures were retained; inspected captures show
the sidebar, smaller arrow, returning map and unchanged pause menu. The initial attempt used a stale journal
layout in the local runtime overlay; it was rejected during visual review, the Windows packaging wrapper was
updated, and both world tours were rerun against source-matching YAML.

All 95 existing Atlas tour documents were structurally checked: only 90 entry and 64 return sequences were
updated for the new route. This is not a claim that all 95 tours were executed again. Reproduction tours for the
new pointer route are `tools/menu_tours/atlas_journal_entry_mm7.yml` and `atlas_journal_entry_mm8.yml`.
Private logs, capture hashes and the build receipt are under `atlas-journal-entry-20261027` in the Windows
development root. These source changes target `0.2.3-atlas` / `106`.

The 0.2.2 update succeeded through USB without uninstalling or resetting data. The user confirmed Guide Mode is
noticeably faster on POCO. Current device evidence and remaining physical gates are in `ATLAS_ANDROID_QA.md`;
the wider complete-MM7/MM8 Atlas objective remains open.

The subsequent local 0.2.3 ARM64 Release was verified and installed on POCO through an in-place update. Its
installed APK hash matches the checked local file, and the official installation and both applications' campaign
save hashes match the fresh pre-update records. Startup confirms OpenGL ES 3.0 and both modules. Physical
acceptance of the new Map Book button and smaller arrow remains open. Local source, SDK, native and runtime
caches are retained outside Git; a no-change Android rebuild took 9.4 seconds. See `android/README.md` for the
reusable local build command and `ATLAS_ANDROID_QA.md` for measured build/install and remaining device gates.

### Map Book note persistence and shared touch-list acceptance, 2026-10-27

Four Windows native AMD/OpenGL tours now cover the new Map Book entry with note editing and list gestures in
both worlds. Notes are created, renamed, filtered by height, reloaded, reassigned to All/Party height, deleted
and reloaded again. The dungeon samples are Temple of the Moon and Abandoned Temple. The MM8 preparation uses
its actual chest source save: the tour closes Simon's authored dialogue and places the held item through the
party portrait before opening the Map Book, then closes the dialogue after reload. It does not recruit Simon.
Atlas entry assertions reject attempts that are still on gameplay/dialogue screens.

The accepted runs contain 276 input steps and 47 fresh captures. Inspected states show the expected note text,
hidden notes on a different height, retained editing/elevation and deletion after reload. In both list tours,
the map viewport stays pixel-identical after dragging an initially pressed region row; the detail viewport
stays identical after dragging an initially pressed site row. End-of-list captures retain the final rows.
These are recognized shared touch frames, not Android OS gesture injection or gamepad input.

Fresh faithful authored interactions prepare both note fixtures. Two post-UI verifiers then check the original
save, final quicksave and another save/load: deleted user-note ownership/elevation is cleared and the original
item/chest discovery remains. MM8 additionally retains item 630 in inventory and leaves Simon's roster QBit 403
clear. The preparation and verification runs pass 67 scenario steps in total. They certify these fields, not
every byte of the save or every campaign interaction. Earlier MM8 attempts using a pose-only fixture and an
unhandled dialogue/held item were rejected, then replaced with the documented fresh interaction fixture.

Public tours and verification scenarios are documented in `tests/fixtures/atlas/README.md`; private logs,
accepted capture hashes, exact source hashes and save fixtures are in `atlas-core-acceptance-20261027` under the
Windows development root. No gameplay C++ or runtime assets changed in this checkpoint, so the installed POCO
0.2.3 package remains current. Wider floor/content coverage and physical Android/controller gates remain open.

### Possible scripted monster sources, 2026-10-27

World Database now indexes local scripted monster possibilities from callback-owned metadata. Four MM7 and ten
MM8 generated catalogs come from reachable legacy `SummonMonsters` instructions, including supported separate
timer/on-load continuations. Invalid slots and empty calls have no gameplay spawn and are not exported. Replaced
or removed callbacks lose their old declarations. The active catalog resolves species and names through current
merged map/monster tables, using the same tier clamp and unique-name lookup as gameplay.

The actual Tularean artifact battle replaces its legacy callback and has different counts. Its six spawn rows
now have one owner in `MM7.TulareanArtifactBattleSpawns`, used by both the unchanged gameplay sequence and a late
metadata declaration. The executed callback regression compares all eight arguments of all six actual calls with
the loaded references, including the current three-soldier group rather than the legacy thirty. Quest gates,
use flags, facet changes and messages are retained. Party-relative Deyja/Malwick ambushes do not receive invented
fixed coordinates; that part of the source audit remains open.

Guide pins and Bestiary links describe possible groups, not live populations, guaranteed encounters or branch
eligibility. Discovery uses actual actor observations; explored terrain or a known species does not disclose
these other sources. No discovery keys, binary save format or new saved state are introduced. The API and
regeneration command are documented in `MODDING.md`.

Visual review found that the shared minimap projection clamped an authored MM8 center outside the chart onto its
north edge. Atlas projection now requests unclamped coordinates and omits markers outside the image, retaining
the original source coordinates/list record. Ordinary minimap callers keep their previous clamped behavior.
No gameplay spawn coordinate is rewritten. Literal/late metadata validation keeps these catalogs independent
of shadow-caster dependencies while rejecting executable expressions; baked-lighting validation remains enabled.

Validation: Windows Release builds; 18 focused native cases / 1,759 assertions, covering the active MM7/MM8 map
program catalog, replacement ownership, schema failures, visibility, Bestiary links, projection and real Tularean
execution; 25 lighting producer tests. The active mounted catalog has 17 MM7 and 49 MM8 spawn descriptions.
Four AMD/OpenGL tours execute 90 input steps and retain 15 fresh captures on Harmondale, Dagger Wound, Abandoned
Temple and the shared Tularean battle view. Inspected frames show the sources in Guide and their absence in
Discovery. The pre/post-Guide Discovery map and list viewports are pixel-identical in all three roundtrip tours.
Two faithful post-UI verifiers pass 34 steps, preserving the checked campaign/use flags, MM8 chest discovery and
stored item through another save/load. This verifies those fields, not every field of every save.

Public tours are `tools/menu_tours/atlas_scripted_spawns*.yml`; private logs, hashes and fixtures are under
`atlas-scripted-spawns-20261027` in the Windows development root. This feature has not yet been packaged into
the POCO 0.2.3 APK. Remaining content, relative-position sources, Android/controller and other completion gates
remain in `ATLAS_COVERAGE.md`; this checkpoint does not complete the full Atlas goal or add an MM6 Atlas.

### Android 0.2.4 package checkpoint, 2026-10-27

The local ARM64 Release now packages the preceding scripted-source and projection changes as 0.2.4 / 107.
Package checks verify 51,586 runtime entries, exact bytes for all 18 source data updates, no unexpected seed
changes/additions, the persistent Extended signing certificate, and 16 KiB ZIP/ELF alignment. The changed-source
Gradle invocation took 879.0 seconds; full asset staging/repackaging remains expensive despite persistent tools
and native objects. No downloads or new CI clone were needed. `ATLAS_ANDROID_QA.md` records hashes and evidence.

ADB listed no phone after verification. This is a prepared update, not device acceptance or full Atlas completion;
POCO remains on the last verified 0.2.3 until an actual update. No MM6 Atlas work was performed.

### Packaged dungeon layer/discovery audit, 2026-10-27

The opt-in unit audit reads every active MM7/MM8 BLV through the regression package filesystem rather than an
exported map sample. All 110 dungeons load: 63 MM7 and 47 MM8, with no authored blank floor plans. Synthetic
unknown knowledge hides geometry; reading Guide leaves it unknown. Partial outline knowledge produces 277 MM7
and 216 MM8 height slices. Each keeps the same center/scale and available-band set; combining the slices exactly
reconstructs the all-heights raster without changing reveal bits or face attributes.

The focused Windows Release suite passes seven cases / 4,334 assertions, including existing negative-height,
note-height, hidden-face and remembered-position cases. This broadens geometry/projection coverage without a
runtime code change. It does not establish physical input, full campaign exploration, or semantic separation of
overlapping rooms in one height band. Those limits remain in `ATLAS_COVERAGE.md`. The package test and its opt-in
invocation are documented in `tests/fixtures/atlas/README.md`; MM6 is excluded. No new APK is needed for test/docs
changes: the previously verified 0.2.4 package remains the prepared device update.

### Search submission keyboard fix, 2026-10-27

Enter previously cleared text-editing state in the SDL event handler, then the same frame activated the focused
search button and opened the field again. Both the map and Guidebook searches now commit after field activation
through one shared field-input path. Enter still opens a focused inactive field; Escape still cancels editing
before leaving the Atlas. No discovery state, asset data or save format changes.

An isolated native MM7 tour reproduced the fault: after Enter, `expect: no_text_input` failed against the actual
SDL window session. After the fix, MM7 and MM8 each pass the 35-step `atlas_search_submit.yml` tour with five fresh
captures. Inspected captures show a committed map query, Guidebook query and return to the ordinary Map Book.
Two faithful ten-step post-UI verifiers preserve the checked map and campaign flag through another save/load;
they do not certify the whole save or every discovery. The public fixtures and session expectations are
documented in `tests/fixtures/atlas/README.md` and `tools/RUN_GAME.md`. Private captures/logs are under
`atlas-search-submit-20261027` in the Windows development root.

The controller source audit confirms native SDL gamepad input still needs implementation; these are keyboard
checks. The shared UI fix is not yet in the prepared Android 0.2.4 package. Device/controller checks, stacked-floor
semantics and the remaining coverage matrix remain open; this checkpoint does not complete the Atlas goal.

### Native controller UI and Map Book sidebar, 2026-10-27

Shared SDL gamepad input now opens the existing Map Book action with North/Y. In menus, the D-pad or shoulders
move focus, South/A selects and East/B backs out; the left stick moves an internal cursor and the right stick
scrolls its target. Device changes, focus loss and UI transitions cancel held interactions and require neutral
controls before continuing. Input stays inside this application; no operating-system pointer or keyboard
injection is used. Existing mouse, keyboard and touch paths remain available. This is UI navigation, not a
complete movement/combat controller profile.

The user tested a physical controller in MM7 and confirmed Map Book opening and Atlas controls. Their requested
follow-up adds visible D-pad focus to the ordinary Map Book sidebar before entering Atlas. South activates its
existing action; Tab/Shift+Tab and Return/Space use the same path. An Atlas-disabled profile omits that focus
target, while mouse/touch takeover clears focus. The requested sidebar behavior is in the current native build;
its separate physical follow-up is pending.

The focused Release suite passes 13 cases / 198 assertions, covering SDL virtual devices, focus navigation,
short taps, neutral barriers, pointer bounds, mouse takeover, unplug cancellation and existing keyboard/touch
regressions. Five native MM7/MM8 tours pass 166 UI steps with 29 fresh captures: controller entry/Guidebook/back,
keyboard search submission and an Atlas-disabled sidebar. Selected captures show the actual highlighted Atlas
or Close button and the returned UI. Five post-UI verifiers pass 50 steps through another save/load, checking
the specified map/campaign flag rather than certifying every saved field. No save format or Atlas data changes.

The public controller tour is `tools/menu_tours/atlas_gamepad.yml`; reproduction notes are in
`tests/fixtures/atlas/README.md` and `tools/RUN_GAME.md`. Private fixtures, logs and captures are retained under
`atlas-controller-20261027` in the Windows development root. Controller and search fixes target Android
0.2.5 / 108; a source version bump is not an APK build or device acceptance. Remaining Android checks and the
agreed coverage matrix stay open. No MM6 Atlas work was performed.

### Android 0.2.5 package and regional artwork checkpoint, 2026-10-27

Source `2f8371462fffd4552a9e5340e6c5a5530b80bd4d` now has a verified local ARM64 Release, 0.2.5 / 108,
including controller UI, ordinary Map Book sidebar focus and the search submission fix. Gradle passed in
527.8 seconds using persistent tools/native objects and up-to-date staged runtime assets; no downloads or
asset refresh were needed. The build ran separately from the open desktop test without rewriting its executable
or runtime packages. The physical session subsequently closed normally.

Package gates confirm 51,586 runtime entries, all 18 declared source-data updates byte-for-byte, the persistent
Extended certificate, non-debuggable flags and 16 KiB ZIP/ELF alignment. A separate read-only runtime audit
successfully decodes artwork for all 26 outdoor Atlas regions (13 per world) and verifies their ODM presence.
It checks availability, not full region calibration or device rendering. APK hashes, reproduction/evidence paths
and limitations are recorded in `ATLAS_ANDROID_QA.md`; artifacts remain outside Git.

ADB sees no device, so the package is prepared but has not been installed on POCO. The last verified installed
version is 0.2.3 / 106. Physical sidebar feedback, remaining phone checks and the agreed map/content audit remain
open; a successful APK build does not complete the Atlas objective.

### POCO 0.2.5 in-place update, 2026-10-27

After the user reconnected USB, the verified 0.2.5 / 108 APK was cached on the phone, hash-checked and installed
in place. The transfer took 97.61 seconds and Package Manager installation 15.38 seconds. Installed version,
non-debuggable flags and the installed base APK hash match the accepted package. The official package path,
three official save hashes and two Extended save hashes match fresh pre-update snapshots. No uninstall,
data reset or restore occurred.

Extended starts with its build marker, OpenGL ES 3.0 and both Atlas/karol.test modules in the new process log.
The actual phone install/startup gate now covers 0.2.5; touch/visual behavior, Guide privacy and the remaining
MM7/MM8 device checks require separate acceptance. A focused user test is pending. Device receipts are retained
with the APK outside Git under `atlas-android-025-20261027`; details are in `ATLAS_ANDROID_QA.md`.

### Stacked-height diagnostics and single keyboard-tap delivery, 2026-10-27

A read-only audit of all 110 active MM7/MM8 BLVs finds 17 candidate maps with large upward, nonhidden/collidable
horizontal surfaces overlapping in XY at least 128 units apart inside one existing 512-unit band. Candidates
can include furniture or platforms; this is not proof of 17 distinct traversable-floor defects. Two concrete
outline witnesses are Castle Gloaming (`d03.blv`, MM7), floor faces 393/622 at Z 0/-240 with 4/9 adjoining
outlines, and Plane of Earth (`eleme.blv`, MM8), faces 1579/3084 at Z -384/-768 with 9/7 adjoining outlines.
The outlines retain those elevations and both witnesses share a band. Fixed bands are therefore insufficient
to distinguish these authored heights. No new floor partitioning or gameplay data changes were made here;
the precision limitation remains open rather than being certified by the earlier raster-recomposition audit.

Two five-step private scenarios prepare geometry-derived diagnostic saves at the paired positions. Four native
AMD/OpenGL tours record their Discovery/Guide party-height/Discovery views (68 steps, 12 fresh captures).
Inspected frames show the current band 0 / -1 selection; Castle Gloaming's paired Guide captures are identical.
All four before/after Discovery map and site-list viewports are pixel-identical in RGB. These isolated poses
are setup shortcuts, not campaign traversal or physical device input; they do not verify semantic floor routing.

The first native attempt exposed a separate test-tool bug: `MenuInputTour` injected a gameplay key's press count
immediately and queued its SDL event, delivering two presses to the journal. Gameplay now counts that tap only
through the queued event; active menu screens retain their direct frame count, and held keys/quick saves remain
available. The previous run failed Atlas entry after five Tab taps; the corrected native route reaches it once.

The new `atlas_map_book_keyboard.yml` tests first focus, forward/reverse focus, accept, return and quicksave in
both worlds. Together with the existing search-submission tours, four regression runs pass 118 steps and 20 fresh
captures. Four post-UI verifiers pass 40 steps, preserving their checked map/flag through another save/load.
The Windows incremental build took seven seconds. This helper fix does not change physical keyboard/gamepad
input or require replacing POCO's verified 0.2.5 for normal play. It is not yet in an Android package.

Private geometry inputs, audit/witness JSON, fixtures, logs, captures and comparisons are under
`atlas-floor-overlap-20261027` in the Windows development root. No user campaigns or official phone data are
test-write targets. The broader floor precision and remaining device/content acceptance remain open.

### Adaptive dungeon height slices, 2026-10-28

The confirmed same-band witnesses now separate in the native Atlas. A 512-unit band is divided into four
128-unit slices when known, collidable horizontal floor faces overlap in XY at heights at least 128 units apart.
The test uses the runtime's indoor face geometry and convex separating axes, including crossing shapes whose
centres do not overlap. Touching edges and separate rooms do not trigger refinement. Discovery considers only
faces belonging to its already-visible outlines and honours live portal/untouchable flags; Guide uses authored
geometry. The layer catalog is local view state and cannot teach exploration. Bounds stay anchored to all known
outlines/positions so selecting a slice does not change projection.

`WorldFloorPlanLayer` identifies a coarse index and optional subdivision. Explicit coarse Core queries remain
valid; the screen uses the raster's resolved height mapping consistently for sites, service records, party arrow
and notes. Available refined ranges are labelled with suffixes such as `0.1`. Indoor surface pins use their
lower edge, so a tall door's centre does not move its venue to a higher slice. Outdoor centroid positions retain
their existing interpretation. This changes derived Atlas positions, not gameplay picking/collision or event IDs.
Notes continue to store their explicit world Z in the existing namespace; no save schema or module requirement
changes. Notes without elevation still appear at every height. The empty-list message now describes the selected
height without asking the user to reselect an already-open dungeon.

The broad Windows suite passed 45 cases / 7,508 assertions; its optional base-house-location audit was not
configured and is not claimed here. Four final focused cases pass 5,872 assertions, including negative heights,
hidden-floor privacy, live attribute changes, crossing/touching projections, indoor venue/trainer placement and
unchanged outdoor centroids. The packaged audit loads all 63 MM7 and 47 MM8 BLVs. Its partial-knowledge slices
recompose the full known raster exactly (460/347 slices), with no blank authored plans. Full Guide geometry
refines 76/55 bands across 50/30 maps; these include platforms and are not counts of named semantic storeys.

Ten native AMD/OpenGL tours pass 450 UI steps with 71 fresh captures, covering the two paired-height witnesses,
ordinary note editing/deletion and controller entry in both worlds, plus dedicated refined-note persistence.
Inspected images show Castle Gloaming's `0.1`/`0.3` and Plane of Earth's `-1.1`/`-1.4` views. All four witness
Discovery map/list viewports remain pixel-identical in RGB after Guide. Six post-UI verifiers pass 93 steps;
two fresh pose fixtures add six setup steps. The fine-note tests preserve active ownership and world Z -192/-704
through another save/load, alongside the checked map/gold/food. Those fields do not certify complete save equality.
Final builds also include copy/formatting cleanup; the fine-note tours exercise the final build.

Reproduction is in `tests/fixtures/atlas/README.md`, with public `atlas_refined_note.yml`, matching MM7/MM8 pose
fixtures and post-UI verifiers. Private source/log/capture receipts are under `atlas-height-refinement-20261028`
in the Windows development root. Pose shortcuts and isolated immortal QA settings are not campaign traversal or
physical device input. The adaptive code is not in the verified Android 0.2.5 package yet. Remaining phone,
sloped/mechanism-driven map and content gates stay open; no MM6 Atlas work was performed.

The subsequent persistent local Android build packages this code as Release `0.2.6-atlas` / `109` in 536.8
seconds. Package/source-data, stable-signing and native/ZIP alignment checks pass; the 18 runtime data updates
are unchanged from 0.2.5. ADB lists no device, so this is a prepared APK rather than a POCO installation or
physical-height acceptance. Artifact hashes and evidence are in `ATLAS_ANDROID_QA.md`.

### Shared service entrances and actual Inn discoveries, 2026-10-28

The base-house audit now compares World Database projections with actual callback/geometry bindings on all 137
available MM7/MM8 maps. It finds 673 physical house IDs and one service used in two maps: Adventurer's Inn 1607,
at Dagger Wound and Ravenshore. The old map equality check dropped the Dagger Wound entrance. Its new regression
first failed with one indexed location instead of two; both now refer to the same canonical house record/table.
The audit checks each usable house's projected map set, while retaining the existing primary-address checks and
excluding transition/movie cards from usable buildings. This is base-package coverage; conditional scene/Lua
overrides still need their broader audit.

Derived locations retain the existing interaction receipt keys, including other faces of a connected door.
Discovery requires knowledge of the source map and a real receipt for an additional address. Legacy global
house discoveries keep their primary table address; missing secondary observations are not reconstructed.
Service/NPC/trainer search and native map selection resolve the known/preferred physical address, so selecting
the Dagger Wound Inn stays on Dagger Wound and cannot expose the name of an unknown primary region. No records
or gameplay tables are duplicated, and there is no save-schema change.

Native inspection also found an unused legacy Inn row beside the newly recovered physical service. Once a map's
bindings are indexed, Guide's building list omits rows with no physical binding. Other reference registries keep
their existing rows. The physical Inn appears once in the list; separate physical door/sign components keep their
normal marker grouping. A real door-interaction fixture then exposed that the special Inn roster-overlay path
returned before the ordinary house-discovery write. Successful Inn handling, including an empty roster, now
records that same house-discovery key before returning.

The Windows World Database/Guide suite passes 112 cases / 13,051 assertions, with the optional base-house cache
actually configured. Four native AMD/OpenGL tours pass 118 UI steps and 24 fresh captures: fresh and actually
visited Dagger Wound Inn, plus MM7/MM8 controller UI regressions using process-local SDL virtual gamepads.
Two real-door fixtures pass 18 setup steps. Four post-UI verifiers pass 41 steps; the visited fixture retains
house 1607 and its exact surface receipt through another save/load, while the unvisited one stays undiscovered.
The checked map/gold/food also remain unchanged. Both Inn Discovery map/list crops are pixel-identical in RGB
before and after Guide, and the inspected selected Guide view retains the Dagger Wound label and artwork.

Use `mm8_shared_house_fixture.yml`, `atlas_shared_house.yml` and the matching privacy/visit verifiers in
`tests/fixtures/atlas/`; the fixture creates separate before/visited saves through the actual press-interact path.
Private receipts, source patch, logs and captures are under `atlas-shared-houses-20261028` in the Windows
development root. The fixture uses an isolated party and an explicit door pose; it is not campaign traversal
or a physical controller/phone acceptance test. These source changes are newer than the verified Android 0.2.6
APK. Remaining device/content gates and the six unbound well declarations remain open.

### All declared outdoor region views and persisted fog, 2026-10-28

Native Windows checks now cover every outdoor entry declared by the two Atlas presentations: thirteen MM7 and
thirteen MM8 regions. The campaign areas include Shoals and the MM8 elemental planes. The MM8 Tutorial entry
and indoor overview anchors are outside this outdoor inventory. The mounted MM7/MM8 packages contain no
`ui/maps/catalog.yml` presentation overrides, including case-insensitive filenames; their minimap and Atlas use
the same default world extent and UV orientation. Future custom presentation support remains a decision item.

Each isolated direct-map fixture captures Map Book, Discovery, Guide and returned Discovery, closes the UI and
quicksaves. A separate native boot loads that quicksave and captures Discovery again. All 26 map and list RGB
crops are identical to their pre-Guide view in both comparisons. Both thirteen-map artwork/contact sheets were
inspected for titles, artwork and party arrows, with individual Map Book/Guide samples also inspected. These
are rendered current-pose checks, not a complete per-pin geographic or campaign-traversal certification.

The run passes 572 native UI steps with 130 fresh captures, 104 post-UI map/gold/food assertions/load steps and
79 fixture steps. Shoals uses its required wetsuit rather than bypassing its lethal campaign rule. Barrow Downs
uses the authored Stone City exit after its generic centre spawn was found to slide against geometry; the old
fixture/logs are retained separately. All accepted fixture XY positions remain stable through the UI/save cycle.
The ordinary game executable is unchanged from the shared-house checkpoint; only diagnostic tools/tours change.

The existing save-inspection tool now emits discovery variables and outdoor exploration for the active and
cached snapshots. Reading all 52 before/after saves finds no additional cached map keys and only the active
map's global visit flag. Native activation legitimately changes the initially unrendered headless fixture:
local terrain becomes 149 fully/315 partially revealed cells and visible-creature observations are recorded.
Those fields are reported explicitly; before/after masks and full saves are not claimed equal. The separate
fresh-process pixel comparison verifies the persisted Discovery result against the view captured before Guide.

Public fixture/tours and reproduction instructions are in `tests/fixtures/atlas/README.md`. Private settings,
saves, logs, 130 captures, contact sheets, inspection YAML and comparison receipts are under
`atlas-region-rendering-20261028` in the Windows development root. These checks use AMD/OpenGL and engine-native
captures. They do not establish Android/physical-controller acceptance, moving/sloped dungeon coverage or the
remaining content matrix. No MM6 Atlas implementation or official phone-data writes were performed.

The subsequent local Android Release `0.2.7-atlas` / `110` packages the shared-house/Inn fixes together with
the adaptive height code. Package/source-data, persistent-signing and 16 KiB alignment checks pass; the
18 runtime data updates are unchanged from 0.2.6. No phone is detected by ADB/Windows PnP, so this is a prepared
APK, not an installed or physically accepted release. See `ATLAS_ANDROID_QA.md` for the archive and hash.

### Dungeon navigation parents, 2026-10-28

The active entrance audit found eleven MM7 dungeons with an outdoor entrance but no primary parent in the
World Database. The existing presentation metadata now supplies 25 missing primary navigation parents:
Markham's Manor, the Dragon's Lair, Tunnels to Eeofol, the Haunted Mansion, all fifteen Barrows, Zokarr's Tomb,
Wromthrax's Cave, Setag's Tower, the Hidden Tomb, the Dragon Caves and the Bandit Caves. Additional internal
Barrows and the Hidden Tomb are backed by the authored connection/return graph. The primary parent describes
navigation; all actual entrances and conditional branches remain in the connection graph. This uses the
existing schema/data path. `MapStats.areaId` also controls respawn, which is why navigation metadata belongs here.

The new regression loads full active geometry, scenes and event overlays for all 27 MM7/MM8 outdoor entries,
including the MM8 Tutorial entry. It validates 78 dungeons with one outdoor entrance region and one with more
than one, plus all 70 explicit parents across both presentations against the authored connection graph.
MM7 dungeons with a fixed primary navigation region now have parents. Celeste and the Pit retain their anchors; Arena,
the portable Strange Temple, Nighon Tunnels and the Small House retain their deliberate unparented status.
The latter maps support non-geographic or multiple-root travel, including the Temple's actual saved-return
override. No single parent is inferred from a random/conditional branch.

The focused hierarchy check passes 1,554 assertions; the broader World Database/Guide/interaction suite passes
127 cases / 46,013 assertions. Four native AMD/OpenGL cases pass 132 UI steps with 24 fresh captures, 40
post-UI state steps and one four-step fresh fixture. They cover region-list entry and dungeon-plan selection
for Zokarr's Tomb, Markham's Manor and the Hidden Tomb, plus an MM8 Abandoned Temple control. Their filtered
Discovery lists match after a fresh quicksave reload. Eight inspected saves keep the source map/party XY,
leave the browsed dungeon undiscovered and add no indoor snapshots or cached outdoor-map keys. The existing
save inspector now exposes cached indoor-map names for this check. It does not claim complete save equality.

Reproduce with `mm7_dungeon_parent_fixture.yml`, `atlas_dungeon_parent.yml` and
`atlas_dungeon_parent_reload.yml`; the fixture README describes the other map/title variants and post-UI
checks. Private source/log/capture/save receipts are under `atlas-dungeon-parents-20261028` in the Windows
development root. The ordinary game executable is unchanged; the runtime presentation data is updated.
These parent metadata changes are newer than the archived Android 0.2.7 APK and need a later package.

The subsequent verified ARM64 Release 0.2.8 / 111 includes this presentation data, matching source byte-for-byte.
It was installed in place over USB on POCO and launched with OpenGL ES 3.0 and both modules loaded. Installed
APK hashing, package flags and fresh before/after checks preserve the official installation and all five campaign
saves. Physical Atlas acceptance remains separate; the user has been asked to test MM8 touch, fog/arrow and
the Guide-to-Discovery roundtrip. Exact package, transfer and startup evidence is in `ATLAS_ANDROID_QA.md`.

### Requested item references, 2026-10-28

The journal relationship audit found seven explicitly requested item objectives missing from the shared
registry. The existing data table now links MM8's portal crystal quest to Power Stone 617 and Sandro's
request to Nightshade Brazier 604. MM7's two Perfect Bow quests both request 1344, the Archmage request uses
the MM7 Divine Intervention book 1289, Tarin requests Lantern of Light 1450 and Davrik requests Signet Ring
1408. The registry now has 145 quests / 602 relationships. No reward, promotion or event behavior changes.

IDs follow active gameplay: global event 21 grants stone 617, rather than identically named 618; the active
MM7 `Global_mm7_mmmerge.lua` promotion overlays 816/820 consume bow 1344 and award 1345, while 845 consumes
book 1289. The original quest requests and item table identify the other objectives. This uses the existing
disclosed-objective path, without adding coordinates or marking unseen item properties as identified.

The targeted shared-data test passes 114 assertions, checking all seven requests, same-name variant exclusion,
Guide isolation, accepted/history labels and identification-gated item links. The broader World Guide suite
passes 47 cases / 1,847 assertions. Two faithful headless scenarios pass 22 steps: actual MM7 Lantern/Signet
requests and MM8 portal-crystal acceptance, with the new objective discovery keys preserved after save/load.
The MM8 fixture first requires the real Portals of Stone topic before the request becomes available; its
initial fixture failure is retained. These dialogue checks address NPCs by ID and do not prove physical travel.

Reproduction scenarios are `mm7_requested_item_knowledge.yml` and `mm8_requested_item_knowledge.yml` under
`tests/scenarios/atlas/`. Private logs and source receipts are under `atlas-quest-items-20261028` in the Windows
development root. This source/runtime update is newer than the installed Android 0.2.8 APK; that archived APK
remains unchanged. The user separately confirmed MM8 fog and arrow on POCO 0.2.8, as recorded in Android QA.

### Full quest-link destination audit, 2026-10-28

The active-package audit now loads geometry and all event overlays for 137 MM7/MM8 maps before projecting
the complete 145-quest Guide. It inventories every emitted clickable item/location link, checks that its
target resolves and is usable in Guide, and verifies that browsing does not write quest/location discoveries.
This found three links to house entries which the fully indexed Atlas could not show.

Two are Thomas Grey's returns (557/559): School of Sorcery event 416 opens NPC 387 directly, rather than
entering house 1157. Their shared quest rows now name the actual School map, retaining their NPC contacts.
The third is Stone City's real throne room. Its MMMerge replacement of event 416 clears the original context
metadata. A small explicit `7d24_zz_site_context.lua` overlay restores house 216 as that door's target; the
quest still links to the real throne room. Generated scripts and the quest gate are unchanged. Runtime checks
execute the fully mounted event with and without QBit 647, confirming the locked message versus house 216
dialogue and preservation of the gate bit. Unknown Discovery still hides the restored location.

The link inventory also exposed Kastore's possible throne-room contact labelled as Emerald Island. Shared
house 221 had stale map ID 62; Castle Gloaming's actual event 5 enters that house after the council changes.
Its authoritative table address is now Castle Gloaming (map 77). No NPC movement, quest state or save ID changes.
The alternate house 219/221 branches still share a physical door; secondary-branch pin coverage remains part
of the existing conditional-location audit, rather than being claimed complete by this address correction.

The inventory records 579 clickable links (313 MM7 / 266 MM8), separately from the registry's 602 relationships.
Resolution checks do not certify every campaign stage, conditional clue, contact placement or physical journey.
Private original-failure, intermediate and final reports are under `atlas-quest-link-audit-20261028` in the
Windows development root. These source/runtime fixes are newer than installed Android 0.2.8.

The final shared World Database/Guide/interaction suite passes 129 cases / 49,618 assertions. It includes the
full active quest-link inventory and the throne-room permission checks with the final table/overlay data.

The localization investigation found no existing shared language selector to integrate: the loader explicitly
uses `engine/data_tables/english/`, and current native menu text lives in code/YAML. A new engine-wide language
system is recorded in `ATLAS_DECISIONS.md`, not introduced as an Atlas-only subsystem or an extra completion gate.

### Conditional house destinations, 2026-10-28

An entrance could previously expose only the first `EnterHouse` target in its metadata. This omitted the
replacement council in Castle Gloaming, the successor Arbiters in Harmondale, and usable venues behind
Balthazar's generic flooded-room dialogues. The shared metadata reader now accepts optional `houseIds`
alongside the existing primary `houseId`. The index retains distinct entrance/destination identities with
shared physical discovery keys; the existing map-pin clustering keeps alternatives on their real doorway.
Discovery still requires each actual venue visit, rather than inferring it from the other branch.

Explicit late metadata declares both council rooms, all three Arbiters and seventeen named/service venues in
Balthazar Lair. Flooded-room dialogue placeholders are not declared as the usable shops. The existing callbacks
continue choosing the live branch; the index never executes them to forecast availability. Castle Gloaming's
local declaration also supplies its two possible council contacts, so Kastore can be found on that selected
map without first browsing the Pit's relocation callback. House variant captions use only visible resident
names, avoiding three indistinguishable Arbiter rows without disclosing unknown residents in Discovery.

These are explicit overlays; generated callbacks remain unchanged. Literal context and NPC declarations use
their reserved catalog files and headers. The lighting validator now accepts strictly bounded literal house
alternatives and fountains alongside wells, rejecting executable Lua, computed values, extra fields and invalid
IDs. Catalog changes preserve tested shadow cards/counts/dependencies; ordinary gameplay dependencies remain
hashed. No bake, renderer rewrite, gameplay eligibility change or save-schema migration is introduced.

The shared World Database/Guide/interaction suite passes 131 cases / 49,946 assertions. Focused cases cover
independent house discovery, duplicate-ID normalization, resident-name/search privacy and 39 actual callback
branches: two council phases, three Arbiters and seventeen flooded/usable venue pairs. The lighting producer
suite passes 26 tests, and all six actual context/NPC catalogs checked in this change validate.

Final native AMD/OpenGL checks pass three tours / 62 UI steps / nine fresh captures, eleven setup steps and
nineteen post-UI save checks. They show Kastore's local possible contact and council-room pin, three named
Arbiter variants at one clustered doorway, and The Shaman's usable Balthazar venue. Discovery map/list RGB
crops match before and after Guide in all three cases. The resulting saves retain the checked map/gold/food
and leave all targeted venue-discovery flags unset. These are sampled privacy checks, not complete save
equality, physical travel, Android input or certification of every conditional venue.

Reproduce with the conditional-house fixtures and tour listed in the fixture README. The first harness run
lost its one-shot UI directive during fixture preparation; the runner now restores it immediately before UI.
Early captures also exposed indistinguishable Arbiter rows and the missing local Kastore reference; final
captions/declarations and fresh captures resolve those findings. The archived failures and final source,
unit, lighting, GPU, capture and save receipts are under `atlas-house-alternatives-20261028` in the Windows
development root. No global mouse/keyboard injection was used. These fixes require a later Android package
than installed 0.2.8, including the updated shared metadata reader.
