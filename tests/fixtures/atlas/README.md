# Conditional trainer-note fixture

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
