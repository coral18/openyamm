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
| Dungeon plans from engine geometry | Known height bands, party-height selection, filtered pins and height-aware notes | Same shared projection and controls | Complex floor separation and special connection views |
| Buildings, resident NPCs, trainers | Current/observed residents, indoor service markers | Same shared index | Conditional venues, shared-service placements and generated/roaming NPCs |
| Trainer requirements and prices | Gameplay evaluator, observed offers and race-specific mastery comparisons | Same shared evaluator | Conditional eligibility audit and starting-skill/race-selection checks |
| Walking and scheduled transport | Directed walking/coach/boat graph, departure pins, last-offered schedules/fare/duration, live local overrides and destination navigation | Same shared knowledge/navigation | Remote scripted timetable references, availability forecasting and arrival-point links |
| Quest giver/objective/return chains | Shared registry, 26 quests and conditional clues | Shared registry, 35 quests and conditional clues | Complete campaign relationships, remaining conditional stages and potential residences |
| Chests, wells, shrines, obelisks | Event-backed Sites layer | Event-backed Sites layer | Effects, loot/open state, inherited decoration events and wider content audit |
| Monster groups and Bestiary | Authored encounter areas and last-observed actors | Same shared index and observed-only Bestiary | Placed/scripted actors, named overrides, automatic visibility discovery and variant/source links |
| Teleports and special connections | Declared map links with native navigation | Declared map links with native navigation | Conditional destinations, Gate semantics and Barrows/Regna connection graphs |
| Items, artifacts and equipment reference | Fixed loot, declared conditional rewards, current NPC-topic sources and taken creature items | Same shared index and discovery history | Remaining handwritten reward declarations, guaranteed scripted drops, typed semantic-source audit, icons and equipment comparisons |
| Alchemy recipes and ingredients | Native recursive recipe view | Native recursive recipe view | Broader canonical-content audit and ingredient-source links |
| Spellbook reference and guild links | Spell/book navigation and eligible guild links from shared stock rules | Same shared rules and discovery filtering | Observed shelves, access/learning eligibility and conditional venue audit |
| Classes and skill caps | Paged family comparison grid, race mastery, HP/SP growth and exact-rank trainer searches | Same shared registry | Promotion quest/branch conditions, starting choices and class-history discoveries |
| Recruitable companions | Shared merged roster reference | Roster biography/stats and recruited-only Discovery | Recruitment locations/conditions and current recruitment state |
| Arcomage/Arena tracking | Existing campaign counters and linked taverns | Existing campaign counters and linked taverns | Visual checklist polish and wider save-state audit |
| Custom markers and notes | Native place/edit/delete, optional dungeon height, All/Party assignment and shared Map Book state | Same shared notes and canonical snapshot ownership | Marker styles, richer editing and Android/controller checks |
| Optional Atlas mod | Profile-controlled native UI module | Same shared module | Launcher toggles and wider device checks |
| Localization parity | Current engine language | Current engine language | Integrate engine localization; no copied guide translation bundle |

Completion requires the missing native functions, data validation against the active game packages, meaningful
unit/runtime checks and visual checks. A placeholder page or a list of unrelated journal text is not a completed
quest guide. Reference data must describe the active merged/modded game, rather than silently assert vanilla rules.

Checkpoint evidence and implementation details are recorded in [ATLAS.md](ATLAS.md). All tests use isolated settings,
saves and captures. Original user campaigns and the official installation remain outside test writes.
