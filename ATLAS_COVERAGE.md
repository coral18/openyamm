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
| Dungeon plans from engine geometry | Adaptive known height slices, clipped known walkable ramp edges, party-height selection, component-aware discovered pins and height-aware notes | Same shared projection and controls | Semantic storeys and wider floor/visibility/device audit; moving geometry awaits user decision |
| Buildings, resident/actor/scripted NPCs, trainers | Current/observed residents, Guide-only possible residences/actor placements/map encounters, last-encounter actor positions or script maps, generated pedestrian/tavern-recruit identity snapshots, indoor service markers and exact physical-address history per canonical service | Same shared index; actual Adventurer's Inn discovery and Dagger Wound/Ravenshore entrances | Conditional service eligibility, handwritten/computed NPC calls, and wider scripted placement audit |
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
| Recruitable companions | Shared merged roster, live party/Inn status, actual generated campaign profiles and recorded invitations | Same shared status, Guide dialogue/unlock contacts and physical rescue links | Complete recruitment eligibility, Inn venue navigation and wider contact audit |
| Arcomage/Arena tracking | Existing campaign counters and linked taverns | Existing campaign counters and linked taverns | Visual checklist polish and wider save-state audit |
| Custom markers and notes | Native place/edit/delete, optional dungeon height, All/Party assignment and shared Map Book state | Same shared notes and canonical snapshot ownership | Marker styles, richer editing and Android/controller checks |
| Optional Atlas mod | Profile-controlled native UI module | Same shared module | Launcher toggles and wider device checks |
| Localization parity | Active engine English data and original native UI text | Same shared English registries/UI | Engine-wide language selection/translation infrastructure requires a separate decision; no copied guide translation bundle |

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

The subsequent hierarchy audit repairs 25 missing MM7 primary dungeon parents using the existing presentation
schema, including the full Barrows/Zokarr cluster and the Hidden Tomb. Active geometry/scene/overlay checks
cover 27 outdoor maps, 78 single-outdoor-region dungeon entries and one multi-region entry. All 70 explicit
MM7/MM8 parents have an authored connection path. The focused check passes 1,554 assertions; the broader
World Database/Guide/interaction suite passes 127 cases / 46,013 assertions. Four native navigation/privacy
cases pass 132 UI steps / 24 captures, 40 post-UI steps and four fixture steps. Eight inspected saves retain
their source map/XY, add no indoor snapshots and keep the browsed dungeon unvisited. These data changes are
newer than the prepared Android 0.2.7 APK. Details and deliberate non-geographic parent exceptions are in `ATLAS.md`.

The subsequent Android Release 0.2.8 / 111 packages these 25 MM7 parents and all preceding height/shared-house
fixes. Package/source-data, stable-signing and 16 KiB alignment checks pass. The in-place USB update on POCO
succeeds; installed APK hash, non-debuggable flags, OpenGL ES startup and both modules are verified. Fresh
before/after snapshots preserve the official package path and all three official/two Extended save hashes.
MM8 touch/fog/arrow and Guide-to-Discovery privacy acceptance on this package is pending the user's test;
installation/startup checks do not close those physical gates. See `ATLAS_ANDROID_QA.md` for exact evidence.

The user subsequently confirms MM8 discovery fog and the party arrow on installed POCO 0.2.8. Guide roundtrip,
wider touch input, dungeon heights and broader physical coverage remain separate checks.

The following shared-data audit adds seven missing requested-item relationships, bringing the registry to
145 quests / 602 relationships. Same-name stone/bow/book variants follow the actual grant/consumption paths;
accepted clues do not identify unseen items. The shared Guide suite passes 47 cases / 1,847 assertions, and
actual MM7/MM8 request/save/load scenarios pass 22 steps. This data update is newer than Android 0.2.8;
the detailed provenance and reproduction paths are in `ATLAS.md`.

The full active quest-link audit subsequently indexes all 137 MM7/MM8 maps and their overlays, then resolves
579 emitted clickable links across all 145 quests. It repairs two Thomas Grey return targets, restores the
Stone City throne-room metadata removed by its script replacement, and corrects Kastore's stale Emerald Island
address to Castle Gloaming in the shared house table. The actual throne-room gate still rejects locked access
and enters house 216 only after its existing condition is met. The final shared suite passes 129 cases /
49,618 assertions. Conditional campaign stages and secondary house branches remain outside this resolution
check; these fixes are newer than Android 0.2.8. `ATLAS.md` records the scope and retained failure evidence.

Shared entrances now accept authored house alternatives, covering Castle Gloaming's councils and all three
Harmondale Arbiters. Seventeen Balthazar named/service venues now point to usable destinations instead of the
generic flood dialogues. Actual visits gate Discovery independently; visible resident names distinguish shared
house variants and participate in search. Local council-contact declarations make Kastore available in the
selected dungeon's Guide index. The final shared suite passes 131 cases / 49,946 assertions; literal-catalog
lighting checks pass 26 tests. Three AMD/OpenGL tours pass 62 UI steps / nine captures, eleven setup and nineteen
post-UI steps. Discovery map/list crops match after Guide and checked venue flags remain unset. Other conditional
locations, physical Android/controller checks and the broader content matrix remain open. These source fixes
are newer than Android 0.2.8; details and reproduction are in `ATLAS.md` and the fixture README.

