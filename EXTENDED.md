# OpenYAMM Extended

## Product contract

Build an upstream-compatible fork that improves playing MM6–MM8, supports large mods and installable community
worlds, and hosts the flagship `Adventurers` overhaul. Preserve classic Might & Magic, with MM7 as the gameplay
foundation and selected ideas from MM6/MM8/MM9. Core supplies APIs and capabilities; mods own gameplay decisions.
Keep changes small enough to regularly merge `pjasicek/openyamm/main`. Extend existing runtime, packages, tables,
Lua, save system and editor rather than introducing another engine.

Required long-term modules and capabilities:

- Native Interactive Atlas: world → continent → region → city → dungeon, with discovery as the default,
  optional Guide Mode, trainers, NPCs, shops, quests, chests, monsters, obelisks, routes, teleports, search,
  filters, player markers and notes. All consumers share one World Database; the Atlas owns no parallel records.
- Launcher/Hub: continue, worlds, saves, mod profiles, dependencies, load order, conflicts and updates;
  Classic+, Adventurers, Adventure, Mobile, Randomizer and custom profiles.
- Adventure Framework: authored worlds, location state, occupying faction, population, threat and last cleared;
  dynamic occupation/repopulation, bounty based on real world state and data-driven travel encounters.
- Seeded Randomizer and modular New Game+: replayability that preserves authored world design.
- Lore-integrated cross-world travel through an Ancients Gate Network, discovery, activation and quest chains.
- Android and controllers as first-class platforms: short sessions, safe autosaves, suspend/resume,
  touch targets, scalable UI, radial spells, inventory comfort and later cross-save/companion views.
- Adventurer's Journal: an automatic party chronicle consuming gameplay events.
- Installable Community Worlds and later Studio/SDK: create classes and quests without recompiling C++.

Modding Core will provide manifests, versions, enable/disable, dependency resolution, deterministic load order,
conflicts, namespaced IDs, allocated QBit ranges, save namespaces, typed patching and provenance. Start with data
and Lua, not DLL mods. Saves must identify required mods and reject incompatible loads before changing game state.
Use existing global gameplay tables and world-scoped geography; namespaced records must not duplicate ownership.

Adventurers preserves world-based promotion quests. The progression model is race, base class, first/second
promotion and legendary/prestige paths, with traits that change play. Class-specific world interactions and
declarative promotion requirements are part of the design. Prototype Knight first; Ranger connects exploration,
tracking, monsters, alchemy, travel and bounty. Paladin has one active aura. Prefer improvements to existing spells
and restrained passives over many active abilities.

Gameplay events cover creation, level-up, promotion, skills, attacks/hit rolls, damage, deaths, spells, equipment,
maps, travel, rest and quest changes. Declarative effects cover common behavior; Lua handles unusual behavior.

Do not prioritize MMO/multiplayer, procedural dungeon generation, renderer rewrites, replacing all monsters with
3D, a new continent, a full MM9 remake, cloud infrastructure or hundreds of active skills.

## Bootstrap acceptance

Branch: `extended/bootstrap-android`. Fork: `coral18/openyamm`. Local upstream remote: `pjasicek/openyamm`.

1. Android installs as `org.openyamm.extended`, beside the official `org.openyamm.android` application.
2. App name and native startup log identify Extended.
3. A secret-free ARM64 CI workflow produces a signed APK and checksum.
4. Verify actual runtime content against official release 1.0; do not assume Git equals a complete release.
5. Knight gains one extra HP per level through its data table; this is a proof of execution, not class balancing.
6. Independently test install, startup, MM7, Knight progression and isolation on the user's POCO phone.

The official 1.0 APK downloaded on 2026-10-06 has SHA256
`76938005894690ff636be14677b4b0eb9f6f29e91e7237ce4d9bfc36b3662b47`, size 3,387,397,715 bytes and 50,930 engine/world
entries. Its runtime entries have source counterparts in Git, apart from generated profile/license records and
explicitly retained archived lighting dependencies. Local inventory comparison found no external-only runtime entries.
The verified upstream packaging path can therefore remain authoritative; no extra binary payload is committed.
Current upstream has asset changes since 1.0, including lighting and travel; parity permits those declared changes.

