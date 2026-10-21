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
  - openyamm.interactive-atlas
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

### Optional native UI modules

The bundled `openyamm.interactive-atlas` mod activates the shared native Atlas using:

```yaml
schema_version: 1
id: openyamm.interactive-atlas
version: 0.2.0
save_required: false
ui_modules:
  - openyamm.interactive-atlas
```

Remove its ID from an explicit profile to hide the Atlas entry in both main and pause menus. Restart to apply the
profile. Core retains shared discovery tracking and world APIs; disabling the UI does not erase discoveries.
An explicit profile from an older build must add the Atlas ID to enable it. With no profile, all installed mods
are enabled as before. The development wrapper installs its manifest without overwriting existing profiles.

`ui_modules` selects native modules supported by this engine version; v1 accepts only
`openyamm.interactive-atlas`. Unknown or duplicate active module providers are errors. This is native UI
activation through a manifest, not arbitrary UI scripting or a DLL interface.

`save_required` defaults to `true`. A `false` module may not declare gameplay patches or QBit ownership. Its
identity is excluded from required-content metadata, so a campaign saved with the Atlas enabled can load with
it disabled. Gameplay mods remain required, including `karol.test`. Existing required-content checks remain
unchanged. Future stateful/scripted mods need an explicit compatibility contract before being made optional.

QBit ranges are inclusive, start at 10000 or higher, and must fit signed 32-bit engine IDs. Overlap between active
mods or with any mounted world's declared range is rejected. Declaring a range reserves it; this version does not
create quest definitions or a Lua QBit API. Future modules must continue using the existing global quest registry.

## Shared quest relationships and learned clues

The shared `engine/data_tables/quest_locations.txt` associates existing journal entries with canonical maps and
merged NPC, house or item IDs. Core owns the reader and knowledge API; content defines the relationships.
This is not a general typed quest-patching interface yet. See [the content audit](ATLAS_QUEST_AUDIT.md).

The six original columns remain valid: `QBit`, `World`, `Role`, `Kind`, `Target`, `Visibility`.
An extended row has eight columns, adding `RequiredQBits` and `ForbiddenQBits`. Lists accept commas or semicolons;
all required bits must be set and all forbidden bits clear. Empty cells or `-` mean no condition. A seven-column
partial row, contradictory requirements, invalid IDs or duplicate target relationships reject the whole load.

```text
QBit	World	Role	Kind	Target	Visibility	RequiredQBits	ForbiddenQBits
541	mm7	objective	map	world.mm7.map.mdt11	journal	569	-
```

`journal` clues become known while that journal entry is active and their conditions hold. `guide` rows stay
Guide-only regardless of their conditions. Guide shows unlearned conditional rows as conditional clues; viewing
them never records knowledge. Discovery retains a learned fact after its conditions or quest entry disappear.
NPC location privacy and terrain exploration remain separate: knowing a name does not award an address or map.

`JournalQuestTable::knowledgeAroundChange` observes both sides of each actual QBit change through EventRuntime.
The existing saved named-global state stores learned relationship keys in the form
`openyamm.discovery.quest/<qbit>/location/<role>/<kind>/<target>`. These keys use semantic IDs, not row positions,
and need no save-format change. Tracking remains active with the optional Atlas UI disabled. Reading the Atlas
does not write facts. Old active quests can expose currently available clues; an old history flag cannot invent
later clues, and missing historical facts are not guessed. All consumers use the shared relationship predicates.

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
The Atlas uses the optional native UI module `openyamm.interactive-atlas`; its manifest is under
`mods/interactive-atlas`. A profile can disable it. Its `save_required: false` presentation manifest does not own
QBits or gameplay patches. World discovery, monster observations and personal map annotations remain shared Core
save state and survive disabling the Atlas. See [ATLAS.md](ATLAS.md) for implementation and native validation.

### Shared map note heights

`WorldMapNote` and `WorldMapNoteEdit` expose optional `int32_t elevation`, an absolute world Z rather than a
screen layer index. A missing value means All heights. Old two-dimensional notes retain that meaning; no floor
is inferred from their XY position, map geometry or the current party. An edit supplies the complete note state,
including its optional elevation: preserve it when renaming, or clear it explicitly to assign All heights.

Core stores this value in existing map-local named variables as `openyamm.map-notes.user/<id>/z`, alongside the
existing ownership marker. Only user-owned notes consume that metadata. Zero is a valid assigned height.
Creation avoids IDs occupied by either the note, ownership marker or height metadata; deletion clears both owned
keys. The normal serializer, canonical map snapshot and refill knowledge retention preserve the annotation.
There is no new save version or Atlas-owned serialization. `worldMapFeatureOnFloorLayer` supplies the shared
height predicate; unassigned notes match every layer. The Atlas editor consumes these APIs independently of
terrain exploration and offers known bands, All heights and the current party's height in the same dungeon.

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


## Shared event arrival knowledge