The 2026-10-29 cross-world venue fix adds Tatalia's actual shared Inn entrance without changing service
1607's MM8 primary address. Physical sites can project a foreign shared venue and its residents into a local
database; each additional entrance still needs its own saved interaction in Discovery. The final shared suite
passes 133 cases / 50,009 assertions. A 13-step actual-door/save fixture and two AMD/OpenGL tours (40 UI steps,
six captures, eight post-save steps) verify the unvisited/visited states. Discovery map/list crops match after
Guide in both states. This is newer than prepared Android 0.2.9 and installed 0.2.8; phone work is deferred by
the user. Wider content and physical acceptance remain open. Reproduction and evidence limits are in `ATLAS.md`.

The subsequent active house-binding audit follows normal scene and event-overlay loading for all 137 maps.
It verifies all 693 declared physical house bindings / 674 services against exact site discovery keys and
checks independent knowledge at three additional-address bindings. Its focused case passes 5,387 assertions.
Undeclared callbacks and conditional eligibility remain outside this binding-resolution check.

The 2026-10-29 native dungeon navigation pass opens all 110 plans (63 MM7 / 47 MM8) through normal search and
result selection. Two AMD/OpenGL tours pass 1,028 UI steps / 114 fresh captures, four fixture and 114 post-save
steps; seven contact sheets cover all titles/plans. Source-continent map/list RGB crops match after the full
Guide browse. Four inspected saves add no indoor caches, remote map visits or outdoor cache keys, retaining
source XYZ, gold and food; only the normal current-region canonical flag initializes. This is all-height
navigation/rendering coverage, not every floor, campaign stage or physical device. The reusable preparation
tool and exact limits are documented in `ATLAS.md` and the fixture README.

The subsequent ramp fix corrects midpoint-only height assignment for known walkable floor edges. Their
original raster samples now follow endpoint heights across intersected slices; walls/ceilings retain their
metadata. The final shared suite passes 137 cases / 56,355 assertions. All 110 BLVs pass exact recomposition
across 817 partial-knowledge slices with no blank authored plans. Real Gloaming/Earth supporting-floor tests
and four native before/after tours (112 UI steps / 24 captures, six fixture, twenty post-save steps) check
filtered changes, identical All heights crops and unchanged Discovery map/list crops. Eight save inspections
retain checked map/resources/cache keys and independent discovery. These are sampled height/state checks;
semantic storeys, mechanisms and physical acceptance remain open. This fix is not in Android 0.2.9 or 0.2.8.

The 2026-11-06 shared-house address fix also prevents an actual visit at another entrance from revealing the
unvisited primary address. Canonical address history belongs to Core and is captured with Atlas disabled.
Legacy global-only saves retain their former primary-address interpretation and migrate only during a real
visit; old ambiguity cannot be reconstructed. Four AMD/OpenGL tours pass 72 UI steps / 12 captures with equal
Discovery map/list crops after Guide; eight inspected saves preserve address history, resources and cached maps.
Four actual-door/save fixtures pass 51 steps across MM8, Atlas-off, legacy and cross-world Tatalia cases.
The focused suite passes 311 cases / 69,766 assertions, excluding two independently reproduced pre-existing
data/environment failures documented in `ATLAS.md`. No Android installation or new physical-device acceptance is claimed.

The subsequent Windows cache-parity fix overlays the current NPC topic table; its only differing row is the
missing Verdant travel topic 10000. That shared dialogue test now passes, and the effective table matches
source bytes. The expanded suite passes 312 cases / 69,790 assertions with only the pre-existing retired-source
portrait case excluded. This corrects the development payload, without downloading full world packages or
changing dialogue behavior; details are in `ATLAS.md`.

The 2026-11-06 conditional-contact acceptance checks Knight/Dragon quests 70/74 through actual alliance
handoffs, picked camp exits, Council resident encounters, final turn-ins and save/load (118 strict steps,
with explicit prerequisite/item fixtures). Four AMD/OpenGL active/history tours pass 108 steps / 24 captures;
two link-selection tours add 40 steps / four captures and select the correct Council NPC. Guide-roundtrip
Discovery crops match in all four cases. Six post-UI verifiers pass 186 steps; twelve inspected saves retain
checked quest/contact/address state, resources, inventory/health and cache sets. Remote-only controls retain
no residence. This closes those sampled relocation stages, not every campaign branch or device gate.
Reproduction and explicit limits are in `ATLAS.md`, the quest audit and the fixture README.

The current PC optional-mod/save requirement is rechecked against the newer Inn-address and Council-contact
history. Two campaigns pass Atlas on -> off -> on across six native AMD/OpenGL processes (122 UI steps /
18 captures). Map Book retains the notes with Atlas absent, including disabled-profile quickload; restored
note editors and map/list crops match. Six fresh verifiers pass 150 steps / twelve save inspections, retaining
note ownership, checked addresses/quest flags, resources, inventory/health and cache sets. Each next input is
the preceding quicksave; no campaign is regenerated between profile phases. This is sampled PC state evidence;
physical Android/controller acceptance remains pending. Reproduction and limits are in `ATLAS.md`.

