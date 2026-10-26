# Atlas quest relationships: content audit

The shared `engine/data_tables/quest_locations.txt` currently describes 145 quests:
76 MM7 and 69 MM8 entries, with 595 relationships. This is partial campaign coverage.
All names and descriptions resolve through the loaded game registries; the relationship file contains IDs,
roles and disclosure metadata, not a second copy of quest prose. Mods and other consumers use the same table.

## Sources and disclosure

The 2026-10-18 expansion was checked against the mounted game's shared `engine/events/Global.lua`, MM7's
`worlds/mm7/events/Global_mm7_mmmerge.lua` overlay, `english/quests.txt`, `npc.txt`, `npc_topic_text.txt`,
`items.txt` and `map_stats.txt`. Generated Lua is a read-only behavior reference; no generated scripts changed.
The earlier audit used the prepared runtime cache for sparse source files. The current checkout also contains
the MM7 overlay and MM8's `Global_mm8_promotions.lua`; explicit overrides take precedence over generated handlers.

`journal` means the accepted request names the place, person or item, including clues in the accompanying
dialogue. It does not reveal unexplored terrain or an unknown NPC's address. `guide` means an exact destination
that the request does not disclose. These rows never award discoveries when browsing Guide Mode.
An item name is readable in the request; its full reference link still requires item knowledge in Discovery.

## Added MM8 relationships

| Quest QBit | Authored global events | Checked relationships |
| --- | --- | --- |
| 61 | 46 | Overdune, ashes, Dust's tomb, return; no guessed base residence |
| 68 | 35–37 | Volog, Ravage Roaming clue; exact Ancient Troll Home requires discovery QBit 69 |
| 85 | 20 and initial journal | Find Dadeross on Dagger Wound; no invented giver |
| 91 | 110, 152–153 | Council Fellmoon and Xanthor; Xanthor's current residence comes from NPC state |
| 101 | 174–176 | Aislen and outer-island deliveries; recipient contacts added in the 2026-10-26 follow-up |
| 109 | 177–178, 190 | Languid, Anointed Herb Potion and the disclosed Ravenshore smugglers clue |
| 111 | 182–183 | Hiss, Idol of the Snake and the journal's Abandoned Temple objective |
| 117 | 188–189, 298 | Arion's report, Regna and recipient Stanley; Stanley completes the delivery |
| 119 | 187 | Arion and his daughter's rescue from Alvar's Ogre Fortress |
| 127 | 193–194 | Lathius and Eclipse; the dialogue discloses Shadowspire, not a precise loot source |
| 129 | 198–200 | Keldon, Alvar roads and Ogre Fortress |
| 135 | 19 | Pascella, Prophecies of the Snake and the disclosed Abandoned Temple |
| 137 | 14–17 | Rohtnax, Isthric and the outer Dagger Wound islands |
| 139 | 184–186 | Maddigan, Ravenshore and the Dire Wolf Den |
| 142 | 207–209, 571–576 | Pole and Rust deliveries; corrected in the 2026-10-26 follow-up |
| 150 | 213–214 | Calindril and Dragonbane Flower; no guessed flower source |
| 152 | 216–217 | Balion and Dragonbane Flower; no guessed flower source |
| 154 | 219 | Avalon and Garrote Gorge wilderness; completion checks wilderness population |
| 157 | 221 | Jerin Flame-eye and Garrote Gorge wilderness |
| 160 | 223–225 | Zelim and Drum of Victory; no guessed location from the Nagas' historical involvement |
| 162 | 229–230 | Benefice, Puzzle Box and the named Mad Necromancer's location in Shadowspire |
| 164 | 235–237 | Hallien, Grave Earth and the explicitly named Vampire Crypt |
| 166 | 226–228 | Tantilion and Bone of Doom; no disclosed location is invented |
| 173 | 580, 597 | Tonk Blueswan; existing Progress reference supplies tavern tracking |
| 176–178 | 656 | Asael Fromago and the three distinct cheeses; no guessed item sources |

