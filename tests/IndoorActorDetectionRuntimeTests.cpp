#include <doctest/doctest.h>

#include "engine/AssetFileSystem.h"
#include "game/FaceEnums.h"
#include "game/data/GameDataLoader.h"
#include "game/gameplay/GameplayActorService.h"
#include "game/scene/IndoorSceneRuntime.h"

#include <array>
#include <filesystem>

using namespace OpenYAMM::Game;

namespace
{
struct GoblinwatchDetectionFixture
{
    GameDataLoader data;
    IndoorMapData map;
    Party party;
    GameplayActorService actorService;
    std::unique_ptr<IndoorSceneRuntime> pScene;

    GoblinwatchDetectionFixture(int actorX = -640, int actorY = 3456)
    {
        OpenYAMM::Engine::AssetFileSystem assets;
        const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
        REQUIRE(assets.initialize(sourceRoot, sourceRoot / "assets_dev", OpenYAMM::Engine::AssetScaleTier::X1, "mm6"));
        REQUIRE(data.loadForHeadlessGameplay(assets));
        REQUIRE(data.loadMapByFileNameForHeadlessGameplay(assets, "6d01.blv"));
        const MapAssetInfo &loaded = *data.getSelectedMap();
        map = *loaded.indoorMapData;
        map.spawns.clear();
        std::optional<MapDeltaData> delta = loaded.indoorMapDeltaData;
        REQUIRE(delta);
        delta->actors.clear();
        const MonsterTable::MonsterStatsEntry *pStats = data.getMonsterTable().findStatsById(550);
        REQUIRE(pStats != nullptr);
        MapDeltaActor actor = {};
        actor.monsterId = actor.monsterInfoId = 550;
        actor.hp = pStats->hitPoints;
        actor.x = actorX;
        actor.y = actorY;
        actor.sectorId = 4;
        actor.radius = 56;
        actor.height = 145;
        actor.hostilityType = 4;
        actor.attributes = uint32_t(EvtActorAttribute::Aggressor);
        delta->actors.push_back(actor);
        party.setItemTable(&data.getItemTable());
        actorService.bindTables(&data.getMonsterTable(), &data.getSpellTable());
        pScene = std::make_unique<IndoorSceneRuntime>(loaded.map.fileName, loaded.map, map,
            data.getMonsterTable(), data.getMonsterProjectileTable(), data.getObjectTable(), data.getSpellTable(),
            data.getItemTable(), data.getChestTable(), party, delta, loaded.eventRuntimeState,
            loaded.localEventProgram, std::nullopt, &actorService, nullptr);
        pScene->partyRuntime().teleportPartyPosition(565.634f, 6210.096f, 0);
        REQUIRE_EQ(pScene->worldRuntime().mapActorCount(), 1u);
        REQUIRE_FALSE(pScene->worldRuntime().mapActorAiState(0)->hasDetectedParty);
    }

    void advance(float seconds)
    {
        for (int step = 0; step < int(seconds * 128); ++step)
        {
            pScene->advanceSimulation(1000.0f / 128);
            pScene->worldRuntime().updateActorAi(1.0f / 128);
        }
    }

    void checkUnseen() const
    {
        const IndoorWorldRuntime &world = pScene->worldRuntime();
        CHECK_FALSE(world.mapActorAiState(0)->hasDetectedParty);
        CHECK_EQ(world.mapDeltaData()->actors[0].attributes & uint32_t(EvtActorAttribute::Nearby), 0u);
    }
};

struct SectorDetectionFixture
{
    GameDataLoader data;
    IndoorMapData map;
    Party party;
    GameplayActorService actorService;
    std::unique_ptr<IndoorSceneRuntime> pScene;