The 2026-11-06 named-actor NPC fix closes the house-only indexing gap for fixed registry NPCs. Guide uses
fresh authored placement possibilities; Discovery saves current actor feet coordinates only on a successful,
matching actor dialogue. Remote conversations do not supply coordinates, and old saves receive no guessed
positions. All 137 active companions and quest/companion link construction pass the expanded 318-case /
71,331-assertion regression set (the existing retired-source portrait test remains excluded). Malwick/S'ton
actual-interaction/save fixtures pass 42 steps. Eight native AMD/OpenGL tours pass 150 steps / 26 captures;
sixteen inspected saves retain checked state and twelve Guide-roundtrip map/list crops match. Points/list
entries select the correct last-encounter details. Wider scripted placement, generated NPC identities and
physical-device acceptance remain open; this change is newer than the deferred Android packages.

The 2026-11-07 scripted-contact checkpoint adds map-level encounter knowledge without fabricating actor XYZ.
Local callback provenance is recorded only when dialogue opens; global/remote calls cannot supply a map.
Regenerated MM7/MM8 references are tied to their active handlers, with 58 surviving contacts audited across
all 137 maps. Guide searches progressively index dungeon data and can find these contacts from the overview.
The expanded 379-case / 73,976-assertion Windows set passes with the existing retired-source portrait exclusion.
Simon first-entry/save-load and the existing blackmail control pass 50 steps. Three native AMD/OpenGL runs pass
67 steps / 12 captures across Simon and Faerie King; six save inspections preserve checked state and six
Discovery map/list crops match after Guide. Wider conditional/handwritten NPC paths, generated identities and
device acceptance remain open. Reproduction and the headless pending-dialogue correction are in `ATLAS.md`.

Completion requires the missing native functions, data validation against the active game packages, meaningful
unit/runtime checks and visual checks. A placeholder page or a list of unrelated journal text is not a completed
quest guide. Reference data must describe the active merged/modded game, rather than silently assert vanilla rules.

Checkpoint evidence and implementation details are recorded in [ATLAS.md](ATLAS.md). All tests use isolated settings,
saves and captures. Original user campaigns and the official installation remain outside test writes.


The 2026-11-07 generated-pedestrian checkpoint adds immutable per-map/actor encounter snapshots, so reusable
NPC slots cannot rename or relocate previously met people. It preserves the version 84 core payload and uses
a validated namespaced save trailer. Jo's actual conversation/save/load passes nine steps. Three native PC
runs pass 78 UI steps / 15 captures; six save inspections retain encounter state and six map/list crops match
after Guide. The focused regression set passes 447 cases / 76,704 assertions with the documented portrait
exclusion. This is actual MM7 pedestrian evidence plus shared MM7/MM8 identity/save logic coverage, not proof
that every MM8 generic role is a generated individual. Generated tavern mercenaries and wider conditional NPC
paths remain open; Android work remains deferred.


The subsequent generated-mercenary checkpoint records actual resident encounters per canonical map/slot and
keeps reserved NPC/roster templates out of ordinary person records. Companions show the campaign character's
name/class/level. The unchanged version 84 core payload is extended by validated mercenary packets alongside
existing namespaces. Real MM7/MM8 door, dialogue and recruitment fixtures pass 26 steps; eight native processes
pass 165 UI steps / 27 captures, including seed preparation. Fourteen save inspections retain checked knowledge
and campaign state; twelve body crops match after Guide. The focused Windows suite passes 453 cases / 76,824
assertions with the established portrait exclusion. Wider eligibility/contact/content checks remain open;
Android remains deferred.


The 2026-11-07 source comparison authenticates the MM7 base archive against its local GOG manifest and finds
no direct 202 binding there either. The installed Merge out12 map is byte-identical to the active port map.
Three installed MM8 variants also lack the five target bindings, but that Merge installation's available hash
database does not authenticate its games archive as pristine MM8. Nine decoded variants and four Git/runtime
fingerprint matches are recorded in `tests/fixtures/atlas/well_source_provenance_20261107.json`. This resolves the missing
MM7 source-input evidence without inventing a location or claiming all original interpreter behavior proved.
Attaching unplaced callback bonuses to selected objects is a separate world-content decision, not an automatic
Atlas mapping repair. Demonstrated import/dispatch bugs remain in the authorized correction scope.

The remaining-use follow-up explains the three placed Emerald Island wells `7out01:112/113/114`: +5 HP,
+5 SP and +2 permanent Luck below 15, with their shared source-use limit. Guide references are generated
without changing callbacks or disclosing them in Discovery. The Windows suite passes 455 cases / 77,112
assertions with the established portrait exclusion. A 16-step native tour and four captures verify the Luck
reader and unchanged Discovery map/list after Guide. The wider drink audit still has 19 placed callbacks
without a complete effect reference; no complete Atlas acceptance or new Android delivery is claimed.
