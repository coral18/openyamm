#include "doctest/doctest.h"

#include "game/events/EventAttributeInfo.h"
#include "game/gameplay/InteractiveDecorationRules.h"
#include "game/events/EventRuntime.h"
#include "game/party/Party.h"
#include "game/party/EventSpellBuffs.h"
#include "game/tables/SpellTable.h"
#include "game/world/WorldMapKnowledge.h"
#include "game/world/WorldSiteObservations.h"
#include "tests/PartySpellTestHarness.h"

#include <stdexcept>

using namespace OpenYAMM::Game;

namespace
{
const std::string Site = "bmodel/38/surface/10";
const std::string Root = "openyamm.discovery.site-effects.v1/bmodel/38/surface/10/event/223/";
WorldSiteValueSnapshot gold(int32_t value)
{
    return {{"Hero"}, {{"Gold", std::nullopt, value}}};
}

TEST_CASE("site observations persist actual party and character spell effects using active spell names")
{
    Party party;
    party.seed(Party::createDefaultSeed());
    EventRuntimeState state;
    const auto before = captureWorldSiteValues(state, party, 42);
    REQUIRE(tryApplyEventSpellBuffs(party, 38, 4, 3));
    REQUIRE(tryApplyEventSpellBuffs(party, 46, 4, 3));
    const auto after = captureWorldSiteValues(state, party, 42);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, after, 42));
    auto observations = worldSiteObservations(state);
    REQUIRE(observations.size() == 1);
    REQUIRE(observations[0].spells.size() == 2);
    CHECK(observations[0].lastChanged);
    CHECK(state.namedMapVars.at(Root + "spell/party/38/actions") == WorldSiteSpellObservation::Applied);
    CHECK(state.namedMapVars.at(Root + "spell/character/46/actions") == WorldSiteSpellObservation::Applied);
    SpellTable spells;
    REQUIRE(spells.loadFromRows({{"38", "", "Custom Stoneskin", "Earth", "", ""},
        {"46", "", "Custom Bless", "Spirit", "", ""}}));
    const std::string text = describeWorldSiteObservation(observations[0], &spells);
    CHECK(text.find("Applied effect from: Custom Stoneskin") != std::string::npos);
    CHECK(text.find("Applied effect from: Custom Bless") != std::string::npos);
    CHECK(text.find("at least one character") != std::string::npos);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, after, after, 43));
    CHECK_FALSE(worldSiteObservations(state)[0].lastChanged);
    CHECK(worldSiteObservations(state)[0].spells.size() == 2);
    EventRuntimeState refilled;
    restoreWorldMapKnowledgeAfterRefill(state, refilled);
    CHECK(worldSiteObservations(refilled)[0].spells.size() == 2);
    CHECK(refilled.namedGlobalVars.empty());
    EventRuntimeState unidentified;
    auto unknown = gold(0), unknownAfter = gold(200);
    unknown.buffs.push_back({std::nullopt, 0, 0, 120, 1, 0, 0});
    unknownAfter.buffs = unknown.buffs;
    REQUIRE(recordWorldSiteObservation(unidentified, Site, 223, unknown, unknownAfter, 43));
    REQUIRE(worldSiteObservations(unidentified)[0].effects.size() == 1);
    CHECK(worldSiteObservations(unidentified)[0].spells.empty());
}

