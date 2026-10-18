#include "game/world/WorldMapKnowledge.h"
#include "game/maps/MapSnapshotLookup.h"
#include "doctest/doctest.h"
#include "game/world/WorldDatabase.h"
#include "game/world/WorldAtlasPresentation.h"
#include "engine/TextTable.h"
#include <filesystem>
#include <cstdlib>
#include <map>
#include <set>
#include <yaml-cpp/yaml.h>
#include "game/FaceEnums.h"
#include "game/ui/GameplayMinimapTransform.h"
#include "game/indoor/IndoorMapData.h"
#include "game/outdoor/OutdoorMapData.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/events/EventRuntime.h"
#include "game/gameplay/MapEncounterDefinition.h"
#include "game/tables/MonsterTable.h"

#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

using namespace OpenYAMM::Game;

namespace
{
std::vector<std::vector<std::string>> canonicalRows(const std::string &name)
{
    std::ifstream file(std::string(OPENYAMM_SOURCE_DIR) + "/assets_dev/engine/data_tables/" + name);
    REQUIRE(file.is_open());
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const auto table = OpenYAMM::Engine::TextTable::parseTabSeparated(text);
    REQUIRE(table.has_value());
    std::vector<std::vector<std::string>> result;
    for (size_t i = 0; i < table->getRowCount(); ++i) result.push_back(table->getRow(i));
    return result;
}

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

MonsterTable atlasMonsters()
{
    std::vector<std::vector<std::string>> rows;
    for (int i = 0; i < 3; ++i)
    {
        std::vector<std::string> row(38, "0");
        row[0] = std::to_string(11 + i);
        row[1] = row[2] = "Test Goblin " + std::string(1, char('A' + i));
        row[3] = "5"; row[4] = "40"; row[7].clear();
        row[10] = "short"; row[11] = "normal"; row[17] = "Physical"; row[18] = "2d4";
        rows.push_back(row);
    }
    MonsterTable monsters;
    REQUIRE(monsters.loadStatsFromRows(rows));
    return monsters;
}
} // namespace

TEST_CASE("world map user notes reuse map-local notes without replacing script-owned records")
{
    EventRuntimeState state;
    state.mapFileName = "7out02.odm";
    state.runtimeMapNotes[3101] = {3101, 4, 5, state.mapFileName, "Sword - Master", true};
    WorldMapNoteEdit edit{"7OUT02.ODM", 0, 123, -456, "  Własna notatka  ", false};
    std::string error;
    REQUIRE(editWorldMapNote(state, state.mapFileName, edit, error));
    CHECK(error.empty());
    CHECK(edit.id == UINT32_MAX);
    CHECK(edit.text == "Własna notatka");
    REQUIRE(worldMapNotes(state, state.mapFileName).size() == 2);
    CHECK_FALSE(worldMapNotes(state, state.mapFileName)[0].userOwned);
    CHECK(worldMapNotes(state, state.mapFileName)[1].userOwned);
    CHECK(state.runtimeMapNotes.at(edit.id).x == 123);
    CHECK(state.runtimeMapNotes.at(edit.id).y == -456);
    edit.text = "Po zmianie";
    REQUIRE(editWorldMapNote(state, state.mapFileName, edit, error));
    CHECK(state.runtimeMapNotes.at(edit.id).text == "Po zmianie");
    CHECK(state.runtimeMapNotes.size() == 2);
    edit.remove = true;
    REQUIRE(editWorldMapNote(state, state.mapFileName, edit, error));
    CHECK(state.runtimeMapNotes.size() == 1);
    CHECK(state.runtimeMapNotes.at(3101).text == "Sword - Master");
    CHECK(state.namedMapVars.empty());
}

TEST_CASE("world map note edits reject foreign maps and ownership before changing saved data")
{
    EventRuntimeState state;
    state.mapFileName = "out01.odm";
    state.runtimeMapNotes[UINT32_MAX] = {UINT32_MAX, 1, 2, state.mapFileName, "Foreign", true};
    state.namedMapVars["openyamm.map-notes.user/4294967294"] = 1; // Unavailable ID, no active record.
    std::string error;
    WorldMapNoteEdit edit{state.mapFileName, UINT32_MAX, 3, 4, "Overwrite", false};
    CHECK_FALSE(editWorldMapNote(state, state.mapFileName, edit, error));
    CHECK(state.runtimeMapNotes.at(UINT32_MAX).text == "Foreign");
    edit.id = 0;
    REQUIRE(editWorldMapNote(state, state.mapFileName, edit, error));
    CHECK(edit.id == UINT32_MAX - 2);
    edit.mapFileName = "d05.blv";
    CHECK_FALSE(editWorldMapNote(state, state.mapFileName, edit, error));
    edit.mapFileName = state.mapFileName;
    for (const std::string &invalid : {std::string("  "), std::string(513, 'a'), std::string("a\0b", 3)})
    {
        edit.text = invalid;
        CHECK_FALSE(editWorldMapNote(state, state.mapFileName, edit, error));
        CHECK(state.runtimeMapNotes.at(edit.id).text == "Overwrite");
    }
    CHECK(state.runtimeMapNotes.size() == 2);
    state.runtimeMapNotes[12] = {12, 1, 2, "d05.blv", "Other map", true};
    state.runtimeMapNotes[13] = {13, 1, 2, state.mapFileName, "Inactive", false};
    CHECK(worldMapNotes(state, state.mapFileName).size() == 2);
}

TEST_CASE("world map notes use the owning snapshot scope when ephemeral runtime context is absent")
{
    EventRuntimeState restored;
    CHECK(restored.mapFileName.empty());
    WorldMapNoteEdit edit{"d05.blv", 0, 100, 200, "Dungeon note after reload", false};
    std::string error;
    REQUIRE(editWorldMapNote(restored, "d05.blv", edit, error));
    REQUIRE(worldMapNotes(restored, "d05.blv").size() == 1);
    CHECK(worldMapNotes(restored, "out01.odm").empty());
    edit.text = "Must not cross map boundaries";
    CHECK_FALSE(editWorldMapNote(restored, "out01.odm", edit, error));
    CHECK(restored.runtimeMapNotes.at(edit.id).text == "Dungeon note after reload");
    CHECK(restored.mapFileName.empty());
}

TEST_CASE("atlas note edits use the canonical snapshot instead of a stale filename alias")
{
    MapStatsEntry map;
    map.fileName = "d05.blv"; map.canonicalId = "mm8/map/d05";
    std::unordered_map<std::string, EventRuntimeState> states;
    states[map.fileName].runtimeMapNotes[3101] = {3101, 0, 0, map.fileName, "Stale alias", true};
    states[map.canonicalId] = {};
    EventRuntimeState *pState = findMapSnapshot(states, map);
    REQUIRE(pState == &states.at(map.canonicalId));
    WorldMapNoteEdit edit{map.fileName, 0, 100, 200, "Canonical note", false};
    std::string error;
    REQUIRE(editWorldMapNote(*pState, map.fileName, edit, error));
    REQUIRE(worldMapNotes(*findMapSnapshot(states, map), map.fileName).size() == 1);
    CHECK(worldMapNotes(*findMapSnapshot(states, map), map.fileName)[0].value.text == "Canonical note");
    CHECK(states.at(map.fileName).runtimeMapNotes.size() == 1);
    CHECK(states.at(map.fileName).runtimeMapNotes.at(3101).text == "Stale alias");
    const auto &savedStates = states;
    CHECK(findMapSnapshot(savedStates, map) == &savedStates.at(map.canonicalId));
    states.erase(map.canonicalId);
    CHECK(findMapSnapshot(states, map) == &states.at(map.fileName));
    states.clear();
    CHECK(findMapSnapshot(states, map) == nullptr);
    map.canonicalId.clear();
    states[""] = {}; states[map.fileName] = {};
    CHECK(findMapSnapshot(states, map) == &states.at(map.fileName));
}

TEST_CASE("map refills preserve party knowledge without restoring cleared gameplay state")
{
    EventRuntimeState previous, refilled;
    WorldMapNoteEdit edit{"d05.blv", 0, 100, 200, "My dungeon note", false};
    std::string error;
    REQUIRE(editWorldMapNote(previous, "d05.blv", edit, error));
    previous.runtimeMapNotes[3101] = {3101, 1, 2, "d05.blv", "Sword - Master", true};
    previous.namedMapVars["openyamm.discovery.site/surface/1/event/5"] = 1;
    recordWorldMonsterObservation(previous, {1, 11, 30, 40, 50, true, 500});
    previous.mapVars[0] = 99;
    previous.openedChestIds.push_back(42);
    previous.namedMapVars["gameplay.trap"] = 99;
    refilled.mapVars[0] = 2;
    refilled.namedMapVars["gameplay.trap"] = 2;
    restoreWorldMapKnowledgeAfterRefill(previous, refilled);
    REQUIRE(worldMapNotes(refilled, "d05.blv").size() == 2);
    CHECK(worldMapNotes(refilled, "d05.blv")[1].userOwned);
    CHECK(refilled.namedMapVars.at("openyamm.discovery.site/surface/1/event/5") == 1);
    REQUIRE(worldMonsterObservations(refilled).size() == 1);
    CHECK(worldMonsterObservations(refilled)[0].dead);
    CHECK(refilled.mapVars[0] == 2);
    CHECK(refilled.openedChestIds.empty());
    CHECK(refilled.namedMapVars.at("gameplay.trap") == 2);
}

