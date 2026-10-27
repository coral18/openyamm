# Atlas fixtures

## Controller UI route

`tools/menu_tours/atlas_gamepad.yml` runs on an isolated Harmondale or Dagger Wound before-save with the Atlas
module enabled. It creates a process-local SDL virtual controller, opens Map Book with North/Y, focuses the
ordinary Atlas sidebar button with five D-pad Down presses and selects it with South/A. It moves the cursor through an SDL
axis, uses D-pad focus and South to open Guidebook, and cancels a text field and leaves the Atlas with East/B.
The pointer targets are calibrated for reproducibility; button/axis input travels through the shared input
system. The eight captures include the original Map Book, highlighted Atlas, moved pointer, Guidebook and return. Inspect them;
input-step completion alone does not prove the intended target was reached.

The `mm7_search_privacy.yml` / `mm8_search_privacy.yml` post-UI verifiers also apply here, with the same isolated
before/quicksave paths and cleared campaign flags. They check those fields, not every saved record. Keyboard
search tours should still pass with the new input path. Unit cases use real SDL virtual devices to cover short
taps, pending-key isolation, resizing, neutral barriers, mouse takeover and unplug cancellation. Virtual-device
results are not physical controller acceptance and do not validate a complete gameplay control profile.

For the Atlas-disabled control, enable only `karol.test` in a separate private `mods/profile.yaml`, open Map Book
with North, press Down five times and South. Close must be highlighted instead of the absent Atlas, and South
must return to gameplay. Inspect the highlighted menu and returned gameplay captures. The same shared focus
path supports Tab/Shift+Tab and Return/Space; mouse or touch input clears the sidebar focus.

## Search submission and Map Book return

Run `tools/menu_tours/atlas_search_submit.yml` with an isolated Harmondale (MM7) or Dagger Wound (MM8) save as
`saves/before.oysav`, with no open dialogue or held item. Use a new campaign with MM7 QBit 760 / MM8 QBit 403
clear for the accompanying state verifiers. The tour enters through Map Book, submits both map and Guidebook
queries, checks that the real SDL text-input session closes, re-enters through keyboard focus, cancels editing
and returns to Map Book/gameplay before quicksaving. It captures five states; inspect the field and Map Book
captures. `no_text_input` is an SDL session check, not an assumption that an Enter press committed the field.

Run `mm7_search_privacy.yml` or `mm8_search_privacy.yml` afterward from the same isolated directory. Each checks
the expected region and campaign flag before/after the UI and another save/load. These ten-step verifiers check
those fields, not the entire campaign or discovery snapshot. The 2026-10-27 native MM7 run failed the session
check before the fix; both worlds pass afterward. This verifies keyboard navigation, not native gamepad input.

## Packaged dungeon projection and discovery audit

The opt-in test `packaged MM7 MM8 dungeon layers preserve discovery projection and privacy` reads every MM7/MM8
BLV listed by the active merged MapStats through the normal package filesystem. Set
`OPENYAMM_REGRESSION_ASSETS_ROOT` to the complete runtime assets and select that test in the unit binary, or use
`tools/extended_windows_dev.sh test '--test-case=packaged MM7 MM8 dungeon layers*'` with the same environment.
Without the environment variable, the test explicitly reports that the package audit was not run.

The unknown/partial knowledge states are deliberately synthetic; no user save is read or written. Guide browsing
must not modify knowledge, unknown geometry must stay absent, and partial-discovery height slices must retain
one projection and recompose the exact all-heights raster. The current Windows audit covers 63 MM7 and 47 MM8
dungeons with 277/216 partial-discovery slices and no authored blank plans. This does not prove campaign
exploration, physical Android/controller input, or semantic separation of rooms sharing one 512-unit band.

## Scripted monster reference privacy

`tools/menu_tours/atlas_scripted_spawns.yml` enters through Map Book on a private Harmondale or Dagger Wound save,
searches Sites for scripted sources in Discovery, opens Guide, reads a source, returns to Discovery and quicksaves.
Inspect all four captures: the sources are Guide possibilities; the before/after Discovery map and list must
remain unchanged. `atlas_scripted_spawns_tularean.yml` separately browses the current six shared-script battle
groups, including the corrected active-game soldier count. Neither tour activates a source callback.