TEST_CASE("site spell observations distinguish refresh removal expiry and roster replacement")
{
    Party party;
    party.seed(Party::createDefaultSeed());
    EventRuntimeState state;
    party.applyPartyBuff(PartyBuffId::Stoneskin, 120, 10, 38, 5, SkillMastery::Master, 0);
    auto before = captureWorldSiteValues(state, party, 10);
    party.advanceTimedStates(60);
    auto decayed = captureWorldSiteValues(state, party, 11);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, decayed, 11));
    CHECK(worldSiteObservations(state)[0].spells.empty());
    party.applyPartyBuff(PartyBuffId::Stoneskin, 120, 10, 38, 5, SkillMastery::Master, 0);
    auto refreshed = captureWorldSiteValues(state, party, 11);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, decayed, refreshed, 11));
    CHECK(state.namedMapVars.at(Root + "spell/party/38/actions") == WorldSiteSpellObservation::Updated);
    party.clearPartyBuff(PartyBuffId::Stoneskin);
    auto removed = captureWorldSiteValues(state, party, 11);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, refreshed, removed, 11));
    CHECK(state.namedMapVars.at(Root + "spell/party/38/actions")
        == (WorldSiteSpellObservation::Updated | WorldSiteSpellObservation::Removed));
    EventRuntimeState expiry;
    party.applyPartyBuff(PartyBuffId::Stoneskin, 5, 10, 38, 5, SkillMastery::Master, 0);
    before = captureWorldSiteValues(expiry, party, 11);
    party.advanceTimedStates(60);
    const auto expired = captureWorldSiteValues(expiry, party, 12);
    REQUIRE(recordWorldSiteObservation(expiry, Site, 223, before, expired, 12));
    CHECK(worldSiteObservations(expiry)[0].spells.empty());
    auto oldRoster = gold(0), newRoster = gold(0);
    oldRoster.buffs.push_back({0, 0, 46, 120, 10, 5, 3});
    newRoster.members[0] = "Replacement";
    newRoster.buffs.push_back({0, 0, 46, 240, 10, 5, 3});
    REQUIRE(recordWorldSiteObservation(expiry, "surface/other", 223, oldRoster, newRoster, 12));
    CHECK_FALSE(expiry.namedMapVars.contains(
        "openyamm.discovery.site-effects.v1/surface/other/event/223/spell/character/46/actions"));
}

TEST_CASE("site spell observations aggregate composite spell components without claiming the whole buff was removed")
{
    Party party;
    party.seed(Party::createDefaultSeed());
    EventRuntimeState state;
    const auto before = captureWorldSiteValues(state, party);
    REQUIRE(tryApplyEventSpellBuffs(party, 85, 4, 3));
    const auto applied = captureWorldSiteValues(state, party);
    CHECK(applied.buffs.size() > 1);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, applied, 1));
    REQUIRE(worldSiteObservations(state)[0].spells.size() == 1);
    party.clearPartyBuff(PartyBuffId::FireResistance);
    CHECK(party.hasPartyBuff(PartyBuffId::MindResistance));
    const auto partial = captureWorldSiteValues(state, party);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, applied, partial, 1));
    CHECK(state.namedMapVars.at(Root + "spell/party/85/actions")
        == (WorldSiteSpellObservation::Applied | WorldSiteSpellObservation::Removed));
    CHECK(describeWorldSiteObservation(worldSiteObservations(state)[0]).find("Removed effect from:")
        != std::string::npos);
}

TEST_CASE("site spell observations reject malformed snapshots atomically and flag corrupt saved spell keys")
{
    const auto before = gold(0);
    for (int variant = 0; variant < 5; ++variant)
    {
        auto after = gold(200);
        after.buffs.push_back({std::nullopt, 0, 38, 120, 10, 5, 3});
        if (variant == 0) after.buffs[0].mastery = 255;
        if (variant == 1) after.buffs[0].member = 10;
        if (variant == 2) after.buffs[0].remainingSeconds = -1;
        if (variant == 3) after.buffs[0].slot = 255;
        if (variant == 4) after.buffs.push_back(after.buffs[0]);
        EventRuntimeState state;
        CHECK_FALSE(recordWorldSiteObservation(state, Site, 223, before, after, 1));
        CHECK(state.namedMapVars.empty());
    }
    for (const char *pKey : {"party/0/actions", "party/038/actions", "party/38/extra", "invalid/38/actions"})
    {
        EventRuntimeState state;
        REQUIRE(recordWorldSiteObservation(state, Site, 223, before, before, 1));
        state.namedMapVars[Root + "spell/" + pKey] = 1;
        CHECK_FALSE(worldSiteObservations(state)[0].error.empty());
        const auto saved = state.namedMapVars;
        CHECK_FALSE(recordWorldSiteObservation(state, Site, 223, before, gold(200), 2));
        CHECK(state.namedMapVars == saved);
    }
}
}