Successful map-local event moves record actual destination coordinates and game time through `recordWorldMapArrival`,
after the destination runtime has loaded and applied its party position. Same-map teleports use the same recorder.
Rejected/cancelled/failed loads, debug map loads and merely declared destinations do not record an arrival. This
tracking runs independently of the optional Atlas UI. `PendingMapMove::sourceMapEventId` and `sourceMapFileName`
carry runtime map-handler identity before queueing; deferred moves retain their original map. Global handlers
cannot borrow a coincident local event ID. Nested callback scope is restored after errors.

Destination map-local named state uses `openyamm.discovery.arrival/<canonical-source-map>/event/<event-id>/`
with `seen`, `x`, `y`, `z` and `time` fields. Each source-event/destination pair retains its latest actual arrival;
different destinations remain separate. The standard serializer and refill discovery retention preserve these
records without a new binary save version. `worldMapArrivals` takes the owning snapshot's filename explicitly,
because the runtime state's temporary `mapFileName` is not serialized. Missing/invalid records add no knowledge.
Old saves and restored pending moves without runtime event identity receive no invented travel history.

`WorldDatabase::indexMapArrivals` derives arrival features; `connectionArrivals` links them to local source events
in deterministic latest-first order. Arrival features identify historical observations, not live transport offers,
current portal eligibility or proof of a return passage. Recording and querying them reveal no terrain. Scheduled
coach/boat arrivals and cross-world presentation are separate remaining work.

## Shared item-source knowledge

`WorldDatabase` derives authored item sources from mounted map state; the Atlas consumes the same index. Actual
pickups and visible chest slots record shared named-map discovery state under `openyamm.discovery.item/` even
when the optional Atlas UI is disabled. The existing save serializer and refill knowledge preservation retain
those facts. See [the item-source checkpoint](ATLAS.md#item-sources-and-retained-observations-2026-10-19) for the
source scope and runtime evidence. Looking up a source does not roll random loot, change artifact state or award
knowledge. Received scripted rewards are also recorded, after successful inventory or held-item delivery.
Queued offers are not discoveries. Pending queue provenance uses `openyamm.item-grant/` in existing named-map
state; it survives save/load with the grant queue, is cleared with that queue, and is not retained across refill.
Old saves with no pending provenance receive no invented reward history.

Lua scripts can declare possible fixed rewards after registering their callback:

```lua
RegisterEvent(123, "Conditional reward", function()
    if IsQBitSet(QBit(10001)) then
        AddValue(InventoryItem(42), 42)
    end
end)
SetMapItemRewards(123, {42})
```

`SetGlobalItemRewards` declares shared callbacks used by NPC topics. Declarations describe possibilities, not
eligibility, remaining stock or an entitlement to receive the item. The engine validates positive uint32 item IDs
in contiguous arrays and binds each declaration to the exact registered handler. Replacing, removing or appending
a callback invalidates its previous declaration; explicitly declare the replacement's full possibilities.
Guide links map rewards to their physical event sites where available and shared rewards to current NPC-topic
providers. Discovery shows only confirmed received rewards and does not guess the NPC who delivered an item.

The legacy exporter emits declarations for reachable fixed `Add`, `Set` and `GiveItem` operations, including
synthetic trigger continuations. It does not execute the event or roll random rewards. Regenerate with
`tools/regenerate_lua_events.sh`; `OPENYAMM_BUILD_DIR` selects an existing native build directory when needed.
The generated `_00_item_rewards.lua` overlays load after base handlers and before gameplay supplements. Use
`--item-rewards-only` to refresh declarations while keeping the callback files and their baked lighting hashes intact.
GrayFace's decompiled `txt` references for all three worlds must be present at the documented reference paths:
the exporter also uses them to resolve object types, so the script rejects missing reference directories.
Handwritten callbacks must own their declarations; actor drops and general semantic-source integration remain pending.


Successful corpse-item removal and pickpocket inventory delivery also record `openyamm.discovery.item/carried/`
facts through the shared runtime. Recorded positions belong to the actual actor at that interaction, not its
spawn. The existing save/refill handling preserves this historical knowledge independently of the Atlas UI.
Merely inspecting a creature does not reveal its loot, and an old save does not acquire a guessed loot history.

### Shared monster observations and Atlas sources

`GameplayRuntimeActorState::uniqueNameIndex` exposes the actor's actual merged `place_mon.txt` identity.
Indoor and outdoor runtimes supply it without consulting the Atlas. Core records it when the player picks or
targets that actor, alongside the existing monster observation fields, under
`openyamm.discovery.monster/<actorId>/name` in the owning map's named variables. Zero clears a previous name;
old observations without the optional key keep an unnamed identity. Names are never inferred from an unseen
package actor. Existing save serialization and discovery retention through map refill preserve the value.

Guide placements are a separate, read-only projection of fresh package `MapDeltaData::actors`. The shared actor
identity/name resolver supplies their definition and label. Do not feed a live/saved population into this projection
or represent it as current occupancy. Discovery only exposes recorded observations, including their Bestiary map
links. Disabling `openyamm.interactive-atlas` does not disable Core observation collection or discard its history.
