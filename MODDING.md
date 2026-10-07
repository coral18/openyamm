# Mod Loader v1

Core loads mods through the existing asset filesystem, shared table loader and required-content save metadata.
The first executable example is [karol.test](mods/karol-test/mod.yaml): Knight HP per level changes from 5 to 6.
The canonical base table retains 5. Mods do not replace a whole TXT or require a native DLL.

## Install and select

Place each mod in a directory under `mods/` beside the game's settings and saves:

```text
mods/
  karol-test/
    mod.yaml
  profile.yaml
```

The prepared Windows launcher uses `%USERPROFILE%\openyamm-extended-dev\runtime\mods`.
The incremental build installs example files once and preserves existing edits and profiles on later builds.
Ordinary development runs use the checkout's `mods` directory. Mods can also be supplied in an already mounted
asset package under the `mods/` namespace. Android packaging includes the examples; user files in the app's
working directory take precedence over bundled files with the same virtual path.

With no profile, all installed mods are enabled, ordered by ID after resolving prerequisites. To select explicitly,
copy [profile.example.yaml](mods/profile.example.yaml) to `mods/profile.yaml`:

```yaml
enabled:
  - karol.test
```

To disable all installed mods:

```yaml
enabled: []
```

Restart the game after changing a mod or profile. Missing profile entries and disabled dependencies are errors;
the resolver never silently enables another mod. The profile order sets priority among mods whose prerequisites
are satisfied. A dependency always loads first; ties without a profile use lexical IDs. `load_after` adds ordering
only when the named mod is enabled. A cycle in dependencies or ordering is rejected.

## Manifest contract

```yaml
schema_version: 1
id: example.adventure
name: Example Adventure
version: 0.1.0
dependencies:
  - id: example.foundation
    min_version: 0.1.0
conflicts:
  - example.incompatible
load_after:
  - example.optional-content
qbits:
  begin: 10000
  end: 10031
patches:
  - target:
      type: class
      id: engine.knight
    operations:
      - op: add_number
        path: progression.hp_per_level
        value: 1
```

`schema_version`, `id` and `version` are required. Other fields are optional. IDs are lowercase namespaced tokens
containing a dot; versions are numeric `MAJOR.MINOR.PATCH`, with no leading zeros or prerelease syntax in v1.
Dependency versions are compared numerically. Unknown/duplicate fields, malformed values, duplicate installed IDs,
missing dependencies, active conflicts and cycles fail loading with a specific log message. Installed manifests
are validated even if a profile disables them; dependency/conflict/range resolution applies only to active mods.

QBit ranges are inclusive, start at 10000 or higher, and must fit signed 32-bit engine IDs. Overlap between active
mods or with any mounted world's declared range is rejected. Declaring a range reserves it; this version does not
create quest definitions or a Lua QBit API. Future modules must continue using the existing global quest registry.

## Typed class patches

v1 supports `set` and `add_number` on integer `progression.base_hp` and `progression.hp_per_level`. Class targets
use `engine.<class>`; existing unqualified class aliases remain accepted at the authoritative class-table boundary.
Unknown classes, paths, operations, fractional values, negative final values and integer overflow are errors.
Skill caps, mana, new class definitions and other record types are not yet patchable.

Patches run in resolved order against a temporary table. All must succeed before the class table changes; a failed
operation does not leave a partial patch behind. Additions from several mods compose. Competing `set` operations
from different mods with different values fail rather than silently overwrite each other. Identical assignments
are allowed. An explicitly ordered `set`/`add_number` mixture follows that order; it is not assumed commutative.

`GameDataLoader::getLoadedMods()` exposes the resolved manifests and provenance for future launcher/editor use.
Every applied change records its owner, canonical class, typed path, operation and before/after values. Normal logs:

```text
Found mod karol.test 0.1.0
Mod patch karol.test: Knight.progression.hp_per_level 5 -> 6
Loaded 1 mod
```

## Saves

Every active mod is recorded in the existing `requiredContentPackages` map as `mods/<id>@<version>` with schema 1,
including mods that change rules without adding items. No binary save version or new serialization path is needed.
Before applying a save, the existing validation requires the same mod version to be enabled. Missing, disabled or
changed versions produce `Save requires mod karol.test@0.1.0 ...`. v1 deliberately uses exact versions until explicit
migration/compatibility contracts exist. Legacy saves without mod metadata remain loadable; adding a mod records
the requirement on the next save. Disabling a mod does not rewrite existing saves or strip their requirements.

This is the manifest/resolver and health-patch slice, not the finished mod platform. Lua entrypoints, mod state APIs,
arbitrary asset overrides, an in-game manager, hot reload and downloadable ZIP installation are future work.
The Atlas currently remains native Core UI. Its World Database/discovery state will remain shared when the optional
Atlas module and UI registration mechanism are added; this loader alone does not yet make the Atlas switchable.

## Validation

Six focused C++ tests / 111 assertions cover strict parsing, versions, selection/order, dependencies, cycles,
conflicts, inclusive QBit collisions with mods/worlds, composition/provenance, atomic failure and save requirements.
Eight Python APK gate tests cover content parity, mod payload inventory and rejection of changed patches or a
double-patched base table. Twenty-nine asset-filesystem tests / 252 assertions cover package lookup, including
the new mod namespace, installed/bundled precedence and isolation from world-local aliases. Native Windows
compilation passed.

Native Windows checks exercised enabled/disabled mods and loaded MM6, MM7 and MM8, plus the required invalid-mod
cases and an unknown class target. Real headless new-game/save/load flows produced Knight HP 41/47 at levels 1/2
with the mod and 40/45 without it (Endurance 14). The serialized mod requirement was present only in the enabled
save. Fresh processes rejected missing/changed versions before applying the save, leaving its bytes untouched.
An isolated native GPU tour loaded a legacy save with the test mod enabled, opened Discovery/Guide Atlas and
resumed gameplay; all four engine-owned captures were inspected. Original user saves/settings were not modified.

Logs and disposable saves are retained outside Git under the Windows `mod-loader-v1-20261007` directory and ignored
`build/mod-loader-tests`. Android packaging/verification was updated, but no new APK or phone test is claimed.
