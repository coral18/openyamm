# Atlas port to OpenYAMM 2.5

This branch ports the native MM7/MM8 Atlas to OpenYAMM **v2.5** (`8de973cc`). The previous 2.3 geometry checkpoint
remains on `atlas/port-v2.3`; the earlier complete Atlas remains on `extended/atlas-mm8`. Work uses an isolated
checkout, installation, campaigns and the official 2.5 runtime packs.

The Windows game compiles and the Atlas opens from the Map Book. This is a development port, not a published
Windows/Android release. Native GPU checks cover MM7, MM8 and a dungeon; controller routing is covered by virtual
SDL gamepad tests. Physical-controller and Android acceptance remain outstanding on this base.

## Integration

- `OPENYAMM_BUILD_ATLAS=ON` builds the optional native screen and shared world index. The existing
  `OPENYAMM_BUILD_ATLAS_CORE` option still supports the geometry-only slice.
- The in-game Map Book exposes Atlas in MM7/MM8. `[atlas] enabled=false` disables the entry. The main menu has no
  Atlas shortcut. This does not introduce a general mod loader or a stable plugin ABI.
- Definitions come from the active upstream tables, scripts and map loader, including scene/ruleset overlays.
  Geographic presentation is separate from gameplay definitions. Typed metadata retains the identity of the
  callback it describes; replacing a callback invalidates its old annotations.
- Discovery uses party observations, native outdoor exploration masks, indoor outlines, actual NPC encounters,
  quest information and the current upstream trainer directory/teacher ledger. Guide is an explicit separate mode.
- Map indexing remains incremental. The selected map is prioritized; Guide does not synchronously index every map
  before drawing its first frame. Dungeon geometry is cached for the selected map and uses current scene loading.
- Controller UI support retains the left-stick pointer, right-stick scrolling, confirm/back and Map Book shortcut.
  D-pad focus includes the Atlas button and preserves upstream's optional Progress tab and trainer finder.
- Personal notes use the upstream Party journal. Atlas only adds optional floor-height metadata. Native map-layout
  identities invalidate stale local projections; party journal entries remain under upstream ownership.
- Actor observations cover sprites and 3D models from the main view, with frustum and line-of-sight checks and a
  bounded ray budget. Hover/interactions, opened chests, displayed guild shelves and successful item delivery have
  separate observation hooks. Opening Guide does not record a visit or advance gameplay RNG.

The old fork's custom Gate Network is not installed in this port. Its annotations are not attached to unrelated
upstream callbacks. Similarly, a former Tularean spawn annotation depended on a helper removed by upstream; that
obsolete annotation is omitted rather than changing 2.5's battle logic. Declared transitions and actual arrivals
remain available independently of that old module. The reserved `pending` map row `zddb05.blv` has no geometry
in the official release; its Atlas presentation explicitly excludes it without editing gameplay tables.

## Saves

The upstream **version 184 core layout is unchanged**. Atlas writes a length-delimited trailer with independent
schema versions for identified-item history, generated NPC encounters, mercenary encounters and ground-loot origins.
Vanilla saves need no trailer. Invalid or unsupported Atlas payloads fail explicitly; they are not silently loaded
as a partly decoded campaign.

Isolated MM7/MM8 save exchanges with the official 2.5 executable confirm that it reads these saves and drops the
supplemental trailer on resave; the Atlas build loads those stock resaves. This is not a promise
of compatibility with future releases. The old Extended version-84 format diverged from upstream's version history;
**do not import those campaigns as ordinary upstream version-84 saves**. No old campaign or official user installation
was modified during this port.

## Runtime content and build

Use the official 2.5 Windows package and all packs from its embedded index. The isolated test installation verifies
all **252** packs; **219** unchanged packs were reused from the verified 2.3 cache. Multi-gigabyte payloads are not
committed to Git. `OPENYAMM_COOK_ON_BUILD=OFF` is appropriate when testing against these already cooked release packs.

```sh
cmake -S . -B build -DOPENYAMM_BUILD_ATLAS=ON -DOPENYAMM_BUILD_TESTS=ON -DOPENYAMM_COOK_ON_BUILD=OFF
cmake --build build --target openyamm openyamm_unit_tests openyamm_atlas_runtime_tests -j25
```

