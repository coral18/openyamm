# Upstream compatibility audit, 2026-10-18

Compared `pjasicek/openyamm/main` at `4af3efd837e2d3a63dee646cec83a7ea35d45a7f` with
Extended `ca778c2fd5c288f4e9c49aca3ee5511e612144c2` and the pending shared quest relationship changes.
The common ancestor is `c1bc5434fbf630a96ac2465818ab25474b14b31c`.
There are 22 incoming upstream commits and 23 Extended commits since that ancestor.

This was a read-only integration assessment: fetch with `--filter=blob:none --no-tags`, path comparison with
`--no-renames`, then `git merge-file -p` using the ancestor, current working file and incoming upstream file for
all 30 overlapping paths. It did not merge, check out upstream, alter the running package or test a combined build.
The upstream asset revision is substantial: 24,228 changed paths including content and Android cooked assets.

## Observed source conflicts

21 overlapping files merged textually without conflicts. Nine need explicit reconciliation:

| File | Reason / integration requirement |
| --- | --- |
| `android/gradle.properties` | Preserve Extended's own version sequence and separate application identity. |
| `assets_dev/engine/data_tables/map_stats.txt` | Preserve explicit world identities while accepting upstream content changes. |
| `game/data/GameDataLoader.cpp` | Retain shared event-program loading for the Atlas alongside upstream map-loading cache changes. |
| `game/maps/MapAssetLoader.cpp` | Reconcile the shared encounter-definition helper with upstream encounter-loading changes. |
| `game/shaders/model_skin.sh` | Upstream rewrites joint-matrix skinning; reassess our earlier renderer fix against that implementation. |
| `game/shaders/vs_model.sc` | Follow the reconciled joint layout, model scrolling and lighting attributes. |
| `game/shaders/vs_model_shadow.sc` | Match the same skinning contract used by the main model shader. |
| `game/ui/MenuDesignScreen.h` | Preserve Atlas button geometry support alongside upstream selection controls. |
| `game/ui/MenuScreenBase.cpp` | Reconcile image decoding with Extended's PNG signature handling for mounted artwork. |

The new Atlas, World Database and Mod Loader source files are not overwritten by incoming files. This supports
continued integration; it does not prove behavioral compatibility. Textually clean merges can still contain API,
content-format, rendering or save-state regressions. This path-level probe also does not replace a complete merge
with rename/delete detection, configuration and compilation.

## Required integration verification

1. Commit and test the current Atlas work, then integrate upstream on a separate branch/worktree.
2. Resolve the conflicts and inspect clean merges involving asset mounting, save/load, application lifetime,
   dialogue and gameplay input.
3. Validate the matching new runtime payload and cooked Android profile; do not pair an updated executable with
   an unreviewed old asset bundle or copy gigabytes into Extended commits.
4. Build Windows first. Check MM6/MM7/MM8 new game and existing isolated saves, Atlas Discovery/Guide privacy,
   fog, party position, annotations, quest knowledge, module disable/enable and typed class patches.
5. Build an Extended Android release APK; check installation beside official OpenYAMM, rendering, input,
   suspend/resume and existing Extended saves. Official application data remains outside testing.

No claim of compatibility with the combined new upstream revision is made until these checks pass.
