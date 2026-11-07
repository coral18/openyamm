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
branches or turn a generated reference into party knowledge. Synchronous identified spell-buff changes are now recorded by the shared site observer; deferred effects,
unidentified spell sources, queued item delivery outside the observation window and item transformations remain open.

The current projection supports acyclic numeric character/resource/resistance comparisons, HP/SP fullness, simple
local and per-character use gates, guarded source-local remaining uses, condition presence/application/removal,
bounded random jumps, and nonnegative
Add/Set/Subtract on supported writable stats/resources. It follows each branch and omits
feedback-only paths and unreachable actions. Map-load/timer continuations remain separate from the interaction.
Comparison-chain recognition also stops on cycles and preserves the normal labelled callback output.
Callback replacement/removal invalidates the declaration; mods may redeclare it or clear it with empty text.
Typed drink declarations may retain an empty effect reference so an unsupported callback still has an honest
interaction category. Random branches describe possibilities without sampling RNG or claiming probabilities.

The effect reference is omitted for unrepresented reachable behavior: character-selection changes,
spells, unsupported damage targets/types, general quest/item/class predicates, unknown operations, loops,
comparisons after mutations,
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

## Elemental prison component grouping, 2026-10-24

The 21 base components for pbp:502/503/504/505 now form four explicit callback-owned passage groups. All raw
physical sites remain in the inventory and retain their discovery keys; World Database features/locations expose
one representative per declared group. The real-catalog regression validates every member and verifies that one
member's discovery does not reveal any other prison. Other multipart entrance bindings still require an audit.


## Packaged courtyard binding follow-up, 2026-10-24

The former Castle Harmondale failure was an incomplete Windows development overlay. Its base BLV and scene
matched current source, but the existing upstream `7d29_dimension_travel.scene.yml` supplement was absent from
the public bootstrap payload and the local overlay list. The Windows wrapper now includes that exact source
file. The original regression passes all 71 assertions, including all six bindings and restored/unrestored
runtime state. The Atlas regression finds one connected passage and recognizes interaction at every water face.
No source geometry, gameplay callback, lighting or save format was changed. Full-source Android packaging already
collects this file; this follow-up does not establish Android/device acceptance or complete Gate destinations.

The six well declarations remain unresolved. Their four ODM/base-scene pairs match Git blobs exactly; all
source scene-overlay filenames are present in the mounted release/overlay packages. Raw inspection finds no
matching event in any model face's cog fields or any sprite's primary/secondary event fields, including hidden
geometry. MM7 out12's authored well uses event 201 and its wishing surface 205, rather than 202. MM8 out06's
well uses 101; its fountains use 104/449, rather than the missing 102. The readable GrayFace EVT references
confirm the six callback effects but supply no coordinates. This rules out the visible-only site extractor and
missing small supplements as explanations; it does not justify inventing a placement or declaring the callbacks
unused in every possible runtime state. Keep their content/binding audit open.


## Synchronous spell-effect observations, 2026-10-25

Actual site interactions now retain applied, updated and removed identified spell effects, separately for party
and character buffs. This extends discovery history, not the generated Guide prediction language. Timed expiry
is excluded using before/after game time; roster replacements do not establish character attribution. Composite
spells aggregate observed component changes without claiming the entire enchantment was applied or removed.
Unidentified source ID zero does not become a guessed spell or prevent independent numeric findings.

The six unbound well declarations above still require original-map/import provenance before any placement repair.
The new spell controls use explicitly synthetic private callbacks at existing MM7/MM8 well surfaces; they do not
change ordinary campaign well rewards or resolve those six missing bindings.


## Journal bookkeeping and real well rewards, 2026-10-26

The exporter now reduces equal-effect paths separated only by journal bookkeeping. It keeps genuine named
journal requirements, rejects unnamed remaining requirements and rejects post-mutation predicates unless the
journal predicate disappears completely. Repeated stable contradictions prune only unreachable paths. Existing
visit/outcome/text bounds remain in force, and no callback runs during generation or browsing.

This adds 21 complete references with actual physical bindings: MM7 7out03:203/204/208/209/210,
7out04:204, 7out05:203/204/205/207, 7out06:461, 7out13:202/203/204, out10:203/204/206, out11:321,
out14:204/205; MM8 out07:101. No previous reference ID is removed. The three unhinted Mount Nighon callbacks
out10:203/204/206 now have explicit well/fountain categories in the existing non-generated context overlay;
normal ODM surfaces already provide their locations. The Murmurwoods callback applies severe poison; it is not
presented as a cure merely because its SetValue argument is zero.

Testing the Tatalia well exposed an actual Core bug: Add/Subtract ArmorClassBonus used generic event variable 8
instead of the character armor modifier. Shared arithmetic now updates that signed modifier with bounded
intermediates. The existing save field is unchanged. Previously consumed personal-use bits are not refunded
from ambiguous historical generic variables; affected old saves need an explicit migration with provenance.
The six original missing well bindings remain open and receive no invented markers.


