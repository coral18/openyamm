#include "doctest/doctest.h"
#include "engine/AssetFileSystem.h"
#include "engine/ImageAssetLoader.h"
#include "engine/Localization.h"
#include "game/data/GameDataLoader.h"
#include "game/maps/MapAssetLoader.h"
#include "game/world/WorldAtlasPresentation.h"
#include "game/world/WorldDatabase.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>

using namespace OpenYAMM;

TEST_CASE("Atlas MM6 pilot: New Sorpigal uses installed geometry and merged data without discovery leaks"
    * doctest::skip(std::getenv("OPENYAMM_ATLAS_ASSET_ROOT") == nullptr))
{
    const char *pRoot = std::getenv("OPENYAMM_ATLAS_ASSET_ROOT");
    const char *pMods = std::getenv("OPENYAMM_ATLAS_MODS_ROOT");
    REQUIRE(pRoot != nullptr);
    REQUIRE(pMods != nullptr);
    Engine::InstalledContentOptions options;
    options.modsRoot = pMods;
    if (const char *pLanguage = std::getenv("OPENYAMM_ATLAS_LANGUAGE")) options.languageId = pLanguage;
    if (const char *pLanguages = std::getenv("OPENYAMM_ATLAS_LANGUAGES_ROOT"))
        options.userLanguagesRoot = pLanguages;
    Engine::AssetFileSystem assets;
    REQUIRE(assets.initialize(std::filesystem::current_path(), pRoot, Engine::AssetScaleTier::X1,
        Engine::createUniformAssetScaleProfile(Engine::AssetScaleTier::X1), "mm6", options));
    REQUIRE(Engine::activateLanguage(assets, options.languageId));
    Game::GameDataLoader loader;
    loader.setActiveWorldId("mm6");
    REQUIRE(loader.loadCommonForGameplay(assets));
    if (options.languageId == "pl")
    {
        CHECK(Engine::localize("New Sorpigal", "world:mm6") == "Nowe Sorpigal");
        CHECK(Engine::localize("Candelabra", "world:mm6") == "Kandelabr");
        CHECK(Engine::localize("House", "world:mm6") == "Dom");
    }
    const Game::WorldAtlasPresentation presentation = Game::loadWorldAtlasPresentation(assets, "mm6");
    CHECK(presentation.name == "Enroth");
    CHECK(presentation.overviewTexture.empty());
    Game::WorldDatabase database(loader.getMapStats(), loader.getHouseTable(), loader.getNpcDialogTable(),
        loader.getMergedTeacherTopicTable(), "mm6", presentation.maps);
    size_t playableMaps = 0, siteReferences = 0, rewards = 0;
    for (const Game::MapStatsEntry &entry : loader.getMapStats().getEntries())
    {
        if (entry.worldId != "mm6") continue;
        CAPTURE(entry.fileName);
        const Game::WorldRecord *pRecord = database.find(Game::WorldDatabase::mapId(entry));
        const bool pilot = entry.fileName == "oute3.odm" || entry.fileName == "6d01.blv"
            || entry.fileName == "6d02.blv";
        CHECK((pRecord != nullptr) == pilot);
        if (!pilot) continue;
        ++playableMaps;
        Game::MapAssetInfo map;
        map.map = entry;
        std::string error;
        REQUIRE_MESSAGE(Game::GameDataLoader::loadEventPrograms(assets, map, error, loader.activeRulesetRoot()), error);
        REQUIRE(map.localEventProgram.has_value());
        siteReferences += map.localEventProgram->siteReferences().size();
        rewards += map.localEventProgram->itemRewards().size();
        std::optional<Game::MapAssetInfo> geometry = Game::MapAssetLoader().load(assets, entry,
            loader.getMonsterTable(), loader.getObjectTable(), Game::MapLoadPurpose::HeadlessGameplay,
            Game::MapCompanionLoadOptions{.rulesetRoot = loader.activeRulesetRoot()});
        REQUIRE(geometry.has_value());
        if (geometry->outdoorMapData)
            database.indexMapLocations(*geometry->outdoorMapData, *map.localEventProgram);
        else
        {
            REQUIRE(geometry->indoorMapData.has_value());
            database.indexMapLocations(entry, *geometry->indoorMapData, *map.localEventProgram);
        }
        CHECK_FALSE(database.visible(*database.find(Game::WorldDatabase::mapId(entry)), {}, false));
    }
    CHECK(playableMaps == 3);
    CHECK(siteReferences > 0);
    CHECK(rewards > 0);
    const Game::WorldRecord *pRegion = database.find("world.mm6.map.oute3");
    REQUIRE(pRegion != nullptr);
    for (const char *pDungeon : {"world.mm6.map.6d01", "world.mm6.map.6d02"})
    {
        const Game::WorldRecord *pRecord = database.find(pDungeon);
        REQUIRE(pRecord != nullptr);
        CHECK(pRecord->parentId == pRegion->id);
    }
    CHECK(database.transportIssues().empty());
    Game::WorldKnowledge knowledge;
    knowledge.visitedMaps.insert("oute3.odm");
    CHECK(database.query(knowledge, false, {}, Game::WorldRecordKind::Region).size() == 1);
    CHECK(database.query(knowledge, false, {}, Game::WorldRecordKind::Dungeon).empty());
    CHECK(database.query(knowledge, false, {}, Game::WorldRecordKind::Trainer).empty());
    CHECK(database.queryLocations(knowledge, false, pRegion->id).empty());
    CHECK(database.queryFeatures(knowledge, false, {}, pRegion->id).empty());
    CHECK(database.queryLocations(knowledge, true, pRegion->id).size() > 20);
    CHECK(database.query(knowledge, true, {}, Game::WorldRecordKind::Trainer, pRegion->id).size() > 5);
    CHECK(database.query(knowledge, true, {}, Game::WorldRecordKind::Travel, pRegion->id).size() > 0);
    for (const Game::WorldRecord *pRecord : database.query(knowledge, true))
    {
        REQUIRE(pRecord->map != nullptr);
        CHECK(database.find(Game::WorldDatabase::mapId(*pRecord->map)) != nullptr);
    }
    size_t quests = 0;
    for (const Game::JournalQuestEntry &quest : loader.getJournalQuestTable().entries())
    {
        if (quest.qbitId != 1105 && quest.qbitId != 1107 && quest.qbitId != 1150 && quest.qbitId != 1163) continue;
        ++quests;
        CHECK_FALSE(quest.locations.empty());
    }
    CHECK(quests == 4);
    Engine::DirectoryAssetPathCache directories;
    Engine::AssetPathLookupCache paths;
    const std::optional<std::string> texture = Engine::findCookedOrImageAssetPath(assets, "worlds/mm6/icons",
        "oute3", directories, paths);
    REQUIRE(texture.has_value());
    CHECK(texture->starts_with("worlds/mm6/"));
    std::cout << "Atlas MM6 pilot audit: maps=" << playableMaps << " site references=" << siteReferences
        << " reward events=" << rewards << '\n';
    REQUIRE(Engine::activateLanguage(assets, "en"));
}

