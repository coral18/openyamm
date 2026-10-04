#include <doctest/doctest.h>

#include "engine/TextTable.h"
#include "game/indoor/IndoorGeometryUtils.h"
#include "game/maps/IndoorSceneYml.h"
#include "game/tables/MonsterTable.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace OpenYAMM::Game;

namespace
{
std::vector<uint8_t> readPlacementFixture(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    REQUIRE(stream.good());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::vector<std::vector<std::string>> readPlacementTable(const std::filesystem::path &path)
{
    const std::vector<uint8_t> bytes = readPlacementFixture(path);
    const std::optional<OpenYAMM::Engine::TextTable> table =
        OpenYAMM::Engine::TextTable::parseTabSeparated(std::string(bytes.begin(), bytes.end()));
    REQUIRE(table.has_value());
    std::vector<std::vector<std::string>> rows;
    for (size_t row = 0; row < table->getRowCount(); ++row)
    {
        rows.push_back(table->getRow(row));
    }
    return rows;
}

void addPlacementFace(
    IndoorMapData &map,
    std::initializer_list<IndoorVertex> vertices,
    uint8_t type,
    uint16_t sectorId)
{
    IndoorFace face = {};
    face.facetType = type;
    face.roomNumber = sectorId;
    for (const IndoorVertex &vertex : vertices)
    {
        face.vertexIndices.push_back(uint16_t(map.vertices.size()));
        map.vertices.push_back(vertex);
    }
    const uint16_t faceId = uint16_t(map.faces.size());
    map.faces.push_back(face);
    IndoorSector &sector = map.sectors[sectorId];
    sector.faceIds.push_back(faceId);
    if (type == 3)
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
}

IndoorMapData placementRoom(int16_t floorZ = 0, int16_t ceilingZ = 300)
{
    IndoorMapData map = {};
    map.sectors.resize(2);
    IndoorSector &sector = map.sectors[1];
    sector.minX = sector.minY = -256;
    sector.maxX = sector.maxY = 256;
    sector.minZ = floorZ;
    sector.maxZ = ceilingZ;
    addPlacementFace(map, {{-256, -256, floorZ}, {256, -256, floorZ},
                          {256, 256, floorZ}, {-256, 256, floorZ}}, 3, 1);
    addPlacementFace(map, {{-256, 256, ceilingZ}, {256, 256, ceilingZ},
                          {256, -256, ceilingZ}, {-256, -256, ceilingZ}}, 5, 1);
    return map;
}

IndoorInitialActorPlacement place(
    const IndoorMapData &map, IndoorFaceGeometryCache &cache, float z, bool canFly = false)
{
    return resolveIndoorInitialActorPlacement(
        map, map.vertices, cache, 0.0f, 0.0f, z, 32.0f, 146.0f, canFly, 1024.0f, 1024.0f);
}
}

TEST_CASE("indoor placement resolves below-room markers without widening movement queries")
{
    const IndoorMapData map = placementRoom(448, 1000);
    IndoorFaceGeometryCache cache(map.faces.size());
    CHECK_FALSE(sampleIndoorFloor(map, map.vertices, 0, 0, 256, 1024, 1024,
                                  std::nullopt, nullptr, &cache).hasFloor);
    for (bool canFly : {false, true})
    {
        const IndoorInitialActorPlacement result = place(map, cache, 256.0f, canFly);
        REQUIRE(result.hasClearance);
        CHECK(result.hasFloor);
        CHECK_EQ(result.sectorId, 1);
        CHECK_EQ(result.x, 0.0f);
        CHECK_EQ(result.y, 0.0f);
        CHECK_EQ(result.z, 448.0f);
    }
}

TEST_CASE("indoor placement keeps valid flyers and selects the lower stacked room")
{
    IndoorMapData map = placementRoom(0, 300);
    map.sectors.push_back(map.sectors[1]);
    map.sectors[2].minZ = 320;
    map.sectors[2].maxZ = 800;
    map.sectors[2].faceIds.clear();
    map.sectors[2].floorFaceIds.clear();
    map.sectors[2].ceilingFaceIds.clear();
    addPlacementFace(map, {{-256, -256, 320}, {256, -256, 320}, {256, 256, 320}, {-256, 256, 320}}, 3, 2);
    IndoorFaceGeometryCache cache(map.faces.size());
    const IndoorInitialActorPlacement flyer = place(map, cache, 150, true);
    REQUIRE(flyer.hasClearance);
    CHECK_EQ(flyer.z, 150.0f);
    CHECK_EQ(flyer.sectorId, 1);
    const IndoorInitialActorPlacement walker = place(map, cache, 24);
    REQUIRE(walker.hasClearance);
    CHECK_EQ(walker.z, 0.0f);
    CHECK_EQ(walker.sectorId, 1);
    const IndoorInitialActorPlacement upper = place(map, cache, 350);
    REQUIRE(upper.hasClearance);
    CHECK_EQ(upper.z, 320.0f);
    CHECK_EQ(upper.sectorId, 2);
}

TEST_CASE("indoor placement uses adjusted geometry and integer positions on slopes")
{
    IndoorMapData map = placementRoom(448, 1000);
    std::vector<IndoorVertex> adjusted = map.vertices;
    for (size_t index = 0; index < 4; ++index)
    {
        adjusted[index].z += 200;
    }
    IndoorFaceGeometryCache cache(map.faces.size());
    const IndoorInitialActorPlacement raised = resolveIndoorInitialActorPlacement(
        map, adjusted, cache, 0, 0, 256, 32, 146, false, 1024, 1024);
    REQUIRE(raised.hasClearance);
    CHECK_EQ(raised.z, 648.0f);

    map.faces[0].facetType = 4;
    map.vertices[1].z += 101;
    map.vertices[2].z += 101;
    cache.reset(map.faces.size());
    const IndoorInitialActorPlacement slope = place(map, cache, 256);
    REQUIRE(slope.hasClearance);
    CHECK_EQ(slope.z, 499.0f);
    CHECK_EQ(place(map, cache, slope.z).z, slope.z);
}

TEST_CASE("indoor placement rejects a floor when the complete body cannot fit")
{
    for (int16_t ceilingZ : {int16_t(120), int16_t(30)})
    {
        const IndoorMapData map = placementRoom(0, ceilingZ);
        IndoorFaceGeometryCache cache(map.faces.size());
        const IndoorInitialActorPlacement result = place(map, cache, 0);
        CHECK(result.hasFloor);
        CHECK_FALSE(result.hasClearance);
    }
}

TEST_CASE("indoor placement clears an overhang intersecting only the upper body")
{
    IndoorMapData map = placementRoom();
    addPlacementFace(map, {{-256, -256, 120}, {0, -256, 120}, {0, 256, 120}, {-256, 256, 120}}, 5, 1);
    IndoorFaceGeometryCache cache(map.faces.size());
    const IndoorInitialActorPlacement result = place(map, cache, 0);
    REQUIRE(result.hasClearance);
    CHECK(result.movedHorizontally);
    CHECK_GE(result.x, 32.0f);
}

TEST_CASE("indoor placement does not move an overlapping body through a dividing wall")
{
    IndoorMapData map = placementRoom();
    addPlacementFace(map, {{8, -256, 0}, {8, 256, 0}, {8, 256, 300}, {8, -256, 300}}, 1, 1);
    IndoorFaceGeometryCache cache(map.faces.size());
    const IndoorInitialActorPlacement result = place(map, cache, 0);
    REQUIRE(result.hasClearance);
    CHECK_LE(result.x, -24.0f);
}

TEST_CASE("indoor placement can leave a wall plane with either face winding")
{
    for (bool reversed : {false, true})
    {
        IndoorMapData map = placementRoom();
        addPlacementFace(map, {{0, -256, 0}, {0, 256, 0}, {0, 256, 300}, {0, -256, 300}}, 1, 1);
        if (reversed)
        {
            std::reverse(map.faces.back().vertexIndices.begin(), map.faces.back().vertexIndices.end());
        }
        IndoorFaceGeometryCache cache(map.faces.size());
        const IndoorInitialActorPlacement result = place(map, cache, 0);
        REQUIRE(result.hasClearance);
        CHECK_EQ(result.x, 32.0f);
    }
}

TEST_CASE("indoor encounter placement is repeatable and reports impossible content")
{
    const IndoorMapData map = placementRoom(448, 1000);
    IndoorFaceGeometryCache cache(map.faces.size());
    IndoorSpawn spawn = {};
    spawn.z = 256;
    for (uint32_t ordinal = 0; ordinal < 6; ++ordinal)
    {
        const IndoorInitialActorPlacement first = resolveIndoorEncounterPlacement(
            map, map.vertices, cache, spawn, ordinal, 66, 145, false);
        IndoorFaceGeometryCache freshCache(map.faces.size());
        const IndoorInitialActorPlacement repeated = resolveIndoorEncounterPlacement(
            map, map.vertices, freshCache, spawn, ordinal, 66, 145, false);
        REQUIRE(first.hasClearance);
        CHECK_EQ(first.x, repeated.x);
        CHECK_EQ(first.y, repeated.y);
        CHECK_EQ(first.z, repeated.z);
        CHECK_EQ(first.sectorId, repeated.sectorId);
        CHECK_EQ(first.z, 448.0f);
    }
    const IndoorMapData impossible = placementRoom(448, 480);
    IndoorFaceGeometryCache impossibleCache(impossible.faces.size());
    CHECK_FALSE(resolveIndoorEncounterPlacement(
        impossible, impossible.vertices, impossibleCache, spawn, 0, 66, 145, false).hasClearance);
}

TEST_CASE("Tularean Caves fresh encounter markers place all tiers above the floor")
{
    const std::filesystem::path root = OPENYAMM_SOURCE_DIR;
    const std::optional<IndoorMapData> loaded = IndoorMapDataLoader().loadFromBytes(
        readPlacementFixture(root / "assets_dev/worlds/mm7/maps/7d08.blv"));
    REQUIRE(loaded.has_value());
    const IndoorMapData &map = *loaded;
    MonsterTable monsters;
    REQUIRE(monsters.loadEntriesFromRows(
        readPlacementTable(root / "assets_dev/engine/data_tables/monster_descriptors.txt")));
    REQUIRE(monsters.loadStatsFromRows(
        readPlacementTable(root / "assets_dev/engine/data_tables/monster_data.txt")));
    IndoorFaceGeometryCache cache(map.faces.size());
    for (size_t marker : {size_t(0), size_t(1)})
    {
        CAPTURE(marker);
        REQUIRE_EQ(map.spawns[marker].z, 256);
        const std::string picture = marker == 0 ? "Troglodyte" : "AWyvern";
        for (char tier : {'A', 'B', 'C'})
        {
            CAPTURE(tier);
            const MonsterTable::MonsterStatsEntry *pStats =
                monsters.findStatsByPictureName(picture + " " + tier);
            REQUIRE(pStats != nullptr);
            const MonsterEntry *pMonster = monsters.findById(int16_t(pStats->id));
            REQUIRE(pMonster != nullptr);
            for (uint32_t ordinal = 0; ordinal < 6; ++ordinal)
            {
                const IndoorInitialActorPlacement result = resolveIndoorEncounterPlacement(
                    map, map.vertices, cache, map.spawns[marker], ordinal, float(pMonster->radius),
                    std::max(float(pMonster->height), float(pMonster->radius) * 2 + 2), pStats->canFly);
                REQUIRE(result.hasClearance);
                CHECK_EQ(result.sectorId, 26);
                CHECK_EQ(result.z, marker == 0 ? 448.0f : 544.0f);
                const IndoorInitialActorPlacement reloaded = resolveIndoorInitialActorPlacement(
                    map, map.vertices, cache, result.x, result.y, result.z, float(pMonster->radius),
                    std::max(float(pMonster->height), float(pMonster->radius) * 2 + 2), pStats->canFly, 1024, 1024);
                CHECK(reloaded.hasClearance);
                CHECK_EQ(result.x, reloaded.x);
                CHECK_EQ(result.y, reloaded.y);
                CHECK_EQ(result.z, reloaded.z);
            }
        }
    }
}

TEST_CASE("corrected indoor scene markers resolve floors without unbounded rescue")
{
    const std::filesystem::path root = OPENYAMM_SOURCE_DIR;
    for (bool sewer : {true, false})
    {
        const std::filesystem::path directory = root / "assets_dev/worlds" / (sewer ? "mm6" : "mm7") / "maps";
        const std::string name = sewer ? "sewer" : "t01";
        std::optional<IndoorMapData> map =
            IndoorMapDataLoader().loadFromBytes(readPlacementFixture(directory / (name + ".blv")));
        REQUIRE(map.has_value());
        const std::vector<uint8_t> sceneBytes = readPlacementFixture(directory / (name + ".scene.yml"));
        std::string error;
        const std::optional<IndoorSceneData> scene =
            IndoorSceneYmlLoader().loadFromText(std::string(sceneBytes.begin(), sceneBytes.end()), error);
        REQUIRE_MESSAGE(scene.has_value(), error);
        MapDeltaData delta;
        REQUIRE_MESSAGE(buildIndoorMapStateFromScene(*scene, *map, delta, error), error);
        const std::vector<IndoorVertex> vertices = buildIndoorMechanismAdjustedVertices(*map, &delta, nullptr);
        IndoorFaceGeometryCache cache(map->faces.size());
        cache.setAttributeOverrides(&delta);
        for (size_t marker : sewer ? std::vector<size_t>{37, 57} : std::vector<size_t>{35})
        {
            for (uint32_t ordinal = 0; ordinal < 6; ++ordinal)
            {
                const IndoorInitialActorPlacement result = resolveIndoorEncounterPlacement(
                    *map, vertices, cache, map->spawns[marker], ordinal,
                    sewer ? 40.0f : 60.0f, sewer ? 107.0f : 170.0f, false);
                REQUIRE(result.hasFloor);
                CHECK(result.hasClearance);
                const IndoorFloorSample support = sampleIndoorFloor(
                    *map, vertices, result.x, result.y, result.z, 1, 1, result.sectorId, nullptr, &cache);
                REQUIRE(support.hasFloor);
                CHECK_EQ(result.z, std::ceil(support.height));
                if (ordinal == 0)
                {
                    CHECK_EQ(result.z, sewer ? 0.0f : 480.0f);
                }
            }
        }
    }
}
