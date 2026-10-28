# Interactive Atlas MM7 and MM8: coverage and completion work

Active objective: complete native interactive atlases for MM7 and MM8, using shared game/world data.
The MM8 world-map slice is a checkpoint, not completion of this objective.

Scope control (2026-10-26): this matrix inventories implemented behavior and possible gaps; it does not
authorize every suggested extension. Finish the agreed map/discovery/navigation functions and their fixes first.
New ideas not originally agreed require the user's decision in [ATLAS_DECISIONS.md](ATLAS_DECISIONS.md).
Pending ideas are not additional completion gates. In particular, party spell-learning assessments, availability
forecasting, extra marker styles and replacement Jadame artwork are deferred pending approval.

Current acceptance priorities:

1. Correct MM7/MM8 map identity, region/dungeon navigation, search and filters.
2. Discovery-only terrain and pins by default; Guide remains an explicit opt-in and must not teach discoveries.
3. Party position/direction, small markers and usable dungeon floor selection.
4. Notes, observations and mod enable/disable behavior that preserve existing saves and gameplay state.
5. Shared UI checks on Android and controller, including a real-device check before claiming device acceptance.

Controller implementation (2026-10-27): the initial audit found no native gamepad input. The shared input path
now initializes SDL gamepads and owns menu focus, accept/back, an internal pointer and wheel scrolling. North
opens the existing Map Book action outside UI; the optional Atlas is entered through its normal sidebar button.
Six controller/navigation cases and seven keyboard/touch regressions pass 198 assertions. Five native UI tours
pass 166 steps with 29 fresh captures across MM7/MM8, keyboard submission and the Atlas-disabled profile;
post-UI save verifiers pass another 50 steps. The user confirms physical-pad Map Book opening and Atlas controls
in MM7. Their requested D-pad focus in the ordinary Map Book sidebar is implemented, with South selecting the
highlighted item and the disabled Atlas omitted; its physical follow-up remains pending. Full gameplay controller
profiles and Android acceptance remain outside these completed UI checks.

The 2026-10-27 local ARM64 Release 0.2.5 / 108 packages those shared input/search fixes and passes package,
source-data, stable-signing and 16 KiB alignment checks. Cached-runtime artwork decodes for all 26 outdoor
regions (13 per world), with corresponding ODM assets present. This is availability coverage, not a complete
visual calibration audit. ADB lists no device, so POCO installation/acceptance remains pending; see
`ATLAS_ANDROID_QA.md` for artifact hashes and the remaining physical gates.

The subsequent USB update installs and starts that verified 0.2.5 / 108 package on POCO. Installed APK hash,
non-debuggable flags, OpenGL ES startup and both modules are confirmed. Fresh before/after records preserve the
official package path and all three official/two Extended saves. The Map Book/arrow/Guide phone follow-up is
pending; installation/startup alone does not close the remaining touch, MM8 or discovery acceptance gates.

The 2026-10-26 Windows Release unit recheck passed 14 focused cases / 996 assertions for fog, party projection,
discovery filtering, MM7/MM8 identity, dungeon reveal/layers and note ownership/projection. The separate current
Castle Harmondale fountain check passed 1 case / 47 assertions against the mounted runtime, including all six
water-face bindings. That previously repaired payload issue is not an outstanding defect. These focused checks
do not certify all map content or replace device QA. ADB reported no connected device at this checkpoint.

The later ARM64 Android Release package gates passed for source `e7abdcd`; installation and physical device
acceptance are tracked separately in [ATLAS_ANDROID_QA.md](ATLAS_ANDROID_QA.md). A successful CI build alone
does not close those device checks.