For the Abandoned Temple tour, use the `item-source.oysav` prepared by `mm8_item_sources.yml` as private
`saves/before.oysav`, then run `atlas_scripted_spawns_mm8_dungeon.yml`. It dismisses Simon's dialogue and stores
the actual held chest item before entering the ordinary Map Book. Guide selects All heights to include the
authored sources; Discovery must still hide unobserved groups and unexplored outlines.

Run `mm7_scripted_spawn_privacy.yml` against the Harmondale before/final quicksave, or
`mm8_scripted_spawn_privacy.yml` against the prepared dungeon before/final quicksave. They verify the source
quest/use flags and another roundtrip; MM8 also retains the original chest record and stored item. These check
the specified fields and do not certify the whole save or Android/controller input.

## Map Book entry, note heights and deletion regression

Prepare fresh isolated saves with `tests/scenarios/atlas/mm7_creature_item_source.yml` and
`tests/scenarios/atlas/mm8_item_sources.yml`, using the mounted current runtime. Their authored item/chest
interactions establish the source-discovery records checked by the post-UI verifiers. Do not substitute the
older floor-plan pose-only fixtures: those do not contain these item/chest records.

The scenario runner writes saves below its process temporary directory, under
`openyamm_scenario_saves/atlas_mm7_creature_item_source/` and `atlas_mm8_item_sources/`. Copy
`taken-creature-item.oysav` (MM7) or `item-source.oysav` (MM8) into the corresponding private UI run as
`saves/before.oysav`. Keep the two runs and their writable save directories separate.

Run `tools/menu_tours/atlas_note_heights.yml` for MM7 or `atlas_note_heights_mm8.yml` for MM8. Both enter
through Map Book, require the Atlas pause screen, create and rename a note on one height, switch heights,
save/reload, reassign All/Party height, delete and save/reload again. The MM8 source fixture retains Simon's
pending dialogue and the chest item on the cursor. Its tour dismisses the dialogue, stores the held item through
the single party portrait, and dismisses the authored dialogue after each reload. It does not recruit Simon or
alter his quest bit. An open dialogue or held item blocks the ordinary Map Book action.
Inspect the fresh captures for the actual text, height filtering and deletion. A completed input script alone
does not prove its clicks reached the intended screen.

Then run `tests/fixtures/atlas/mm7_note_heights_privacy.yml` or `mm8_note_heights_privacy.yml` against those private
directories. They check the initial save, final `saves/quicksave.oysav` and another roundtrip: the deleted user
note is inactive, its ownership/elevation entries are cleared, and the original authored source discovery remains.
These fixtures check those records, not every field of the save or physical Android/controller behavior.

The paired `atlas_touch_lists_mm7.yml` / `atlas_touch_lists_mm8.yml` tours cover recognized shared touch-drag
frames, including dragging from an initially pressed row, one-row movement and bottom scrolling. Use an isolated
outdoor save with no open dialogue for these tours. They do not deliver Android OS gestures or test a gamepad.

## Conditional trainer-note fixture

`Global_zz_hide_bow.lua` is QA-only. It hides the real Expert Bow topic (315) through the ordinary
`CanShowTopic` API. It must never be installed into the normal runtime or replace generated Lua.

Prepare a new isolated asset root on the same filesystem as the full runtime packages:

```bash
python3 tests/fixtures/atlas/prepare_assets.py --assets /path/to/runtime/assets --output /same-volume/hidden-assets
```

The large engine/world archives are hard links opened read-only by these tests; do not run package writers on
this fixture. Its mutable `assets.zip` is a private copy. The overlay is placed in the shared engine event namespace
and a receipt records its hash. Copy normal settings/mod profiles into an isolated working directory and set only
`[assets] root` there to this new asset root. Do not reuse user saves as test-write targets.

Run `tests/scenarios/atlas/mm8_hidden_trainer_note.yml` against that fixture. Run the paired
`mm8_trainer_observation.yml` against the unchanged runtime. Both use the real house door, resident selection and
save system. The visible control must record offer 315 and map note 1105; the hidden case must record neither,
including after save/load. This guards against revealing hidden trainers through the Map Book/Atlas notes layer.

## Legacy Pure Accuracy saves

