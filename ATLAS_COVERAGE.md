# Interactive Atlas MM7 and MM8: coverage and completion work

Active objective: complete native interactive atlases for MM7 and MM8, using shared game/world data.
The MM8 world-map slice is a checkpoint, not completion of this objective.

Reference feature inventories:

- [MM7 community guide](https://www.reddit.com/r/MightAndMagic/comments/1ww8pq0/i_made_an_interactive_world_map_guide_for_might/)
- [MM8 community guide](https://www.reddit.com/r/MightAndMagic/comments/1x7x7ok/simon_found_a_map_of_jadame_i_made_it_clickable/)

The downloaded MM8 archive contains one self-contained HTML file and no declared reuse license. Keep it outside
Git as a behavioral reference. Implement original native behavior, reuse mounted game artwork and authoritative
tables, and do not bundle its HTML, JS, translated prose, generated illustration or embedded assets.

| Feature | MM7 native | MM8 native | Remaining work |
| --- | --- | --- | --- |
| Region browsing, search, zoom/pan | Implemented | Implemented | More extensive map/content audit |
| Continent overview | Existing game illustration | Original regional schematic | Optional licensed/original illustrated Jadame artwork |
| Discovery fog and current party arrow | Implemented | Implemented | Wider dungeon/device checks |
| Dungeon plans from engine geometry | Implemented | Implemented | Elevation selection, annotations and special connection views |
| Buildings, resident NPCs, trainers | Implemented | Implemented | Runtime relocation and conditional-event coverage |
| Trainer requirements and prices | Shared gameplay evaluator | Shared gameplay evaluator | Class/skill comparison with links to matching trainers |
| Walking and scheduled transport | Existing table index | Existing table index | Visual route links and neighboring-region navigation |
| Quest giver/objective/return chains | Not integrated | Not integrated | Shared quest/location relationships and discovery gating |
| Chests, monster groups, wells, obelisks | Not integrated | Not integrated | Geometry/event-backed point layers and details |
| Teleports and special connections | Not integrated | Not integrated | Gate/discovery semantics and Barrows/Regna connection views |
| Items, artifacts and equipment reference | Not integrated | Not integrated | Shared ItemTable view and item-source relationships |
| Alchemy recipes and ingredients | Not integrated | Not integrated | Shared mixing/reagent graph, intermediate recipes |
| Spellbook reference and guild links | Not integrated | Not integrated | Shared SpellTable view and guild availability |
| Recruitable companions | Not applicable as an MM7 guide section | Not integrated | Roster details and recruitment locations/state |
| Arcomage/Arena tracking | Not integrated | Not integrated | Existing party state, linked taverns and arena records |
| Custom markers and notes | Existing Map Book only | Existing Map Book only | Native Atlas integration and namespaced persistence |
| Optional Atlas mod | Still built-in | Still built-in | UI module registration and profile-controlled activation |
| Localization parity | Current engine language | Current engine language | Integrate engine localization; no copied guide translation bundle |

Completion requires the missing native functions, data validation against the active game packages, meaningful
unit/runtime checks and visual checks. A placeholder page or a list of unrelated journal text is not a completed
quest guide. Reference data must describe the active merged/modded game, rather than silently assert vanilla rules.

Checkpoint evidence and implementation details are recorded in [ATLAS.md](ATLAS.md). All tests use isolated settings,
saves and captures. Original user campaigns and the official installation remain outside test writes.