TEST_CASE("site observations retain actual gold gain across no-change visits and map refill")
{
    EventRuntimeState state;
    REQUIRE(recordWorldSiteObservation(state, Site, 223, gold(0), gold(200), 0));
    auto observations = worldSiteObservations(state);
    REQUIRE(observations.size() == 1);
    REQUIRE(observations[0].effects.size() == 1);
    CHECK(observations[0].effects[0].attribute == "Gold");
    CHECK(observations[0].effects[0].minDelta() == 200);
    CHECK(observations[0].effects[0].maxDelta() == 200);
    CHECK(describeWorldSiteObservation(observations[0]).find("Party gold +200") != std::string::npos);
    CHECK(state.namedMapVars.at(Root + "stat/Gold/min-before") == 0);
    CHECK(state.namedMapVars.at(Root + "stat/Gold/min-after") == 200);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, gold(200), gold(200), 1440));
    observations = worldSiteObservations(state);
    CHECK_FALSE(observations[0].lastChanged);
    CHECK(observations[0].effects[0].minDelta() == 200);
    CHECK(describeWorldSiteObservation(observations[0]).find("No additional changes recorded") != std::string::npos);
    EventRuntimeState refilled;
    restoreWorldMapKnowledgeAfterRefill(state, refilled);
    CHECK(worldSiteObservations(refilled)[0].effects[0].maxDelta() == 200);
    CHECK(refilled.variables.empty());
}

TEST_CASE("site observations preserve endpoints and record per-character ranges without inventing attribution")
{
    EventRuntimeState state;
    WorldSiteValueSnapshot before{{"A", "B"}, {{"CurrentHealth", 0, 95}, {"CurrentHealth", 1, 20}}};
    WorldSiteValueSnapshot after{{"A", "B"}, {{"CurrentHealth", 0, 100}, {"CurrentHealth", 1, 45}}};
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, after, 1));
    auto observed = worldSiteObservations(state);
    REQUIRE(observed.size() == 1);
    REQUIRE(observed[0].effects.size() == 1);
    CHECK(observed[0].effects[0].minDelta() == 5);
    CHECK(observed[0].effects[0].maxDelta() == 25);
    CHECK(describeWorldSiteObservation(observed[0]).find("Health +5 to +25 (observed range)") != std::string::npos);
    before = after;
    after.values[0].value = 75;
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, after, 2));
    observed = worldSiteObservations(state);
    CHECK(observed[0].effects[0].minDelta() == -25);
    CHECK(observed[0].effects[0].maxDelta() == 25);
    CHECK(describeWorldSiteObservation(observed[0]).find("Character changes observed") != std::string::npos);
}

TEST_CASE("site observations do not turn unchanged visits or roster replacements into rewards")
{
    EventRuntimeState state;
    auto before = gold(0), after = gold(0);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, after, 1));
    CHECK(worldSiteObservations(state)[0].effects.empty());
    before.values.push_back({"BaseMight", 0, 15});
    after.values.push_back({"BaseMight", 0, 200});
    after.members[0] = "Replacement";
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, after, 2));
    CHECK(worldSiteObservations(state)[0].effects.empty());
    CHECK_FALSE(worldSiteObservations(state)[0].lastChanged);
    CHECK(worldSiteObservations(state)[0].error.empty());
}

TEST_CASE("site observation capture uses the same runtime attribute reads as gameplay")
{
    Party party;
    party.seed(Party::createDefaultSeed());
    EventRuntimeState state;
    const auto before = captureWorldSiteValues(state, party);
    const int initial = party.gold();
    party.addGold(200);
    const auto after = captureWorldSiteValues(state, party);
    CHECK(party.gold() == initial + 200);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, after, 1));
    REQUIRE(worldSiteObservations(state)[0].effects.size() == 1);
    CHECK(worldSiteObservations(state)[0].effects[0].attribute == "Gold");
    CHECK(worldSiteObservations(state)[0].effects[0].minDelta() == 200);
    CHECK(state.namedGlobalVars.empty());
    CHECK(state.variables.empty());
    CHECK(eventAttributeInfo("ActualMight") != nullptr);
    CHECK_FALSE(eventAttributeInfo("ActualMight")->writable);
}

