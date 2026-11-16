#include "doctest/doctest.h"
#include "game/maps/SaveGame.h"
#include "game/world/WorldMapNotes.h"
#include "game/world/WorldMapKnowledge.h"
#include "game/world/WorldDatabase.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace OpenYAMM::Game;

namespace
{
struct AtlasSaveFixture
{
    std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("atlas-port-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".oysav");
    ~AtlasSaveFixture() { std::error_code error; std::filesystem::remove(path, error); }
};
}

TEST_CASE("Atlas port: upstream saves and Atlas observations round trip without a core version change")
{
    AtlasSaveFixture fixture;
    Party party;
    party.seed(Party::createDefaultSeed());
    GameSaveData save;
    save.party = party.snapshot();
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(fixture.path, save, error), error);
    std::ifstream vanillaFile(fixture.path, std::ios::binary);
    const std::vector<char> vanilla{std::istreambuf_iterator<char>(vanillaFile), {}};
    vanillaFile.close();
    REQUIRE(loadGameDataFromPath(fixture.path, error));

    save.party.identifiedItemIds = {23, 81};
    save.outdoorWorld.eventRuntimeState.emplace();
    WorldGeneratedNpcObservation npc;
    npc.actorKey = "out01.odm#7#0#Citizen";
    npc.npcId = 1184;
    npc.name = "Remembered citizen";
    npc.pictureId = 1;
    npc.professionId = 1;
    npc.location = {"world.mm8.map.out01", 7, 12, 34, 56, 100};
    npc.teacherTopics = {123, 456};
    save.outdoorWorld.eventRuntimeState->observedGeneratedNpcs = {npc};
    REQUIRE_MESSAGE(saveGameDataToPath(fixture.path, save, error), error);
    std::ifstream atlasFile(fixture.path, std::ios::binary);
    const std::vector<char> atlas{std::istreambuf_iterator<char>(atlasFile), {}};
    REQUIRE(atlas.size() > vanilla.size());
    CHECK(std::equal(vanilla.begin(), vanilla.begin() + 12, atlas.begin()));
    const std::optional<GameSaveData> loaded = loadGameDataFromPath(fixture.path, error);
    REQUIRE_MESSAGE(loaded, error);
    CHECK(loaded->party.identifiedItemIds == save.party.identifiedItemIds);
    REQUIRE(loaded->outdoorWorld.eventRuntimeState);
    CHECK(loaded->outdoorWorld.eventRuntimeState->observedGeneratedNpcs == std::vector{npc});
    Party restored;
    restored.restoreSnapshot(loaded->party);
    CHECK(restored.hasIdentifiedItem(23));
    CHECK_FALSE(restored.hasIdentifiedItem(24));
}

TEST_CASE("Atlas port: a damaged extension is rejected without returning a partially decoded campaign")
{
    AtlasSaveFixture fixture;
    GameSaveData save;
    save.party.identifiedItemIds = {10};
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(fixture.path, save, error), error);
    const uintmax_t length = std::filesystem::file_size(fixture.path);
    std::filesystem::resize_file(fixture.path, length - 1);
    CHECK_FALSE(loadGameDataFromPath(fixture.path, error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("Atlas port: Map Book and Atlas share personal notes and preserve script pins")
{
    Party party;
    EventRuntimeState state;
    party.setJournalMapNote({5, 10, 20, "out01.odm", "A script pin", false, false});
    WorldMapNoteEdit edit;
    edit.mapFileName = "out01.odm";
    edit.x = 30;
    edit.y = 40;
    edit.text = "  My note  ";
    edit.elevation = 123;
    std::string error;
    REQUIRE(editWorldMapNote(party, state, "out01.odm", edit, error));
    REQUIRE(party.findJournalMapNote("out01.odm", edit.id));
    CHECK(party.findJournalMapNote("out01.odm", edit.id)->text == "My note");
    CHECK(party.findJournalMapNote("out01.odm", edit.id)->personal);
    REQUIRE(worldMapNotes(party, state, "out01.odm").size() == 2);
    CHECK(worldMapNotes(party, state, "out01.odm")[1].elevation == 123);
    edit.id = 5;
    edit.text = "Overwrite";
    CHECK_FALSE(editWorldMapNote(party, state, "out01.odm", edit, error));
    CHECK(party.findJournalMapNote("out01.odm", 5)->text == "A script pin");
    CHECK_FALSE(editWorldMapNote(party, state, "out02.odm", edit, error));
}

TEST_CASE("Atlas port: changed map layouts discard projections while retaining party notes and other maps")
{
    MapStatsEntry map;
    map.worldId = "mm7";
    map.fileName = "7out02.odm";
    const std::string canonical = WorldDatabase::mapId(map);
    WorldKnowledge knowledge;
    knowledge.mapLayoutIdentities[canonical] = 100;
    knowledge.regionMaps[map.fileName].fullyRevealedCells = {255};
    knowledge.regionMaps["other.odm"].fullyRevealedCells = {255};
    knowledge.notes[map.fileName] = {{{10, 20, 30, map.fileName, "Personal history", false, true}, true, 512}};
    CHECK_FALSE(updateWorldMapLayoutKnowledge(knowledge, map, 100));
    CHECK(knowledge.regionMaps.contains(map.fileName));
    CHECK(updateWorldMapLayoutKnowledge(knowledge, map, 200));
    CHECK_FALSE(knowledge.regionMaps.contains(map.fileName));
    CHECK(knowledge.regionMaps.contains("other.odm"));
    REQUIRE(knowledge.notes[map.fileName].size() == 1);
    CHECK(knowledge.notes[map.fileName][0].value.text == "Personal history");
    CHECK_FALSE(knowledge.notes[map.fileName][0].elevation);
    CHECK_FALSE(updateWorldMapLayoutKnowledge(knowledge, map, 200));
}