Prepare another private package view with `--fixture legacy-pure-accuracy`. It shadows only the explicit
`worlds/mm8/events/Global_mm8_accuracy.lua` supplement with a comment; the generated legacy handlers remain intact,
including their original incorrect flag writes. This also works when the supplement lives in a cooked world ZIP.
The normal fixed asset root must fail the legacy fixture's deliberately old assertions.

```bash
python3 tests/fixtures/atlas/prepare_assets.py --assets /path/to/runtime/assets \
  --output /same-volume/legacy-accuracy-assets --fixture legacy-pure-accuracy
```

With that asset view and private settings/saves, run `mm8_accuracy_legacy_fixture.yml` and
`mm8_accuracy_legacy_blazen_fixture.yml` from this fixture directory. These use strict `faithful` execution but
explicit reagent/Gem inventory fixtures. The Blazen variant executes his actual Cure handler; it does not test
gathering, travel or the complete promotion chain. Each creates `legacy-active.oysav` and `legacy-completed.oysav`
under the platform temporary scenario-save directory, in its own scenario-ID directory.

Copy each pair into a separate private working directory's `saves/`, switch its asset setting back to the normal
fixed packages, then run the matching `mm8_accuracy_migration.yml` or `mm8_accuracy_migration_blazen.yml`.
The verifiers load active/completed/active saves in one process, checking state replacement, history, original
experience and reload idempotence. Keep the two pairs separate. No binary save or large archive belongs in Git.

For the initial compatibility check, original saves were captured with the pre-fix `0b47beb` executable and its
retained package overlay. The reproducible shadow fixture reconstructs that legacy quest state with a current
engine; it does not claim to substitute for an original-format/version golden when the save schema changes.
The fresh `mm8_accuracy_quest_knowledge.yml` scenario belongs to the normal fixed runtime. Its controlled partial
recipe must be refused without consuming the usable ingredients; one final reagent then permits the real reward.

## Possible NPC locations and Guide privacy

Use the normal fixed packages and an isolated working directory with the Atlas enabled. Run the strict
`mm7_npc_relocation.yml` scenario, then copy its temporary `npc-relocated.oysav` to private `saves/before.oysav`.
For MM8, use `mm8_automatic_npc_observation.yml` and its `automatic-npc.oysav` instead. Leave the originals intact.

Launch that private save at 1600x900 with the corresponding
`tools/menu_tours/atlas_mm{7,8}_possible_npc_locations.yml` as `debug.menu_input_tour_path`. Both tours browse Guide,
return to Discovery/gameplay and write private `saves/quicksave.oysav`. MM7 checks the Butler's current, possible
and last-met locations. MM8 scans the mounted sources, opens Quixote's possible council location and returns to
Discovery. Inspect the fresh native captures; existence alone does not prove the right view was rendered.

Run the matching `mm{7,8}_possible_npc_persistence.yml` from this fixture directory. Each loads both private saves
and round-trips the result, checking unchanged residence/knowledge. MM8 additionally checks selected alliance/
promotion flags, original XP and no NPC/house/map discovery from Guide scanning/navigation. Its initial new-game
step initializes the headless loader's NPC registry before importing saves; it does not provide the asserted
saved state. MM7's prerequisite/combat flags are explicit controlled fixtures in the original scenario, rather
than a test of the scavenger hunt or castle combat. These fixtures target the base campaign, not arbitrary NPC mods.

## Site journal references, cross-map persistence and privacy

Run `tests/scenarios/atlas/mm7_well_journal.yml` and `mm8_well_journal.yml` against the normal packages from
separate private working directories. They use real well targets in Harmondale / Dagger Wound Island, verify
Might +10 / Intellect +15 without repeated rewards, and round-trip saves and map changes. Repeat MM8 with a
private `mods/profile.yaml` enabling only `karol.test` to verify Core behavior with the Atlas module disabled.
Scenario saves are written beneath the platform temporary scenario-save directory, grouped by scenario ID.

For the learned-note UI tours, copy each scenario's `site-reused.oysav` to private `saves/before.oysav`; for the
MM8 unlearned tour use `before-site.oysav`. Keep the two MM8 cases in different working directories. Launch at
1600x900 with `tools/menu_tours/atlas_mm{7,8}_site_journal_known.yml` or
`atlas_mm8_site_journal_unlearned.yml` as `debug.menu_input_tour_path`. The tours save a private quicksave before
and after reading and capture both Guide and Discovery. Inspect the actual reading page: matching filenames
alone do not establish correct input or privacy. The unlearned tour explicitly clears the search before typing
again because the search persists when toggling Guide. It must show a description in Guide and no recorded
note in Discovery. The two known tours must retain their respective real well descriptions.

