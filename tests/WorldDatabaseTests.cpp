#include "doctest/doctest.h"
#include "game/world/WorldDatabase.h"
#include "engine/TextTable.h"
#include "game/FaceEnums.h"
#include "game/indoor/IndoorMapData.h"

#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

using namespace OpenYAMM::Game;

namespace
{
std::vector<std::string> mapRow(int id, const std::string &name, const std::string &file, const std::string &world,
                                int area = 0)
{
    std::vector<std::string> row(35);
    row[0] = std::to_string(id); row[1] = name; row[2] = file;
    row[32] = std::to_string(area); row[34] = world;
    return row;
}

struct Fixture
{
    MapStats maps;
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    std::vector<WorldMapPresentation> layout = {{"7out02.odm", "", 2, {{0.6f, 0.5f}}}};

    Fixture()
    {
        REQUIRE(maps.loadFromRows({mapRow(63, "Harmondale", "7out02.odm", "mm7"),
                                  mapRow(95, "White Cliff Cave", "7d21.blv", "mm7", 2),
                                  mapRow(1, "Dagger Wound", "out01.odm", "mm8")}));
        std::vector<std::string> house(24);
        house[0] = "11"; house[2] = "Weapon Shop"; house[3] = "63"; house[5] = "Blade's End";
        REQUIRE(houses.loadFromRows({house}));
        std::vector<std::string> npc(16);
        npc[0] = "42"; npc[1] = "Townsaver"; npc[6] = "11"; npc[10] = "305";
        REQUIRE(npcs.loadNpcRows({npc}));
        REQUIRE(teachers.loadFromRows({{"TopicId", "Notes", "SkillId", "Mastery", "TextId"},
                                      {"305", "Grand Master Sword", "1", "3", "305", "8000", "10"}}));
    }
    WorldDatabase database() const { return WorldDatabase(maps, houses, npcs, teachers, "mm7", layout); }
};
} // namespace

TEST_CASE("world database filters undiscovered information before search")
{
    Fixture f;
    const auto database = f.database();
    WorldKnowledge knowledge;
    CHECK(database.query(knowledge, false).empty());
    CHECK(database.query(knowledge, false, "Sword").empty());
    CHECK(database.query(knowledge, true, "Sword").size() == 1);
    CHECK(database.query(knowledge, true, "Dagger").empty());
    CHECK(knowledge.variables.empty()); // Guide queries have no discovery side effects.

    knowledge.visitedMaps.insert("7out02.odm");
    CHECK(database.query(knowledge, false).size() == 1);
    CHECK(database.query(knowledge, false, "Weapon").empty());
    knowledge.variables[WorldDatabase::houseDiscoveryKey(11)] = 1;
    CHECK(database.query(knowledge, false, "blade").size() == 1);
    CHECK(database.query(knowledge, false, "Sword").empty());
    knowledge.variables[WorldDatabase::npcDiscoveryKey(42)] = 1;
    CHECK(database.query(knowledge, false, "sWoRd", WorldRecordKind::Trainer).size() == 1);
}

TEST_CASE("world database indexes canonical MM7 maps residents and trainers")
{
    const auto rows = [](const std::string &name)
    {
        std::ifstream file(std::string(OPENYAMM_SOURCE_DIR) + "/assets_dev/engine/data_tables/" + name);
        REQUIRE(file.is_open());
        const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        const auto table = OpenYAMM::Engine::TextTable::parseTabSeparated(text);
        REQUIRE(table.has_value());
        std::vector<std::vector<std::string>> result;
        for (size_t i = 0; i < table->getRowCount(); ++i) result.push_back(table->getRow(i));
        return result;
    };
    MapStats maps;
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    REQUIRE(maps.loadFromRows(rows("map_stats.txt")));
    REQUIRE(maps.applyOutdoorNavigationRows(rows("map_navigation.txt")));
    MergedOutdoorTravelTable outdoorTravels;
    REQUIRE(outdoorTravels.loadFromRows(rows("outdoor_travels.txt")));
    REQUIRE(maps.applyMergedOutdoorTravels(outdoorTravels));
    REQUIRE(houses.loadFromRows(rows("house_data.txt")));
    REQUIRE(npcs.loadNpcRows(rows("npc.txt")));
    REQUIRE(teachers.loadFromRows(rows("teacher_topics.txt")));
    WorldDatabase database(maps, houses, npcs, teachers, "mm7");
    CHECK(database.query({}, true, "", WorldRecordKind::Region).size() == 13);
    CHECK(database.query({}, true, "", WorldRecordKind::Dungeon).size() > 50);
    CHECK(database.query({}, true, "", WorldRecordKind::Travel).size() > 10);
    const auto trainers = database.query({}, true, "Sword", WorldRecordKind::Trainer);
    REQUIRE(!trainers.empty());
    for (const auto *trainer : trainers)
    {
        CHECK(trainer->teacher == teachers.get(trainer->teacher->topicId));
        CHECK(trainer->map->worldId == "mm7");
        CHECK(trainer->house == houses.get(trainer->npc->houseId));
    }
}