TEST_CASE("world map notes are indexed without revealing terrain or deleting other site layers")
{
    Fixture fixture;
    WorldDatabase database = fixture.database();
    EventRuntimeState state;
    state.mapFileName = "7out02.odm";
    WorldMapNoteEdit edit{state.mapFileName, 0, 1200, -500, "My secret camp", false};
    std::string error;
    REQUIRE(editWorldMapNote(state, state.mapFileName, edit, error));
    WorldKnowledge knowledge;
    knowledge.notes[state.mapFileName] = worldMapNotes(state, state.mapFileName);
    database.indexMapNotes(knowledge);
    CHECK(database.queryFeatures(knowledge, false, "secret").empty());
    REQUIRE(database.queryFeatures(knowledge, true, "secret").size() == 1);
    knowledge.visitedMaps.insert(state.mapFileName);
    const auto features = database.queryFeatures(knowledge, false, "secret");
    REQUIRE(features.size() == 1);
    CHECK(features[0]->userNote);
    CHECK(features[0]->noteId == edit.id);
    CHECK(knowledge.regionMaps.empty());
    CHECK(knowledge.variables.empty());
    std::string programError;
    const auto program = ScriptedEventProgram::loadFromLuaText("return {}", "notes-test",
        ScriptedEventScope::Map, programError);
    REQUIRE(program.has_value());
    OutdoorMapData geometry;
    geometry.worldId = "mm7"; geometry.fileName = state.mapFileName;
    database.indexMapLocations(geometry, *program);
    REQUIRE(database.queryFeatures(knowledge, false, "secret").size() == 1);
    database.indexMapNotes(knowledge);
    CHECK(database.queryFeatures(knowledge, false, "secret").size() == 1);
}

TEST_CASE("atlas dungeon note projection reverses the rendered floor-plan transform")
{
    WorldFloorPlanRaster plan;
    plan.centerX = 5000; plan.centerY = -3000; plan.pixelsPerWorldUnit = 0.03;
    for (const auto point : {std::pair{5000.f, -3000.f}, std::pair{-1000.f, 2000.f}, std::pair{12000.f, -7000.f}})
    {
        const auto uv = plan.worldToUv(point.first, point.second);
        const auto restored = plan.uvToWorld(uv.first, uv.second);
        CHECK(restored.first == doctest::Approx(point.first).epsilon(0.00001));
        CHECK(restored.second == doctest::Approx(point.second).epsilon(0.00001));
    }
}

TEST_CASE("world encounter definitions share slot tier and difficulty encoding with gameplay")
{
    for (uint16_t i = 1; i <= 12; ++i)
    {
        const auto descriptor = resolveEncounterSpawnDescriptor(i);
        CHECK(descriptor.encounterSlot == (i <= 3 ? int(i) : int((i - 4) % 3) + 1));
        CHECK(descriptor.fixedTier == (i <= 3 ? '\0' : char('A' + (i - 4) / 3)));
    }
    CHECK(resolveEncounterSpawnDescriptor(0).encounterSlot == 0);
    CHECK(resolveEncounterSpawnDescriptor(13).encounterSlot == 0);
    CHECK(resolveEncounterSpawnDescriptor(UINT16_MAX).encounterSlot == 0);
    CHECK(encounterTierWeights(0) == std::array<int, 3>{100, 0, 0});
    CHECK(encounterTierWeights(-1) == encounterTierWeights(0));
    CHECK(encounterTierWeights(10) == encounterTierWeights(5));
    MapStatsEntry map{};
    CHECK(mapEncounterInfo(map, 0) == nullptr);
    CHECK(mapEncounterInfo(map, 1) == &map.encounter1);
    CHECK(mapEncounterInfo(map, 3) == &map.encounter3);
    CHECK(mapEncounterInfo(map, 4) == nullptr);
}

TEST_CASE("world monster layers hide authored populations and retain only last observed positions")
{
    Fixture f;
    auto database = f.database();
    MonsterTable monsters = atlasMonsters();
    MapStatsEntry map = *f.maps.findById(63);
    map.encounter1.monsterName = "Test Goblin";
    map.encounter1.pictureName = "Test Goblin";
    map.encounter1.minCount = 4; map.encounter1.maxCount = 2;
    OutdoorMapData geometry;
    geometry.spawns = {{1000, 2000, 0, 128, 3, 1}, {3000, 4000, 0, 64, 3, 10},
                       {0, 0, 0, 0, 2, 1}, {0, 0, 0, 0, 3, 13}};
    const auto authored = buildWorldEncounterFeatures(map, geometry, monsters);
    REQUIRE(authored.size() == 2);
    REQUIRE(authored[0].monster.has_value());
    CHECK(authored[0].monster->definitions == std::vector<int16_t>{11}); // Difficulty zero permits only A.
    CHECK(authored[1].monster->definitions == std::vector<int16_t>{13}); // Explicit C bypasses weights.
    CHECK(authored[0].monster->minCount == 2);
    CHECK(authored[0].monster->maxCount == 4);
    map.encounter1.difficulty = 3;
    CHECK(buildWorldEncounterFeatures(map, geometry, monsters)[0].monster->definitions.size() == 3);
    WorldKnowledge knowledge;
    knowledge.regionMaps["7out02.odm"].fullyRevealedCells.assign(88 * 88 / 8, 255);
    database.indexMapMonsters(map, geometry, monsters);
    CHECK(database.queryFeatures(knowledge, false, "Goblin").empty()); // A fully explored region is not a census.
    EventRuntimeState state;
    recordWorldMonsterObservation(state, {42, 12, 100, 200, 300, false, 540});
    const auto observations = worldMonsterObservations(state);
    REQUIRE(observations.size() == 1);
    database.indexMapMonsters(map, geometry, monsters, observations);
    const auto known = database.queryFeatures(knowledge, false, "Goblin");
    REQUIRE(known.size() == 1);
    CHECK(known[0]->site.x == 100);
    CHECK(known[0]->monster->observed);
    CHECK_FALSE(known[0]->monster->dead);
    CHECK(database.queryFeatures(knowledge, true, "Goblin").size() == 3);
    geometry.spawns[0].x = 9000; // Unseen world data cannot move the last observation.
    database.indexMapMonsters(map, geometry, monsters, observations);
    CHECK(database.queryFeatures(knowledge, false, "Goblin")[0]->site.x == 100);
    CHECK(worldMonsterObservations(state)[0].x == 100); // Browsing never rewrites knowledge.
    recordWorldMonsterObservation(state, {42, 12, 150, 250, 300, true, 600});
    const auto updated = worldMonsterObservations(state);
    REQUIRE(updated.size() == 1);
    CHECK(updated[0].x == 150);
    CHECK(updated[0].dead);
}

TEST_CASE("world monster knowledge ignores incomplete invalid and noncanonical observation records")
{
    EventRuntimeState state;
    state.namedMapVars["openyamm.discovery.monster/0/type"] = 11;
    state.namedMapVars["openyamm.discovery.monster/bad/type"] = 11;
    CHECK(worldMonsterObservations(state).empty());
    recordWorldMonsterObservation(state, {0, 11, 10, 20, 30, false, 100});
    REQUIRE(worldMonsterObservations(state).size() == 1);
    CHECK(worldMonsterObservations(state)[0].actorId == 0);
    recordWorldMonsterObservation(state, {1, -1, 0, 0, 0, false, 100});
    recordWorldMonsterObservation(state, {2, 11, 0, 0, 0, false, -1});
    CHECK(worldMonsterObservations(state).size() == 1);
    for (const char *suffix : {"type", "x", "y", "z", "dead", "time"})
        state.namedMapVars[std::string("openyamm.discovery.monster/00/") + suffix]
            = state.namedMapVars.at(std::string("openyamm.discovery.monster/0/") + suffix);
    CHECK(worldMonsterObservations(state).size() == 1);
}

TEST_CASE("world database sites distinguish connected faces from reused event objects")
{
    IndoorMapData map;
    map.vertices = {{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0},
                    {100, 100, 0}, {110, 100, 0}, {100, 110, 0}};
    map.faces.resize(4);
    map.faces[0].vertexIndices = {0, 1, 2};
    map.faces[1].vertexIndices = {0, 2, 3};
    map.faces[2].vertexIndices = {4, 5, 6};
    map.faces[3].vertexIndices = {65535};
    for (auto &face : map.faces)
        face.cogTriggered = 10;
    auto sites = worldInteractionSites(map);
    REQUIRE(sites.size() == 2);
    CHECK(sites[0].x == doctest::Approx(5));
    CHECK(sites[0].faceIds.size() == 2);
    CHECK(sites[1].x == doctest::Approx(103.3333));
    CHECK(sites[1].faceIds.size() == 1);
    map.faces[2].attributes = faceAttributeBit(FaceAttribute::Invisible);
    CHECK(worldInteractionSites(map).size() == 1);
    OutdoorMapData outdoor;
    outdoor.bmodels.resize(1);
    outdoor.bmodels[0].vertices = {{0, 0, 0}, {10, 0, 0}, {10, 10, 0},
        {0, 0, 0}, {10, 10, 0}, {0, 10, 0}, {100, 100, 0}, {110, 100, 0}, {100, 110, 0}};
    outdoor.bmodels[0].faces.resize(3);
    outdoor.bmodels[0].faces[0].vertexIndices = {0, 1, 2};
    outdoor.bmodels[0].faces[1].vertexIndices = {3, 4, 5}; // Same physical seam, separate vertex indices.
    outdoor.bmodels[0].faces[2].vertexIndices = {6, 7, 8};
    for (auto &face : outdoor.bmodels[0].faces)
        face.cogTriggeredNumber = 10;
    sites = worldInteractionSites(outdoor);
    REQUIRE(sites.size() == 2);
    CHECK(sites[0].faceIds.size() == 2);
    CHECK(sites[0].x == doctest::Approx(5));
    CHECK(sites[0].id == "bmodel/0/surface/0");
    CHECK(sites[1].id == "bmodel/0/surface/2");
}