Run the corresponding `mm7_known_journal_privacy.yml`, `mm8_known_journal_privacy.yml` or
`mm8_unlearned_journal_privacy.yml` verifier here. Each loads before/after saves and another round trip, asserting
the exact note flag and character bonus. Browsing must not use the well, teach its effect or repeat its reward.

For migration acceptance, retain an actual pre-fix save with a learned well note as private
`saves/legacy-known.oysav`, then run `mm{7,8}_journal_migration.yml` with the fixed engine. The original acceptance
used frozen previous-build saves, not current-engine fabricated legacy state. The verifiers require canonical
note=1 and migration marker=1, then leave the discovery map and round-trip again. Pure `JournalStateTests`
additionally cover cached-only notes, unrelated named globals, E1/DF alias conflicts, active-map clears,
canonical zero tombstones and idempotence. Binary saves and large asset packages remain outside Git.

## Guide site effects and requirements

Run `tests/scenarios/atlas/mm7_unnoted_gold_well.yml` in an isolated directory. Its one-Knight, zero-gold party
is an explicit controlled fixture; it uses the actual Harmondale well 223, verifies +200, reloads and verifies
no repeated reward. Copy its temporary `before-gold-well.oysav` to private `saves/before.oysav` for the MM7 tour.
For MM8, use `site-reused.oysav` from the existing MM8 well-journal scenario. Keep them in separate directories.

At 1600x900, run `tools/menu_tours/atlas_mm7_gold_site_reference.yml` and
`atlas_mm8_intellect_site_reference.yml`. Each searches Guide by effect, opens **Read details**, returns to
Discovery and saves. Inspect fresh captures: the unnoted MM7 well must have no learned description in Discovery;
the MM8 case must retain its learned journal note and any actual recorded outcomes, without the generated
Guide requirements.

Then run `mm7_gold_site_reference_privacy.yml` / `mm8_intellect_site_reference_privacy.yml` here. They compare
before/after saves: MM7 gold and actual MapVar(21) remain zero; MM8 Intellect bonus and journal note remain
15/known. Generated reference text is never evidence of an earned reward or present availability.


## Actual site observations and privacy

Use separate private working directories with normal packages and Atlas enabled. The extended
`tests/scenarios/atlas/mm7_unnoted_gold_well.yml` and `mm8_well_journal.yml` now assert actual observation state,
repeat-use history, save/load and map transitions. They produce temporary `observed-gold.oysav` and
`observed-intellect.oysav`. Copy these to separate private `saves/before.oysav` files for the learned UI cases.
For two more isolated unseen-site cases, copy `before-gold-well.oysav` / `before-site.oysav` instead.

Run `tools/menu_tours/atlas_mm{7,8}_site_observations_known.yml` and the corresponding `_unseen.yml` tours at
1600x900. Inspect fresh native captures: learned Discovery finds +200 gold / +15 temporary Intellect and reads
actual historical changes; Guide additionally shows its reference conditions; returning to Discovery hides
those conditions. The unseen cases must find no actual outcome in Discovery before or after Guide browsing.
An already-known journal description and an actual recorded outcome are separate sources of party knowledge.

After the tours, create one private verifier directory per world. Copy the learned case's `before.oysav` and
`quicksave.oysav` to `saves/before-known.oysav` / `saves/after-known.oysav`; copy the unseen equivalents to
`saves/before-unseen.oysav` / `saves/after-unseen.oysav`. Run this directory's
`mm7_site_observation_privacy.yml` / `mm8_site_observation_privacy.yml`. They verify unchanged resources,
recorded endpoints, no additional-change flag, MM8 journal/stat state, and zero observations in unseen saves,
then round-trip both histories. Unknown named-map values read as zero; the pure reader tests separately cover
malformed partial records. Repeat the MM8 gameplay scenario with only `karol.test` enabled to check Atlas-off Core.


## Class starting-skill profiles

