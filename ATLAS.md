# Native Interactive Atlas, first vertical slice

Reference: [hulululuh's MM7 interactive world-map guide](https://www.reddit.com/r/MightAndMagic/comments/1ww8pq0/i_made_an_interactive_world_map_guide_for_might/).
This is an original native implementation using OpenYAMM's renderer, menus and game data. The reference HTML,
JavaScript, SVG floor plans, translated descriptions and embedded artwork are not bundled. The downloaded guide
did not include a declared reuse license; a future import of its authored material requires an appropriate license.

## Working slice

- Interactive Antagarich overview and all 13 outdoor region images from the existing game packages.
- Region/dungeon selection, resident NPCs, buildings, mastery trainers, walking routes and transport schedules.
- Off-map dungeon plans projected from the engine's actual geometry. Discovery respects saved outline bits,
  seen faces and runtime invisible-face overrides; Guide renders the authored visible outlines.
- Case-insensitive search and typed filters. Trainer names and topics come from the game's NPC/topic tables.
- Default Discovery Mode; opt-in Guide Mode is a read-only view of the same index.
- Successful map loads record discoveries; actual opened house/NPC dialogue records encountered services/residents.
- Existing visited-map snapshots reveal locations from older saves. Older saves do not retroactively mark every
  resident as met. Opening the atlas, searching and switching modes never award discoveries.
- Zoom, drag, search input, native button focus and scroll controls. Entry in main menu and pause menu.

`game/world/WorldDatabase` indexes **references** to `MapStats`, `HouseTable`, `NpcDialogTable` and
`MergedTeacherTopicTable`. It owns neither copies of those records nor an alternative balancing/content database.
Map IDs reuse the canonical IDs already supplied by the engine. Temporary legacy house/NPC IDs use the existing
merged numeric IDs; richer package IDs can replace these as the upstream registries evolve.

`assets_dev/engine/world/atlas/mm7.yml` supplies presentation only: region anchors and area groupings. Names,
trainer topics, shop metadata, destinations and schedules stay in their authoritative tables. Anchors on the
illustrated continent artwork are approximate; they are not gameplay coordinates or precise points of interest.
Dungeon grouping uses the existing `areaId` plus the presentation's area definitions. Locations without declared
parent metadata remain searchable rather than being assigned a guessed parent.

Discoveries use `openyamm.discovery.*` integer keys in the existing session/runtime named-variable store, whose
serialization is already supported by saves. No save version bump or separate atlas save file is introduced.
Unknown services are filtered before search; routes require both endpoint maps to be known in Discovery Mode.

## Remaining guide coverage

This slice is not the complete external guide port. Precise building/POI coordinates, quest chains, chests, monsters, wells, obelisks, teleport links, Barrows connections, alchemy/spell
reference pages and trackers remain to be added through shared game/world data. Explored dungeon maps and player
notes continue to use the existing Map Book. Dungeon plans currently project all elevations together; floor
selection, party position and annotations are not yet shown in the Atlas. The index currently describes authored residents and teaching topics;
runtime NPC relocation/topic overrides and information learned from books or rumours require additional shared
discovery/data integration. The Android implementation uses shared native code but this slice has not yet been
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
`runtime/assets/assets.zip`, supplying the Extended Knight table and Atlas presentation from source. Other overlay
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
It also loads the actual canonical source tables and resolves MM7 Sword trainers. Seven cases / 96 assertions pass,
including MSB-first dungeon reveal bits, invisible geometry, invalid indices and extreme coordinates.

Windows Release compilation succeeded. Engine-owned captures verified the overview, Harmondale map, trainer filter
and `Grand Master Sword` search, plus White Cliff Cave geometry and zoom, using native OpenGL on AMD hardware.
The live Harmondale check passed gameplay -> paused Atlas -> pause menu -> resumed gameplay. A real quicksave
contains the discovery key in the existing save namespaces; a fresh process loaded that save, displayed
only discovered Harmondale in Discovery Mode and resumed gameplay. A live White Cliff Cave session displayed
only the explored entrance chamber in Discovery Mode and resumed the same dungeon. Missing/invalid presentation
packages are reported in the menu and log without aborting the game. Captures/logs live outside Git in the isolated
runtime and ignored build directory. This does not constitute a complete guide-content audit or Android test.