The initial Rust audit found that the journal named Hobert while the actual giver and reward handlers belong
to Pole (NPC 84). Hobert's handler 575 is one of the six deliveries. The 2026-10-26 follow-up below corrects
the authoritative English journal row and adds recipient contacts; the Atlas owns no duplicate quest prose.

## MM8 campaign expansion, 2026-10-21

This adds 33 quests and 133 relationships. It follows the current generated global handlers, world map events
and the promotion override; no generated Lua or campaign behavior was changed.

| Quest QBit | Authored events | Checked relationships |
| --- | --- | --- |
| 13–18 | `out03` 197; globals 48–49, 64–67, 94–103; `d09` council entrance | Bastian, faction regions and named guild/camp destinations; unlocated Dragon Cave, Balthazar Lair and unnamed leaders are Guide hints |
| 25 | global 42; `out03` 197 | Ironsand witness request; Overdune's identity remains a Guide hint |
| 31, 33 | globals 49, 64 | Dragon Egg; Quixote discloses Zog's fortress, Deftclaw does not |
| 36 | globals 106–108 | Council Fellmoon, Regna fleet and disclosed Dagger Wound outpost clue |
| 41–44 | globals 152–153; `out02` 75 | Xanthor, four distinct elemental Hearts and their planes |
| 46 | `out02` 75; globals 159–166 | Crystal Gateway; exact Palace, intervening plane and Escaton remain Guide hints |
| 48, 50, 52, 54 | globals 164–170 | Escaton and the four lords; exact prison maps remain Guide hints |
| 70, 72 | globals 50, 52–58; promotion override 58 | Leane, Blazen, Ebonest, regional search and Dervish's cure contact |
| 74 | globals 60–62; promotion override 62 | Deftclaw, the named Dragon Hunter camp and Whistlebone's sword |
| 76 | globals 69–73; promotion override 71 | Dark Dwarf Compound, Axe, Dadeross and certificate, Tessalar |
| 78 | globals 78–81; promotion override 81 | Stephen, Prophecies of the Sun and Abandoned Temple |
| 80 | globals 83, 90; promotion override 90 | Lathean, Sarcophagus and Remains; no invented initial source location |
| 82 | globals 86–89; promotion override 89 | Vetrinus, Khel's library/book and the explicitly named Mad Necromancer's jar supply |
| 113, 115, 121, 123, 125 | globals 179–181, 201–203, 210–212, 232–234, 238–240 | Actual potion request/reward contacts; alternatives are not misrepresented as individually mandatory reagents |
| 236 | global 249 | One-Eye; no invented treasure location |
| 284 | globals 29, 32 | Separate return-to-Fellmoon stage after Arion reads the blackmail letter |

The Champion and Great Wyrm return contacts include distinct council representative records (52 and 53).
QBits 21/22 select the corresponding stage; current versus observed NPC indices still determine whether a
contact has a navigable residence. These relationships never guess a relocated NPC's address.

Arion's house 772 is corrected to an objective of the blackmail delivery, not its return location.
The letter's existing `d09` relationship is retained: despite its loaded display name "Merchant House of
Alvar", actual Ravenshore entrance events 503/505 lead there. Placeholder house 750 has no valid map and is
not used as a navigation target.

At the 2026-10-21 checkpoint, three remaining nonempty MM8 journal rows were not presented as verified playable
quest chains. Accuracy (132) is corrected by the subsequent 2026-10-22 checkpoint below; 5/172 remain stale:

- QBit 5 (old pirate-leader objective) and 172 (old Arcomage objective) have no active setter in the current
  MM8/base event sources. The active tournament uses QBit 173 and already has relationships and Progress tracking.