The 2026-10-27 Map Book follow-up passed four current Windows native UI tours: note height/edit/delete and
save/load in Temple of the Moon (MM7) and Abandoned Temple (MM8), plus shared touch-list gestures in each world.
There are 276 UI steps and 47 fresh captures; selected states were visually inspected. Matching map/detail
viewports prove dragging from a pressed row does not accidentally open a region or select a site. Fresh authored
item/chest preparation and post-UI verifiers passed another 67 scenario steps, preserving the checked source
discoveries, clearing deleted-note ownership/elevation, and retaining MM8's stored chest item and unrecruited
Simon flag. This checks those records and samples, not the entire save, every dungeon floor or physical gestures.
Reproduction instructions are in `tests/fixtures/atlas/README.md`. POCO has verified Release 0.2.3 / 106;
new Map Book/arrow physical acceptance and remaining Android/controller gates remain separate.

The subsequent packaged-dungeon audit passes for all 110 MM7/MM8 BLVs in the active merged MapStats: 63 MM7
and 47 MM8, with 277/216 partial-discovery height slices and no authored blank plans. The focused Windows suite
passes seven cases / 4,334 assertions. It verifies hidden geometry, Guide knowledge immutability, stable
projection between bands and exact recomposition of their raster. Its knowledge states are synthetic; semantic
stacked-floor separation, campaign exploration and device input remain separate checks. Reproduction is in
`tests/fixtures/atlas/README.md`.

The follow-up authored-surface audit finds 17 candidate maps whose overlapping surfaces can share one 512-unit
band. Concrete outline witnesses occur at Z 0/-240 in Castle Gloaming and -384/-768 in Plane of Earth. Four
native diagnostic poses confirm the current band selections and preserve identical Discovery map/list viewports
after Guide. The candidates are not all proven traversable floors. This identifies a precision limitation;
it does not close stacked-floor separation. The associated tour-helper keyboard-count fix passes 118 keyboard
UI steps / 20 captures and 40 post-UI state checks. Details and evidence paths are in `ATLAS.md`.

The 2026-10-28 fix refines a band into four 128-unit slices where already-known horizontal support faces overlap.
It separates both confirmed witnesses, keeps indoor surface pins at their base and preserves unchanged outdoor
centroids. A focused package recheck passes all 110 BLVs and 807 partial-knowledge slices; Guide refines 131 bands
across 80 maps, including platforms. Ten native tours pass 450 steps / 71 captures and six post-UI verifiers pass
93 steps. Refined-note ownership/elevation survives save/load, and the four diagnostic Discovery map/list views
are unchanged after Guide. This improves height filtering; semantic storeys, slopes and moving geometry are not
certified by it. The subsequent local Android Release 0.2.6 / 109 packages this fix and passes package,
source-data, persistent-signing and 16 KiB alignment checks. ADB lists no device, so installation and physical
acceptance of that package remain pending. Details are in `ATLAS.md` and `ATLAS_ANDROID_QA.md`.

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
| Dungeon plans from engine geometry | Adaptive known height slices, party-height selection, component-aware discovered pins and height-aware notes | Same shared projection and controls | Sloped/mechanism-driven geometry and wider floor/visibility/device audit |
| Buildings, resident NPCs, trainers | Current/observed residents, Guide-only declared possible residences, indoor service markers and multiple physical entrances per canonical service | Same shared index; actual Adventurer's Inn discovery and Dagger Wound/Ravenshore entrances | Conditional service eligibility, wider scripted placement audit and generated/roaming NPCs |
| Trainer requirements and prices | Gameplay evaluator, observed offers and race-specific mastery comparisons | Same shared evaluator | Conditional eligibility audit and starting-skill/race-selection checks |
| Walking and scheduled transport | Directed walking/coach/boat graph, departure pins, last-offered schedules/fare/duration, live local overrides and destination navigation | Same shared knowledge/navigation | Remote scripted timetable references, availability forecasting and arrival-point links |
| Quest giver/objective/return chains | Shared registry, 76 quests, conditional clues and possible contact-location links | Shared registry, 69 quests, repaired Accuracy state, stage-sensitive Aislen/Pole delivery contacts and possible contact-location links | Remaining conditional stages and wider campaign audit; unused legacy MM8 rows 5/172 are not invented playable chains |
| Chests, wells, shrines, obelisks and interactive objects | Event-backed Sites, inherited object bindings, last chest inspection, known empty status, learned journal notes, actual observed numeric/condition/spell/item outcomes, confirmed inherited-object deliveries, conditional drinks, journal-bookkeeping reduction, typed statue/keyed-entrance references and initial Guide references | Same shared references and per-object variant history | Deferred effect attribution and queued-delivery quantities, unidentified spell sources, item transformations, unsupported reference effect/predicate families, six base-site bindings from the site-reference audit, broader special-interaction coverage and wider content audit |
| Monster groups and Bestiary | Package-authored placements, encounter areas, callback-owned scripted possibilities including the current Tularean battle, named and passive sprite/corpse observations, variant navigation, Bestiary map links and typed corpse/ground-loot references | Same shared index, local scripted possibilities and observed-only Bestiary | Party-relative scripted sources, replacement-model sight, wider visibility/content audit |
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