Run `mm7_class_starting_fixture.yml` and `mm8_class_starting_fixture.yml` from this directory against the normal
packages, in separate private working directories. They explicitly seed one level-one Human Cleric, 500 gold,
10 food and level-one Mace/Body Magic/Meditation/Learning. This is a controlled reference fixture, not proof of
selecting those choices through the character-creation UI. Copy each temporary `before-class-reference.oysav`
from `openyamm_scenario_saves/atlas_mm*_class_starting_fixture/` to private `saves/before.oysav`.

Launch both worlds at 1600x900 with `tools/menu_tours/atlas_starting_skills.yml`. Inspect the new **Granted** and
**Optional choices** text in Discovery; only the Human profile may appear there. Guide may show the other
registered race/class combinations. The tour scrolls the longer profiles, checks the existing comparison grid,
returns to Discovery and saves a private quicksave. Inspect fresh captures, not just their timestamps.

Run `mm7_class_starting_privacy.yml` / `mm8_class_starting_privacy.yml` in the corresponding directories.
It checks the same class/race/level, XP, resources and four known skills before/after browsing, verifies that
Sword/Axe/Perception were not granted, and round-trips the result. It does not test every starting race's portrait
availability or the full character-creation workflow. Keep all saves and captures private and outside Git.


## Item icons and base comparison

Run `mm7_item_comparison_fixture.yml` / `mm8_item_comparison_fixture.yml` against normal packages in separate
private working directories. They seed two level-one Human Knights with Sword 1 and equipped base items 1/2,
500 gold and 10 food. These explicit fixtures are not a test of acquiring either sword in the campaign.
Copy the resulting temporary `before-item-reference.oysav` from each `atlas_mm*_item_comparison_fixture/`
scenario-save directory to private `saves/before.oysav`.

Launch at 1600x900 with `tools/menu_tours/atlas_item_comparison.yml`. Inspect the inventory artwork, its transparent
background/aspect ratio, the known Iron/Trollish sword comparison, scrolling and the Guide-only Dark Elven sword.
Returning to Discovery must clear that pin/comparison; searching the unseen item must find no entry. The tour
uses individual Backspace events because Atlas text input does not implement Ctrl+A selection.

Then run `mm7_item_comparison_privacy.yml` / `mm8_item_comparison_privacy.yml` from the respective private
directories. They compare before/after class, race, level, XP, Sword skill, gold/food and possession counts for
items 1/2/3/1335 and their equipped member/slot, followed by another save/load. The base-item fixture does not validate enchanted or unidentified
instance-history privacy; that broader knowledge gate remains explicitly open in `ATLAS_COVERAGE.md`.

The companion `tools/menu_tours/atlas_item_comparison_scroll.yml` uses the same initial fixture in another
private working directory. It pins the unowned Elven Chainmail (1335) in Guide, compares it to the Iron Longsword,
and scrolls the long armor description. Inspect both captures: the left column must move while the right stays
at its original position; the armor's actual race restriction must be visible in the full text. Returning to
Discovery clears the artifact comparison. Run the same world's privacy verifier against this case's saves too.


## Remote companion topic declarations

Prepare a separate package view with `--fixture remote-companion-contact` using the same helper:

```bash
python3 tests/fixtures/atlas/prepare_assets.py --assets /path/to/runtime/assets \
  --output /same-volume/remote-contact-assets --fixture remote-companion-contact
```

The QA-only `d19_zz_companion_contact_fixture.lua` declares a possible Cauri invitation at the existing NPC 28
(Fredrick) from a callback owned by the distant Mad Necromancer's Lab map. No physical site or automatic trigger
calls it. This is deliberately synthetic content: it tests modded declarations and ID-based joins, not a claim
about ordinary MM8 recruitment. Its callback must never run during an Atlas scan.

Create the isolated seed with `tests/scenarios/atlas/mm8_companion_contacts.yml` against normal packages, then copy
its temporary `before.oysav` to the test working directory's `saves/before.oysav`. Copy normal settings/profiles;
point only this directory's asset setting to the fixture package view. Run
`tools/menu_tours/atlas_mm8_remote_companion_contacts.yml` with the native menu-input tour option. Before the source
scan, the extra contact is absent. Afterwards Cauri has a possible scripted contact at Fredrick's independently
authored Dagger Wound residence, plus her ordinary Murmurwoods rescue site. Follow the first link and return to
Discovery: only the two owned companions remain visible.