## Actual item outcome follow-up, 2026-10-26

The existing Core observer now retains synchronous net changes in carried item quantities, including equipment
and the cursor stack, through the same saved site namespace. This is separate from Guide predictions and the
existing reward-origin index. Actual Tatalia altar and Murmurwoods statue checks record the statuette/scroll cost;
a private modded-well control checks successful queued delivery of two unidentified swords. That private reward
is not installed into normal campaign assets. Native search/read pages use the active item table and party
identification state. No cancelled, unchanged or mere pack/cursor transfer becomes a reward.

This does not make unsupported item/quest predicates exportable Guide effects. Deferred/inherited queued grants
after the observation window, instance transformations and the six unresolved physical well bindings remain open.


## Confirmed inherited-object receipts, 2026-10-26

The site page/search now reads confirmed item receipts from the existing reward-origin owner, matching the
same map and exact inherited slot. This makes queued deliveries visible after the immediate callback snapshot,
including when the callback depletes its object first. Pending/authored rewards remain excluded. Actual item
types use current names with identification gates; repeated types across callback variants show the last
recorded day once. The existing receipt schema has no quantity, so quantities and net deltas are not inferred.
The saved immediate snapshot remains unchanged. Broader deferred attribution and all six missing well bindings
remain open. Private callback controls are documented in the fixture README and ATLAS.md.


## Active binding and runtime-mutability check, 2026-10-29

The active asset loader rechecks the four affected maps with their current scenes and event overlays.
All six local callback references are present, but their event IDs occur in neither raw scripted decoration
bindings nor raw model-surface trigger IDs, including hidden surfaces. The Atlas emits no located feature
for them. The focused case `well references without active physical bindings*` passes 40 assertions and
also enumerates 46 inherited decoration slots using the active decoration table and initial map state.

The supported runtime mutators do not supply the missing local binding: `luaSetSprite` stores only visibility
and texture overrides; `luaSetFacetBit` changes facet masks. `EventRuntimeState::SpriteOverride` has no event-ID
field. Inherited decoration dispatch selects a global event from its declared family/range, rather than
assigning a missing local well callback; the family registry contains no well family. This narrows the known
active-engine explanation to map/import content, not a visibility-only Atlas extractor or a normal late event
reassignment. It does not certify future mods or establish the intended original coordinates.

Original map/import provenance is still required before changing geography. The readable GrayFace callbacks
establish effects, not placement. No Atlas-only pin, invented coordinate, generated event edit or gameplay
asset change is introduced. The test permits a future authoritative map repair to supply an actual binding.
Private evidence is retained in `atlas-well-bindings-20261029` under the Windows development root.


## Local source-archive comparison, 2026-11-07

The original-input search now has concrete local sources. The MM7 base `DATA/GAMES.LOD` matches its GOG
installer hash database (`1207658916`, MD5 `c651ef69758771ca97fd3fb042721af5`). Its version-7 out12 map
has no binding for local callback 202 in any examined model cog field or sprite event field. The installed
Merge version-8 out12 member is byte-identical to the current OpenYAMM map; neither contains that binding.
This rules out losing a direct 202 binding during that map's Merge-to-OpenYAMM import. It does not establish
that no original interpreter, patch, save or future mod could invoke the callback indirectly.

The three installed MM8 map members likewise contain none of the five target bindings. They differ in geometry
from the current port's maps. This installation contains Merge/Redone, and its available GOG hash database
covers six localization/support files, not `games.lod`; its archive is therefore **not authenticated as pristine
MM8**. These comparisons add a source variant, not proof about every original MM8 release or patch.

The machine-readable fingerprints, raw map versions/counts, target IDs and qualification are retained in
[the source receipt](tests/fixtures/atlas/well_source_provenance_20261107.json). Nine map variants were decoded. The four
active map fingerprints match their current Git blobs. Positive event controls retain out12's well/wishing
surface 201/205 and Shadowspire's active 101; the negative result is not a parser that reports every event absent.
Original archives and extracted map bytes stay outside Git, in the Windows development evidence folder
`atlas-original-well-bindings-20261107/`. Installed games were opened read-only.

These six declarations still lack direct physical bindings in the active world and receive no fabricated
Atlas coordinates. Assigning their effects to a chosen object would be world/gameplay authoring; that proposal
is separately recorded for the user's later decision. A future demonstrated import or dispatch bug remains a
normal authorized fix, rather than a reason to invent an Atlas-only placement. The registry retains the
references, and the existing binding regression permits a real source repair to supply a binding.

## Emerald Island remaining-use wells, 2026-11-07

The current 137-map inventory found 22 physically placed well/fountain/shrine/drink callbacks without a
complete Guide effect reference. Three are now explained: MM7 `7out01:112/113/114`, restoring 5 HP, restoring
5 SP, and adding 2 permanent Luck when base Luck is below 15. These consume a shared source-local reserve,
not a per-character use flag. The exporter follows the guarded decrement and preserves the Luck threshold.
It does not infer initial stock, replenishment timing or current remaining stock from the interaction alone.
Multiple reserve variables, replenishment within the callback, unsupported decrements and post-write
requirements remain rejected rather than producing a partial description.

