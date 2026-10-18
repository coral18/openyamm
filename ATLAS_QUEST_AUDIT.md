# Atlas quest relationships: content audit

The shared `engine/data_tables/quest_locations.txt` currently describes 61 quests:
26 MM7 and 35 MM8 entries, with 231 relationships. This is partial campaign coverage.
All names and descriptions resolve through the loaded game registries; the relationship file contains IDs,
roles and disclosure metadata, not a second copy of quest prose. Mods and other consumers use the same table.

## Sources and disclosure

The 2026-10-18 expansion was checked against the mounted game's shared `engine/events/Global.lua`, MM7's
`worlds/mm7/events/Global_mm7_mmmerge.lua` overlay, `english/quests.txt`, `npc.txt`, `npc_topic_text.txt`,
`items.txt` and `map_stats.txt`. Generated Lua is a read-only behavior reference; no generated scripts changed.
The prepared runtime audit cache supplies the packaged MM7 overlay when it is absent from the sparse checkout.

`journal` means the accepted request names the place, person or item, including clues in the accompanying
dialogue. It does not reveal unexplored terrain or an unknown NPC's address. `guide` means an exact destination
that the request does not disclose. These rows never award discoveries when browsing Guide Mode.
An item name is readable in the request; its full reference link still requires item knowledge in Discovery.

## Added MM8 relationships

| Quest QBit | Authored global events | Checked relationships |
| --- | --- | --- |
| 61 | 46 | Overdune, ashes, Dust's tomb, return; no guessed base residence |
| 68 | 35–37 | Volog, Ravage Roaming clue; exact Ancient Troll Home remains Guide-only |
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

## Added MM7 relationships

Promotion handlers in the mounted MM7 overlay take precedence over generated base handlers. This expansion adds
geographical/contact relationships, not another promotion implementation or vanilla class eligibility rules.

| Quest QBit | Authored global events | Checked relationships |
| --- | --- | --- |
| 541 | 813–814 | Stephan, School of Sorcery, cipher and Erathia; exact hidden tomb is Guide-only |
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
Precise stone circles, bounty halls, later cipher disclosure, conditional stages and contacts without a known
physical residence still need richer links. The parent [coverage matrix](ATLAS_COVERAGE.md) remains authoritative
for the full objective.

## Verification

The shared-content test resolves every relationship against loaded maps, NPCs, houses and items. Additional
tests exercise the real cipher and Troll Homeland datasets: Discovery hides exact undisclosed destinations,
Guide exposes them without marking them informed, and an unknown cipher's full item reference stays hidden.
Native isolated Windows scenarios accept Languid's and Pascella's requests and Anthony's pilgrimage through
authored dialogue, then verify quest knowledge after save/load. Scenarios do not claim the quests were completed.
