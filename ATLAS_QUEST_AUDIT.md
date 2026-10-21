# Atlas quest relationships: content audit

The shared `engine/data_tables/quest_locations.txt` currently describes 94 quests:
26 MM7 and 68 MM8 entries, with 364 relationships. This is partial campaign coverage.
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
| 101 | 174–176 | Aislen and outer-island deliveries; six individual huts are not yet linked |
| 109 | 177–178, 190 | Languid, Anointed Herb Potion and the disclosed Ravenshore smugglers clue |
| 111 | 182–183 | Hiss, Idol of the Snake and the journal's Abandoned Temple objective |
| 117 | 188–189, 298 | Arion's report, Regna and recipient Stanley; Stanley completes the delivery |
| 119 | 187 | Arion and his daughter's rescue from Alvar's Ogre Fortress |
| 127 | 193–194 | Lathius and Eclipse; the dialogue discloses Shadowspire, not a precise loot source |
| 129 | 198–200 | Keldon, Alvar roads and Ogre Fortress |
| 135 | 19 | Pascella, Prophecies of the Snake and the disclosed Abandoned Temple |
| 137 | 14–17 | Rohtnax, Isthric and the outer Dagger Wound islands |
| 139 | 184–186 | Maddigan, Ravenshore and the Dire Wolf Den |
| 142 | 207–209, 575 | Pole and Rust deliveries; see the stale-journal discrepancy below |
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

The Rust journal mentions Hobert, but the actual quest-giving and reward handlers are attached to Pole (NPC 84).
Hobert's handler 575 is one of the six potion deliveries. Relationships follow the actual interactions; the
original journal text is not silently rewritten. Exact delivery houses and stage-sensitive links remain work.

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

Three remaining nonempty MM8 journal rows are deliberately not presented as verified playable quest chains:

- QBit 5 (old pirate-leader objective) and 172 (old Arcomage objective) have no active setter in the current
  MM8/base event sources. The active tournament uses QBit 173 and already has relationships and Progress tracking.
- QBit 132 (Galvinus' Pure Accuracy request) is not set by global 244. The actual handler sets 133 and its
  reward handler 246 clears 133 and sets unrelated Blazen flag 134. The local decompiled MM8 reference contains
  the same discrepancy. This requires an explicit world event correction and a reviewed old-save migration;
  Atlas data must not invent a successful active journal entry or silently reinterpret those save bits.

Thus 68 relationships-bearing quests are not a claim of complete MM8 content, flawless campaign scripts or
full stage coverage. Named side-quest sources, potion reagent quantities/alternatives, optional dialogue clues,
remaining historical state and relocation stages still require audit. MM7 remains at 26 linked quests.

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