Candidate `14a4060` passed [Actions run 37429447979](https://github.com/coral18/openyamm/actions/runs/37429447979).
The verified APK is 3,535,123,017 bytes, with SHA256
`1539c959faf23982807dc3e0e5c4cda44aa104a6af4d5ee0ff0dcf8408c4bd64`. Both CI and local checks verify identity,
ARM64-only libraries, 47 shaders, the native Extended marker, Knight data and release-content parity. The APK has
50,939 engine/world entries; changes from the 50,930-entry baseline correspond to declared source changes.
The signed APK and compact verification report are separate artifacts; APK retention is seven days.

A focused native check compiled the actual table parser and health calculation: Knight base HP remains 35,
HP/level is 6, Champion remains 8; at Endurance 14 without Bodybuilding, level 1 gives 41 HP and level 2 gives 47.
This is code/data verification, not a claim of leveling the Knight on the phone or running the full test suite.
The POCO wireless ADB connection works (Android 16, ARM64). Installation succeeded after the user enabled
`Install via USB` and accepted Xiaomi's separate installation prompt. Failed attempts showed the prompt closing
after about 12 seconds. Staging the verified APK once in `/data/local/tmp` allowed retrying the installer without
another full transfer: `adb push` took 85.8 seconds at 39.3 MB/s, and the staged file matched the candidate SHA256.
The temporary APK was removed after installation.

Physical-device smoke checks passed: version 0.1.1-bootstrap/code 102, separate Extended app/data directory,
native Extended startup marker, rendered menu, MM7 `7out01.odm` loaded with `initialize_view=true`, and inspected
engine-native terrain/HUD capture. A Home/return cycle retained the same process and rendered gameplay again.
The user independently confirmed the game started. Local evidence is retained in ignored
`build/extended-bootstrap/phone/` (menu, gameplay capture, trace and resume result).

This was a temporary seeded MM7 smoke session, not a full campaign, leveling or performance test. Xiaomi rejects
ADB touch injection with `INJECT_EVENTS` disabled, so the physical Knight level-up check remains unperformed;
Knight progression has the packaged-table and focused native-formula evidence above. No input-permission bypass
was attempted. The original Extended settings were restored byte-for-byte and the ordinary menu reopened.

Only read operations were issued against official app files. Its installed APK path, settings and two named saves
remain unchanged. Its autosave changed at 09:42:30 local time, before the Extended installation attempt started;
do not claim all files match the earlier 09:29 inventory. A fresh pre-install hash inventory is kept locally for
comparison after the permitted install. The official APK path and all four files matched this fresh inventory
after the successful Extended install and gameplay test. Never restore or overwrite the user's official autosave.

For the phone: install Extended, open MM7, create a Knight, check level-up health progression, then reopen the
official game and confirm its existing saves/settings. Do not use the disposable-emulator script on the phone.
The debug APK is explicitly the bootstrap deliverable; use release builds for later UX/performance evaluation.

## Iteration performance

Measured candidate CI stages: source checkout 337 seconds, official baseline fetch/verification 139 seconds,
host configuration 83 seconds, host tool compilation 119 seconds, runtime packaging 124 seconds and Android
build 620 seconds. Gradle dependencies, the checksum-pinned official baseline and debug signing key are cached.
Native object files and generated runtime packages are not yet cached across runners.

Per the user's 2026-10-06 instruction, finish this candidate through the already started Android flow. Subsequent
iterations in this session start with local Windows development/tests on this PC, keeping build directories and
existing assets rather than re-downloading or rebuilding them. For later Android iterations, retain native build
outputs or add compiler caching with versioned toolchain keys, and reuse unchanged runtime packages. The existing
Gradle assets task already declares inputs/outputs, so preserving its directory lets unchanged inputs skip work.
Data-only tests should investigate the existing `[assets] root` setting for an external development asset root
before repackaging a full 3.5 GB APK; verify Android mounting, completeness and isolation before relying on it.
Native changes still require a verified APK update. These are
next-iteration decisions, not optimizations measured or validated by the present candidate.

## Next milestone: Mod Loader v1

After bootstrap acceptance, load `mods/karol-test/mod.yaml`, log `Found mod karol.test 0.1.0` and `Loaded 1 mod`.
Reject duplicate IDs, missing dependencies, cycles and QBit collisions. Then apply Knight HP/level +1 through
a typed class patch without changing the base TXT. Keep the bootstrap data modification temporary and move it
to that test mod once the loader and patch semantics exist.

Continue with Class/Promotion Registry, Trait Registry, typed patch provenance, Lua gameplay events and the
Adventurers Knight prototype. Expand into the remaining classes, Adventure Framework, bounty, travel encounters,
world state, the shared World Database/Atlas, Journal, Randomizer, NG+, Hub and community SDK. Introduce shared
data contracts early when a dependency requires them; the order is not rigid.

Verified upstream integration points at the bootstrap continuation:

- `engine/AssetFileSystem.*`: mounted content, package enumeration and shared asset lookup.
- `game/content/ContentManifest.*`: existing `world.yml` parsing, dependencies, QBit/ID ranges and table contributions.
  This is a world manifest, not an already implemented Mod Loader. Preserve its existing filename/contract.
- `game/data/GameDataLoader.cpp`: mounted package validation and class table loading before gameplay consumes them.
- `game/tables/ClassMultiplierTable.*` and `ClassSkillTable.*`: resource progression, skill caps, metadata and
  promotion/race rules. Add patch support at the existing table/registry boundary.
- `game/maps/SaveGame.*`: required-content-package schema validation already exists. Extend it for enabled mod
  identity/version/state rather than starting a separate save format.