TEST_CASE("world database feature layers use event metadata and gate discoveries before search")
{
    Fixture f;
    auto database = f.database();
    std::string error;
    const auto program = ScriptedEventProgram::loadFromLuaText(R"lua(
        for i = 10, 15 do evt.map[i] = function() end end
        evt.meta.map.hint = { [10] = "Chest", [11] = "Fountain", [12] = "Obelisk", [13] = "Passage" }
        evt.meta.map.contextActions = {
            [10] = { kind = "open_chest", source = "opcode", chestIds = {0} },
            [11] = { kind = "fountain", source = "title" },
            [12] = { kind = "obelisk", source = "title" },
            [13] = { kind = "passage", source = "opcode", targetMap = "7d21.blv" },
            [14] = { kind = "open_chest", source = "opcode", chestIds = {1}, hidden = true },
            [15] = { kind = "open_chest", source = "opcode" }
        }
    )lua", "@sites-test", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program.has_value(), error);
    IndoorMapData geometry;
    geometry.entities.resize(7);
    for (size_t i = 0; i < geometry.entities.size(); ++i)
    {
        geometry.entities[i].eventIdSecondary = uint16_t(10 + i);
        geometry.entities[i].x = 100 * int(i);
    }
    geometry.entities[6].eventIdSecondary = 10; // A second chest using the same event stays undiscovered.
    const MapStatsEntry &map = *f.maps.findById(95);
    database.indexMapLocations(map, geometry, *program);
    CHECK(database.features().size() == 5);
    CHECK(database.queryFeatures({}, true).size() == 5);
    CHECK(database.queryFeatures({}, false).empty());
    WorldKnowledge knowledge;
    knowledge.visitedMaps.insert("7d21.blv");
    CHECK(database.queryFeatures(knowledge, false, "Chest").empty());
    EventRuntimeState state;
    recordWorldSiteDiscovery(state, 10, "decoration/0");
    recordWorldSiteDiscovery(state, 0, "decoration/6");
    REQUIRE(state.namedMapVars.size() == 1);
    knowledge.interactedSites["7d21.blv"].insert(state.namedMapVars.begin()->first);
    const auto found = database.queryFeatures(knowledge, false, "chest");
    REQUIRE(found.size() == 1);
    CHECK(found[0]->site.id == "decoration/0");
    CHECK(found[0]->chestIds == std::vector<uint32_t>{0});
    CHECK(database.queryFeatures(knowledge, false, "Obelisk").empty());
    CHECK(database.queryFeatures(knowledge, true, "Obelisk").size() == 1);
    CHECK(knowledge.interactedSites.at("7d21.blv").size() == 1);
    database.indexMapLocations(map, geometry, *program);
    CHECK(database.features().size() == 5);
}

TEST_CASE("world database feature visibility follows outdoor cells and indoor surfaces without remote leaks")
{
    Fixture f;
    const auto database = f.database();
    WorldMapFeature feature;
    feature.mapId = WorldDatabase::mapId(*f.maps.findById(95));
    feature.site = {"surface/0", 10, 0, 0, 0, {0, 1}};
    WorldKnowledge knowledge;
    knowledge.visitedMaps.insert("7d21.blv");
    CHECK_FALSE(database.featureVisible(feature, knowledge, false));
    knowledge.floorPlans["7d21.blv"].faceAttributes = {faceAttributeBit(FaceAttribute::SeenByParty), 0};
    CHECK(database.featureVisible(feature, knowledge, false));
    knowledge.floorPlans["7d21.blv"].invisibleFaces.insert(0);
    CHECK_FALSE(database.featureVisible(feature, knowledge, false));
    CHECK(database.featureVisible(feature, knowledge, true));
    knowledge.interactedSites["7out02.odm"].insert(worldSiteDiscoveryKey(10, "surface/1"));
    CHECK_FALSE(database.featureVisible(feature, knowledge, false));
    knowledge.interactedSites["7d21.blv"].insert(worldSiteDiscoveryKey(10, "surface/1"));
    CHECK(database.featureVisible(feature, knowledge, false));
    feature.site = {"surface/2", 10, 0, 0, 0, {2}};
    CHECK_FALSE(database.featureVisible(feature, knowledge, false));
    feature.mapId = WorldDatabase::mapId(*f.maps.findById(63));
    feature.site = {"decoration/0", 10, -32768, 32768};
    CHECK_FALSE(database.featureVisible(feature, knowledge, false));
    knowledge.regionMaps["7out02.odm"].partiallyRevealedCells = {0x80};
    CHECK_FALSE(database.featureVisible(feature, knowledge, false));
    knowledge.regionMaps["7out02.odm"].fullyRevealedCells = {0x80};
    CHECK(database.featureVisible(feature, knowledge, false));
    feature.site.x += 1024;
    CHECK_FALSE(database.featureVisible(feature, knowledge, false));
    CHECK(knowledge.regionMaps.at("7out02.odm").partiallyRevealedCells == std::vector<uint8_t>{0x80});
}

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
    knowledge.variables[worldNpcHouseKey(42)] = 11;
    knowledge.variables[worldNpcTeacherKey(42, 305)] = 1;
    CHECK(database.query(knowledge, false, "sWoRd", WorldRecordKind::Trainer).size() == 1);
}

