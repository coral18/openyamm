#include "engine/AssetFileSystem.h"
#include "game/app/GameSession.h"
#include "game/data/GameDataLoader.h"
#include "game/party/SpellIds.h"
#include "tests/PartySpellTestHarness.h"

#include <doctest/doctest.h>

#include <filesystem>

TEST_CASE("out of mana spells yield the character turn without blocking combat")
{
    using namespace OpenYAMM;
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    Engine::AssetFileSystem assets;
    REQUIRE(assets.initialize(sourceRoot, sourceRoot / "assets_dev", Engine::AssetScaleTier::X1));
    Game::GameDataLoader loader;
    REQUIRE(loader.loadForHeadlessGameplay(assets));
    Game::GameDataRepository data;
    data.bind(loader);
    Game::PartySeed seed;
    seed.members.push_back(Tests::makeSpellRegressionPartyMember("Mage", "Sorcerer", "PC07-01", 7));
    seed.members.push_back(Tests::makeSpellRegressionPartyMember("Knight", "Knight", "PC01-01", 1));
    Game::Party party;
    party.seed(seed);
    Game::Character *pCaster = party.member(0);
    REQUIRE(pCaster != nullptr);
    pCaster->skills["FireMagic"] = {"FireMagic", 5, Game::SkillMastery::Normal};
    pCaster->spellPoints = 0;
    Tests::PartySpellTestWorldRuntime world;
    world.bindParty(&party);
    Game::GameSession session;
    session.bindDataRepository(&data);
    session.bindActiveWorldRuntime(&world);
    Game::GameplayScreenRuntime &runtime = session.gameplayScreenRuntime();
    Game::GameplaySpellService &spells = session.gameplaySpellService();
    Game::TurnBasedCombatRuntime &turns = session.turnBasedCombatRuntime();
    Game::PartySpellCastRequest request;
    request.spellId = Game::spellIdValue(Game::SpellId::TorchLight);

    SUBCASE("turn mode advances through the party and remains usable next round")
    {
        REQUIRE(turns.begin(party, nullptr));
        turns.update(&party, nullptr, 0.6f);
        REQUIRE(turns.canBeginPlayerAction(party));
        for (int round = 0; round < 2; ++round)
        {
            for (int action = 0; action < 8 && turns.stage() == Game::TurnBasedCombatStage::Attack; ++action)
            {
                REQUIRE_EQ(party.activeMemberIndex(), 0);
                request.quickCast = round == 0;
                const Game::PartySpellCastResult result = spells.castSpell(runtime, request);
                CHECK(result.status == Game::PartySpellCastStatus::NotEnoughSpellPoints);
                CHECK_EQ(pCaster->spellPoints, 0);
                CHECK_EQ(pCaster->recoverySecondsRemaining, 0.0f);
                CHECK(world.projectileRequests().empty());
                REQUIRE_EQ(party.activeMemberIndex(), 1);
                REQUIRE(turns.canBeginPlayerAction(party));
                REQUIRE(turns.applyPlayerAction(party, 1, 0.0f));
            }
            REQUIRE(turns.stage() == Game::TurnBasedCombatStage::Movement);
            CHECK_EQ(turns.movementActionPoints(), 130);
            REQUIRE(turns.finishMovementPhase());
            turns.update(&party, nullptr, 0.016f);
            turns.update(&party, nullptr, 0.6f);
            REQUIRE(turns.canBeginPlayerAction(party));
        }
    }

    SUBCASE("real time keeps an unsuccessful cast free of recovery")
    {
        const Game::PartySpellCastResult result = spells.castSpell(runtime, request);
        CHECK(result.status == Game::PartySpellCastStatus::NotEnoughSpellPoints);
        CHECK_EQ(party.activeMemberIndex(), 0);
        CHECK_EQ(pCaster->recoverySecondsRemaining, 0.0f);
    }

    SUBCASE("casts explicitly excluding recovery do not consume a turn")
    {
        REQUIRE(turns.begin(party, nullptr));
        turns.update(&party, nullptr, 0.6f);
        request.applyRecovery = false;
        const Game::PartySpellCastResult result = spells.castSpell(runtime, request);
        CHECK(result.status == Game::PartySpellCastStatus::NotEnoughSpellPoints);
        CHECK_EQ(party.activeMemberIndex(), 0);
        CHECK(turns.canBeginPlayerAction(party));
    }

    SUBCASE("choosing a spell target does not consume a turn")
    {
        REQUIRE(turns.begin(party, nullptr));
        turns.update(&party, nullptr, 0.6f);
        pCaster->spellPoints = 20;
        request.spellId = Game::spellIdValue(Game::SpellId::FireBolt);
        const Game::PartySpellCastResult result = spells.castSpell(runtime, request);
        CHECK(result.status == Game::PartySpellCastStatus::NeedActorTarget);
        CHECK_EQ(party.activeMemberIndex(), 0);
        CHECK(turns.canBeginPlayerAction(party));
        CHECK_EQ(pCaster->spellPoints, 20);
    }
}
