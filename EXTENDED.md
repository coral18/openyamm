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

Previous bootstrap commit `d1232df` built successfully in Actions run `37421718314`, but compilation alone does not
establish device acceptance. The new gate checks packaged identity, signature, ARM64, shaders, marker, Knight data
and release-content parity. No phone was connected at the start of this continuation. Device acceptance stays open
until there is actual installation/gameplay evidence. APK/report results are recorded after the candidate CI run.

For the phone: install Extended, open MM7, create a Knight, check level-up health progression, then reopen the
official game and confirm its existing saves/settings. Do not use the disposable-emulator script on the phone.
The debug APK is explicitly the bootstrap deliverable; use release builds for later UX/performance evaluation.

## Next milestone: Mod Loader v1

After bootstrap acceptance, load `mods/karol-test/mod.yaml`, log `Found mod karol.test 0.1.0` and `Loaded 1 mod`.
Reject duplicate IDs, missing dependencies, cycles and QBit collisions. Then apply Knight HP/level +1 through
a typed class patch without changing the base TXT. Keep the bootstrap data modification temporary and move it
to that test mod once the loader and patch semantics exist.

Continue with Class/Promotion Registry, Trait Registry, typed patch provenance, Lua gameplay events and the
Adventurers Knight prototype. Expand into the remaining classes, Adventure Framework, bounty, travel encounters,
world state, the shared World Database/Atlas, Journal, Randomizer, NG+, Hub and community SDK. Introduce shared
data contracts early when a dependency requires them; the order is not rigid.