Run `tests/scenarios/atlas/mm8_remote_companion_contacts_verify.yml` against the resulting private quicksave.
It checks that the remote dungeon, Dagger Wound residence and Murmurwoods remain undiscovered, the resident was
not observed by reading Guide, gold/map/unlock state are intact across resave/load, and subsequently opening his
actual dialogue still shows the original topics 10/11 rather than the fixture's unexecuted assignment 630.
Never install this overlay in the normal runtime, modify generated Lua, or run a package writer on the hard-linked
fixture world archives.


## Synchronous site spell-effect history

Use `prepare_assets.py --fixture site-spells-mm7` or `site-spells-mm8` to create separate package views using the
same `--assets` / fresh `--output` arguments above. Each view changes only one well callback in private assets:
Harmondale 223 or Dagger Wound 101 applies Stone Skin and Bless through the ordinary event-spell API. These are
synthetic observation controls, not changes to normal well rewards. Keep the overlays out of normal packages.

Run `tests/scenarios/atlas/mm*_site_spell_effects_fixture.yml` with isolated settings/mods and that world's private
asset view. Copy its temporary `before.oysav` to `saves/before.oysav`. The faithful scenario interacts with the
real surface and checks previously absent party/character spell receipts plus save/load and unchanged gold.
Launch the native game with `tools/menu_tours/atlas_mm*_site_spell_effects.yml` as its menu-input tour. Searching
Stone Skin should find the observed site, whose reading page shows the two actual effects in both modes.
Run the matching `mm*_site_spell_effects_verify.yml` against the private quicksave after the UI closes.
The same fixture scenario can be run with Atlas disabled (`profile.yaml` enabling only `karol.test`) to verify that
Core observation is independent of UI activation. Never run archive writers on these hard-linked world packages.


## Journal-bookkeeping well references and actual effects

Use normal packages and separate isolated settings/mods/saves directories. No synthetic callback or asset-root
replacement is required. Run `tests/scenarios/atlas/mm7_journal_well_fixture.yml` and its MM8 equivalent.
These controlled two-Knight parties use the actual Tatalia armor well / Murmurwoods poison well, check selected
versus other-member effects, actual site receipts and journal state, reload, and interact again without an
additional reward. They do not test campaign acquisition of these sites or every well reset schedule.

From each temporary `openyamm_scenario_saves/atlas_mm*_journal_well_fixture/` directory, copy `known.oysav` to
one private working directory's `saves/before.oysav`; copy `before.oysav` to another directory's file of the same
name. Run `tools/menu_tours/atlas_mm*_journal_well_known.yml` and `_unseen.yml` respectively at 1600x900.
"Unseen" here means unused/no effect learned: the immediate area may be explored by the native game. Guide
must show the real reward and personal-use requirement without presenting annotation bookkeeping as eligibility;
Discovery must show only historical findings and learned notes. Inspect the actual captures in both modes.

Run `tests/scenarios/atlas/mm*_journal_well_known_verify.yml` or `_unseen_verify.yml` in each corresponding
working directory after the UI tour. They load the fresh private quicksave, check both characters, journal flag,
observation flag, actual effect endpoint, map/gold and another save/load. Reading Guide must not apply armor/poison,
learn the note or create an effect receipt. Preserve original campaigns; keep binary saves/captures outside Git.


## Actual item costs and queued rewards at sites

Use normal packages for `tests/scenarios/atlas/mm7_site_item_effects_fixture.yml` and its MM8 counterpart.
They explicitly seed test parties, the accepted statuette quest/item or a Stone to Flesh scroll, then use the
actual Tatalia altar / Murmurwoods statue. Setup shortcuts precede the save/reload and follow camera setup;
a later runtime pose sync would discard unsynchronized harness fixture changes. These are isolated interaction
tests, not proof of obtaining the quest/item through the campaign. They check exact costs, real quest flags and
four saved item-range fields through reload. For a Core-only control run MM8 in another directory with only
`karol.test` enabled; keep its saves separate from Atlas-required ones.

Prepare `--fixture site-items-mm7` with the same private-assets helper for the queued-reward control. It replaces
only Harmondale well 223 with a QA-only callback that queues two unidentified item-1 deliveries through SetHeldItem.
Normal game rewards are unchanged. Run `mm7_site_item_gains_fixture.yml` against that private asset root; the
headless control checks receipt +2 after actual delivery and save/load. Do not install the overlay in normal assets.

