# Atlas site reference audit, 2026-10-22

References are generated from complete supported callbacks. Guide-only reference/search never awards discoveries.
The shared journal remains authoritative for learned notes. The initial table below matches the 2026-10-22 declarations against the
interaction-site extractor on packaged base geometry, before live event, scene or save overrides.
The drink follow-up at the end records subsequent coverage; the initial counts are historical.

| World | Map | Referenced events | Base physical sites | Events without a base site |
| --- | --- | --- | --- | --- |
| mm7 | 7out01.odm | 115 | 1 | — |
| mm7 | 7out02.odm | 223 | 1 | — |
| mm7 | 7out03.odm | 202 | 1 | — |
| mm7 | 7out04.odm | 202 | 1 | — |
| mm7 | 7out05.odm | 202 | 1 | — |
| mm7 | 7out06.odm | 457 | 1 | — |
| mm7 | out10.odm | 207, 208 | 2 | — |
| mm7 | out12.odm | 202 | 0 | 202 |
| mm7 | out14.odm | 202, 203 | 2 | — |
| mm8 | out01.odm | 101, 102, 103, 104 | 5 | — |
| mm8 | out02.odm | 101 | 1 | — |
| mm8 | out03.odm | 101, 102 | 2 | — |
| mm8 | out04.odm | 101, 102 | 0 | 101, 102 |
| mm8 | out05.odm | 101, 103 | 2 | — |
| mm8 | out06.odm | 101, 102 | 1 | 102 |
| mm8 | out07.odm | 102 | 1 | — |
| mm8 | out08.odm | 101, 102 | 0 | 101, 102 |
| mm8 | out13.odm | 101 | 1 | — |

MM7 has 11 referenced callbacks in nine maps; MM8 has 17 in nine maps. Six declarations have no matching
visible interactive site in the base geometry. They do not receive fabricated markers: MM7 out12:202 and
MM8 out04:101/102, out06:102, out08:101/102 need a live/scene/inherited-decoration binding audit. One callback
can have multiple physical sites. The shared exporter also emits 12 MM6 callbacks in eight maps; those receive
data validation, rather than new MM6 UI acceptance.

The follow-up scene audit also checked the mounted `maps/out12.scene.yml` (MM7) and
`maps/out04.scene.yml`, `out06.scene.yml`, `out08.scene.yml` (MM8). None contains a matching event/trigger
binding for these six callbacks, including entity and initial-state fields. Thus applying the packaged scene
alone does not recover the missing positions. This is an asset/binding investigation, not grounds to invent
an Atlas-only location or to delete an apparently unused callback.

The outdoor inherited-decoration classifier covers barrels, cauldrons, trash, campfires, casks and crystals;
it does not classify these as local well callbacks. Local callback declarations and MM8 decompiled EVT
behavior confirm the intended effects, but do not establish physical coordinates. Live mechanism/save
state and original map/import provenance still require checking before repairing authoritative geometry.
This audit does not certify all inherited-decoration Atlas content.

Discovery now retains actually observed synchronous numeric/condition outcomes through the shared Core site
interaction hook, including at sites without journal notes. This separate history does not expose unexecuted
branches or turn a generated reference into party knowledge. Deferred/spell-buff/item outcomes remain open.

The current projection supports acyclic numeric character/resource/resistance comparisons, HP/SP fullness, simple
local and per-character use gates, condition presence/application/removal, bounded random jumps, and nonnegative
Add/Set/Subtract on supported writable stats/resources. It follows each branch and omits
feedback-only paths and unreachable actions. Map-load/timer continuations remain separate from the interaction.
Comparison-chain recognition also stops on cycles and preserves the normal labelled callback output.
Callback replacement/removal invalidates the declaration; mods may redeclare it or clear it with empty text.
Typed drink declarations may retain an empty effect reference so an unsupported callback still has an honest
interaction category. Random branches describe possibilities without sampling RNG or claiming probabilities.

The effect reference is omitted for unrepresented reachable behavior: character-selection changes,
spells, damage instructions, quest/item/class predicates, unknown operations, loops, comparisons after mutations,
and unsupported writes. A flag assigned 1 is not an incrementing multi-use counter. These are remaining content/API
work; this generated set does not establish complete well/shrine effect coverage.

Two unhinted Mount Nighon callbacks (out10:207/208) had generic context metadata despite being physical wells.
An explicit out10_zz_site_context.lua overlay restores their interaction category. Shared journal rows 280/281
now say 25 HP/SP, matching executed callbacks; their old text incorrectly said 50.

Separate existing acceptance debt: the wider dimensional-travel fountain regression still fails six Castle
Harmondale water-face event bindings in the available 7d29 payload. Earlier baseline-fixture and matching-bake
logs already contain this failure. This checkpoint does not repair or certify that gate; it remains part of
the connection/content audit. The original assertion is preserved.


## Conditional drink follow-up, 2026-10-23

The full 137-map physical inventory identified ten previously generic drink callbacks: MM7 7out06:460,
7out13:214, d02:451, 7d12:451, 7d24:196/197/198 and 7d25:452; MM8 d20:451 and d35:451. They now have
callback-owned drink context and Guide references, with nine MM7 and three MM8 real physical sites.
Repeated event IDs at separate pools retain separate discovery identity.

The real-catalog test validates all ten callbacks, actual physical Water features, nonempty active-game
references and Discovery/Guide separation. Windows checks exercised conditional paralysis versus high Endurance,
temporary Body resistance +25 once per location reset, all seven temporary primary stats +25 once per character,
and an MM8 status-only pool with no invented bonus. Five native UI tours and five post-UI save/roundtrip checks
validate observed-only findings and untouched neighboring sites. The broader site/content audit remains open.

The active merged 7d12 callback applies Paralysis; the readable GrayFace D12 reference names PoisonedRed.
This checkpoint describes the active callback and does not alter generated gameplay or silently import vanilla
condition numbering. Resolving that existing exporter/gameplay discrepancy is separate work.


## Active special-interaction follow-up, 2026-10-24

Seventeen callback-owned typed declarations now cover seven Red Dwarf Mines rescues, five Murmurwoods statues,
Escaton's Crystal and four elemental prisons. Their mounted geometry contains twelve statue sites and twenty-two
keyed-entrance components. They use the active Merge overlays: dwarves require the retained Elixir; MM8 statues
accept an expended Stone to Flesh item or the actual spell; opened gates retain their source-map flags. Base EVT
alone is insufficient because those callbacks are replaced. Names resolve through the current shared tables.

The 137-map interaction inventory now counts either generated text or a typed interaction declaration in its
reference column. The dedicated catalog test checks all seventeen definitions against real physical sites,
item/NPC/spell/destination tables, event-to-entity keys and Discovery/Guide gates. These are reference possibilities,
not forecasts of remote availability or a claim that all special interactions have been catalogued.