    SectorDetectionFixture(int partyX, bool blockingWall = true, int actorX = -1024)
    {
        OpenYAMM::Engine::AssetFileSystem assets;
        const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
        REQUIRE(assets.initialize(sourceRoot, sourceRoot / "assets_dev", OpenYAMM::Engine::AssetScaleTier::X1, "mm6"));
        REQUIRE(data.loadForHeadlessGameplay(assets));

        map.sectors.resize(4);
        const auto addFace = [&](std::initializer_list<IndoorVertex> vertices, uint8_t type,
                                 uint16_t sectorId, uint16_t backSectorId = 0)
        {
            IndoorFace face = {};
            face.facetType = type;
            face.roomNumber = sectorId;
            face.roomBehindNumber = backSectorId;
            face.isPortal = backSectorId != 0;
            face.attributes = face.isPortal ? faceAttributeBit(FaceAttribute::IsPortal) : 0;
            for (const IndoorVertex &vertex : vertices)
            {
                face.vertexIndices.push_back(uint16_t(map.vertices.size()));
                map.vertices.push_back(vertex);
            }
            const uint16_t faceId = uint16_t(map.faces.size());
            map.faces.push_back(std::move(face));
            IndoorSector &sector = map.sectors[sectorId];
            sector.faceIds.push_back(faceId);
            if (backSectorId != 0)
            {
                sector.portalFaceIds.push_back(faceId);
                map.sectors[backSectorId].portalFaceIds.push_back(faceId);
                map.sectors[backSectorId].faceIds.push_back(faceId);
            }
            else if (type == 3)
            {
                sector.floorFaceIds.push_back(faceId);
            }
            else if (type == 5)
            {
                sector.ceilingFaceIds.push_back(faceId);
            }
            else
            {
                sector.wallFaceIds.push_back(faceId);
            }
        };

        const std::array<int, 4> boundaries = {-8192, 0, 2048, 4096};
        for (uint16_t sectorId = 1; sectorId < 4; ++sectorId)
        {
            const int minX = boundaries[sectorId - 1];
            const int maxX = boundaries[sectorId];
            IndoorSector &sector = map.sectors[sectorId];
            sector.minX = minX;
            sector.maxX = maxX;
            sector.minY = -512;
            sector.maxY = 512;
            sector.minZ = 0;
            sector.maxZ = 512;
            addFace({{minX, -512, 0}, {maxX, -512, 0}, {maxX, 512, 0}, {minX, 512, 0}}, 3, sectorId);
            addFace({{minX, 512, 512}, {maxX, 512, 512}, {maxX, -512, 512}, {minX, -512, 512}}, 5, sectorId);
            if (sectorId < 3)
            {
                addFace({{maxX, -512, 0}, {maxX, -512, 512}, {maxX, 512, 512}, {maxX, 512, 0}},
                    1, sectorId, sectorId + 1);
            }
        }
        if (blockingWall)
        {
            addFace({{-512, -512, 0}, {-512, -512, 512}, {-512, 512, 512}, {-512, 512, 0}}, 1, 1);
        }

        const MonsterTable::MonsterStatsEntry *pStats = data.getMonsterTable().findStatsById(550);
        REQUIRE(pStats != nullptr);
        MapDeltaActor actor = {};
        actor.monsterId = actor.monsterInfoId = 550;
        actor.hp = pStats->hitPoints;
        actor.x = actorX;
        actor.sectorId = 1;
        actor.radius = 56;
        actor.height = 145;
        actor.hostilityType = 4;
        actor.attributes = uint32_t(EvtActorAttribute::Aggressor);
        std::optional<MapDeltaData> delta = MapDeltaData{};
        delta->actors.push_back(actor);
        MapStatsEntry mapStats = {};
        mapStats.fileName = "detection.blv";
        mapStats.worldId = "mm6";
        party.setItemTable(&data.getItemTable());
        actorService.bindTables(&data.getMonsterTable(), &data.getSpellTable());
        pScene = std::make_unique<IndoorSceneRuntime>(mapStats.fileName, mapStats, map,
            data.getMonsterTable(), data.getMonsterProjectileTable(), data.getObjectTable(), data.getSpellTable(),
            data.getItemTable(), data.getChestTable(), party, delta, EventRuntimeState{},
            std::nullopt, std::nullopt, &actorService, nullptr);
        pScene->partyRuntime().teleportPartyPosition(partyX, 0, 0);
        REQUIRE_EQ(pScene->worldRuntime().mapActorAiState(0)->sectorId, 1);
        REQUIRE_FALSE(pScene->worldRuntime().mapActorAiState(0)->hasDetectedParty);
    }
};
}

TEST_CASE("indoor detection ignores blocking faces within the same or directly neighboring sector")
{
    int partyX = -256;
    int16_t partySectorId = 1;
    SUBCASE("same sector")
    {
        partyX = -256;
        partySectorId = 1;
    }
    SUBCASE("one portal hop")
    {
        partyX = 1024;
        partySectorId = 2;
    }
    SectorDetectionFixture fixture(partyX);
    IndoorSceneRuntime &scene = *fixture.pScene;
    REQUIRE_EQ(scene.partyRuntime().movementState().sectorId, partySectorId);
    REQUIRE_FALSE(scene.worldRuntime().hasIndoorCombatLineOfSight(
        {-1024, 0, 108.75f}, 1, {float(partyX), 0, 96}, partySectorId));

    scene.worldRuntime().updateActorAi(1.0f / 128);
    CHECK(scene.worldRuntime().mapActorAiState(0)->hasDetectedParty);
    CHECK_NE(scene.worldRuntime().mapDeltaData()->actors[0].attributes & uint32_t(EvtActorAttribute::Nearby), 0u);
    CHECK_FALSE(scene.worldRuntime().partyAttackActorHasLineOfSight(0));
}

