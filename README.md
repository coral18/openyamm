<div align="center">

# OpenYAMM Atlas

### Interactive Atlas for Might & Magic VII and VIII

**Explore Antagarich. Navigate Jadame. Keep the mystery of discovery.**

A community project by **Karol**, built on OpenYAMM: exploration fog, dungeon plans, party tracking and a connected guidebook.

[See it in action](#see-it-in-action) · [Get started](#get-started) · [Integration handover](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS_HANDOFF.md) · [Upstream OpenYAMM](https://github.com/pjasicek/openyamm)

</div>

![The native MM8 Atlas showing Shadowspire in Guide Mode, with regional artwork, clustered map markers and a searchable location list](https://raw.githubusercontent.com/coral18/openyamm/refs/heads/extended/atlas-mm8/docs/atlas/images/mm8-shadowspire.png)

<p align="center"><em>Shadowspire, Jadame — captured in the native Windows build. Guide Mode is explicitly enabled.</em></p>

> **Project status:** Atlas v1 is ready for source review on the current Atlas base.
> The implementation lives on **[extended/atlas-mm8](https://github.com/coral18/openyamm/tree/extended/atlas-mm8)**.
> It has not been ported to OpenYAMM 2.2; the mod manifest alone is not an installable plugin for stock OpenYAMM.

## An atlas that grows with your adventure

By default, the Atlas shows what your party has discovered. Unexplored terrain stays hidden; visiting a building,
meeting a trainer or learning a clue adds the corresponding knowledge. Opening the Atlas does not reveal a region.

When you want a reference, switch to **Guide Mode**. Browse authored locations and linked game information,
then return to Discovery without teaching the party anything you only looked up.

| Explore | Find your way | Connect the details |
| --- | --- | --- |
| Persistent exploration fog and a party arrow showing position and heading | Continent, region and dungeon navigation; search, filters, zoom and pan | Buildings, NPCs, trainers, travel routes, interactive sites and map connections |
| Small clustered markers and editable personal notes | Dungeon plans generated from engine geometry, with known height slices | Linked reference pages for items, spells, classes, alchemy, companions, progress, monsters and quests |

**26 outdoor regions · 110 dungeon plans · two world catalogs**

MM7 and MM8 share the same native interface and world index. Switching the viewed catalog does not move your party.
Guide indexing progresses one map at a time, giving the current location priority.

## See it in action

The screenshots below are unmodified captures from the November 12, 2026 Windows build.

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

Spell references link to possible guild stock and the corresponding Atlas location. Possible stock describes the
shared stock pool; it does not promise that a book is currently on sale or that the party can access the guild.

</details>

## Get started

1. **Use the Atlas branch:** [browse `extended/atlas-mm8`](https://github.com/coral18/openyamm/tree/extended/atlas-mm8).
   The repository's `main` branch hosts this presentation; Atlas source development remains on the linked branch.
2. **Prepare the matching base and assets.** Follow the [engine build documentation](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/README_ENGINE.md)
   and [Windows iteration notes](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS.md#windows-iteration).
   Use runtime assets matching this branch; compatibility with a newer official payload is not established.
3. **Enable the Atlas module.** Install the bundled `mods/interactive-atlas` manifest. If you use an explicit
   `mods/profile.yaml`, include the module ID in its enabled list:

   ```yaml
   enabled:
     - openyamm.interactive-atlas
   ```

   Retain any other mods your campaign requires. See the [mod/profile documentation](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/MODDING.md).
4. **Open it in game:** select MM7 or MM8, click the minimap to open the **Map Book**, then choose **Atlas** in its sidebar.
   Discovery is the default; Guide Mode is optional.

The Atlas UI is optional. Disabling its module hides the entry while preserving shared discoveries and notes.
The module selects native C++ functionality already compiled into this Atlas build; it does not load a DLL or embed a website.

## Built on shared game data

The Atlas uses OpenYAMM's renderer, menus, geometry, tables and event metadata. A shared **World Database** references
authoritative game records instead of maintaining a separate Atlas copy. Discovery and observations use the game's
save systems. Guide browsing neither travels nor awards visits, items or quest progress.

The accompanying **Mod Loader v1** provides manifest validation, dependencies, load order, conflicts, QBit-range
checks, typed class patches and save requirements. A graphical mod manager and VCMI-style launcher remain future plans.

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

The selected suite excludes one existing unrelated test requiring a retired portrait-asset path. These checks
establish the recorded behavior; they do not certify every campaign state or a complete playthrough.
See the [handover](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS_HANDOFF.md),
[coverage matrix](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS_COVERAGE.md) and
[reproducible fixtures](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/tests/fixtures/atlas/README.md).

**For upstream integration:** begin with [ATLAS_HANDOFF.md](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/ATLAS_HANDOFF.md).
It maps the shared index, knowledge, UI and mod-loader changes and records save-format and API boundaries.
This branch contains other fork changes, so an upstream port needs a scoped review rather than a wholesale merge.
MM6/MM9 atlases and upstream 2.2 compatibility are outside this handover.

## Support development

I currently develop OpenYAMM Atlas with **ChatGPT Plus**. Subscription usage limits are the main constraint on
my development pace: I could do more work, but I cannot currently keep up with upstream's release cadence.

If community support lets me move to a higher-usage plan, I will use the extra capacity to substantially
accelerate Atlas development, compatibility work and collaboration with the OpenYAMM author.
The project remains free and open source.

[![Support Atlas development](https://raw.githubusercontent.com/coral18/openyamm/refs/heads/extended/atlas-mm8/docs/atlas/support.svg)](https://github.com/coral18/openyamm/blob/main/SUPPORT.md)

See the [support page](https://github.com/coral18/openyamm/blob/main/SUPPORT.md) for the funding goal and current payment-link status.

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

Their guides inspired the Atlas workflow. This is an original native implementation: their HTML, JavaScript,
authored descriptions and embedded artwork are not bundled, and their complete content has not been imported.

Code is distributed under the repository's [GNU GPL v3 license](https://github.com/coral18/openyamm/blob/extended/atlas-mm8/LICENSE).
Original Might & Magic game assets retain their respective rights; the code license does not grant rights to those assets.