TEST_CASE("world database hides travel destinations until both ends are known")
{
    Fixture f;
    std::vector<std::string> navigation(29, "0");
    navigation[0] = "7out02.odm";
    navigation[1] = navigation[3] = "-100";
    navigation[2] = navigation[4] = "100";
    navigation[5] = "7d21.blv"; navigation[6] = "2";
    REQUIRE(f.maps.applyOutdoorNavigationRows({navigation}));
    const auto database = f.database();
    WorldKnowledge knowledge;
    knowledge.visitedMaps.insert("7out02.odm");
    CHECK(database.query(knowledge, false, "", WorldRecordKind::Travel).empty());
    const auto guide = database.query(knowledge, true, "", WorldRecordKind::Travel);
    REQUIRE(guide.size() == 1);
    CHECK(guide.front()->destination == f.maps.findById(95));
    CHECK(guide.front()->walkingRoute->travelDays == 2);
    knowledge.visitedMaps.insert("7d21.blv");
    CHECK(database.query(knowledge, false, "", WorldRecordKind::Travel).size() == 1);
}

TEST_CASE("world database uses canonical records and location hierarchy")
{
    Fixture f;
    const auto database = f.database();
    const auto *cave = database.find("world.mm7.map.7d21");
    REQUIRE(cave != nullptr);
    CHECK(cave->parentId == "world.mm7.map.7out02");
    const auto *trainer = database.find("mm7:npc/42/trainer/305");
    REQUIRE(trainer != nullptr);
    CHECK(trainer->teacher == f.teachers.get(305));
    CHECK(trainer->npc == f.npcs.getNpc(42));
    CHECK(trainer->house == f.houses.get(11));
    CHECK(trainer->map == f.maps.findById(63));
    CHECK(trainer->teacher->requiredGold == 8000);
    CHECK(database.query({}, true, "", WorldRecordKind::Dungeon, "world.mm7.map.7out02").size() == 1);
}

TEST_CASE("world database does not disclose service location without knowledge of the map")
{
    Fixture f;
    const auto database = f.database();
    WorldKnowledge knowledge;
    knowledge.variables[WorldDatabase::npcDiscoveryKey(42)] = 1;
    CHECK(database.query(knowledge, false, "Sword").empty());
    knowledge.variables[WorldDatabase::mapDiscoveryKey(*f.maps.findById(63))] = 1;
    CHECK(database.query(knowledge, false, "Sword").size() == 1);
}

TEST_CASE("world database rejects invalid map presentation")
{
    Fixture f;
    f.layout.push_back(f.layout.front());
    CHECK_THROWS_AS(f.database(), std::invalid_argument);
    f.layout.pop_back();
    f.layout[0].position = {{std::numeric_limits<float>::quiet_NaN(), 0.5f}};
    CHECK_THROWS_AS(f.database(), std::invalid_argument);
    f.layout[0].position = {{0.5f, 0.5f}};
    f.layout[0].parentFileName = "out01.odm";
    CHECK_THROWS_AS(f.database(), std::invalid_argument);
    f.layout[0].parentFileName = "7d21.blv";
    CHECK_THROWS_AS(f.database(), std::invalid_argument); // The cave is already a child of this region.
}

TEST_CASE("world database dungeon plan respects reveal bits and hidden faces")
{
    IndoorMapData map;
    map.vertices = {{0, 0, 0}, {100, 0, 0}, {100, 100, 0}, {0, 100, 0}};
    map.faces.resize(2);
    map.outlines = {{0, 1, 0, 1}, {1, 2, 0, 1}, {2, 3, 0, 1}, {3, 0, 0, 1}, {99, 0, 0, 1}};
    WorldFloorPlanKnowledge knowledge;
    CHECK(buildWorldFloorPlan(map, knowledge, false).pixelsBgra.empty());
    CHECK(buildWorldFloorPlan(map, knowledge, true).outlineCount == 4);
    knowledge.visibleOutlines = {0x80};
    auto explored = buildWorldFloorPlan(map, knowledge, false);
    CHECK(explored.outlineCount == 1);
    CHECK(explored.pixelsBgra.size() == size_t(explored.width) * explored.height * 4);
    knowledge.visibleOutlines = {0x01}; // Only invalid/out-of-range outline bit, not the first outline.
    CHECK(buildWorldFloorPlan(map, knowledge, false).outlineCount == 0);
    knowledge.faceAttributes = {faceAttributeBit(FaceAttribute::SeenByParty), 0}; // SeenByParty also reveals adjoining outlines.
    CHECK(buildWorldFloorPlan(map, knowledge, false).outlineCount == 4);
    knowledge.invisibleFaces.insert(1);
    CHECK(buildWorldFloorPlan(map, knowledge, false).outlineCount == 0);
    CHECK(buildWorldFloorPlan(map, knowledge, true).outlineCount == 4);
    map.faces[1].attributes = faceAttributeBit(FaceAttribute::Invisible); // Authored invisible geometry is hidden in Guide, too.
    CHECK(buildWorldFloorPlan(map, knowledge, true).outlineCount == 0);
    map.vertices[1].x = std::numeric_limits<int>::max();
    map.vertices[0].x = std::numeric_limits<int>::min();
    map.faces[1].attributes = 0;
    CHECK(buildWorldFloorPlan(map, {}, true).outlineCount == 4);
}