Copy each scenario's temporary `known.oysav` to private `saves/before.oysav` for the two cost tours. For the native
reward tour copy the reward scenario's unused `before.oysav` instead: that tour performs the real queued delivery
through ordinary native input, including the cursor handoff, before opening Atlas. Run the matching
`tools/menu_tours/atlas_mm*_site_item_effects.yml` or `atlas_mm7_site_item_gains.yml` at 1600x900. Inspect reading
pages and Discovery-after-Guide, not just screenshot existence. Search uses Eagle Statuette, Scroll and Longsword:
the latter two are deliberately the unidentified names. Guide's authored statue requirement may name the spell,
but its actual historical receipt stays unidentified until the party identifies that item type.

Run the matching `tests/scenarios/atlas/mm*_site_item_effects_verify.yml` or `mm7_site_item_gains_verify.yml`
after the UI closes. The native reward verifier checks one item in the pack plus one actual held item, rather
than treating the pack-only event query as a cursor-inclusive count. They check range endpoints, known-site marker, quest flags where applicable,
map/gold/food and another round trip. Reading must neither grant more items nor consume another scroll/statuette.
Keep all settings/saves/captures private and outside Git; never write to the hard-linked large fixture archives.


## Inherited callback deliveries after the immediate snapshot

Prepare `--fixture inherited-deliveries` using a fresh private output directory. This QA-only engine-global
supplement replaces MM7's beacon callback 551 and MM8's red-barrel callback 269. Each depletes its existing
object first, then queues two unidentified item-1 deliveries. It tests modded shared callbacks and the existing
confirmed receipt pipeline; it does not alter or certify ordinary beacon/barrel rewards.

Run `tests/scenarios/atlas/mm*_inherited_delivery_fixture.yml` in separate isolated directories using this asset
view. These are three-step preparation scenarios only: create a controlled party, set the known pose, save the
unused object as `before.oysav`. Copy that save from each temporary `atlas_mm*_inherited_delivery_fixture/`
scenario directory into private `saves/before.oysav`. Native `tools/menu_tours/atlas_mm*_inherited_delivery.yml`
performs the actual interaction, then searches Longsword and opens the physical object. The MM7 tour selects
the second result (beacon fire), since the existing item-source marker sorts first; MM8 selects Barrel first.

Inspect fresh reading pages and Discovery-after-Guide. They must show Items received here with the unidentified
Longsword name and recorded day, even though the immediate callback snapshot contains no net item delta.
Run the matching `mm*_inherited_delivery_verify.yml` against the native quicksave. It preserves confirmed origin
coordinates, exact slot/event receipt, immediate changed=0, depleted contents plus initialization marker, one item in pack plus one genuinely held, original
Might/skill points, map/gold/food and another round trip. Those checks distinguish actual delivery from merely
having queued a grant. The headless preparation does not prove opaque sprite picking or activation; native
input/captures and post-UI state do. Keep private saves and the QA overlay out of normal runtime and Git assets.


## Individual MM8 delivery contacts

Use unchanged normal packages and two private working directories with the Atlas enabled. Run
`tests/scenarios/atlas/mm8_delivery_101_knowledge.yml` (Aislen) and
`tests/scenarios/atlas/mm8_delivery_142_knowledge.yml` (Pole). These faithful scenarios reach NPCs by ID,
execute all actual dialogue/delivery/reward callbacks, and supply the missing three items explicitly. They
are not physical travel, ingredient gathering or alchemy tests. The authored third item is held on the cursor;
the documented `held_item active:false` helper explicitly places it into inventory before deliveries.

Copy each scenario's `partial.oysav` from its platform-temporary scenario-save directory into its own
`saves/before.oysav`. Use the matching `tools/menu_tours/atlas_mm8_delivery_{101,142}.yml` at 1600x900.
Inspect the actual captures: Discovery should name just the delivered recipient, Guide all six, and Discovery
should remain restricted afterward. Native input saves `saves/quicksave.oysav`; run the matching
`tests/scenarios/atlas/mm8_delivery_{101,142}_privacy.yml` in that working directory. Each verifier loads the
before/native saves and another round trip, checking XP, gold, delivery flags and all six contact-history keys.
The English quest-table correction must be mounted for Pole's journal entry; no QA Lua override is needed.