TEST_CASE("world database indexes canonical MM7 maps residents and trainers")
{
    MapStats maps;
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    REQUIRE(maps.loadFromRows(canonicalRows("map_stats.txt")));
    REQUIRE(maps.applyOutdoorNavigationRows(canonicalRows("map_navigation.txt")));
    MergedOutdoorTravelTable outdoorTravels;
    REQUIRE(outdoorTravels.loadFromRows(canonicalRows("outdoor_travels.txt")));
    REQUIRE(maps.applyMergedOutdoorTravels(outdoorTravels));
    REQUIRE(houses.loadFromRows(canonicalRows("house_data.txt")));
    REQUIRE(npcs.loadNpcRows(canonicalRows("npc.txt")));
    REQUIRE(teachers.loadFromRows(canonicalRows("teacher_topics.txt")));
    CHECK(houses.get(56)->mapId == maps.findByFileName("out14.odm")->id); // Avlee Outpost.
    CHECK(houses.get(247)->mapId == maps.findByFileName("out09.odm")->id); // Evenmorn Island.
    CHECK(houses.get(54)->mapId == maps.findByFileName("out10.odm")->id); // Mount Nighon.
    CHECK(houses.get(409)->mapId == maps.findByFileName("out12.odm")->id); // Colony Zod transition.
    WorldDatabase database(maps, houses, npcs, teachers, "mm7");
    CHECK(database.find("mm7:house/409") == nullptr); // Dungeon transition cards are not building discoveries.
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

TEST_CASE("world database MM8 catalog preserves canonical records and isolates party discoveries")
{
    std::ifstream file(std::string(OPENYAMM_SOURCE_DIR) + "/assets_dev/engine/world/atlas/mm8.yml");
    REQUIRE(file.is_open());
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const auto presentation = parseWorldAtlasPresentation(text, "mm8");
    CHECK(presentation.name == "Jadame");
    CHECK(presentation.overviewTexture.empty());
    MapStats maps;
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    REQUIRE(maps.loadFromRows(canonicalRows("map_stats.txt")));
    REQUIRE(houses.loadFromRows(canonicalRows("house_data.txt")));
    REQUIRE(npcs.loadNpcRows(canonicalRows("npc.txt")));
    REQUIRE(teachers.loadFromRows(canonicalRows("teacher_topics.txt")));
    WorldDatabase mm8(maps, houses, npcs, teachers, "mm8", presentation.maps);
    CHECK(mm8.query({}, true, "", WorldRecordKind::Region).size() == 14);
    CHECK(mm8.query({}, true, "", WorldRecordKind::Dungeon).size() == 47);
    CHECK(mm8.query({}, false).empty());
    WorldKnowledge knowledge;
    knowledge.visitedMaps = {"7out02.odm"};
    CHECK(mm8.query(knowledge, false).empty());
    knowledge.visitedMaps.insert("out01.odm");
    const auto discovered = mm8.query(knowledge, false, "", WorldRecordKind::Region);
    REQUIRE(discovered.size() == 1);
    CHECK(discovered[0]->map == maps.findByFileName("out01.odm"));
    for (const auto &record : mm8.records())
        CHECK(record.map->worldId == "mm8");
    const auto trainers = mm8.query({}, true, "Sword", WorldRecordKind::Trainer);
    REQUIRE(!trainers.empty());
    CHECK(trainers[0]->teacher == teachers.get(trainers[0]->teacher->topicId));
    CHECK(mm8.find(WorldDatabase::mapId(*maps.findByFileName("d05.blv")))->parentId
        == WorldDatabase::mapId(*maps.findByFileName("out01.odm")));
    CHECK(mm8.find(WorldDatabase::mapId(*maps.findByFileName("d27.blv")))->parentId
        == WorldDatabase::mapId(*maps.findByFileName("elema.odm")));
    CHECK_THROWS(parseWorldAtlasPresentation(text, "mm7"));
    CHECK_THROWS(parseWorldAtlasPresentation("schema: 1\nworld: mm8\nname: Jadame\nmaps:"
        "\n  - {file: out01.odm, position: [.nan, 0]}\n", "mm8"));
}

TEST_CASE("world database separates MM7 discoveries from distinct walking routes")
{
    MapStats maps;
    REQUIRE(maps.loadFromRows(canonicalRows("map_stats.txt")));
    MergedOutdoorTravelTable outdoorTravels;
    REQUIRE(outdoorTravels.loadFromRows(canonicalRows("outdoor_travels.txt")));
    REQUIRE(maps.applyMergedOutdoorTravels(outdoorTravels));
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    WorldDatabase database(maps, houses, npcs, teachers, "mm7");
    WorldKnowledge knowledge;
    // Reproduce the reported journey without inventing building/NPC discoveries.
    knowledge.visitedMaps = {"7out02.odm", "7out04.odm", "out14.odm"};
    const auto regions = database.query(knowledge, false, "", WorldRecordKind::Region);
    REQUIRE(regions.size() == 3);
    CHECK(regions[0]->name() == "Avlee");
    CHECK(regions[1]->name() == "Harmondale");
    CHECK(regions[2]->name() == "The Tularean Forest");

    for (const auto &file : {"7out02.odm", "out14.odm"})
    {
        const auto *map = maps.findByFileName(file);
        REQUIRE(map != nullptr);
        const auto scope = WorldDatabase::mapId(*map);
        // Two genuine exits used to masquerade as duplicate discovered regions.
        CHECK(database.query(knowledge, false, "", {}, scope).size() == 3);
        const auto local = database.query(knowledge, false, "", {}, scope, false);
        REQUIRE(local.size() == 1);
        CHECK(local.front()->map == map);
        CHECK(local.front()->kind == WorldRecordKind::Region);
        CHECK(database.query(knowledge, false, "Tularean", {}, scope, false).empty());

        const auto routes = database.query(knowledge, false, "", WorldRecordKind::Travel, scope);
        REQUIRE(routes.size() == 2);
        CHECK(routes[0]->id != routes[1]->id);
        CHECK(routes[0]->name() != routes[1]->name());
        CHECK(routes[0]->walkingEdge != routes[1]->walkingEdge);
        for (const auto *route : routes)
        {
            CHECK(route->destination == maps.findByFileName("7out04.odm"));
            CHECK(route->name().starts_with("Walk "));
            CHECK((route->walkingRoute->travelDays == 5 || route->walkingRoute->travelDays == 7));
        }
    }
    knowledge.visitedMaps.erase("7out04.odm");
    CHECK(database.query(knowledge, false, "", WorldRecordKind::Travel).empty());
}

TEST_CASE("world database walking graph preserves directed exits and canonical quest locks")
{
    MapStats maps;
    REQUIRE(maps.loadFromRows(canonicalRows("map_stats.txt")));
    MergedOutdoorTravelTable travels;
    REQUIRE(travels.loadFromRows(canonicalRows("outdoor_travels.txt")));
    REQUIRE(maps.applyMergedOutdoorTravels(travels));
    std::ifstream file(std::string(OPENYAMM_SOURCE_DIR) + "/assets_dev/engine/world/atlas/mm7.yml");
    REQUIRE(file.is_open());
    const std::string yaml{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const WorldAtlasPresentation presentation = parseWorldAtlasPresentation(yaml, "mm7");
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    WorldDatabase world(maps, houses, npcs, teachers, "mm7", presentation.maps);
    WorldKnowledge knowledge;
    knowledge.visitedMaps = {"7out02.odm", "7out04.odm"};
    const auto links = world.travelLinks(knowledge, false);
    REQUIRE(links.size() == 3);
    std::unordered_set<std::string> distinct;
    for (const WorldTravelLink &link : links)
    {
        CHECK(distinct.insert(link.recordId).second);
        const WorldRecord *pRoute = world.find(link.recordId);
        REQUIRE(pRoute != nullptr);
        CHECK(link.source == *world.find(link.sourceId)->position);
        CHECK(link.destination == *world.find(link.destinationId)->position);
        CHECK_FALSE(link.locked);
        CHECK(pRoute->map->worldId == "mm7");
        CHECK(pRoute->destination->worldId == "mm7");
    }
    knowledge.visitedMaps.erase("7out04.odm");
    CHECK(world.travelLinks(knowledge, false).empty());
    knowledge.visitedMaps = {"out14.odm", "7out15.odm"};
    const auto lockedLinks = world.travelLinks(knowledge, false);
    const auto entersShoals = [](const WorldTravelLink &link)
    { return link.sourceId == "world.mm7.map.out14" && link.destinationId == "world.mm7.map.7out15"; };
    CHECK(std::none_of(lockedLinks.begin(), lockedLinks.end(), entersShoals));
    const auto guide = world.travelLinks(knowledge, true);
    const auto shoals = std::find_if(guide.begin(), guide.end(), entersShoals);
    REQUIRE(shoals != guide.end());
    CHECK(shoals->locked);
    const WorldRecord *pRoute = world.find(shoals->recordId);
    REQUIRE(pRoute != nullptr);
    CHECK(pRoute->walkingRoute->requiredOriginSurface == MapTransitionSurfaceRequirement::Water);
    // The game accepts any one of the suit-related bits, not all three.
    for (uint32_t bit : {642, 643, 783})
    {
        knowledge.questBits = {bit};
        const auto unlocked = world.travelLinks(knowledge, false);
        const auto entrance = std::find_if(unlocked.begin(), unlocked.end(), entersShoals);
        REQUIRE(entrance != unlocked.end());
        CHECK(entrance->recordId == shoals->recordId);
        CHECK_FALSE(entrance->locked);
    }
    knowledge.questBits = {999};
    const auto unrelated = world.travelLinks(knowledge, false);
    CHECK(std::none_of(unrelated.begin(), unrelated.end(), entersShoals));
    CHECK(knowledge.questBits == std::unordered_set<uint32_t>{999});
    CHECK(knowledge.visitedMaps.size() == 2);
    WorldDatabase noPresentation(maps, houses, npcs, teachers, "mm7");
    CHECK(noPresentation.travelLinks(knowledge, true).empty());
    CHECK_FALSE(noPresentation.query(knowledge, true, "", WorldRecordKind::Travel).empty());
}

TEST_CASE("world database MM8 walking graph uses active navigation without disclosing unknown regions")
{
    MapStats maps;
    REQUIRE(maps.loadFromRows(canonicalRows("map_stats.txt")));
    REQUIRE(maps.applyOutdoorNavigationRows(canonicalRows("map_navigation.txt")));
    std::ifstream file(std::string(OPENYAMM_SOURCE_DIR) + "/assets_dev/engine/world/atlas/mm8.yml");
    REQUIRE(file.is_open());
    const std::string yaml{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const WorldAtlasPresentation presentation = parseWorldAtlasPresentation(yaml, "mm8");
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    WorldDatabase world(maps, houses, npcs, teachers, "mm8", presentation.maps);
    WorldKnowledge knowledge;
    knowledge.visitedMaps = {"out02.odm", "out03.odm", "7out03.odm"};
    const auto links = world.travelLinks(knowledge, false);
    REQUIRE(links.size() == 2);
    for (const WorldTravelLink &link : links)
    {
        const WorldRecord *pRoute = world.find(link.recordId);
        REQUIRE(pRoute != nullptr);
        CHECK(pRoute->map->worldId == "mm8");
        CHECK(pRoute->destination->worldId == "mm8");
        CHECK(pRoute->walkingRoute->travelDays == 1);
    }
    knowledge.visitedMaps.erase("out03.odm");
    CHECK(world.travelLinks(knowledge, false).empty());
    CHECK_FALSE(world.travelLinks(knowledge, true).empty());
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

TEST_CASE("world travel graph draws only learned transport endpoints and keeps distinct service slots")
{
    MapStats maps;
    HouseTable houses;
    MergedHouseRuleTable rules;
    MergedTransportLocationTable locations;
    REQUIRE(maps.loadFromRows(canonicalRows("map_stats.txt")));
    REQUIRE(houses.loadFromRows(canonicalRows("house_data.txt")));
    REQUIRE(rules.loadFromRows(canonicalRows("house_rules.txt")));
    REQUIRE(locations.loadFromRows(canonicalRows("transport_locations.txt")));
    REQUIRE(houses.applyHouseRules(rules, locations, maps));
    std::string world = "mm7", destination = "7out01.odm", foreign = "out01.odm";
    uint32_t houseId = 462;
    WorldTravelKind kind = WorldTravelKind::Coach;
    SUBCASE("MM7 coach") {}
    SUBCASE("MM8 boat")
    {
        world = "mm8"; destination = "out02.odm"; foreign = "7out02.odm";
        houseId = 479; kind = WorldTravelKind::Boat;
    }
    INFO(world);
    std::ifstream file(std::string(OPENYAMM_SOURCE_DIR) + "/assets_dev/engine/world/atlas/" + world + ".yml");
    REQUIRE(file.is_open());
    const std::string yaml{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const auto presentation = parseWorldAtlasPresentation(yaml, world);
    const HouseEntry *pHouse = houses.get(houseId);
    REQUIRE(pHouse != nullptr);
    const MapStatsEntry *pSource = maps.findById(pHouse->mapId);
    REQUIRE(pSource != nullptr);
    EventRuntimeState state;
    TransportOffer offer;
    offer.route.routeIndex = 99;
    offer.route.mapFileName = destination;
    offer.route.requiredQBit = 10023;
    offer.quote = {2, 8};
    recordWorldTransportObservation(state, houseId, offer, maps.getEntries());
    WorldKnowledge knowledge;
    knowledge.variables = state.namedGlobalVars;
    knowledge.variables[WorldDatabase::houseDiscoveryKey(houseId)] = 1;
    knowledge.visitedMaps.insert(pSource->fileName);
    const auto observed = worldObservedTransport(houses, maps, state.namedGlobalVars);
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    WorldDatabase database(maps, houses, npcs, teachers, world, presentation.maps, {}, observed);
    const auto links = database.travelLinks(knowledge, false);
    REQUIRE(links.size() == 1);
    const WorldTravelLink &link = links.front();
    CHECK(link.kind == kind);
    CHECK_FALSE(link.locked); // Retained last-offered knowledge does not forecast availability.
    CHECK(link.sourceId == WorldDatabase::mapId(*pSource));
    CHECK(link.destinationId == WorldDatabase::mapId(*maps.findByFileName(destination)));
    CHECK(link.source == *database.find(link.sourceId)->position);
    CHECK(link.destination == *database.find(link.destinationId)->position);
    CHECK(database.travelLinks(knowledge, false, "unrelated search").empty());
    CHECK(database.travelLinks(knowledge, false, pHouse->name).size() == 1);
    WorldKnowledge unknown = knowledge;
    unknown.variables.erase(WorldDatabase::mapDiscoveryKey(*maps.findByFileName(destination)));
    CHECK(database.travelLinks(unknown, false).empty());
    unknown = knowledge;
    unknown.variables.erase(WorldDatabase::houseDiscoveryKey(houseId));
    CHECK(database.travelLinks(unknown, false).empty());
    WorldDatabase withoutAnchors(maps, houses, npcs, teachers, world, {}, {}, observed);
    CHECK(withoutAnchors.travelLinks(knowledge, false).empty());
    CHECK(withoutAnchors.query(knowledge, false, "", WorldRecordKind::Travel).size() == 1);

    offer.route.routeIndex = 100;
    recordWorldTransportObservation(state, houseId, offer, maps.getEntries());
    knowledge.variables = state.namedGlobalVars;
    knowledge.variables[WorldDatabase::houseDiscoveryKey(houseId)] = 1;
    WorldDatabase parallel(maps, houses, npcs, teachers, world, presentation.maps, {},
        worldObservedTransport(houses, maps, state.namedGlobalVars));
    const auto distinct = parallel.travelLinks(knowledge, false);
    REQUIRE(distinct.size() == 2);
    CHECK(distinct[0].recordId != distinct[1].recordId);
    CHECK(distinct[0].destinationId == distinct[1].destinationId);
    const WorldMapLocation departure{"test-door", link.sourceId, database.find(link.recordId)->parentId};
    CHECK(parallel.recordsAtLocation(departure, knowledge, false).size() == 1);
    CHECK(parallel.recordsAtLocation(departure, knowledge, false, true).size() == 3);

    WorldTransportState current;
    current.houses[houseId].push_back({offer.route, {}, false, true});
    WorldDatabase live(maps, houses, npcs, teachers, world, presentation.maps, {}, current);
    const std::string routeId = departure.recordId + "/route/100";
    const auto locked = live.travelLinks(knowledge, true);
    const auto match = std::find_if(locked.begin(), locked.end(), [&](const WorldTravelLink &candidate)
    { return candidate.recordId == routeId; });
    REQUIRE(match != locked.end());
    CHECK(match->locked);
    CHECK(live.travelLinks(knowledge, false).empty()); // Current state is never an observed timetable.
    knowledge.questBits.insert(10023);
    const auto unlocked = live.travelLinks(knowledge, true);
    CHECK_FALSE(std::find_if(unlocked.begin(), unlocked.end(), [&](const WorldTravelLink &candidate)
        { return candidate.recordId == routeId; })->locked);

    current.houses[houseId].front().route.mapFileName = foreign;
    WorldDatabase otherWorld(maps, houses, npcs, teachers, world, presentation.maps, {}, current);
    CHECK(otherWorld.find(routeId) == nullptr);
}

TEST_CASE("world database transport knowledge keeps observed quotes separate from live and authored timetables")
{
    MapStats maps;
    HouseTable houses;
    MergedHouseRuleTable rules;
    MergedTransportLocationTable transport;
    REQUIRE(maps.loadFromRows(canonicalRows("map_stats.txt")));
    REQUIRE(houses.loadFromRows(canonicalRows("house_data.txt")));
    REQUIRE(rules.loadFromRows(canonicalRows("house_rules.txt")));
    REQUIRE(transport.loadFromRows(canonicalRows("transport_locations.txt")));
    REQUIRE(houses.applyHouseRules(rules, transport, maps));
    REQUIRE(houses.get(462) != nullptr);
    EventRuntimeState state;
    EventRuntimeState::TransportRouteOverride replacement;
    replacement.houseId = 462;
    replacement.routeIndex = 4;
    replacement.mapFileName = "7out01.odm";
    replacement.destinationName = "Emerald Island";
    replacement.travelDays = 4;
    replacement.daysAvailable = {false, true, false, false, false, false, false};
    state.transportRouteOverrides[EventRuntime::transportRouteOverrideKey(462, 4)] = replacement;
    const WorldTransportState current = worldCurrentTransport(houses, 64, state);
    REQUIRE(current.houses.contains(462));
    const auto route = std::find_if(current.houses.at(462).begin(), current.houses.at(462).end(),
        [](const WorldTransportEntry &entry) { return entry.route.routeIndex == 4; });
    REQUIRE(route != current.houses.at(462).end());
    CHECK(route->current);
    CHECK(route->route.mapFileName == "7out01.odm");
    const TransportOffer offer{route->route, {2, 8}};
    recordWorldTransportObservation(state, 462, offer, maps.getEntries());
    const WorldTransportState observed = worldObservedTransport(houses, maps, state.namedGlobalVars);
    REQUIRE(observed.issues.empty());
    REQUIRE(observed.houses.at(462).size() == 1);
    CHECK(observed.houses.at(462).front().quote->gold == 8);
    CHECK(observed.houses.at(462).front().quote->travelDays == 2);
    CHECK(observed.houses.at(462).front().route.travelDays == 4);
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    WorldKnowledge knowledge;
    knowledge.variables = state.namedGlobalVars;
    knowledge.variables[WorldDatabase::houseDiscoveryKey(462)] = 1;
    knowledge.visitedMaps = {"7out03.odm", "7out01.odm", "7out06.odm"};
    WorldDatabase memory(maps, houses, npcs, teachers, "mm7", {}, {}, observed);
    const auto known = memory.query(knowledge, false, "", WorldRecordKind::Travel);
    REQUIRE(known.size() == 1);
    CHECK(known.front()->destination->fileName == "7out01.odm");
    CHECK(known.front()->transport->quote->gold == 8);
    CHECK(known.front()->transport->observed);
    // A secret change to destination, schedule and duration must not alter last-offered knowledge.
    replacement.mapFileName = "7out06.odm";
    replacement.travelDays = 6;
    replacement.daysAvailable = {false, false, true, false, false, false, false};
    state.transportRouteOverrides[EventRuntime::transportRouteOverrideKey(462, 4)] = replacement;
    WorldDatabase live(maps, houses, npcs, teachers, "mm7", {}, {}, worldCurrentTransport(houses, 64, state));
    const WorldRecord *pLive = live.find("mm7:house/462/route/4");
    REQUIRE(pLive != nullptr);
    CHECK(pLive->destination->fileName == "7out06.odm");
    CHECK(pLive->transport->route.travelDays == 6);
    CHECK_FALSE(live.visible(*pLive, knowledge, false));
    CHECK(memory.find(known.front()->id)->destination->fileName == "7out01.odm");
    WorldDatabase authored(maps, houses, npcs, teachers, "mm7");
    CHECK(authored.query(knowledge, false, "", WorldRecordKind::Travel).empty());
    CHECK_FALSE(authored.query(knowledge, true, "", WorldRecordKind::Travel).empty());
    // Same slot, newly offered destination: explicit zeros retire the previous learned endpoint.
    TransportOffer newOffer{pLive->transport->route, {5, 12}};
    recordWorldTransportObservation(state, 462, newOffer, maps.getEntries());
    CHECK(state.namedGlobalVars.at(worldTransportKey(462, 4) + "map/world.mm7.map.7out01") == 0);
    const auto updated = worldObservedTransport(houses, maps, state.namedGlobalVars);
    REQUIRE(updated.issues.empty());
    REQUIRE(updated.houses.at(462).size() == 1);
    CHECK(updated.houses.at(462).front().route.mapFileName == "7out06.odm");
    CHECK(updated.houses.at(462).front().quote->gold == 12);
    state.namedGlobalVars[worldTransportKey(462, 4) + "map/world.mm7.map.7out01"] = 1;
    const auto ambiguous = worldObservedTransport(houses, maps, state.namedGlobalVars);
    CHECK_FALSE(ambiguous.issues.empty());
    CHECK(ambiguous.houses.empty());
    state.namedGlobalVars.clear();
    CHECK(worldObservedTransport(houses, maps, state.namedGlobalVars).houses.empty());
}

TEST_CASE("world transport rejects invalid observations without changing previous knowledge")
{
    Fixture f;
    EventRuntimeState state;
    TransportOffer offer;
    offer.route.mapFileName = "7out02.odm";
    offer.route.routeIndex = 7;
    offer.route.travelDays = 3;
    offer.route.daysAvailable[0] = true;
    offer.route.requiredQBit = std::numeric_limits<uint32_t>::max();
    offer.quote = {2, 15};
    recordWorldTransportObservation(state, 11, offer, f.maps.getEntries());
    const auto original = state.namedGlobalVars;
    const auto observed = worldObservedTransport(f.houses, f.maps, original);
    REQUIRE(observed.issues.empty());
    REQUIRE(observed.houses.at(11).size() == 1);
    CHECK(observed.houses.at(11).front().route.requiredQBit == std::numeric_limits<uint32_t>::max());

    SUBCASE("unknown destination") { offer.route.mapFileName = "missing.odm"; }
    SUBCASE("negative price") { offer.quote.gold = -1; }
    SUBCASE("zero personal travel time") { offer.quote.travelDays = 0; }
    SUBCASE("declared duration overflow") { offer.route.travelDays = std::numeric_limits<uint32_t>::max(); }
    SUBCASE("no scheduled days") { offer.route.daysAvailable.fill(false); }
    CHECK_THROWS_AS(recordWorldTransportObservation(state, 11, offer, f.maps.getEntries()), std::invalid_argument);
    CHECK(state.namedGlobalVars == original);
}

TEST_CASE("world transport reports incomplete saved offers rather than inventing table knowledge")
{
    Fixture f;
    EventRuntimeState state;
    TransportOffer offer;
    offer.route.mapFileName = "7out02.odm";
    offer.route.daysAvailable[0] = true;
    offer.quote = {1, 0};
    recordWorldTransportObservation(state, 11, offer, f.maps.getEntries());
    const std::string prefix = worldTransportKey(11, 0);
    SUBCASE("missing quote") { state.namedGlobalVars.erase(prefix + "gold"); }
    SUBCASE("invalid week mask") { state.namedGlobalVars[prefix + "week"] = 128; }
    SUBCASE("unresolved map") { state.namedGlobalVars.erase(prefix + "map/world.mm7.map.7out02"); }
    SUBCASE("noncanonical house ID")
    {
        state.namedGlobalVars.erase(prefix + "seen");
        state.namedGlobalVars["openyamm.discovery.transport/011/0/seen"] = 1;
    }
    const auto observed = worldObservedTransport(f.houses, f.maps, state.namedGlobalVars);
    CHECK(observed.observedOnly);
    CHECK(observed.houses.empty());
    CHECK(observed.issues.size() == 1);
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
    knowledge.variables[worldNpcHouseKey(42)] = 11;
    knowledge.variables[worldNpcTeacherKey(42, 305)] = 1;
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

TEST_CASE("world database outdoor fog shares saved exploration without revealing empty maps")
{
    GameplayMinimapState projection;
    WorldRegionMapKnowledge knowledge;
    const auto alpha = [](const WorldRegionFog &fog, int x, int y)
    { return fog.pixelsBgra[size_t(y * WorldRegionFog::Size + x) * 4 + 3]; };
    auto hidden = buildWorldRegionFog(knowledge, projection);
    REQUIRE(hidden.pixelsBgra.size() == WorldRegionFog::Size * WorldRegionFog::Size * 4);
    CHECK(alpha(hidden, 0, 0) == 255);
    CHECK(alpha(hidden, 87, 87) == 255);
    knowledge.fullyRevealedCells = {0x80};
    knowledge.partiallyRevealedCells = {0xc0};
    auto fog = buildWorldRegionFog(knowledge, projection);
    CHECK(alpha(fog, 0, 0) == 0); // Fully revealed wins over partially revealed.
    CHECK(alpha(fog, 1, 0) == 144);
    CHECK(alpha(fog, 2, 0) == 255);
    CHECK(knowledge.fullyRevealedCells == std::vector<uint8_t>{0x80});
    projection.flipU = true;
    auto flipped = buildWorldRegionFog(knowledge, projection);
    CHECK(alpha(flipped, 87, 0) == 0);
    CHECK(alpha(flipped, 86, 0) == 144);
    CHECK(alpha(flipped, 0, 0) == 255);
    projection.flipU = false;
    projection.flipV = false;
    CHECK(alpha(buildWorldRegionFog(knowledge, projection), 0, 87) == 0);
    projection.revealEntireMap = true; // Atlas still requires actual discoveries even if art allows a full map.
    CHECK(alpha(buildWorldRegionFog({}, projection), 0, 0) == 255);
}

TEST_CASE("world database party marker follows minimap direction and dungeon projection")
{
    constexpr float pi = std::numbers::pi_v<float>;
    CHECK(gameplayMinimapArrowIndex(0) == 7);
    CHECK(gameplayMinimapArrowIndex(pi / 2) == 1);
    CHECK(gameplayMinimapArrowIndex(pi) == 3);
    CHECK(gameplayMinimapArrowIndex(-pi / 2) == 5);
    CHECK(gameplayMinimapArrowIndex(2 * pi) == 7);
    const auto northWest = gameplayMinimapWorldToUv({}, -32768, 32768);
    CHECK(northWest.x == 0);
    CHECK(northWest.y == 0);
    IndoorMapData map;
    map.vertices = {{-100, -100, 0}, {100, 100, 0}};
    map.faces.resize(2);
    map.outlines = {{0, 1, 0, 1}};
    const auto plan = buildWorldFloorPlan(map, {}, true);
    const auto center = plan.worldToUv(0, 0);
    CHECK(center.first == doctest::Approx(0.5));
    CHECK(center.second == doctest::Approx(0.5));
    const auto northEast = plan.worldToUv(100, 100);
    CHECK(northEast.first > center.first);
    CHECK(northEast.second < center.second);
}

TEST_CASE("world database locates real doors without revealing unvisited services")
{
    Fixture f;
    auto database = f.database();
    std::string error;
    const auto program = ScriptedEventProgram::loadFromLuaText(R"lua(
        evt.map[3] = function() end
        evt.map[4] = function() end
        evt.map[5] = function() end
        evt.map[6] = function() end
        evt.map[7] = function() end
        evt.meta.map.contextActions = {
            [3] = { kind = "enter_house", source = "opcode", houseId = 11 },
            [4] = { kind = "enter_dungeon", source = "script", targetMap = "7D21.BLV" },
            [5] = { kind = "enter_dungeon", source = "opcode", targetMap = "7d21.blv", hidden = true },
            [6] = { kind = "enter_house", source = "opcode", houseId = 9999 },
            [7] = { kind = "travel", source = "opcode", targetMap = "out01.odm" }
        }
    )lua", "@door-test", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program.has_value(), error);
    OutdoorMapData map;
    map.fileName = "7out02.odm";
    map.worldId = "mm7";
    map.bmodels.resize(2);
    map.bmodels[0].positionX = 999; // ODM vertices already use world coordinates.
    map.bmodels[0].vertices = {{-200, 100, 0}, {0, 100, 0}, {0, 100, 100}, {-200, 100, 100}};
    OutdoorBModelFace face;
    face.cogTriggeredNumber = 3;
    face.vertexIndices = {0, 1, 2, 65535};
    map.bmodels[0].faces.push_back(face);
    face.vertexIndices = {0, 2, 3};
    map.bmodels[0].faces.push_back(face); // Second triangle must not create a duplicate pin.
    map.bmodels[1] = map.bmodels[0]; // A different physical door remains a distinct site.
    OutdoorEntity entry;
    entry.eventIdSecondary = 4;
    entry.x = 12000; entry.y = 8000;
    map.entities.push_back(entry);
    for (uint16_t event : {5, 6, 7})
    {
        entry.eventIdSecondary = event;
        map.entities.push_back(entry);
    }
    database.indexMapLocations(map, *program);
    REQUIRE(database.locations().size() == 3);
    CHECK(database.locations()[0].x == -100);
    CHECK(database.locations()[0].y == 100);
    CHECK(database.locations()[0].id != database.locations()[1].id);
    database.indexMapLocations(map, *program); // Reindex is idempotent.
    REQUIRE(database.locations().size() == 3);
    const auto &shop = database.locations()[0];
    const auto &cave = database.locations()[2];
    const auto *caveRecord = database.find(cave.recordId);
    REQUIRE(caveRecord != nullptr);
    WorldKnowledge knowledge;
    knowledge.visitedMaps.insert("7out02.odm");
    CHECK_FALSE(database.locationVisible(shop, knowledge, false));
    CHECK_FALSE(database.locationVisible(cave, knowledge, false));
    CHECK_FALSE(database.visible(*caveRecord, knowledge, false));
    CHECK(database.query(knowledge, false, "Sword").empty());
    CHECK(database.locationVisible(shop, knowledge, true));
    CHECK(database.recordsAtLocation(shop, knowledge, true).size() == 3);
    CHECK(knowledge.variables.empty());

    knowledge.regionMaps["7out02.odm"].fullyRevealedCells.assign(88 * 88 / 8, 255);
    CHECK(database.locationVisible(cave, knowledge, false));
    CHECK(database.visible(*caveRecord, knowledge, false));
    CHECK(database.query(knowledge, false, "Cave", WorldRecordKind::Dungeon, shop.mapId).size() == 1);
    CHECK_FALSE(database.locationVisible(shop, knowledge, false)); // Terrain exploration does not meet NPCs.
    CHECK(knowledge.floorPlans.empty()); // Knowing an entrance never grants its interior plan.
    knowledge.variables[WorldDatabase::houseDiscoveryKey(11)] = 1;
    CHECK(database.locationVisible(shop, knowledge, false));
    CHECK(database.recordsAtLocation(shop, knowledge, false).size() == 1);
    knowledge.variables[WorldDatabase::npcDiscoveryKey(42)] = 1;
    knowledge.variables[worldNpcHouseKey(42)] = 11;
    knowledge.variables[worldNpcTeacherKey(42, 305)] = 1;
    CHECK(database.recordsAtLocation(shop, knowledge, false).size() == 3);
    knowledge.visitedMaps.clear();
    CHECK_FALSE(database.locationVisible(shop, knowledge, false));
    CHECK_FALSE(database.locationVisible(cave, knowledge, false));
    CHECK(database.query(knowledge, false, "Sword").empty());
}

TEST_CASE("world database entrance discovery requires full exploration of the correct cell")
{
    WorldRegionMapKnowledge knowledge;
    CHECK_FALSE(worldRegionPointExplored(knowledge, -32768, 32768));
    knowledge.partiallyRevealedCells = {0xc0};
    CHECK_FALSE(worldRegionPointExplored(knowledge, -32768, 32768));
    knowledge.fullyRevealedCells = {0x80};
    CHECK(worldRegionPointExplored(knowledge, -32768, 32768));
    CHECK_FALSE(worldRegionPointExplored(knowledge, -31000, 32768));
    CHECK_FALSE(worldRegionPointExplored(knowledge, 0, 0));
    CHECK_FALSE(worldRegionPointExplored(knowledge, std::numeric_limits<float>::quiet_NaN(), 0));
    CHECK_FALSE(worldRegionPointExplored(knowledge, 32769, 0));
    knowledge.fullyRevealedCells.assign(88 * 88 / 8, 255);
    CHECK(worldRegionPointExplored(knowledge, 32768, -32768));
}

TEST_CASE("world database informed quest locations reveal no terrain or unrelated services")
{
    Fixture fixture;
    const WorldDatabase world = fixture.database();
    WorldKnowledge knowledge;
    const WorldRecord *pDungeon = world.find("world.mm7.map.7d21");
    const WorldRecord *pRegion = world.find("world.mm7.map.7out02");
    REQUIRE(pDungeon != nullptr);
    REQUIRE(pRegion != nullptr);
    knowledge.informedRecords.insert(pDungeon->id);
    CHECK(world.visible(*pDungeon, knowledge, false));
    CHECK_FALSE(world.visible(*pRegion, knowledge, false));
    CHECK(knowledge.visitedMaps.empty());
    CHECK(knowledge.regionMaps.empty());
    CHECK(knowledge.floorPlans.empty());
    CHECK(knowledge.variables.empty());
    knowledge.informedRecords.insert("mm7:house/11");
    CHECK(world.query(knowledge, false, "Blade's End").empty());
    knowledge.informedRecords.insert(pRegion->id);
    CHECK(world.query(knowledge, false, "Blade's End").size() == 1);
    CHECK(world.query(knowledge, false, "Townsaver").empty());
}

TEST_CASE("world database NPCs use current relocation topic and availability overrides in Guide")
{
    Fixture fixture;
    std::vector<std::string> house(24);
    house[0] = "12"; house[2] = "House"; house[3] = "63"; house[5] = "New residence";
    REQUIRE(fixture.houses.loadFromRows({house}));
    REQUIRE(fixture.teachers.loadFromRows({{"TopicId", "Notes", "SkillId", "Mastery", "TextId"},
        {"305", "Grand Master Sword", "1", "3", "305"}, {"306", "Master Axe", "3", "2", "306"}}));
    EventRuntimeState state;
    state.npcHouseOverrides[42] = 12;
    state.npcTopicOverrides[42][0] = 306;
    state.npcNameOverrides[42] = "Changed NPC name";
    WorldNpcState current = worldCurrentNpcs(fixture.npcs, state);
    WorldDatabase world(fixture.maps, fixture.houses, fixture.npcs, fixture.teachers, "mm7", {}, current);
    const WorldRecord *pNpc = world.find("mm7:npc/42");
    REQUIRE(pNpc != nullptr);
    CHECK(pNpc->house->id == 12);
    CHECK(pNpc->parentId == "mm7:house/12");
    CHECK(pNpc->name() == "Changed NPC name");
    CHECK_FALSE(pNpc->lastObservedNpc);
    CHECK(world.find("mm7:npc/42/trainer/305") == nullptr);
    REQUIRE(world.find("mm7:npc/42/trainer/306") != nullptr);
    // The record keeps external table pointers and value-owned overrides through a move/copy of the index.
    WorldDatabase copied = world;
    world = fixture.database();
    CHECK(copied.find("mm7:npc/42")->name() == "Changed NPC name");
    state.unavailableNpcIds.insert(42);
    current = worldCurrentNpcs(fixture.npcs, state);
    WorldDatabase unavailable(fixture.maps, fixture.houses, fixture.npcs, fixture.teachers, "mm7", {}, current);
    CHECK(unavailable.find("mm7:npc/42") == nullptr);
    state.unavailableNpcIds.clear();
    state.npcHouseOverrides[42] = 0;
    current = worldCurrentNpcs(fixture.npcs, state);
    WorldDatabase removed(fixture.maps, fixture.houses, fixture.npcs, fixture.teachers, "mm7", {}, current);
    CHECK(removed.find("mm7:npc/42") == nullptr);
}

TEST_CASE("world database NPC discovery is last observed rather than a secret current address")
{
    Fixture fixture;
    EventRuntimeState state;
    recordWorldNpcObservation(state, 42, 11, {305});
    state.npcHouseOverrides[42] = 0;
    state.npcTopicOverrides[42][0] = 0;
    state.npcNameOverrides[42] = "Secret identity";
    const WorldNpcState observed = worldObservedNpcs(fixture.npcs, fixture.teachers, state.namedGlobalVars);
    WorldDatabase world(fixture.maps, fixture.houses, fixture.npcs, fixture.teachers, "mm7", {}, observed);
    const WorldRecord *pNpc = world.find("mm7:npc/42");
    REQUIRE(pNpc != nullptr);
    CHECK(pNpc->name() == "Townsaver");
    CHECK(pNpc->house->id == 11);
    CHECK(pNpc->lastObservedNpc);
    CHECK(world.find("mm7:npc/42/trainer/305") != nullptr);
    CHECK(fixture.npcs.getNpc(42)->houseId == 11);
    // A new actual meeting updates known topics and address, including removal of a teacher offer.
    recordWorldNpcObservation(state, 42, 11, {});
    CHECK(state.namedGlobalVars.at("openyamm.discovery.npc/42/teacher/305") == 0);
    const WorldNpcState updated = worldObservedNpcs(fixture.npcs, fixture.teachers, state.namedGlobalVars);
    WorldDatabase metAgain(fixture.maps, fixture.houses, fixture.npcs, fixture.teachers, "mm7", {}, updated);
    REQUIRE(metAgain.find("mm7:npc/42") != nullptr);
    CHECK(metAgain.find("mm7:npc/42/trainer/305") == nullptr);
    recordWorldNpcObservation(state, 42, 0, {305});
    const WorldNpcState remote = worldObservedNpcs(fixture.npcs, fixture.teachers, state.namedGlobalVars);
    WorldDatabase noAddress(fixture.maps, fixture.houses, fixture.npcs, fixture.teachers, "mm7", {}, remote);
    CHECK(noAddress.find("mm7:npc/42") == nullptr);
}

TEST_CASE("world database old NPC discovery flags do not invent a last met address")
{
    Fixture fixture;
    std::unordered_map<std::string, int32_t> legacy{{WorldDatabase::npcDiscoveryKey(42), 1}};
    const WorldNpcState observed = worldObservedNpcs(fixture.npcs, fixture.teachers, legacy);
    CHECK(observed.observedOnly);
    CHECK(observed.houses.empty());
    WorldDatabase world(fixture.maps, fixture.houses, fixture.npcs, fixture.teachers, "mm7", {}, observed);
    CHECK(world.query(WorldKnowledge{}, true, "Townsaver").empty());
    EventRuntimeState state;
    recordWorldNpcObservation(state, 42, UINT32_MAX, {});
    const WorldNpcState fullRange = worldObservedNpcs(fixture.npcs, fixture.teachers, state.namedGlobalVars);
    CHECK(fullRange.houses.at(42) == UINT32_MAX);
    CHECK_THROWS_AS(recordWorldNpcObservation(state, 0, 11, {}), std::invalid_argument);
}

TEST_CASE("NPC topic resolution preserves sparse slot overrides without walking absent slots")
{
    Fixture fixture;
    const std::unordered_map<uint32_t, uint32_t> overrides{{0, 0}, {UINT32_MAX, 306}, {3, 307}};
    CHECK(fixture.npcs.getTopicIdsForNpc(42, &overrides) == std::vector<uint32_t>{307, 306});
    CHECK(fixture.npcs.getTopicIdsForNpc(42) == std::vector<uint32_t>{305});
    CHECK(fixture.npcs.getTopicIdsForNpc(1000000, &overrides).empty());
}

TEST_CASE("world database packaged houses agree with base event metadata and physical geometry")
{
    const char *pRoot = std::getenv("OPENYAMM_ATLAS_AUDIT_ROOT");
    if (pRoot == nullptr)
    {
        MESSAGE("Packaged-content audit requires OPENYAMM_ATLAS_AUDIT_ROOT from tools/prepare_atlas_audit.py.");
        return;
    }
    const std::filesystem::path root(pRoot);
    const YAML::Node manifest = YAML::LoadFile((root / "manifest.json").string());
    REQUIRE(manifest["format"].as<int>() == 1);
    REQUIRE(manifest["scope"].as<std::string>() == "atlas-mm7-mm8-base-locations");
    const auto read = [&root, &manifest](const std::string &virtualPath) -> std::optional<std::string>
    {
        if (!manifest["files"][virtualPath]) return std::nullopt;
        std::ifstream file(root / virtualPath, std::ios::binary);
        REQUIRE_MESSAGE(file.is_open(), virtualPath);
        return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    };
    const auto support = read("engine/scripts/common/event_support.lua");
    REQUIRE(support.has_value());
    const auto rows = [&read](const std::string &file)
    {
        const auto text = read("engine/data_tables/" + file);
        REQUIRE(text.has_value());
        const auto table = OpenYAMM::Engine::TextTable::parseTabSeparated(*text);
        REQUIRE(table.has_value());
        std::vector<std::vector<std::string>> result;
        for (size_t i = 0; i < table->getRowCount(); ++i)
            result.push_back(table->getRow(i));
        return result;
    };
    MapStats maps; HouseTable houses;
    REQUIRE(maps.loadFromRows(rows("map_stats.txt")));
    const auto houseRows = rows("house_data.txt");
    REQUIRE(houses.loadFromRows(houseRows));
    std::set<uint32_t> transitionCards;
    for (const auto &row : houseRows)
    {
        if (row.size() > 2 && row[2] == "Dungeon Ent")
            transitionCards.insert(uint32_t(std::stoul(row[0])));
    }
    std::map<uint32_t, std::set<int>> locations;
    size_t auditedMaps = 0;
    for (const std::string &world : {"mm7", "mm8"})
    {
        for (const MapStatsEntry &map : maps.getEntries())
        {
            if (map.worldId != world) continue;
            const std::string prefix = "worlds/" + world + "/";
            const auto lua = read(prefix + "events/maps/"
                + std::filesystem::path(map.fileName).stem().string() + ".lua");
            const auto binary = read(prefix + "maps/" + map.fileName);
            if (!lua || !binary) continue; // Only maps supplied in this package revision are audited.
            const std::vector<uint8_t> bytes(binary->begin(), binary->end());
            ++auditedMaps;
            std::string error;
            const auto program = ScriptedEventProgram::loadFromLuaText(*support + "\n" + *lua, map.fileName,
                ScriptedEventScope::Map, error);
            REQUIRE_MESSAGE(program.has_value(), (map.fileName + ": " + error));
            std::vector<WorldInteractionSite> sites;
            if (std::filesystem::path(map.fileName).extension() == ".odm")
            {
                const auto geometry = OutdoorMapDataLoader().loadFromBytes(bytes);
                REQUIRE_MESSAGE(geometry.has_value(), map.fileName);
                sites = worldInteractionSites(*geometry);
            }
            else
            {
                const auto geometry = IndoorMapDataLoader().loadFromBytes(bytes);
                REQUIRE_MESSAGE(geometry.has_value(), map.fileName);
                sites = worldInteractionSites(*geometry);
            }
            for (const WorldInteractionSite &site : sites)
            {
                const auto action = program->getContextActionMetadata(site.eventId);
                if (action && action->kind == "enter_house" && action->houseId)
                    locations[*action->houseId].insert(map.id);
            }
        }
    }
    REQUIRE(auditedMaps > 80);
    REQUIRE(locations.size() > 300);
    size_t ambiguous = 0;
    for (const auto &[id, actual] : locations)
    {
        // Unlabelled legacy transition/movie cards are deliberately not loaded as usable venues.
        if (transitionCards.contains(id)) continue;
        const HouseEntry *pHouse = houses.get(id);
        REQUIRE_MESSAGE(pHouse != nullptr, id);
        if (actual.size() != 1)
        {
            ++ambiguous;
            continue; // Shared services need multiple location records, not an arbitrary single-map repair.
        }
        INFO("House ", id, " ", pHouse->name);
        CHECK(pHouse->mapId == uint32_t(*actual.begin()));
    }
    MESSAGE("Audited ", auditedMaps, " maps, ", locations.size(), " physical house IDs; ",
        ambiguous, " shared service requires multiple locations.");
}

TEST_CASE("world database indoor services locate resident NPCs and trainers on the floor plan")
{
    Fixture fixture;
    std::vector<std::string> room(24);
    room[0] = "11"; room[2] = "House"; room[3] = "95"; room[5] = "Council room";
    REQUIRE(fixture.houses.loadFromRows({room}));
    WorldDatabase world = fixture.database();
    IndoorMapData geometry;
    geometry.entities.resize(1);
    geometry.entities[0].x = 50000; geometry.entities[0].y = 60000;
    geometry.entities[0].eventIdSecondary = 10;
    std::string error;
    const auto program = ScriptedEventProgram::loadFromLuaText(R"lua(
        evt.map[10] = function() end
        evt.meta.map.contextActions = {
            [10] = { kind = "enter_house", source = "opcode", houseId = 11 }
        }
    )lua", "@indoor-service", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program.has_value(), error);
    const MapStatsEntry &map = *fixture.maps.findById(95);
    world.indexMapLocations(map, geometry, *program);
    REQUIRE(world.locations().size() == 1);
    const WorldMapLocation &location = world.locations()[0];
    CHECK(location.mapId == "world.mm7.map.7d21");
    CHECK(location.recordId == "mm7:house/11");
    CHECK(location.x == 50000); // Indoor projection is not limited by the outdoor terrain coordinate range.
    CHECK(location.y == 60000);
    CHECK(world.recordsAtLocation(location, {}, true).size() == 3);
    CHECK_FALSE(world.locationVisible(location, {}, false));
    WorldKnowledge known;
    known.visitedMaps.insert("7d21.blv");
    known.variables[WorldDatabase::houseDiscoveryKey(11)] = 1;
    known.variables[WorldDatabase::npcDiscoveryKey(42)] = 1;
    known.variables[worldNpcHouseKey(42)] = 11;
    known.variables[worldNpcTeacherKey(42, 305)] = 1;
    CHECK(world.locationVisible(location, known, false));
    CHECK(world.recordsAtLocation(location, known, false).size() == 3);
    world.indexMapLocations(map, geometry, *program);
    CHECK(world.locations().size() == 1);
}

TEST_CASE("world database shared discovery queries reject secret current NPC locations and teaching offers")
{
    Fixture fixture;
    WorldKnowledge knowledge;
    knowledge.variables[WorldDatabase::mapDiscoveryKey(*fixture.maps.findById(63))] = 1;
    knowledge.variables[WorldDatabase::npcDiscoveryKey(42)] = 1;
    const WorldDatabase base = fixture.database();
    CHECK(base.query(knowledge, false, "", WorldRecordKind::Npc).empty());
    knowledge.variables[worldNpcHouseKey(42)] = 11;
    CHECK(base.query(knowledge, false, "", WorldRecordKind::Npc).size() == 1);
    CHECK(base.query(knowledge, false, "", WorldRecordKind::Trainer).empty());
    knowledge.variables[worldNpcTeacherKey(42, 305)] = 1;
    CHECK(base.query(knowledge, false, "", WorldRecordKind::Trainer).size() == 1);
    knowledge.variables[worldNpcHouseKey(42)] = 12;
    CHECK(base.query(knowledge, false, "", WorldRecordKind::Npc).empty());
    CHECK(base.query(knowledge, false, "", WorldRecordKind::Trainer).empty());
    CHECK(base.query(knowledge, true, "", WorldRecordKind::Npc).size() == 1);
}
