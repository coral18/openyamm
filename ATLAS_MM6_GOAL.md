# Goal: complete MM6 Atlas on OpenYAMM 2.5

Requested by Karol on 2026-11-16. Continue from `0ca87a9c` (the accepted New Sorpigal pilot).
The thread goal service currently retains an older paused MM7/MM8 goal and rejects replacing it; this document
records the authorized MM6 objective and acceptance criteria without falsely completing that older goal.

## Scope

Use the existing native Atlas and upstream content systems. Cover the installed MM6 world, including regional
maps, dungeons, entrances and return routes, services, trainers, residents, transport, authored interactions,
item/monster sources and quest relationships. English and Polish are the supported languages for this work.
Preserve Discovery/Guide, exploration fog, position/direction, notes and the existing guidebook sections.

Do not add unrelated features, a launcher, another database, new gameplay rules, an engine update, MM9 Atlas data
or new languages. Android-specific implementation and POCO testing remain deferred; keep shared code portable.
Reuse the verified local 2.5 packs and build cache. Never change the user's ordinary installations or campaigns.

## Acceptance

- Every available MM6 map is represented once, with valid canonical identity and installed geometry/art.
- Region/dungeon grouping follows the active game's entrances and return routes. Conditional, one-way and secret
  connections retain their actual semantics; a navigation parent does not imply a gameplay route.
- Generated metadata belongs to the exporter and the callbacks it describes. No gameplay Lua is overwritten.
- Services, NPCs, trainers, transport and journal links use merged registry IDs and active world context.
- A new party does not learn undiscovered geography, people, secrets or quest solutions by opening the Atlas.
- Guide does not write discoveries or advance gameplay state. Search and world switching preserve filtering.
- New Atlas descriptions have complete Polish translations; reuse official game translations for existing terms.
- Actual installed-pack audits cover MM6/MM7/MM8 in English and Polish. Relevant pure-logic, localization,
  discovery, geometry and save/input checks pass.
- Native Windows checks cover representative regions and dungeons, travel/training/quest views, discovery,
  guidebook navigation, Unicode searches and returning to gameplay. New game, save and load pass.
- Deliver a separate runnable Windows test package, reproducible overlay staging, source changes and an accurate
  coverage/limitations record. An untested APK is not an acceptance requirement for this stage.

## Status: complete for the authorized Windows scope

- Complete: New Sorpigal pilot, including Goblinwatch and the Abandoned Temple; English/Polish and Windows checks.
- Complete: all-world inventory, hierarchy for 40 dungeons, 15 region anchors, 212 metadata sidecars and 203 journal
  relationships for 61 entries. Eleven reserved test maps remain excluded from player navigation.
- Complete: English/Polish descriptions, shared regressions, ownership tests, installed-pack audits and the MM6
  new-game/save/load scenario. Native tours cover a region, complex dungeons, height layers, quests and Unicode search.
- Complete: final package checks use only the official ZIP packs and owned overlays. All 680 staged files match
  their SHA-256 manifest; regeneration reproduces all 639 MM6/MM7/MM8 metadata sidecars byte for byte.
- Verified: 91 shared cases / 5,304 assertions; three ownership cases / 19 assertions; full installed-map audit
  in each language / 1,868 assertions; MM6 discovery checks / 2,284 English and 2,287 Polish assertions; and
  25 input/controller cases / 278 assertions. The five-map new-game/save/load scenario passes.
- Native acceptance: Free Haven, Control Center and Tomb of VARN in EN/PL, plus Polish Unicode search and the
  final staged-package tour. Guide shows all 15 regions; Discovery retains only visited maps after browsing Guide.

Delivery: separate `atlas-v2.5-mm6` Windows package with `Play MM6.cmd`, Polish default settings and its own
`play-mm6` campaigns. Android/POCO remains outside this completed scope, as requested.
