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
