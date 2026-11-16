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
