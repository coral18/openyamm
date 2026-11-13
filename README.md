<div align="center">

# OpenYAMM Atlas

### Interactive Atlas for Might & Magic VII and VIII

Explore Antagarich and Jadame with maps that reveal what your party has discovered.

A community project by Karol, built on OpenYAMM, with exploration fog, dungeon plans, party tracking and a guidebook linked to the maps.

[See it in action](#see-it-in-action) · [Get started](#get-started) · [Integration handover](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS_HANDOFF.md) · [Upstream OpenYAMM](https://github.com/pjasicek/openyamm)

</div>

![The native MM8 Atlas showing Shadowspire in Guide Mode, with regional artwork, clustered map markers and a searchable location list](https://raw.githubusercontent.com/coral18/openyamm/refs/heads/extended/atlas-mm8/docs/atlas/images/mm8-shadowspire.png)

<p align="center"><em>Shadowspire, Jadame, in the native Windows build with Guide Mode enabled.</em></p>

> **Project status:** Atlas v1 is ready for source review on this fork's current base.
> The implementation lives on **[extended/atlas-mm8](https://github.com/coral18/openyamm/tree/extended/atlas-mm8)**.
> It has not been ported to OpenYAMM 2.2; the mod manifest alone is not an installable plugin for stock OpenYAMM.

## Discovery and Guide Mode

By default, the Atlas shows what your party has discovered. Unexplored terrain stays hidden; visiting a building,
meeting a trainer or learning a clue adds that information to the map. Opening the Atlas does not reveal a region.

Switch to Guide Mode to browse locations and look up game information. When you return to Discovery Mode,
places you only looked up stay undiscovered.

| Explore | Find your way | Connect the details |
| --- | --- | --- |
| Persistent exploration fog and a party arrow showing position and heading | Continent, region and dungeon navigation; search, filters, zoom and pan | Buildings, NPCs, trainers, travel routes, interactive sites and map connections |
| Small clustered markers and editable personal notes | Dungeon plans generated from engine geometry, with known height slices | Linked reference pages for items, spells, classes, alchemy, companions, progress, monsters and quests |

**26 outdoor regions · 110 dungeon plans · two world catalogs**

MM7 and MM8 share the same native interface and world index. Switching the viewed catalog does not move your party.
Guide Mode loads its index one map at a time, starting with the current location.

## See it in action

These are unmodified screenshots from the November 12, 2026 Windows build.

<table>
  <tr>
    <td width="50%">
      <a href="https://raw.githubusercontent.com/coral18/openyamm/refs/heads/extended/atlas-mm8/docs/atlas/images/mm7-discovery.png"><img src="https://raw.githubusercontent.com/coral18/openyamm/refs/heads/extended/atlas-mm8/docs/atlas/images/mm7-discovery.png" alt="MM7 Emerald Island in Discovery Mode: unexplored terrain is black and the party arrow marks the small explored area" width="100%"></a>
      <p><strong>Discovery first</strong><br>Emerald Island, MM7. The party knows only the explored area.</p>
    </td>
    <td width="50%">
      <a href="https://raw.githubusercontent.com/coral18/openyamm/refs/heads/extended/atlas-mm8/docs/atlas/images/mm7-the-pit.png"><img src="https://raw.githubusercontent.com/coral18/openyamm/refs/heads/extended/atlas-mm8/docs/atlas/images/mm7-the-pit.png" alt="MM7 The Pit in Guide Mode: a native geometry-derived dungeon plan, grouped location markers and height controls" width="100%"></a>
      <p><strong>Inside the dungeon</strong><br>The Pit, MM7. Engine geometry supplies the plan and height filters.</p>
    </td>
  </tr>
</table>

<details>
<summary><strong>Open the linked spell-reference example</strong></summary>

![Armageddon in the MM7 Guidebook, with spellbook links and actual guild sources in Deyja and The Pit](https://raw.githubusercontent.com/coral18/openyamm/refs/heads/extended/atlas-mm8/docs/atlas/images/mm7-spell-sources.png)

Spell pages link to guilds that may stock each book and show their Atlas locations. These links use the shared
stock pool; a book may not be on sale during your visit, and your party may not have access to the guild.

</details>

## Get started

1. Start with [the `extended/atlas-mm8` branch](https://github.com/coral18/openyamm/tree/extended/atlas-mm8).
   The repository's `main` branch has this project page; Atlas development happens on the linked branch.
2. Prepare the matching base and assets. Follow the [engine build documentation](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/README_ENGINE.md)
   and [Windows iteration notes](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS.md#windows-iteration).
   Use runtime assets matching this branch; compatibility with a newer official payload is not established.
3. Enable the Atlas module. Install the bundled `mods/interactive-atlas` manifest. If you use an explicit
   `mods/profile.yaml`, include the module ID in its enabled list:

   ```yaml
   enabled:
     - openyamm.interactive-atlas
   ```

   Retain any other mods your campaign requires. See the [mod/profile documentation](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/MODDING.md).
4. Select MM7 or MM8, click the minimap to open the Map Book, then choose Atlas in its sidebar.
   Discovery is the default; Guide Mode is optional.

The Atlas UI is optional. Disabling its module hides the entry while preserving shared discoveries and notes.
The module enables C++ functionality compiled into this Atlas build. It does not load a DLL or embed a website.

## Built on shared game data

The Atlas uses OpenYAMM's renderer, menus, geometry, tables and event metadata. Its shared World Database reads
the game's records, so the Atlas does not need a separate copy of those records. The game's save systems store
discoveries and observations. Browsing the guide does not move the party or grant visits, items or quest progress.

Mod Loader v1 validates manifests and checks dependencies, load order, conflicts and QBit ranges. It also supports
typed class patches and save requirements. A graphical mod manager and VCMI-style launcher are future plans.

## Verification and compatibility

The November 12, 2026 checkpoint includes:

| Check | Result and scope |
| --- | --- |
| Windows Release | Build succeeds; **563 selected regression cases / 281,275 assertions** pass |
| Guide navigation | **24,623 links** resolve across eight sections after indexing all 137 MM7/MM8 map entries, including the developer map |
| Native desktop UI | Two MM7/MM8 GPU tours: **68 steps and 14 captures** |
| Guide privacy and saves | Four save inspections, four matching Discovery crops, and 26 post-UI save/load steps |
| Android | Shared interface; last physically tested POCO release is **0.2.8 / 111**. Later PC fixes are not installed there |
| Controller | Physical Map Book opening and Atlas use confirmed; the later Map Book D-pad follow-up still needs physical acceptance |

The selected suite excludes one existing unrelated test that requires a retired portrait-asset path. The results
cover the checks listed here, not every campaign state or a complete playthrough.
See the [handover](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS_HANDOFF.md),
[coverage matrix](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS_COVERAGE.md) and
[reproducible fixtures](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/tests/fixtures/atlas/README.md).

To port the Atlas, start with [ATLAS_HANDOFF.md](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS_HANDOFF.md).
It covers the shared index, knowledge, UI and mod-loader changes, including save formats and API boundaries.
This branch also contains other fork changes that need to be separated from an upstream port.
MM6/MM9 atlases and upstream 2.2 compatibility are outside this handover.

## Support development

I use ChatGPT Plus to develop OpenYAMM Atlas and regularly hit its usage limits. Those limits are the main
reason I can't work faster or keep up with OpenYAMM's frequent releases.

If support lets me upgrade to a plan with higher limits, I'll use it to get much more Atlas development and
compatibility testing done. It would also make working with the OpenYAMM author more practical and give me
room for other Might & Magic mods, tools and projects.

One day, I'd love to build a larger original RPG in Unreal Engine with the spirit of Might & Magic.
For now, I'm working on OpenYAMM Atlas, which remains free and open source.

[![Support Atlas development](https://raw.githubusercontent.com/coral18/openyamm/refs/heads/extended/atlas-mm8/docs/atlas/support.svg)](https://ko-fi.com/kvonloraque)

See the [support page](https://github.com/coral18/openyamm/blob/main/SUPPORT.md) for the funding goal, or [support Karol on Ko-fi](https://ko-fi.com/kvonloraque).

## Documentation

- [Atlas implementation and development notes](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS.md)
- [Current coverage and limitations](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS_COVERAGE.md)
- [Mod Loader, profiles and typed patches](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/MODDING.md)
- [Android verification history](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS_ANDROID_QA.md)
- [Deferred ideas awaiting a decision](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS_DECISIONS.md)
- [Base engine build and project documentation](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/README_ENGINE.md)

## Credits and license

Built on **[OpenYAMM by pjasicek and its contributors](https://github.com/pjasicek/openyamm)**.
Both community guides were created by **[u/hulululuh](https://www.reddit.com/user/hulululuh/)**:

- [MM7: I made an interactive world map / guide for Might and Magic VII](https://www.reddit.com/r/MightAndMagic/comments/1ww8pq0/i_made_an_interactive_world_map_guide_for_might/)
- [MM8: Simon “found” a map of Jadame. I made it clickable.](https://www.reddit.com/r/MightAndMagic/comments/1x7x7ok/simon_found_a_map_of_jadame_i_made_it_clickable/)

Their guides inspired the Atlas's navigation and browsing. The native implementation is original; it does not
bundle their HTML, JavaScript, authored descriptions or embedded artwork, or import their complete content.

Code is distributed under the repository's [GNU GPL v3 license](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/LICENSE).
Original Might & Magic game assets retain their respective rights; the code license does not grant rights to those assets.