Only `7out01_00_site_refs.lua` changed on MM7/MM8 metadata regeneration. Gameplay callbacks and save formats
are unchanged. The real-EVT regression exercises zero/one/two remaining uses, Luck 14/15, repeated use and
the unaffected second party member; separate negative fixtures cover unsupported reserve patterns.
The physical-site integration test includes all three Water features and their Guide/Discovery separation.
The Windows native tour `tools/menu_tours/atlas_mm7_well_reserve.yml` checks the Luck reference in its reader;
Discovery map and result-list crops match before and after Guide. Other unrepresented callbacks remain open.

## Town Portal and feedback-only fountains, 2026-11-07

The exporter now reads destination names from the shared `town_portal_switch.txt` through the existing typed
table loader. It resolves canonical QBits with the same per-map import aliases as gameplay. Only explicitly
defined destination bits can be described as Town Portal unlocks; an arbitrary quest-table comment cannot
define one. A destination shared by several rows retains all distinct names. Unknown bits, clearing an
unlock, and testing an unlock after rewriting it remain unsupported. A destination write can precede an
independent HP/SP or resource check, without pretending the written bit is a pre-interaction condition.

The four active MM8 fountains are Alvar `out03:104`, Shadowspire `out06:104`, Regna `out13:104` and Balthazar's
Lair `d24:451`. They unlock party travel even when HP/SP is full; restorative effects retain their own
conditions. The generated legacy Ravenshore `out02:104` description includes its gold condition, but the
active Merge dimensional-travel replacement invalidates it. The physical integration regression verifies
that the stale gold-fountain reference does not survive that replacement.

Eleven feedback-only callbacks also get an explicit statement that they define no attribute/condition change.
This includes three previously undescribed placed wells: MM7 `7d22:197`, MM8 `out03:103` and `out13:102`.
The other declarations do not invent physical sites. Bare hints without feedback stay undescribed, and a
feedback message cannot hide a reachable unsupported effect. Regeneration changes eleven MM7/MM8 metadata
files, adding sixteen declarations; gameplay handlers, geometry and save formats remain unchanged.

The refreshed 137-map inventory leaves twelve recognized, physically indexed water/shrine/drink callbacks
without a complete reference, all in MM7. This count is scoped to that interaction inventory, not full Atlas
completion. Native Alvar verification uses `mm8_portal_fountain_reference.yml`, the corresponding menu tour,
and `mm8_portal_fountain_reference_verify.yml`: reading and scrolling the Guide leaves QBit 301 clear after
quicksave, load, save and another load. Discovery map/list crops remain unchanged.

## Bracada wishing well and altar mechanisms, 2026-11-07

MM7 `7out06:459` now has a complete read-only projection of its declared callback branches: fourteen outcomes,
including the selected-character damage instruction, temporary bonuses, conditions and gold changes. Currency
subtraction includes the Lua formatter's actual early-return affordability guard. Below 99 gold there is no
charge or random selection; 99 gold buys the feedback-only branch; the random branch requires at least 100.
Currency bounds prune contradictory guard paths. Effects before a failed payment are retained, and unsupported
post-write currency tests/payments still reject the whole reference. No gameplay handler or RNG was changed.

The altar `t02:13` now describes its local door toggle. Door action IDs remain part of internal outcome identity,
so identical wording cannot merge branches that affect different doors. The legacy `7d10:6` "Take a Drink"
callback also receives a mechanism description, but its active context remains `open_door`; it does not become
an Atlas well. No positions or geometry were added. Six MM7 metadata files change, including the clarified
wording that random references list defined branches rather than evaluated live probabilities.

The current interaction inventory now requires nonempty text or a typed interaction reference, rather than
counting an empty drink declaration as a complete description. Ten recognized placed MM7 water/shrine/drink
callbacks still lack such a reference; MM8 has none in this scoped inventory. This remains narrower than
complete Atlas acceptance. The native 28-step Bracada tour checks scrolling to the damage and 99-gold branches;
two inspected saves retain the party, gold, food and map, with matching Discovery map/list crops. A five-step
post-UI roundtrip retains 5,000 gold.

Accuracy limits remain explicit: `luaDamagePlayer` currently ignores its damage-type argument; the reference
therefore describes the amount/selected target without inventing an elemental mitigation calculation. The
original readable OUT06 reference names Insanity, Curse and Death where the active generated callback names
Mild disease, Sleep and Eradication. The Steadwick cure callback also has a raw-selector discrepancy. These
are existing import/gameplay investigations, not reasons to silently rewrite their effects in the Atlas.
Additionally, the current `_RandomJump` selects with the low bits of an LCG; successive nested choices can be
correlated (a two-way choice followed by a four-way choice does not prove all child branches reachable).
The Guide lists the callback's defined branches and does not certify their runtime distribution/reachability.
