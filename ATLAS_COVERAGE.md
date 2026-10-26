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
| Dungeon plans from engine geometry | Known height bands, party-height selection, filtered pins and height-aware notes | Same shared projection and controls | Complex floor separation and wider floor/visibility audit |
| Buildings, resident NPCs, trainers | Current/observed residents, Guide-only declared possible residences, indoor service markers | Same shared index | Conditional service eligibility, shared-service placements and generated/roaming NPCs |
| Trainer requirements and prices | Gameplay evaluator, observed offers and race-specific mastery comparisons | Same shared evaluator | Conditional eligibility audit and starting-skill/race-selection checks |
| Walking and scheduled transport | Directed walking/coach/boat graph, departure pins, last-offered schedules/fare/duration, live local overrides and destination navigation | Same shared knowledge/navigation | Remote scripted timetable references, availability forecasting and arrival-point links |
| Quest giver/objective/return chains | Shared registry, 76 quests, conditional clues and possible contact-location links | Shared registry, 69 quests, repaired Accuracy state, stage-sensitive Aislen/Pole delivery contacts and possible contact-location links | Remaining conditional stages and wider campaign audit; unused legacy MM8 rows 5/172 are not invented playable chains |
| Chests, wells, shrines, obelisks and interactive objects | Event-backed Sites, inherited object bindings, last chest inspection, known empty status, learned journal notes, actual observed numeric/condition/spell/item outcomes, confirmed inherited-object deliveries, conditional drinks, journal-bookkeeping reduction, typed statue/keyed-entrance references and initial Guide references | Same shared references and per-object variant history | Deferred effect attribution and queued-delivery quantities, unidentified spell sources, item transformations, unsupported reference effect/predicate families, six base-site bindings from the site-reference audit, broader special-interaction coverage and wider content audit |
| Monster groups and Bestiary | Package-authored placements, encounter areas, named and passive sprite/corpse observations, variant navigation, Bestiary map links and typed corpse/ground-loot references | Same shared index and observed-only Bestiary | Scripted spawn declarations, replacement-model sight, wider visibility/content audit |
| Teleports and special connections | Declared links, canonical cross-world arrival history, last-offered/shared authored gate destinations, exported multi-branch dungeon choices, Barrows connection graph, exported quest/item, Barrow lever and random-exit routing requirements and origin/destination navigation | Same shared gate/history data, Regna connection graph, initial exported item routing requirements, campaign-persistent submarine unlocks, keyed-entrance references, explicit elemental-prison groups and height filtering | Full conditional/proximity/Gate and handwritten-reference audit, remaining condition families/live eligibility, remaining entrance-component grouping and wider graph/content audit |
| Items, artifacts and equipment reference | Fixed loot, declared conditional rewards including selected timers/on-load callbacks and MM8 gem exchanges, current NPC-topic sources, typed recovered corpse/ground items, inherited-object reward provenance, configured death-drop links, item art, pinned base-item comparison and identification-aware references | Same shared index and discovery history | Remaining handwritten reward declarations, guaranteed scripted drops, typed semantic-source audit and wider observation/content audit |
| Alchemy recipes and ingredients | Recursive recipes, direct alternative ingredient links and creature-source navigation | Same shared rules and discovery gates | Broader canonical-content audit |
| Spellbook reference and guild links | Spell/book navigation, observed shelf history and possible guild links from shared stock rules | Same shared observations and discovery filtering | Access/learning eligibility, conditional venue and wider content audit |
| Classes and skill caps | Paged family comparison grid, race mastery, starting-skill profiles, HP/SP growth and exact-rank trainer searches | Same shared registry | Promotion quest/branch conditions, starting stats/portrait eligibility and class-history discoveries |
| Recruitable companions | Shared merged roster, live party/Inn status and recorded invitations | Same shared status, Guide dialogue/unlock contacts and physical rescue links | Complete recruitment eligibility, Inn venue navigation and wider contact audit |
| Arcomage/Arena tracking | Existing campaign counters and linked taverns | Existing campaign counters and linked taverns | Visual checklist polish and wider save-state audit |
| Custom markers and notes | Native place/edit/delete, optional dungeon height, All/Party assignment and shared Map Book state | Same shared notes and canonical snapshot ownership | Marker styles, richer editing and Android/controller checks |
| Optional Atlas mod | Profile-controlled native UI module | Same shared module | Launcher toggles and wider device checks |
| Localization parity | Current engine language | Current engine language | Integrate engine localization; no copied guide translation bundle |

Completion requires the missing native functions, data validation against the active game packages, meaningful
unit/runtime checks and visual checks. A placeholder page or a list of unrelated journal text is not a completed
quest guide. Reference data must describe the active merged/modded game, rather than silently assert vanilla rules.

Checkpoint evidence and implementation details are recorded in [ATLAS.md](ATLAS.md). All tests use isolated settings,
saves and captures. Original user campaigns and the official installation remain outside test writes.