TEST_CASE("site observations reject malformed input atomically and expose incomplete saved records")
{
    EventRuntimeState state;
    CHECK_FALSE(recordWorldSiteObservation(state, "", 223, gold(0), gold(200), 1));
    CHECK_FALSE(recordWorldSiteObservation(state, Site, 0, gold(0), gold(200), 1));
    CHECK_FALSE(recordWorldSiteObservation(state, Site, 223, gold(0), gold(200), -1));
    CHECK(state.namedMapVars.empty());
    REQUIRE(recordWorldSiteObservation(state, Site, 223, gold(0), gold(200), 1));
    state.namedMapVars.erase(Root + "stat/Gold/min-after");
    const auto fields = state.namedMapVars;
    auto observations = worldSiteObservations(state);
    REQUIRE(observations.size() == 1);
    CHECK_FALSE(observations[0].error.empty());
    CHECK(describeWorldSiteObservation(observations[0]).find("Incomplete") != std::string::npos);
    CHECK_FALSE(recordWorldSiteObservation(state, Site, 223, gold(0), gold(300), 2));
    CHECK(state.namedMapVars == fields);
    state.namedMapVars.erase(Root + "seen");
    observations = worldSiteObservations(state);
    REQUIRE(observations.size() == 1);
    CHECK_FALSE(observations[0].error.empty());
}

TEST_CASE("site observation endpoint storage does not overflow signed delta arithmetic")
{
    EventRuntimeState state;
    WorldSiteValueSnapshot before{{"A"}, {{"CurrentHealth", 0, INT32_MIN}}};
    WorldSiteValueSnapshot after{{"A"}, {{"CurrentHealth", 0, INT32_MAX}}};
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, after, 1));
    const auto observations = worldSiteObservations(state);
    REQUIRE(observations[0].effects.size() == 1);
    CHECK(observations[0].effects[0].maxDelta() == INT64_C(4294967295));
}


TEST_CASE("observed healing uses actual clamped health and includes visible skill and resistance gains")
{
    Party party;
    party.seed(Party::createDefaultSeed());
    Character *pMember = party.member(0);
    REQUIRE(pMember);
    pMember->maxHealth = 100;
    pMember->health = 95;
    EventRuntimeState state;
    const auto before = captureWorldSiteValues(state, party);
    EventRuntime::addVariableValue(state, EventRuntime::decodeVariable(uint32_t(EvtVariable::CurrentHealth)),
        25, &party, {0});
    EventRuntime::addVariableValue(state, EventRuntime::decodeVariable(uint32_t(EvtVariable::NumSkillPoints)),
        2, &party, {0});
    EventRuntime::addVariableValue(state, EventRuntime::decodeVariable(uint32_t(EvtVariable::FireResistanceBonus)),
        10, &party, {0});
    const auto after = captureWorldSiteValues(state, party);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, after, 1));
    const auto observations = worldSiteObservations(state);
    REQUIRE(observations.size() == 1);
    const std::string text = describeWorldSiteObservation(observations[0]);
    CHECK(text.find("Health +5") != std::string::npos);
    CHECK(text.find("Health +25") == std::string::npos);
    CHECK(text.find("Skill points +2") != std::string::npos);
    CHECK(text.find("Fire resistance bonus +10") != std::string::npos);
    CHECK(eventAttributeInfo("PhysicalResistance") == nullptr);
    CHECK(eventAttributeInfo("MagicResistance") == nullptr);
}

TEST_CASE("site observations record only conditions actually applied or removed")
{
    Party party;
    party.seed(Party::createDefaultSeed());
    EventRuntimeState state;
    const auto healthy = captureWorldSiteValues(state, party);
    const auto poison = EventRuntime::decodeVariable(uint32_t(EvtVariable::PoisonedGreen));
    EventRuntime::addVariableValue(state, poison, 1, &party, {0});
    const auto poisoned = captureWorldSiteValues(state, party);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, healthy, poisoned, 1));
    const std::string applied = describeWorldSiteObservation(worldSiteObservations(state)[0]);
    CHECK(applied.find("Applied: Mild poison") != std::string::npos);
    CHECK(applied.find("Severe poison") == std::string::npos);
    EventRuntime::subtractVariableValue(state, poison, 1, &party, {0});
    const auto cured = captureWorldSiteValues(state, party);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, poisoned, cured, 2));
    const std::string history = describeWorldSiteObservation(worldSiteObservations(state)[0]);
    CHECK(history.find("Applied and removed: Mild poison") != std::string::npos);
    CHECK(history.find("Severe poison") == std::string::npos);
    CHECK(history.find("Spell points") == std::string::npos);
}