TEST_CASE("indoor detection keeps geometry checks beyond directly neighboring sectors")
{
    bool blockingWall = true;
    SUBCASE("two portal hops with a wall")
    {
        blockingWall = true;
    }
    SUBCASE("two portal hops with a clear ray")
    {
        blockingWall = false;
    }
    SectorDetectionFixture fixture(3072, blockingWall);
    IndoorSceneRuntime &scene = *fixture.pScene;
    REQUIRE_EQ(scene.partyRuntime().movementState().sectorId, 3);
    scene.worldRuntime().updateActorAi(1.0f / 128);
    CHECK_EQ(scene.worldRuntime().mapActorAiState(0)->hasDetectedParty, !blockingWall);
}

TEST_CASE("indoor detection shortcuts retain the fresh detection distance limit")
{
    int partyX = -256;
    SUBCASE("same sector beyond detection range")
    {
        partyX = -256;
    }
    SUBCASE("neighboring sector beyond detection range")
    {
        partyX = 1024;
    }
    SectorDetectionFixture fixture(partyX, true, -7000);
    fixture.pScene->worldRuntime().updateActorAi(1.0f / 128);
    CHECK_FALSE(fixture.pScene->worldRuntime().mapActorAiState(0)->hasDetectedParty);
    CHECK_EQ(fixture.pScene->worldRuntime().mapDeltaData()->actors[0].attributes
        & uint32_t(EvtActorAttribute::Nearby), 0u);
}

TEST_CASE("indoor detection stays blocked until the entrance walk reaches a neighboring Goblinwatch sector")
{
    GoblinwatchDetectionFixture fixture;
    IndoorSceneRuntime &scene = *fixture.pScene;
    fixture.advance(1);
    fixture.checkUnseen();
    REQUIRE(scene.activateEvent(1, "face", 0));
    fixture.advance(3);
    fixture.checkUnseen();

    // Walls still block detection across sectors 4 -> 3 -> 2. Once the walk enters
    // sector 3, its direct portal to the goblin's sector 4 grants awareness.
    REQUIRE_EQ(scene.worldRuntime().mapActorAiState(0)->sectorId, 4);
    bool reachedNeighboringSector = false;
    for (int step = 0; step < 5 * 128; ++step)
    {
        scene.partyRuntime().update((535.851f - 565.634f) / 5, (4885.110f - 6210.096f) / 5,
            false, true, 1.0f / 128);
        fixture.advance(1.0f / 128);
        CAPTURE(step);
        CAPTURE(scene.partyRuntime().movementState().sectorId);
        CAPTURE(scene.partyRuntime().partyX());
        CAPTURE(scene.partyRuntime().partyY());
        reachedNeighboringSector = reachedNeighboringSector || scene.partyRuntime().movementState().sectorId == 3;
        REQUIRE_EQ(scene.worldRuntime().mapActorAiState(0)->hasDetectedParty, reachedNeighboringSector);
    }
    CHECK(scene.partyRuntime().partyX() == doctest::Approx(535.851f).epsilon(0.001));
    CHECK(scene.partyRuntime().partyY() == doctest::Approx(4885.110f).epsilon(0.001));
    CHECK(reachedNeighboringSector);
    fixture.advance(1);
    CHECK(scene.worldRuntime().mapActorAiState(0)->hasDetectedParty);
    CHECK_FALSE(scene.worldRuntime().partyAttackActorHasLineOfSight(0));
}

TEST_CASE("indoor detection crosses a closed neighboring Goblinwatch doorway and preserves the encounter on restore")
{
    GoblinwatchDetectionFixture fixture(-192, 4000);
    IndoorSceneRuntime &scene = *fixture.pScene;
    scene.partyRuntime().teleportPartyPosition(-192, 4560, 0);
    fixture.advance(1);
    CHECK(scene.worldRuntime().mapActorAiState(0)->hasDetectedParty);
    CHECK_FALSE(scene.worldRuntime().partyAttackActorHasLineOfSight(0));
    REQUIRE(scene.activateEvent(61, "face", 0));
    fixture.advance(3);
    CHECK_GT(scene.eventRuntimeState()->mechanisms.at(23).currentDistance, 0);
    CHECK(scene.worldRuntime().mapActorAiState(0)->hasDetectedParty);

    // Return behind the entrance door. Current visibility must not erase a previous encounter.
    scene.partyRuntime().teleportPartyPosition(565.634f, 6210.096f, 0);
    const IndoorSceneRuntime::Snapshot saved = scene.snapshot();
    scene.restoreSnapshot(saved);
    fixture.advance(1);
    CHECK(scene.worldRuntime().mapActorAiState(0)->hasDetectedParty);
    CHECK_NE(scene.worldRuntime().mapDeltaData()->actors[0].attributes & uint32_t(EvtActorAttribute::Nearby), 0u);
}
