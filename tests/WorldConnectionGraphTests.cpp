#include "doctest/doctest.h"
#include "game/world/WorldConnectionGraph.h"
#include "game/world/WorldDatabase.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/events/EventRuntime.h"
#include "game/data/GameDataLoader.h"
#include "tests/RegressionMapLoader.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>

using namespace OpenYAMM::Game;

namespace
{
ScriptedEventProgram declarations(const std::string &body)
{
    std::ifstream input(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    REQUIRE(input.is_open());
    const std::string support{std::istreambuf_iterator<char>(input), {}};
    std::string error;
    const auto program = ScriptedEventProgram::loadFromLuaText(support + body,
        "@connection-graph", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program, error);
    return *program;
}

struct GraphFixture
{
    MapStats maps;
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;

    GraphFixture()
    {
        std::vector<std::vector<std::string>> rows;
        for (const std::string &file : {"a.blv", "b.blv", "c.blv", "outside.odm", "unrelated.blv"})
        {
            std::vector<std::string> row(35);
            row[0] = std::to_string(rows.size() + 1); row[1] = file; row[2] = file; row[34] = "mm7";
            rows.push_back(row);
        }
        REQUIRE(maps.loadFromRows(rows));
    }
    WorldDatabase world() const
    {
        return WorldDatabase(maps, houses, npcs, teachers, "mm7");
    }
    void connect(WorldDatabase &world, const std::string &file, const std::string &targets) const
    {
        const auto program = declarations("evt.map[502]=function() error('Never execute travel') end\n"
            "SetMapConnectionTargets(502, {" + targets + "})");
        world.indexMapConnections(*maps.findByFileName(file), program);
    }
};
}

TEST_CASE("connection graphs bound dungeon clusters at outdoor endpoints and preserve authored direction")
{
    GraphFixture f;
    auto world = f.world();
    f.connect(world, "a.blv", "'b.blv', 'c.blv'");
    f.connect(world, "b.blv", "'outside.odm'");
    f.connect(world, "c.blv", "'a.blv'");
    // The outside connects a different dungeon, but it is a boundary for this cluster.
    f.connect(world, "outside.odm", "'a.blv', 'unrelated.blv'");
    const WorldKnowledge knowledge;
    const auto graph = buildWorldConnectionGraph(world, knowledge, "world.mm7.map.a", true);
    REQUIRE(graph.nodes.size() == 4);
    CHECK(graph.nodes.front()->fileName == "a.blv");
    REQUIRE(graph.edges.size() == 5);
    CHECK(std::none_of(graph.nodes.begin(), graph.nodes.end(), [](const MapStatsEntry *pMap)
        {
            return pMap->fileName == "unrelated.blv";
        }));
    CHECK(std::none_of(graph.edges.begin(), graph.edges.end(), [](const WorldConnection &edge)
        {
            return edge.sourceId == "world.mm7.map.b" && edge.destinationId == "world.mm7.map.a";
        }));
    for (const WorldConnection &edge : graph.edges)
    {
        CHECK(edge.declared);
        CHECK_FALSE(edge.observedGameMinutes);
    }
    CHECK(buildWorldConnectionGraph(world, knowledge, "world.mm7.map.a", false).nodes.empty());
    CHECK(buildWorldConnectionGraph(world, knowledge, "missing.map", true).nodes.empty());
    CHECK(knowledge.visitedMaps.empty());
    CHECK(knowledge.variables.empty());
}

TEST_CASE("connection graphs show only actual journeys in Discovery even when other destinations were visited")
{
    GraphFixture f;
    auto world = f.world();
    f.connect(world, "a.blv", "'b.blv', 'c.blv'");
    f.connect(world, "b.blv", "'outside.odm'");
    WorldKnowledge knowledge;
    knowledge.visitedMaps = {"a.blv", "b.blv", "c.blv", "outside.odm"};
    EventRuntimeState destination;
    destination.mapFileName = "b.blv";
    REQUIRE(recordWorldMapArrival(destination, "world.mm7.map.a", 502, 1, 2, 3, 1440));
    knowledge.arrivals["world.mm7.map.b"] = worldMapArrivals(destination, destination.mapFileName);
    const auto discovered = buildWorldConnectionGraph(world, knowledge, "world.mm7.map.a", false);
    REQUIRE(discovered.nodes.size() == 2);
    REQUIRE(discovered.edges.size() == 1);
    CHECK(discovered.edges[0].destinationId == "world.mm7.map.b");
    CHECK(discovered.edges[0].observedGameMinutes == 1440);
    CHECK_FALSE(discovered.edges[0].declared);
    const auto guide = buildWorldConnectionGraph(world, knowledge, "world.mm7.map.a", true);
    REQUIRE(guide.edges.size() == 3);
    CHECK(std::count_if(guide.edges.begin(), guide.edges.end(), [](const WorldConnection &edge)
        {
            return edge.observedGameMinutes.has_value();
        }) == 1);
    CHECK(knowledge.visitedMaps.size() == 4);
    CHECK(knowledge.floorPlans.empty());
    CHECK(knowledge.regionMaps.empty());
    CHECK(knowledge.variables.empty());
    // A mismatched snapshot owner and unknown source must not invent observations.
    knowledge.arrivals["world.mm7.map.c"] = knowledge.arrivals.at("world.mm7.map.b");
    knowledge.arrivals["world.mm7.map.b"].push_back({"world.mm8.map.a", 502, "b.blv", 0, 0, 0, 2880});
    CHECK(buildWorldConnectionGraph(world, knowledge, "world.mm7.map.a", false).edges.size() == 1);
}

TEST_CASE("connection graphs replace stale declarations while retaining independently observed journeys")
{
    GraphFixture f;
    auto world = f.world();
    f.connect(world, "a.blv", "'b.blv'");
    f.connect(world, "a.blv", "'c.blv'");
    const auto graph = buildWorldConnectionGraph(world, {}, "world.mm7.map.a", true);
    REQUIRE(graph.edges.size() == 1);
    CHECK(graph.edges[0].destinationId == "world.mm7.map.c");
    WorldKnowledge knowledge;
    knowledge.visitedMaps = {"a.blv", "b.blv"};
    EventRuntimeState destination;
    destination.mapFileName = "b.blv";
    REQUIRE(recordWorldMapArrival(destination, "world.mm7.map.a", 502, 1, 2, 3, 1440));
    knowledge.arrivals["world.mm7.map.b"] = worldMapArrivals(destination, destination.mapFileName);
    const auto history = buildWorldConnectionGraph(world, knowledge, "world.mm7.map.a", true);
    CHECK(history.edges.size() == 2);
    CHECK(buildWorldConnectionGraph(world, knowledge, "world.mm7.map.a", false).edges.size() == 1);
    f.connect(world, "a.blv", "");
    CHECK(world.connections().empty());
    REQUIRE(buildWorldConnectionGraph(world, knowledge, "world.mm7.map.a", true).edges.size() == 1);
    CHECK_FALSE(buildWorldConnectionGraph(world, knowledge, "world.mm7.map.a", true).edges.front().declared);
    const auto invalid = declarations("evt.map[502]=function() end; SetMapConnectionTargets(502, {'missing.blv'})");
    CHECK_THROWS_AS(world.indexMapConnections(*f.maps.findByFileName("a.blv"), invalid), std::invalid_argument);
}

TEST_CASE("connection graphs use active Barrows and Regna programs without loading map geometry")
{
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(), OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &fixture = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = fixture.gameDataLoader;
    for (const std::string worldId : {"mm7", "mm8"})
    {
        WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
            data.getMergedTeacherTopicTable(), worldId);
        for (const MapStatsEntry &entry : data.getMapStats().getEntries())
        {
            if (entry.worldId != worldId) continue;
            MapAssetInfo map;
            map.map = entry;
            std::string error;
            REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fixture.assetFileSystem, map, error), error);
            REQUIRE(map.localEventProgram);
            world.indexMapConnections(entry, *map.localEventProgram);
            CHECK_FALSE(map.indoorMapData);
            CHECK_FALSE(map.outdoorMapData);
        }
        const auto graph = buildWorldConnectionGraph(world, {}, "world." + worldId + ".map."
            + (worldId == "mm7" ? "mdk01" : "out13"), true);
        std::set<std::string> files;
        for (const MapStatsEntry *pMap : graph.nodes) files.insert(pMap->fileName);
        if (worldId == "mm7")
        {
            CHECK(files.contains("out11.odm"));
            CHECK(files.contains("7d13.blv"));
            for (const char *pPrefix : {"mdk0", "mdt0", "mdr0"})
                for (int number = 1; number <= 5; ++number)
                    CHECK(files.contains(std::string(pPrefix) + std::to_string(number) + ".blv"));
            CHECK_FALSE(files.contains("7out02.odm"));
        }
        else
        {
            for (const char *pFile : {"out13.odm", "d31.blv", "d32.blv", "d34.blv", "d06.blv", "out01.odm"})
                CHECK(files.contains(pFile));
            CHECK(std::any_of(graph.edges.begin(), graph.edges.end(), [](const WorldConnection &edge)
                {
                    return edge.sourceId == "world.mm8.map.d34" && edge.destinationId == "world.mm8.map.d06";
                }));
        }
    }
}