TEST_CASE("object observations retain only actual local removals and reject mismatched object bindings")
{
    Party party;
    PartySeed seed;
    seed.members.push_back(Character{});
    party.seed(seed);
    OpenYAMM::Tests::PartySpellTestWorldRuntime world;
    world.bindParty(&party);
    EventRuntimeState &state = *world.eventRuntimeState();
    state.mapFileName = "out07.odm";
    GameplayWorldHit hit;
    hit.hasHit = true;
    hit.kind = GameplayWorldHitKind::EventTarget;
    hit.eventTarget.emplace();
    hit.eventTarget->targetKind = GameplayWorldEventTargetKind::Decoration;
    hit.eventTarget->targetIndex = 1370;
    hit.eventTarget->sourceEntityIndex = 1418;
    hit.eventTarget->eventIdPrimary = 20;
    hit.eventTarget->eventIdSecondary = 132;
    hit.eventTarget->contextActionMetadata.emplace();
    hit.eventTarget->contextActionMetadata->kind = "stone_to_flesh";
    hit.eventTarget->contextActionMetadata->interactionReference.emplace();
    hit.eventTarget->contextActionMetadata->interactionReference->spriteId = 20;
    const std::string root = "openyamm.discovery.site-effects.v1/decoration/1418/event/132/";
    {
        const WorldSiteInteractionObservation observation(world, hit);
        observation.finish(world, true);
    }
    REQUIRE(state.namedMapVars.contains(root + "object-removed"));
    CHECK(state.namedMapVars.at(root + "object-removed") == 0);
    {
        const WorldSiteInteractionObservation observation(world, hit);
        state.spriteOverrides[20].hidden = true;
        observation.finish(world, false);
    }
    CHECK(state.namedMapVars.at(root + "object-removed") == 0);
    state.spriteOverrides.clear();
    {
        const WorldSiteInteractionObservation observation(world, hit);
        state.spriteOverrides[20].hidden = true;
        observation.finish(world, true);
    }
    auto observations = worldSiteObservations(state);
    REQUIRE(observations.size() == 1);
    CHECK(observations[0].objectRemoved == true);
    CHECK(describeWorldSiteObservation(observations[0]).find("Observed object removed") != std::string::npos);
    {
        const WorldSiteInteractionObservation observation(world, hit);
        observation.finish(world, true);
    }
    CHECK(worldSiteObservations(state)[0].objectRemoved == true);
    CHECK_FALSE(worldSiteObservations(state)[0].lastChanged);
    state.namedMapVars[root + "object-removed"] = 2;
    CHECK_FALSE(worldSiteObservations(state)[0].error.empty());
    state.namedMapVars.clear();
    state.spriteOverrides.clear();
    hit.eventTarget->contextActionMetadata->interactionReference->spriteId = 21;
    {
        const WorldSiteInteractionObservation observation(world, hit);
        state.spriteOverrides[21].hidden = true;
        observation.finish(world, true);
    }
    CHECK_FALSE(worldSiteObservations(state)[0].objectRemoved.has_value());
    state.namedMapVars.clear();
    {
        const WorldSiteInteractionObservation observation(world, hit);
        state.mapFileName = "out02.odm";
        state.namedMapVars.clear();
        observation.finish(world, true);
    }
    CHECK(state.namedMapVars.empty());
    hit.eventTarget->sourceEntityIndex = GameplayInvalidWorldIndex;
    {
        const WorldSiteInteractionObservation observation(world, hit);
        observation.finish(world, true);
    }
    CHECK(state.namedMapVars.empty());
}


