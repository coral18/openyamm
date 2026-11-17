# Atlas port to OpenYAMM 2.6

The current source ports the native MM6/MM7/MM8 Atlas to OpenYAMM **v2.6** (`2b09864b`). [Atlas 0.3.0](ATLAS_RELEASE.md) is a Windows x64 preview. The previous 2.5 source remains on `atlas/port-v2.5`; the earlier implementation and Mod Loader history remain on `extended/atlas-mm8`.

The game, tests and runtime checks use isolated installations and copies of QA saves. Original installations and campaigns are not modified.

## Integration

- `OPENYAMM_BUILD_ATLAS=ON` builds the native screen and shared world index. `OPENYAMM_BUILD_ATLAS_CORE` still supports the geometry-only slice.
- The main-menu **Interactive Atlas: On / Off** button persists `[atlas] enabled`. The Atlas opens from the Map Book in MM6/MM7/MM8. Turning it off hides that entry and keeps discoveries and notes.
- The screen uses upstream menus, active tables, scripts, scene/ruleset overlays and map geometry. Typed metadata retains callback ownership; replaced callbacks do not inherit stale annotations.
- Discovery uses party observations, outdoor exploration masks, indoor outlines, NPC encounters, disclosed quest information and the upstream teacher directory/ledger. Guide is a separate view and does not grant discoveries or advance gameplay RNG.
- Guide indexing is incremental and prioritizes the selected map. Dungeon geometry is cached for that map using the existing loader.
- Controller routing retains pointer/scrolling, confirm/back, Map Book shortcuts and D-pad focus. Upstream's optional Progress tab and trainer finder remain available. Physical-controller acceptance on this base is pending.
- Personal notes use the upstream Party journal, with optional floor-height metadata. Native layout identities invalidate stale projections while journal ownership remains upstream.
- Actor observations cover sprites and 3D models with frustum/line-of-sight checks and a bounded ray budget. Chests, guild shelves, item delivery and interactions have their own observation hooks.

This port uses OpenYAMM 2.6's existing content mounting and its support for legacy overlay roots. It does not carry the old fork's Mod Loader API or promise a stable plugin ABI. The old custom Gate Network is not installed, and obsolete spawn annotations do not modify upstream battle logic. Reserved maps without installed geometry remain excluded from player navigation.

## Saves

The **upstream version-191 core layout is unchanged**. Atlas attaches a length-delimited trailer with separately versioned identified-item history, generated NPC encounters, mercenary encounters and ground-loot origins. Vanilla saves need no trailer. Invalid or unsupported Atlas payloads fail explicitly.

A test save migrated from 2.5 version 184 to 2.6 version 191 while retaining its identified-item history. The 2.6 game also passed save/load through New Sorpigal, Free Haven, the sewers, Control Center and Tomb of VARN.

Save exchange with the official 2.6 executable passed in both directions. Its resave drops the supplemental trailer: the probe had 23 recorded identified items before the exchange, zero in the stock resave, and the Atlas reconstructed 21 from inventory. This does not preserve the whole extension or promise compatibility with future releases. Back up campaigns before switching builds.

The old Extended version-84 format diverged from upstream's version history. Do not import those campaigns as ordinary upstream version-84 saves.

## Runtime content and build

Use the official **2.6 desktop content index** and matching packs. The validation installation used 252 base packs and five Polish language packs; 247 files were reused from cache, with ten changed/missing packs downloaded and verified. Multi-gigabyte runtime content is not committed to Git.

```sh
cmake -S . -B build -DOPENYAMM_BUILD_ATLAS=ON -DOPENYAMM_BUILD_TESTS=ON -DOPENYAMM_COOK_ON_BUILD=OFF -DOPENYAMM_USE_VULKAN=OFF
cmake --build build --target openyamm openyamm_unit_tests openyamm_atlas_runtime_tests -j25
```

Upstream 2.6 moved shared loading/localization/map code into `core/`. The port incorporates that structure. A Windows compiler-tool fix uses `relativePath.generic_string()` for a narrow prefix check in `core/compile/EditorDocument.cpp`; Windows `native()` returns a wide string.

Stage only the owned overlays:

```sh
python3 tools/stage_atlas_overlay.py --output output/atlas-overlay
```

The stage contains 679 overlay files, the Polish Atlas catalog, a SHA-256 manifest and documentation. Install `mods` and `languages` under the user-data root used by the Atlas-enabled executable. The metadata, journal layout and event support are overlays; never copy a complete old engine/world package over 2.6. The [Windows package](ATLAS_RELEASE.md) includes the staged files.

To regenerate Atlas annotations without replacing gameplay handlers:

```sh
OPENYAMM_BUILD_DIR=build \
OPENYAMM_ASSET_ROOT=/path/to/isolated/assets \
OPENYAMM_METADATA_MODS_ROOT=/path/to/metadata-inputs \
    tools/regenerate_lua_events.sh --atlas-metadata
```

The metadata root supplies the per-world `_legacy/events` inputs and the configured GrayFace reference folders. Shared global export is owned by MM8/default. Those reference inputs do not ship as runtime mods. Generated sidecars must not be edited by hand.

## Verified on 2.6

| Check | Result |
| --- | --- |
| Atlas, geometry, save extensions, localization and discovery logic | 91 cases / 5,304 assertions passed |
| Input/controller regression | 25 cases / 278 assertions passed |
| Real installed MM6/MM7/MM8 maps, English | 2 audit cases / 4,146 assertions passed |
| Real installed MM6/MM7/MM8 maps, Polish | 2 audit cases / 4,149 assertions passed |
| Map audit scope | 204 event programs and 203 installed geometries in each language; reserved geometry-less row excluded |
| Native Windows | MM6/MM7/MM8 Map Book, Discovery, Guide, fog/arrow and return to gameplay; AMD/OpenGL 3.3 |
| Save migration and exchange | 2.5 migration, five MM6 map save/load cycle and official-2.6 exchange passed |
| Main-menu toggle | 34 focused cases / 4,394 assertions; mouse, Space, restart persistence, PL/EN labels and Atlas availability checked natively |

The menu checks left the copied campaign save byte-identical. These checks are not full campaign playthroughs or hardware benchmarks. Android and physical-controller testing have not been performed for this release.

## English, Polish and MM6

See [ATLAS_LOCALIZATION.md](ATLAS_LOCALIZATION.md) and [ATLAS_MM6.md](ATLAS_MM6.md). The shared data APIs serve all three Atlases. No MM9 Atlas, gameplay overhaul, VCMI-style launcher or public modding contract is added by this release.

The repository also contains the independent [Polish MM9 retail importer](devdocs/MM9_POLISH_RETAIL.md) and local-catalog precedence fix. Its generated media and private review decisions are not public release assets.

## Credits

Maintained by Karol on [OpenYAMM by pjasicek and contributors](https://github.com/pjasicek/openyamm). Inspired by [u/hulululuh](https://www.reddit.com/user/hulululuh/)'s [MM7 guide](https://www.reddit.com/r/MightAndMagic/comments/1ww8pq0/i_made_an_interactive_world_map_guide_for_might/) and [MM8 guide](https://www.reddit.com/r/MightAndMagic/comments/1x7x7ok/simon_found_a_map_of_jadame_i_made_it_clickable/). Their HTML, JavaScript, guide text and artwork are not bundled.
