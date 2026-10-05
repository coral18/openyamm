# MM6 demon 3D trial

The renderer follow-up is documented in the [shared 3D creature rendering plan](../../../../CREATURE_3D_RENDERING_PLAN.md).
Material lighting, animated sunlight mesh shadows, and shared sky reflections are implemented and validated on
the native NVIDIA/OpenGL desktop. The plan links the visual, gameplay, lifecycle and performance evidence.

Run `./tools/run_demon_3d_test.sh` from the repository root. It creates a private New Sorpigal save,
spawns three hostile **Devil Spawn** actors (merged monster id **502**, descriptor **Demon A**) in the
grass south of town, and keeps the game open for review. Add `--seconds 20` for automatic exit.
The isolated party is immortal and sunlight shadows are enabled. Close the window to end the test;
normal saves and settings are untouched. Add `--set video.shadows=false` for a shadow comparison.

The appearance binding is opt-in via the launch-only setting `debug.actor_models=true`.
`actors.yml` matches the existing descriptor and maps the eight native animation states to GLB clips.
`Walking` and `Running` are retained in the GLB; native AI uses `Walk_Native` because it has the native
1.125-second loop. Melee and ranged attacks share `Attack`.

Actors retain their table stats, Demon kind, spells, sound ids, hostility, collision cylinders, AI,
pathfinding, combat impact timing, turn-based behavior and save data. Models are derived presentation
instances, rebuilt from actor state after load and cleared on map changes. Model sampling uses the
actor's 128-Hz animation clock and does not advance its own simulation. Shrink follows the same visual
multiplier as sprites. Mouse picking uses the posed model bounds; health bars and hover outlines retain
the existing actor identity and colors. Actor height overrides scale the model consistently with sprites. Death includes the authored morph plume; `NoCorpse` still hides
the demon when the native death timer ends, so no permanent lootable ash is added.

The shared binding path works indoors and outdoors. Worlds without `models/actors.yml` keep their
existing appearance and load no model assets. The prototype uses CPU skinning with all eight available
influence slots, including the source vertices with more than four influences. GPU skinning is deferred.
Rendering uses linear-colour GGX metallic/roughness shading, normal maps, emissive factors, scene depth,
filtered mip chains, outdoor sun/sky probes and selected local lights, room lights indoors, outdoor fog,
sunlight self/ground shadows and roughness-filtered sky reflections. Indoor environment response follows room
ambient illumination; it does not show an exterior sky. Outdoor baked maps require explicit model sunlight
probes (v4+) and direct-only receiver pages for mesh shadows (v5). All 42 installed classic outdoor bakes are v5.
The source rig and material master
remain under `level_generation/creatures/mm6_demon_meshy_reference_20261004/texturing_fixed_20261005/`.

Recreate the runtime GLB (Pillow is required):

```sh
python3 tools/prepare_actor_glb.py \
  level_generation/creatures/mm6_demon_meshy_reference_20261004/texturing_fixed_20261005/mm6_demon_textured.glb \
  assets_dev/worlds/mm6/models/mm6_demon.glb
```

Only the embedded JPEG maps are transcoded to PNG for the existing runtime decoder. Geometry,
UVs, skin weights, inverse binds, morph accessors and all nine animation clips are preserved.
The renderer uploads complete colour-space-aware mip chains and respects supported glTF sampler settings.

For additional live spawns, open the debug console and use:

```text
actor spawn 502 3 -9728 -11919 161
actor count 502
```

Checks:

```sh
cmake --build build --target openyamm openyamm_unit_tests -j25
./build/tests/openyamm_unit_tests --test-case='*actor AI*,*ModelAnimation*,*launch directives*'
OPENYAMM_REGRESSION_FILTER=mm6_demon_actor_models \
  ./build/game/openyamm --world mm6 --headless-run-regression-suite actors
```

The native review capture is `output/demon_3d_review/three-demons.png`.

Validated on 2026-10-05: desktop build, 67 focused unit cases (614 assertions), outdoor factory/hit/death/restore/reset
and indoor binding checks, four existing indoor/outdoor crowding checks, and an OpenGL 3.3 native NVIDIA run.
The three-model synchronization took roughly 1.8–1.9 ms per frame in this short run; this is a prototype observation,
not a crowd-performance guarantee. The original source GLB had SHA-256
`efcca759e86c6f5c19c787e2356d3cb9480353b68a3d8a051b154b3cffd322b2`.
The original runtime GLB had SHA-256 `2336f851ff1a3533e214aed6c5ccedef33d2d9d77295ef06dd832a4fdc03b340`.

Material tuning subsequently set body metallic to zero and iris/pupil roughness to 0.22/0.18. The GLB binary chunk,
base colour and normal images, geometry, bind, morphs and animation accessors are unchanged. Backups, current hashes,
and the tuning record are in `output/creature_3d_rendering_review/material_tuning/`. The editable Blender masters and
material authoring script include the same changes. Retained material-pass captures are in
`output/creature_3d_rendering_review/pass1_tuned/tour/`.

The completed renderer review includes a [face close-up](../../../../output/creature_3d_rendering_review/head_detail/tour/01-head-front.png),
day/shade/night/torch/spell/indoor captures, native-clock pose review, live combat/turn mode, MM6/MM7/MM8 map
transitions, resize/fullscreen and resource cleanup. The latest focused run passes 152 cases / 68,446 assertions;
the two demon regressions also pass. One unrelated Ravenshore stairs check is excluded because a concurrent map
event edit invalidates its bake dependency; dependency validation is still enforced.

The [controlled performance report](../../../../output/creature_3d_rendering_review/performance/report.json) records
three models at GPU 1.471 / 1.712 ms p50/p95 and six at 2.479 / 3.012 ms on an RTX 3060 Ti at 1600×900,
VSync off. These are medians of stable one-second window percentiles. Three and six share texture/target storage;
CPU skinning remains proportional to actor count. First-use model/mip/environment preparation still hitches for
about one second. Sky reflections cover the sky only; indoor point-light shadows and local reflection probes are deferred.
