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