TEST_CASE("Atlas port runtime: all MM7 and MM8 programs load with the installed release and Atlas overlays"
    * doctest::skip(std::getenv("OPENYAMM_ATLAS_ASSET_ROOT") == nullptr))
{
    const char *pRoot = std::getenv("OPENYAMM_ATLAS_ASSET_ROOT");
    const char *pMods = std::getenv("OPENYAMM_ATLAS_MODS_ROOT");
    REQUIRE(pRoot != nullptr);
    REQUIRE(pMods != nullptr);
    for (const std::string world : {"mm7", "mm8"})
    {
        CAPTURE(world);
        Engine::InstalledContentOptions options;
        options.modsRoot = pMods;
        if (const char *pLanguage = std::getenv("OPENYAMM_ATLAS_LANGUAGE")) options.languageId = pLanguage;
        if (const char *pLanguages = std::getenv("OPENYAMM_ATLAS_LANGUAGES_ROOT"))
            options.userLanguagesRoot = pLanguages;
        Engine::AssetFileSystem assets;
        const Engine::AssetScaleProfile scale = Engine::createUniformAssetScaleProfile(Engine::AssetScaleTier::X1);
        REQUIRE(assets.initialize(std::filesystem::current_path(), pRoot, Engine::AssetScaleTier::X1,
            scale, world, options));
        REQUIRE(Engine::activateLanguage(assets, options.languageId));
        Game::GameDataLoader loader;
        loader.setActiveWorldId(world);
        REQUIRE(loader.loadCommonForGameplay(assets));
        const Game::WorldAtlasPresentation presentation = Game::loadWorldAtlasPresentation(assets, world);
        CHECK(presentation.worldId == world);
        CHECK_FALSE(presentation.maps.empty());
        Game::WorldDatabase database(loader.getMapStats(), loader.getHouseTable(), loader.getNpcDialogTable(),
            loader.getMergedTeacherTopicTable(), world, presentation.maps);
        size_t maps = 0, references = 0, rewards = 0;
        for (const Game::MapStatsEntry &entry : loader.getMapStats().getEntries())
        {
            if (entry.worldId != world) continue;
            CAPTURE(entry.fileName);
            if (std::filesystem::path(entry.fileName).extension() == ".odm")
            {
                Engine::DirectoryAssetPathCache directories;
                Engine::AssetPathLookupCache paths;
                const std::string directory = "worlds/" + world + "/icons";
                const std::optional<std::string> texture = Engine::findCookedOrImageAssetPath(assets, directory,
                    std::filesystem::path(entry.fileName).stem().string(), directories, paths);
                CHECK_MESSAGE(texture.has_value(), "Atlas region art must resolve through its owning icon package");
                if (texture) CHECK(texture->starts_with("worlds/" + world + "/"));
            }
            Game::MapAssetInfo map;
            map.map = entry;
            std::string error;
            const bool loaded = Game::GameDataLoader::loadEventPrograms(assets, map, error, loader.activeRulesetRoot());
            CHECK_MESSAGE(loaded, error);
            if (!loaded) continue;
            CHECK(map.localEventProgram.has_value());
            if (!map.localEventProgram) continue;
            const auto layout = std::find_if(presentation.maps.begin(), presentation.maps.end(),
                [&entry](const Game::WorldMapPresentation &value) { return value.fileName == entry.fileName; });
            const bool hidden = layout != presentation.maps.end() && layout->hidden;
            if (hidden)
            {
                CHECK(database.find(Game::WorldDatabase::mapId(entry)) == nullptr);
                CHECK_FALSE(Game::MapAssetLoader::findAssetPath(assets, world, entry.fileName).has_value());
            }
            else
            {
                std::optional<Game::MapAssetInfo> geometry = Game::MapAssetLoader().load(assets, entry,
                    loader.getMonsterTable(), loader.getObjectTable(), Game::MapLoadPurpose::HeadlessGameplay,
                    Game::MapCompanionLoadOptions{.rulesetRoot = loader.activeRulesetRoot()});
                REQUIRE_MESSAGE(geometry.has_value(), "Atlas map geometry must load from the active release");
                if (geometry->outdoorMapData)
                    CHECK_NOTHROW(database.indexMapLocations(*geometry->outdoorMapData, *map.localEventProgram));
                else if (geometry->indoorMapData)
                    CHECK_NOTHROW(database.indexMapLocations(entry, *geometry->indoorMapData, *map.localEventProgram));
                else
                    FAIL_CHECK("Atlas map geometry is empty");
            }
            references += map.localEventProgram->siteReferences().size();
            rewards += map.localEventProgram->itemRewards().size();
            ++maps;
        }
        CHECK(maps > 50);
        CHECK(references > 0);
        CHECK(rewards > 0);
        std::cout << "Atlas pack audit: " << world << " maps=" << maps << " site references=" << references
            << " reward events=" << rewards << '\n';
        REQUIRE(Engine::activateLanguage(assets, "en"));
    }
}
