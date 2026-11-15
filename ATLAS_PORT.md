# Atlas port to OpenYAMM 2.3

This is the first compatibility investigation and a small, tested port of the Atlas's geometry and exploration
logic. It is **not a playable Atlas release for OpenYAMM 2.3**. The complete working Atlas remains on
[`extended/atlas-mm8`](https://github.com/coral18/openyamm/tree/extended/atlas-mm8).

## Source and official runtime

- Target: [OpenYAMM v2.3](https://github.com/pjasicek/openyamm/releases/tag/v2.3), released November 13, 2026.
- Source commit: `f4a6b8032759a218505532b2600ac56b6d80c7af`. Upstream `main` points to the same commit at this check.
- Atlas source checkpoint: `ad91587f` (based on upstream `c1bc5434`, October 5).
- Research and isolated trials: November 15, 2026, Europe/Warsaw.
- Official Windows ZIP: 24,681,738 bytes; SHA-256 checked against the release checksum.
- Matching desktop content: all 245 packs, 8.63 GiB, checked against the size and SHA-256 in the release's embedded
  index. The official channel still advertises 2.3. Downloads are cached locally and can resume.

The runtime and content live outside Git in a separate Windows development directory. No old content packages,
user campaigns or Android installation were replaced. The source checkout uses a separate worktree and branch,
`atlas/port-v2.3`.

## What happens with the existing Atlas manifest

The official release does not load it. Its log explicitly reports that `mods/interactive-atlas` and
`mods/profile.yaml` are ignored: that folder accepts only `engine` and `worlds` file overrides.

Fresh MM7 and MM8 campaigns both pass a five-step scenario: create a game, check its map, save, reload, and check
the map again. These use the official Windows executable and current packs, with a copy of the Atlas manifest in
that isolated user-data folder. They establish that stock gameplay works and the manifest is ignored, not that
Atlas functions have been installed.

A five-step native Map Book input tour also completes, including returning to gameplay. Its minimized-window PNG
is black and is **not accepted as visual evidence**. The official game reports OpenGL 3.3 on AMD. Visual review of
an Atlas port remains open.

## What the Atlas depends on

The native Atlas reads a shared World Database over gameplay tables, map geometry and event metadata. Discovery
filters that index using the party's saved exploration, visits and observations; Guide Mode reads authored records
without awarding knowledge. The interface opens from the Map Book through a C++ callback. Its mod manifest enables
code already compiled into the fork, so a file override alone cannot install it into another executable.

The original branch includes changes beyond the Atlas. Merging the whole fork would also import its loader,
class patches, gate work, controller changes and content corrections. Those need separate review rather than being
used as an automatic solution to compilation errors.

## First port slice

`OPENYAMM_BUILD_ATLAS_CORE` is off by default. Turning it on compiles:

- `game/world/WorldMapExploration.*`: the outdoor reveal-mask projection and fog raster;
- `game/world/WorldFloorPlan.*`: indoor outlines, known-height slices, overlapping floors and ramp clipping;
- eight existing Atlas regression cases, registered with `openyamm_unit_tests`.

The tests now use upstream's `gameplayMinimapDisplayYaw`, including mirrored map presentations, instead of the
fork's missing `gameplayMinimapArrowIndex` helper. The fog code reuses upstream's reveal-coordinate conversion.
The floor-plan implementation uses the target upstream's actual `IndoorGeometryUtils`, not an old geometry copy.

This slice adds no UI, observations, gameplay hooks or save fields. It therefore does not yet provide a working
Discovery screen, party arrow or Guidebook in the game.

### Verification

The isolated geometry suite passes **8 cases / 133 assertions on Linux and native Windows**. The tests cover empty
and partial reveal masks, invisible faces, negative and invalid heights, undiscovered floors, overlapping floors,
continuous ramps and party/map projection. This is logic coverage, not visual or campaign acceptance.

The full native Windows CMake project configures with `OPENYAMM_BUILD_ATLAS_CORE=ON` and Vulkan disabled. Its
standard `openyamm_unit_tests` target builds successfully, and the selected Atlas suite passes the same 8 cases /
133 assertions there. The other 2,919 cases were not run; this is not a full-suite result.

The standard Windows test target needed explicit `winhttp` and `bcrypt` link dependencies: it compiles the HTTP
transport directly and links FFmpeg's Windows random source. That build fix is kept separate from the port.

```sh
cmake -S . -B build -DOPENYAMM_BUILD_ATLAS_CORE=ON -DOPENYAMM_USE_VULKAN=OFF
cmake --build build --target openyamm_unit_tests -j25
./build/tests/openyamm_unit_tests --test-case='Atlas port:*'
```

## Confirmed integration gaps

| Area | Difference and required work |
| --- | --- |
| Module activation | Upstream mounts file overrides and rulesets. The old manifest resolver is absent. Keep the Atlas optional using the current configuration/UI mechanisms; importing the entire old loader is a separate decision. |
| Map Book and trainers | Upstream already has `TrainerDirectory` and the journal trainer finder, including discovered trainers and teacher ledgers. Reuse those records and current trainer visibility instead of maintaining a second directory. |
| Travel | Keep upstream's transport schedules and route behavior authoritative when rebuilding Atlas links. |
| Quest references | The old UI fails to compile against `JournalQuestTable::discoveryKey` and `JournalQuestLocation`. Current quest entries have owner and explicit QBit visibility rules, but not the fork's location metadata. Port the metadata separately while retaining current quest visibility. |
| Identified items | The old UI calls `Party::hasIdentifiedItem`, which upstream does not expose. Ever-owned items alone do not prove identification. Restore the observation contract without revealing properties of unknown items. |
| Decorations | The old UI calls the absent `MapAssetLoader::loadDecorationTable`. Use the current loading/cache ownership; do not replace the new loader with the old implementation. |
| Actor observations | Current actor previews distinguish `modelled` actors. Sprite-only visibility hooks cannot establish Discovery parity for 3D actors. |
| Notes | Upstream has party-owned `journalMapNotes`; map-local `runtimeMapNotes` are legacy input migrated on load. New Atlas notes must use the current owner. |
| Save format | Upstream writes version 179; Extended writes version 84. Their version histories diverged. An Extended 84 save must not be treated as an ordinary upstream 84 save. No old campaign import was attempted. |
| Map changes | Upstream saves map layout identities and resets changed map state. Atlas projections and cached observations must respect that reset instead of restoring stale coordinates. |
| Localization | New visible UI text must use upstream localization. Record identities and save keys remain language independent. |
| Updates and packs | Leave official packs intact. Keep Atlas data in supported overlays; a custom executable must use a separate update/distribution identity before release. |

A direct syntax probe of the original `AtlasScreen.cpp` against the target upstream confirms the quest,
identification and decoration API gaps above. That failed probe is diagnostic material outside tracked source,
not a compiled fallback or a finished port.

## Next integration sequence

1. Complete the shared index and metadata port against current tables and event programs. Keep trainer records,
   transport and quest visibility tied to upstream's existing owners.
2. Add the optional native entry and map screen, then verify position, heading, reveal masks and dungeon geometry
   in both MM7 and MM8. Keep new campaigns isolated until the save-state contract is settled.
3. Reconnect observations and notes through current persistence, including modeled actors and changed-map resets.
4. Port the Guidebook sections and typed navigation, then repeat the old privacy, save/load and link audits against
   the new content. Re-run controller and Android acceptance after the PC port works.

This investigation does not add MM6/MM9 Atlas catalogs, a launcher, a public plugin API or new gameplay features.
It does not claim that the original full Atlas suite passes on 2.3.

## Credits

Port maintained by Karol, on [OpenYAMM by pjasicek and contributors](https://github.com/pjasicek/openyamm).
The Atlas was inspired by [u/hulululuh](https://www.reddit.com/user/hulululuh/)'s
[MM7 guide](https://www.reddit.com/r/MightAndMagic/comments/1ww8pq0/i_made_an_interactive_world_map_guide_for_might/)
and [MM8 guide](https://www.reddit.com/r/MightAndMagic/comments/1x7x7ok/simon_found_a_map_of_jadame_i_made_it_clickable/).
Their HTML, JavaScript, guide text and artwork are not bundled.