TEST_CASE("object observations distinguish canonical entities that reuse one renderer index")
{
    GameplayEventTargetHit pressed;
    pressed.targetKind = GameplayWorldEventTargetKind::Decoration;
    pressed.targetIndex = 1370;
    pressed.sourceEntityIndex = 1418;
    GameplayEventTargetHit released = pressed;
    CHECK(pressed.isSameTarget(released));
    released.sourceEntityIndex = 1419;
    CHECK_FALSE(pressed.isSameTarget(released));
    released.sourceEntityIndex = GameplayInvalidWorldIndex;
    CHECK_FALSE(pressed.isSameTarget(released));
    released = pressed;
    released.targetIndex = 1371;
    CHECK_FALSE(pressed.isSameTarget(released));
}


TEST_CASE("site item snapshots include equipment and cursor stacks without inventing transfers "
    "or identification rewards")
{
    Party party;
    PartySeed seed = Party::createDefaultSeed();
    seed.members[0].inventory.clear();
    seed.members[0].equipment = {};
    party.seed(seed);
    InventoryItem stack;
    stack.objectDescriptionId = 42;
    stack.quantity = 3;
    stack.identified = false;
    party.setHeldItemForQueries(stack);
    EventRuntimeState state;
    const auto before = captureWorldSiteValues(state, party);
    REQUIRE(before.items.at(42) == 3);
    party.member(0)->inventory.push_back(stack);
    party.clearHeldItemForQueries();
    auto after = captureWorldSiteValues(state, party);
    CHECK(after.items == before.items);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, after, 1));
    CHECK(worldSiteObservations(state)[0].items.empty());
    const auto packed = after;
    party.member(0)->inventory[0].identified = true;
    party.member(0)->equipment.mainHand = 42;
    --party.member(0)->inventory[0].quantity;
    after = captureWorldSiteValues(state, party);
    CHECK(after.items == packed.items);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, packed, after, 2));
    CHECK(worldSiteObservations(state)[0].items.empty());
    party.member(0)->inventory[0].quantity = UINT32_MAX;
    CHECK(party.carriedItemQuantities().at(42) == uint64_t(UINT32_MAX) + 1);
    party.setHeldItemForQueries(stack);
    CHECK(party.carriedItemQuantities().at(42) == uint64_t(UINT32_MAX) + 4);
    party.seed(seed);
    CHECK(party.carriedItemQuantities().empty());
}

TEST_CASE("site item observations preserve signed 64 bit net ranges across no change visits and refill")
{
    EventRuntimeState state;
    auto before = gold(0), after = gold(0);
    after.items[42] = UINT32_MAX;
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, after, 1));
    auto observed = worldSiteObservations(state);
    REQUIRE(observed.size() == 1);
    REQUIRE(observed[0].items.size() == 1);
    CHECK(observed[0].items[0].minDelta == int64_t(UINT32_MAX));
    CHECK(observed[0].lastChanged);
    CHECK(describeWorldSiteObservation(observed[0]).find("Unknown item +4294967295") != std::string::npos);
    REQUIRE(recordWorldSiteObservation(state, Site, 223, after, before, 2));
    observed = worldSiteObservations(state);
    CHECK(observed[0].items[0].minDelta == -int64_t(UINT32_MAX));
    CHECK(observed[0].items[0].maxDelta == int64_t(UINT32_MAX));
    REQUIRE(recordWorldSiteObservation(state, Site, 223, before, before, 3));
    observed = worldSiteObservations(state);
    CHECK_FALSE(observed[0].lastChanged);
    CHECK(observed[0].items[0].minDelta == -int64_t(UINT32_MAX));
    EventRuntimeState refilled;
    restoreWorldMapKnowledgeAfterRefill(state, refilled);
    REQUIRE(worldSiteObservations(refilled)[0].items.size() == 1);
    CHECK(worldSiteObservations(refilled)[0].items[0].maxDelta == int64_t(UINT32_MAX));
    CHECK(refilled.variables.empty());
    CHECK(refilled.namedGlobalVars.empty());
    EventRuntimeState wide;
    auto empty = gold(0), full = gold(0);
    full.items[42] = uint64_t(INT64_MAX);
    REQUIRE(recordWorldSiteObservation(wide, Site, 223, empty, full, 1));
    CHECK(worldSiteObservations(wide)[0].items[0].maxDelta == INT64_MAX);
    REQUIRE(recordWorldSiteObservation(wide, Site, 223, full, empty, 2));
    CHECK(worldSiteObservations(wide)[0].items[0].minDelta == -INT64_MAX);
    EventRuntimeState reordered;
    before.members = {"One", "Two"};
    after.members = {"Two", "One"};
    after.items[42] = 2;
    REQUIRE(recordWorldSiteObservation(reordered, Site, 223, before, after, 1));
    CHECK(worldSiteObservations(reordered)[0].items[0].minDelta == 2);
    EventRuntimeState recruitment;
    after.members.push_back("New companion");
    after.values[0].value = 200;
    REQUIRE(recordWorldSiteObservation(recruitment, Site, 223, before, after, 1));
    CHECK(worldSiteObservations(recruitment)[0].items.empty());
    CHECK(worldSiteObservations(recruitment)[0].effects[0].minDelta() == 200);
}

