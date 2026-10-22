#include "doctest/doctest.h"

#include "game/events/EventAttributeInfo.h"
#include "game/events/EventRuntime.h"
#include "game/party/Party.h"
#include "game/world/WorldMapKnowledge.h"
#include "game/world/WorldSiteObservations.h"

using namespace OpenYAMM::Game;

namespace
{
const std::string Site = "bmodel/38/surface/10";
const std::string Root = "openyamm.discovery.site-effects.v1/bmodel/38/surface/10/event/223/";
WorldSiteValueSnapshot gold(int32_t value)
{
    return {{"Hero"}, {{"Gold", std::nullopt, value}}};
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
