#include <doctest/doctest.h>

#include "engine/AssetFileSystem.h"
#include "game/data/GameDataLoader.h"
#include "game/gameplay/GameplayActorService.h"
#include "game/scene/IndoorSceneRuntime.h"

#include <filesystem>

using namespace OpenYAMM::Game;

TEST_CASE("a lone flying devil can rise through the Hive pit portal to the party")
{
    OpenYAMM::Engine::AssetFileSystem assets;
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    REQUIRE(assets.initialize(sourceRoot, sourceRoot / "assets_dev", OpenYAMM::Engine::AssetScaleTier::X1, "mm6"));
    GameDataLoader data;
    REQUIRE(data.loadForHeadlessGameplay(assets));
    REQUIRE(data.loadMapByFileNameForHeadlessGameplay(assets, "hive.blv"));
    const MapAssetInfo &loaded = *data.getSelectedMap();
    REQUIRE(loaded.indoorMapData);
    IndoorMapData map = *loaded.indoorMapData;
    map.spawns.clear();
    std::optional<MapDeltaData> delta = loaded.indoorMapDeltaData;
    REQUIRE(delta);
    delta->actors.clear();

    const MonsterTable::MonsterStatsEntry *pStats = data.getMonsterTable().findStatsById(501);
    REQUIRE(pStats != nullptr);
    MapDeltaActor devil = {};
    devil.monsterId = devil.monsterInfoId = 501;
    devil.hp = pStats->hitPoints;
    devil.x = 754;
    devil.y = 7241;
    devil.z = -400;
    devil.sectorId = 4;
    devil.radius = 73;
    devil.height = 225;
    devil.hostilityType = 4;
    devil.attributes = uint32_t(EvtActorAttribute::Aggressor) | uint32_t(EvtActorAttribute::Nearby);
    delta->actors.push_back(devil);

    Party party;
    party.setItemTable(&data.getItemTable());
    GameplayActorService actorService;
    actorService.bindTables(&data.getMonsterTable(), &data.getSpellTable());
    IndoorSceneRuntime scene(loaded.map.fileName, loaded.map, map,
        data.getMonsterTable(), data.getMonsterProjectileTable(), data.getObjectTable(), data.getSpellTable(),
        data.getItemTable(), data.getChestTable(), party, delta, loaded.eventRuntimeState,
        loaded.localEventProgram, std::nullopt, &actorService, nullptr);
    scene.partyRuntime().teleportPartyPosition(712.484f, 7147.493f, 0);
    const IndoorWorldRuntime &world = scene.worldRuntime();
    REQUIRE_EQ(world.mapActorCount(), 1u);
    REQUIRE(world.mapActorAiState(0)->canFly);
    REQUIRE_LT(world.mapActorAiState(0)->preciseZ, -330);

    // With nobody to collide with, the actor must climb past the former false ceiling at about Z=-330.
    for (int step = 0; step < 3 * 128; ++step)
    {
        scene.advanceSimulation(1000.0f / 128);
        scene.worldRuntime().updateActorAi(1.0f / 128);
    }
    CHECK(world.mapActorAiState(0)->hasDetectedParty);
    CHECK_GT(world.mapActorAiState(0)->preciseZ, -100);
}