## Passage item scope and campaign unlocks

Use normal runtime packages and isolated settings/mods/saves. The three-step
`tests/scenarios/atlas/mm7_passage_scope_fixture.yml` and `mm8_passage_scope_fixture.yml` create two-member
parties without Wetsuits / Pirate Leader's Key, in the Lincoln / Pirate Outpost. Copy each temporary
`before.oysav` into its private `saves/before.oysav`. These scenarios prepare unused sites; they do not activate
an exit or prove an ocean journey.

Run the corresponding `tools/menu_tours/atlas_mm{7,8}_passage_scope.yml` at 1600x900. Inspect the reading page:
Lincoln requires possession on every member, while the submarine permits any party member's key or the retained
campaign unlock. The Lincoln callback checks carried/equipped items, not solely equipped suits. Shoals' separate
survival rules still require equipped suits. The reference describes this passage's item checks, not safe arrival.

Each tour returns to Discovery and saves `saves/quicksave.oysav`. The matching
`tests/scenarios/atlas/mm{7,8}_passage_scope_privacy.yml` loads before/native saves and another round trip,
checking the exact uninteracted site, destination discovery, absent items, unset unlock, map, gold, food and both
members' XP. Callback execution is independently tested against loaded normal scripts in
`canonical Lincoln and submarine references match active party scope and persistent unlock callbacks`:
1/2/5-member parties, a missing member despite duplicate suits elsewhere, a key on the last member, persistent
unlock without a key, and no item consumption. Lincoln's successful callback offers a transition confirmation;
that pending dialogue is not an already completed journey.


## Actual guild shelf observations

Use normal assets and separate private runtimes for MM7/MM8. Run
`tests/scenarios/atlas/mm{7,8}_guild_shelf_fixture.yml`, copy the temporary `before.oysav` to private
`saves/before.oysav`, then run `tools/menu_tours/atlas_mm{7,8}_guild_shelf_observe.yml` at 1600x900. The fixture
positions a one-member Knight at an actual guild door and sets the stock RNG seed before any stock exists
(MM7 5500129, MM8 5500182). The new `house_stock_seed` setup shortcut rejects an existing stock state; it is a
controlled test fixture, not a player-facing reroll feature. Native input opens the real door and Buy Spellbooks,
then closes the shelf/dialogue and writes `saves/quicksave.oysav`. No stock or knowledge is injected by Lua.

Retain that save as `shelf-seen.oysav`. Copy it to another private runtime's `saves/before.oysav` and run
`atlas_mm{7,8}_guild_shelf.yml`. The seeded shelf exposes Torch Light in Harmondale / Stone to Flesh on Dagger
Wound. Inspect Discovery's encountered-book status, Last seen guild link/date and Guide's separately labelled
possible pools. Run `mm{7,8}_guild_shelf_privacy.yml`: each 170-step verifier checks all twelve book IDs/history
keys, absent book inventory, gold 5000, food 10 and zero XP across before/native saves, another round trip and
explicit fixture map changes. It does not test the normal inter-region travel route. Expected books describe
these base-package seeds, not arbitrary item/stock mods. Timestamp equality was also checked in retained save
artifacts; the verifier does not assume a machine-dependent native frame duration.

For real purchase controls, start separate private runtimes from `shelf-seen.oysav`, run the corresponding
`atlas_mm{7,8}_guild_shelf_buy.yml`, then `mm{7,8}_guild_shelf_purchase.yml`. MM7 buys the affordable second
book (Fire Resistance 1904, 900 gold); MM8 buys the first (Feather Fall 1916, 400 gold). The 41-step verifiers
require eleven remaining observed books, an explicitly retired last slot, the purchased book still known,
exactly one in inventory, gold 4100/4600 and unchanged food/XP through another load. The initial MM7 first-slot
trial correctly refused an unaffordable Inferno; it is not reported as a successful purchase.

A separate MM8 observation run with `mods/profile.yaml` enabling only `karol.test` verifies Core capture while
the Atlas module is disabled. Use the same unused fixture and observe tour, then `mm8_guild_shelf_core_off.yml`.
Its nine steps check observation/count/gold and another round trip. Preliminary door-only native controls
also left the shelf's seen marker clear. User saves and official-installation data are never test-write targets.