Atlas overlays mirror the supported upstream `mods/engine/` and `mods/worlds/<world>/` roots. They consist of the
Atlas presentation YAML, `quest_locations.txt`, the shared event-support additions, the journal layout and Atlas
metadata sidecars. Do not copy an entire old engine/world package over 2.5. A distributable overlay bundle and its
installation instructions are separate from the engine sources. Stage the owned files into a new directory with:

```sh
python3 tools/stage_atlas_overlay.py --output output/atlas-overlay
```

The output contains `mods/`, the Polish `languages/` supplement, a SHA-256 manifest and the port/localization documents. Copy its `mods/` contents into an isolated user-data
directory used by an **Atlas-enabled executable built from this branch**. These script/UI overlays do not turn a stock
executable into the Atlas build. Keep updater checks disabled for this development executable.

To regenerate only Atlas metadata without replacing gameplay handlers:

```sh
OPENYAMM_BUILD_DIR=build \
OPENYAMM_ASSET_ROOT=/path/to/isolated/assets \
OPENYAMM_METADATA_MODS_ROOT=/path/to/metadata-inputs \
    tools/regenerate_lua_events.sh --atlas-metadata
```

The metadata input root contains `worlds/mm7/_legacy/events` and `worlds/mm8/_legacy/events`. GrayFace reference
folders named in the exporter configs must be present. Those legacy/reference inputs are not runtime mod contents.
The exporter accepts installed release packs and applies the same namespaced file overrides as the game.

## Verification checkpoint

- Native Windows Release: game, standard unit target, exporter and the focused runtime test executable compile.
- Atlas geometry, Guide/discovery privacy, notes, layout reset and save-extension checks: **77 cases / 2,287
  assertions passed**. The other unit cases were not claimed as passing.
- Real 2.5 installed-pack audit: **76 MM7 + 62 MM8 map programs** load with Atlas overlays. Callback-owned site
  references and reward metadata are indexed without resurrecting old gameplay patches. All **137 installed map
  geometries** validate their Atlas site bindings; the one explicitly reserved row remains excluded (**622 assertions**).
- Input/controller checks: **25 cases / 278 assertions passed**, including virtual-device isolation, focus, pointer
  cancellation, Map Book routing and preservation of mobile input behavior.
- Shared activation test: **25 assertions passed**, covering successful, rejected, hidden and map-changing actions.
- Both the stock 2.5 executable and the Atlas executable passed isolated MM7/MM8 new-game → save → load scenarios.
- Native GPU tours cover Map Book → Atlas via keyboard focus, Discovery/Guide, item art/details, alchemy, quests and
  return to gameplay in MM8 and Castle Harmondale in MM7. Regional fog/position arrows and dungeon outlines render
  correctly. Package-qualified map-texture lookup and an obsolete Ravenshore site grouping were corrected.
- Valid GPU captures were obtained on a separate, non-input Windows desktop (AMD, OpenGL 3.3), without displaying
  the game over the user's work. Earlier transparent minimized captures were excluded.
- Disabling `[atlas] enabled` hides the native Map Book entry. Android and physical-controller tests have not
  been run on this port.
- The tracked metadata-only regeneration workflow reproduced all **427 generated sidecars** byte for byte.

This port does not add MM6/MM9 Atlas catalogs, a launcher, new gameplay rules, or a public modding contract.

## English and Polish

The localization branch adds the shared display hooks and Polish Atlas catalog. See
[ATLAS_LOCALIZATION.md](ATLAS_LOCALIZATION.md) for coverage, installation and translation inventory checks.

## Credits

Maintained by Karol, on [OpenYAMM by pjasicek and contributors](https://github.com/pjasicek/openyamm).
Inspired by [u/hulululuh](https://www.reddit.com/user/hulululuh/)'s
[MM7 guide](https://www.reddit.com/r/MightAndMagic/comments/1ww8pq0/i_made_an_interactive_world_map_guide_for_might/)
and [MM8 guide](https://www.reddit.com/r/MightAndMagic/comments/1x7x7ok/simon_found_a_map_of_jadame_i_made_it_clickable/).
Their HTML, JavaScript, guide text and artwork are not bundled.