The 2026-10-27 replacement-model sight audit found no `models/actors.yml` in the active MM7/MM8 runtime
packages. Actor model replacements are currently experimental, behind the default-off `debug.actor_models`
setting; normal sprite observations remain the active path. The model observation limitation is retained in the
matrix as a conditional gap and recorded for a later decision in `ATLAS_DECISIONS.md`; no model hook was added.

The 2026-10-28 house audit now checks projected Atlas locations against physical base callbacks on 137 packaged
maps, covering 673 house IDs and excluding transition cards from usable-venue checks. Its one shared service,
Adventurer's Inn 1607, retains both Dagger Wound and Ravenshore entrances. The shared index no longer drops an
entrance because its map differs from the table's primary address. Additional addresses require their own saved
interaction in Discovery; native selection, labels and search use the known local address. Guide omits unused
building rows after their map's actual bindings are indexed. The Inn's special roster overlay now records a real
house visit through the ordinary discovery key.

The Windows suite passes 112 cases / 13,051 assertions. Four native tours pass 118 UI steps / 24 captures;
post-UI verifiers pass 41 steps and two fresh real-door fixture runs add 18 steps. Both visited and unvisited
Discovery map/list crops remain identical after Guide. This validates the shared-house fix and sampled UI/save
paths, not every conditional placement or physical Android input. It is newer than the prepared 0.2.6 APK.

The subsequent native regional check covers all 26 declared outdoor maps, thirteen per world. Map/list RGB
crops remain identical after Guide and after loading the resulting quicksave in a fresh native process.
The run passes 572 UI steps / 130 fresh captures, 104 post-UI state steps and 79 fixture steps. Contact sheets
show all regional artwork/titles/arrows. Reading 52 saves verifies that no extra cached map keys or remote
global map visits appear; ordinary initial rendering reveals only the local terrain (149/315 full/partial cells).
This strengthens regional rendering/persistence coverage without certifying every pin or campaign state.
Shoals requires the real wetsuit; Barrow Downs uses the authored Stone City exit to avoid a colliding centre
spawn. Reproduction and the precise limits are recorded in `ATLAS.md` and the fixture README.

Local Android Release 0.2.7 / 110 now packages the shared-house/Inn fixes, retaining adaptive height slices.
Its package/source-data, stable-signing and 16 KiB alignment gates pass. ADB and a targeted Windows PnP check
find no phone, so installation and physical device acceptance remain open; the last verified installed version
is 0.2.5 / 108. Artifact hashes and exact package evidence are in `ATLAS_ANDROID_QA.md`.

Completion requires the missing native functions, data validation against the active game packages, meaningful
unit/runtime checks and visual checks. A placeholder page or a list of unrelated journal text is not a completed
quest guide. Reference data must describe the active merged/modded game, rather than silently assert vanilla rules.

Checkpoint evidence and implementation details are recorded in [ATLAS.md](ATLAS.md). All tests use isolated settings,
saves and captures. Original user campaigns and the official installation remain outside test writes.