- QBit 132 (Galvinus' Pure Accuracy request) is not set by global 244. The actual handler sets 133 and its
  reward handler 246 clears 133 and sets unrelated Blazen flag 134. The local decompiled MM8 reference contains
  the same discrepancy. This requires an explicit world event correction and a reviewed old-save migration;
  Atlas data must not invent a successful active journal entry or silently reinterpret those save bits.

Thus 68 relationships-bearing quests are not a claim of complete MM8 content, flawless campaign scripts or
full stage coverage. Named side-quest sources, potion reagent quantities/alternatives, optional dialogue clues,
remaining historical state and relocation stages still require audit. MM7's subsequent expansion is recorded below.

## Added MM7 relationships

Promotion handlers in the mounted MM7 overlay take precedence over generated base handlers. This expansion adds
geographical/contact relationships, not another promotion implementation or vanilla class eligibility rules.

| Quest QBit | Authored global events | Checked relationships |
| --- | --- | --- |
| 541 | 813–814; Erathia 402–406 | Stephan, school, cipher and Erathia; the exact hidden tomb requires QBit 569 |
| 545 | 821–822 | Leda and the Arena |
| 546 | 823–824 | Frederick and the Haunted Mansion in the Barrow Downs |
| 547 | 825–826 | Frederick and Castle Navan's treasury |
| 548 | 827–828 | Lysander and the Oldest Tree in Tularean Forest |
| 549 | 829–830, 833 | Ebednezer, Avlee and the Faerie King who performs the promotion |
| 550 | 831–832 | Ebednezer and the bounty requirement; individual town halls still need links |
| 551 | 834–835 | Oldest Tree, Mercenary Guild in Tatalia and merged item Heart of the Wood |
| 554 | 836–837 | Rebecca and Evenmorn's Grand Temple of the Moon, not Emerald Island's Temple |
| 555 | 838–839 | Daedalus, Tidewater Caverns and Map to Evenmorn Island |
| 556 | 840–841 | Daedalus and Evenmorn's Grand Temple of the Sun |
| 560 | 846–847 | Halfgild, Celeste, Walls of Mist and empty Lich Jars |
| 561 | 848–849 | Anthony and the three disclosed pilgrimage regions |
| 566 | 850–851 | Anthony, Nighon tunnels, Zokarr's skull and Zokarr's Tomb |
| 567 | 852–853 | Tor, Land of the Giants' Dragon Caves and the merged MM7 Dragon Egg |

The Lich quest journal calls its source the "Proving Grounds", while the actual dialogue identifies Walls of
Mist. The Atlas resolves that loaded map and item 1417; it does not substitute MM8's Lich Jar or a nonexistent map.
Precise stone circles, bounty halls, remaining conditional stages and contacts without a known
physical residence still need richer links. The parent [coverage matrix](ATLAS_COVERAGE.md) remains authoritative
for the full objective.

## Verification

The shared-content test resolves every relationship against loaded maps, NPCs, houses and items. Additional
tests exercise the real cipher and Troll Homeland datasets: Discovery hides precise destinations before the
corresponding discovery flags, then exposes the learned clues. Guide exposes future clues without marking them
informed, and an unknown cipher's full item reference stays hidden.
Native isolated Windows scenarios accept Languid's and Pascella's requests and Anthony's pilgrimage through
authored dialogue, then verify quest knowledge after save/load. Scenarios do not claim the quests were completed.

The conditional-clue checkpoint adds an explicitly hybrid Ninja scenario: Initiate/Dark-path prerequisites are
fixtures; actual Stephan dialogue gives the quest and actual Erathian button interactions solve the puzzle.
Only then is the exact tomb relationship recorded. Save/load preserves the fact. A controlled chest-completion
flag and removed puzzle flag isolate the final authored reward and historical retention; this does not test
the promotion prerequisites, tomb combat or its chests. Other quest stages still need content work.

The 2026-10-21 expansion passes 193 focused Windows cases / 7330 assertions and 88 Linux ASan/UBSan cases /
4797 assertions. The full shared-content validator resolves all 364 relationships; disclosure tests contrast
Quixote/Deftclaw's egg clues, Champion/Lich source clues, the Crystal Gateway and the four secret prison maps.
Browsing Guide leaves quest, location and QBit knowledge unchanged.

`mm8_promotion_requests_knowledge.yml` accepts the real Champion and Lich requests and round-trips their knowledge.
`mm8_blackmail_return_knowledge.yml` uses the strict `faithful` runner with an explicitly declared inventory
fixture: after the initial authored request, `grant_item` places the letter in inventory before the handoff,
avoiding reliance on transient cursor state across map reloads. This does not test cursor stowing, travel or combat.
Arion's actual topic 32 then sets the distinct return stage, actual Fellmoon topic 29 awards the reward and clears
it, and strict assertions verify both saves and retained knowledge. It is not a fixture-free campaign playthrough.
Private Windows captures show Champion's laboratory only in Guide, Lich's explicitly disclosed library/laboratory
in Discovery, and the distinct active return-to-Fellmoon entry. Android/controller acceptance remains pending.

## MM7 campaign expansion, 2026-10-21

This adds 50 quests and 214 relationships. MM7 now has 76 linked entries: all nonempty, non-placeholder campaign
journal rows in the current 512–1023 range except QBit 757. The latter is a transient `Congratulations` notification,
set and immediately cleared by several map events; it is not a quest. This inventory claim does not establish
complete dialogue-stage, site or campaign behavior coverage.

| Quest QBits | Current sources | Added relationships |
| --- | --- | --- |
| 513–518 | `7out01` initial map event, global 756 | Six hunt items and Thomas; no invented NPC giver for the initial assignment |
| 528 | globals 784–785; override 783 | Markham and Emerald Island; precise dragon cave and shield proof are Guide hints |
| 530–531, 533 | globals 794–800 and promotion overrides | Lasker, vase/Manor, Watchtower 6, Seknit and Celeste; exact dagger proof is a Guide hint |
| 534, 536, 538–539 | globals 801–810, promotion overrides; `7d13` 376 | Quixote/Wromthrax, Alice/Setag, Hume and the Barrow Downs |
| 589–590, 606–607 | globals 874–886 and follower supplements | Real/false Riverstride plans, real/imposter Loren, Catherine and the loaded ElfKing record |
| 591 | `MM7.UpdateTulareanArtifactBattle`, globals 868/872/880/887/890 | Messenger, Trumpet and Tularean Forest; optional recipient choices are Guide hints |
| 613, 640 | globals 895–896, 917–918 | Walls of Mist/Gavin and Breeding Zone/Archibald |
| 615–616, 634–635 | globals 897–900, 907–910 | Distinct altar pieces, both temples, Colony Zod and the correct Light/Dark recipient |
| 618, 620–621, 636–637, 639 | globals 901–906, 911–916 | Wine Cellar, Soul Jar Case, current Control Cube, Clanker's defenses and the corresponding contacts |
| 642–643 | globals 919–922 and final-task overrides | Lincoln, Shoals, Overthruster and Resurectra/Kastore |
| 663–665 | `7out02` judge events; current common arbiter logic | Announced faction access routes and a return to Castle Harmondale; candidate identities remain Guide hints |
| 668, 670–671 | globals 931–932, 935–937 | Haldar/Maze and the separate Norbert letter/Quill return stages |
| 691–692 | globals 938–940 | Johann's letter, Hall under the Hill, Faerie King and the separate Pipes return stage |
| 693–694 | `7out02`/`7out13`, globals 941–943 | Niles, Mercenary Guild, Tapestry and the disclosed faction-specific castle |
| 698–699, 706–707 | globals 944–949, 1063–1064 | Troglodytes, both Griffin regions, Darron/White Cliff and Gary/Stole/Hall of the Pit |
| 712, 716–717 | globals 1212–1217 | Statuette regions, the three distinct paintings and Gina's tournament request |

Merged item IDs follow current handlers: vase 1426, Carmine proof 1342, both Soul Jar Case requests 1404, Control
Cube 1477, altar pieces 1478/1479 and Overthruster 1407. Exact unannounced proof, meditation and statuette-form
hints stay in Guide; the initial generic potion/instrument requests do not assert that the party already knows
their precise Cure Wounds/Lute definitions. The meditation hint follows current `7d13`'s Hume interaction,
not an assumed vanilla barrow number. Names and descriptions still come from the loaded registries.

The Tapestry request uses QBits 611/612 to disclose Castle Lambent or Castle Gloaming. Guide deliberately offers
both alternatives; it does not record either as learned. After quest completion, Discovery keeps the actually
learned castle instead of inferring the other branch from unrelated later state. Optional Ambassador/candidate
dialogue clues, remaining reward stages, precise sites, conditional residence and deadline presentation need
further work; one indexed journal entry does not prove that every stage is represented.

Verification: 194 focused Windows cases / 7477 assertions and 89 Linux ASan/UBSan cases / 4944 assertions pass.
The shared-content validator resolves all 578 relationships against mounted definitions. The new pure test checks
both Tapestry branches and historical retention, plus undisclosed contestant/meditation/assassin/Arcomage clues.
`mm7_campaign_requests_knowledge.yml` verifies actual initial hunt assignment and authored Markham, Rogue and
Initiate requests with persistence. The two `mm7_tapestry_{light,dark}_knowledge.yml` scenarios run with strict
`faithful` assertions but explicit faction/Malwick prerequisite and Tapestry inventory fixtures. The actual
request and reward handlers record and retain only their branch's clue across save/load; the castle's physical
discovery flag remains zero. Faction selection, the theft, castle access, travel and combat are not tested there.


## MM8 Pure Accuracy correction and legacy import, 2026-10-22

QBit 132 now has Galvinus giver/return and Ravage Roaming relationships. The world-owned
`Global_mm8_accuracy.lua` supplement replaces handlers 244/246 without editing the generated global export:
132 is active, 133 records completion, and the reward never writes unrelated Blazen bit 134. The original
2/1/4 reagent groups, one Pure Accuracy potion and 5000 XP per party member are retained. All groups are checked
before any are consumed. The replacement explicitly declares its fixed item reward 269 for shared source indexing.

The one-time import uses effective Galvinus topic slots and the known legacy patterns:

| Case | Legacy evidence | Import |
| --- | --- | --- |
| 0 | No 132/133, request 244 and reward 246 still present | Preserve unstarted state |
| 1 | No 132, 133 set, request closed, reward 246 still present | Restore active 132 and clear legacy-active 133 |
| 2 | No 132/133, both quest slots closed and legacy reward's 134 set | Set completion 133 and remember journal 132 |
| 3 | Anything else | Preserve raw state; record that it was unclassified |

Version, case, original 132/133/134 flags and original request/reward slots are retained under
`openyamm.migration.mm8.pure-accuracy-v1`. The import never awards items/XP and never clears bit 134: an old
completed potion quest may already have set it incorrectly, but its source is ambiguous. Existing Blazen
435/1542 state is preserved. The pattern import targets the verified legacy base scripts, not arbitrary mod
rewrites. Corrected quest state requires the matching corrected world supplement; save format compatibility
is not a claim that older quest scripts understand the corrected flag meanings.
Global supplements belong to the shared mounted-content base, so the migration may initialize during MM7 play
when MM8 is mounted. The MM7 request regression checks that this creates neither MM8 quest flags nor knowledge.

Core's neutral `GetNPCTopic` query uses the loaded registry plus explicit overrides, including hidden zero slots.
The shared dialogue evaluator now supplies that registry to read-only eligibility queries. `RememberQuest`
imports knowledge without synthesizing acceptance/completion, changing flags or inferring conditional clues.
No binary save format changed. The Atlas remains an optional UI consumer.

Verification includes actual pre-fix `0b47beb` saves, independently loaded by its retained executable/package
view to establish original state and XP. Four valid legacy variants (active/completed, with/without authored
Blazen Cure) migrate, round-trip and retain their original XP; active/completed/active switching in one process
also passes. A deliberately inconsistent legacy state stays unchanged with case 3. The fresh strict scenario
refuses a partial recipe, succeeds after the missing reagent is supplied, records exactly 5000 additional XP,
leaves only the ingredient-information topic and retains history after reload. The same fresh scenario passes
with the Atlas UI mod disabled. Reagent/Gem inventory and deliberately inconsistent-state fixtures are controlled;
these checks do not claim ingredient gathering, alchemy mixing or a full promotion playthrough.

Targeted Windows Atlas/save/package/mod coverage passes 194 cases / 7479 assertions; Lua/NPC coverage passes
73 / 2198, including the new API and real shared dialogue-evaluator checks. The curated Linux ASan/UBSan suite
passes 89 / 4946. A wider GameplayRuleRegression run encountered 11 missing-source-fixture failures (sparse
MM6/MM9 scenes/binaries and source asset-root setup); it is not reported as an all-green full suite.
Native AMD OpenGL captures verify active and imported historical entries. After Atlas browsing, all three
quicksaves retain quest history, flags and XP through another save/load. The MM7 campaign request scenario also
passes with the shared MM8 supplement mounted. Private records are under
`atlas-accuracy-20261022/` in the Windows development root; public fresh/legacy/migration inputs and the
fixture asset-view preparer are documented in `tests/fixtures/atlas/README.md`.

Only stale MM8 rows 5/172 remain outside the playable journal inventory. Complete Atlas acceptance still needs
remaining conditional/site/reference coverage and Android/controller checks; this correction is a checkpoint.

### Possible contact residences, 2026-10-22

Quest/NPC IDs remain references to the loaded shared registries. The World Database now combines current or
last-observed state with callback-owned declarations of possible NPC destinations. Guide quest contacts can
navigate those alternatives; Discovery does not infer them from a quest contact's name. The quest inventory
stays at 145 entries / 581 relationships, with no duplicated residence table in the Atlas or quest data.
`ATLAS.md` records the source/export audit, native navigation checks and remaining eligibility limitations.


### Individual delivery contacts, 2026-10-26

QBits 101 (Aislen) and 142 (Pole) now reference the six actual recipient NPCs and the requested item.
This adds fourteen relationships, bringing the registry to 145 quests / 595 relationships. Names, items and
residences resolve through the existing shared registries. There is no copied list of house coordinates.

The request identifies a group of huts/houses, not all six residents by name. Each individual contact is a
journal-disclosed conditional clue gated by its real delivery flag: 102–107 for Aislen, 143–148 for Pole.
Discovery exposes that clue after the actual handoff; Guide can show it beforehand without recording it.
A failed delivery or a repeated conversation reveals no additional recipient. Historical contact knowledge
survives quest completion and save/load. This is contact history, not a new pending/done checklist.
Unrecorded old completion flags do not synthesize historical recipient discoveries for an inactive quest.

The authoritative English journal row 142 now returns the party to Pole, matching global handlers 207/208
and NPC 84. Hobert (NPC 82, handler 575) remains one of the recipients. Only that English text changes;
generated Lua comments and other language tables are not rewritten. The Windows development overlay now
includes the English quest table so runtime text matches the reviewed source.

Two faithful scenarios use actual authored NPC-topic callbacks for acceptance, all six deliveries, refusal
without supplies, repetition without an extra reward, final turn-in and save/load. Explicit NPC IDs bypass
physical travel. Each request gives three items; the held third item is explicitly placed into inventory
through the harness's real placement helper. The missing three supplies are controlled fixtures, not a test
of gathering or alchemy. Aislen yields 3000 XP and 1500 gold; Rust yields 7500 XP and 1500 gold when handler
576 is last. Other Rust completion orders have differing authored XP; this change does not rebalance them.
Both scenarios start at zero XP / 500 gold and finish at 2000 gold. Exact inventory counts guard against
extra consumption/rewards; all six learned-contact keys survive reload.

The eleven focused Windows unit cases pass 810 assertions, including independent reverse delivery order,
undelivered-recipient privacy, Guide browsing, historical retention and validation of the full shared registry.
The two real-dialogue scenarios pass 100 and 98 steps. Public inputs are
`tests/scenarios/atlas/mm8_delivery_{101,142}_knowledge.yml`, corresponding native `tools/menu_tours/` inputs
and the paired `_privacy.yml` post-UI verifiers. Native acceptance and remaining limitations are recorded in
`ATLAS.md`. Full quest-stage coverage and the parent coverage matrix remain open.