TEST_CASE("site item observations reject malformed snapshots and corrupt records atomically")
{
    for (const auto &[id, quantity] : {std::pair{0u, uint64_t(1)}, {42u, uint64_t(0)},
        {42u, uint64_t(INT64_MAX) + 1}})
    {
        EventRuntimeState state;
        auto after = gold(200);
        after.items[id] = quantity;
        CHECK_FALSE(recordWorldSiteObservation(state, Site, 223, gold(0), after, 1));
        CHECK(state.namedMapVars.empty());
    }
    for (const char *pKey : {"0/min-lo", "042/min-lo", "42/extra", "42", "42/min-lo"})
    {
        EventRuntimeState state;
        REQUIRE(recordWorldSiteObservation(state, Site, 223, gold(0), gold(0), 1));
        state.namedMapVars[Root + "item/" + pKey] = 1;
        CHECK_FALSE(worldSiteObservations(state)[0].error.empty());
        const auto saved = state.namedMapVars;
        CHECK_FALSE(recordWorldSiteObservation(state, Site, 223, gold(0), gold(200), 2));
        CHECK(state.namedMapVars == saved);
    }
}


TEST_CASE("decoration initialization preserves depleted zero states and resets only with fresh map state")
{
    EventRuntimeState state;
    MapDeltaData original;
    REQUIRE(beginInteractiveDecorationInitialization(state, &original));
    CHECK(state.namedMapVars.empty()); // No assets/bindings ready: initialization has not happened yet.
    state.decorVars[0] = 1;
    markInteractiveDecorationStateInitialized(state);
    state.decorVars.fill(0);
    EventRuntimeState restored = state;
    CHECK_FALSE(beginInteractiveDecorationInitialization(restored, &original));
    CHECK(restored.decorVars == state.decorVars);
    CHECK(restored.namedMapVars == state.namedMapVars);
    EventRuntimeState refilled;
    restoreWorldMapKnowledgeAfterRefill(state, refilled);
    CHECK(beginInteractiveDecorationInitialization(refilled, &original));
    CHECK(refilled.namedMapVars.empty());
    EventRuntimeState legacy;
    legacy.decorVars[5] = 3;
    CHECK_FALSE(beginInteractiveDecorationInitialization(legacy, &original));
    CHECK(legacy.decorVars[5] == 3);
    legacy.decorVars.fill(0);
    CHECK_FALSE(beginInteractiveDecorationInitialization(legacy, nullptr));
    EventRuntimeState authored;
    original.eventVariables.decorVars[4] = 2;
    CHECK_FALSE(beginInteractiveDecorationInitialization(authored, &original));
    CHECK(original.eventVariables.decorVars[4] == 2);
    CHECK(authored.decorVars[4] == 0);
    authored.namedMapVars["openyamm.decorations.initialized.v1"] = 2;
    const auto corrupt = authored.namedMapVars;
    CHECK_THROWS_AS(beginInteractiveDecorationInitialization(authored, &original), std::invalid_argument);
    CHECK(authored.namedMapVars == corrupt);
}
