#include "doctest/doctest.h"

#include "game/data/GameDataLoader.h"
#include "game/maps/MapAssetLoader.h"
#include "game/world/WorldDatabase.h"
#include "tests/RegressionMapLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

TEST_CASE("Atlas campaign interaction inventory reads canonical map programs and physical sites")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &loader = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = loader.gameDataLoader;
    std::ofstream inventory, maps;
    if (const char *pDirectory = std::getenv("OPENYAMM_ATLAS_INTERACTION_AUDIT"))
    {
        inventory.open(std::filesystem::path(pDirectory) / "interactions.tsv");
        maps.open(std::filesystem::path(pDirectory) / "maps.tsv");
        REQUIRE(inventory.is_open());
        REQUIRE(maps.is_open());
        inventory << "world\tmap\tsite\tevent\tkind\thidden\ttitle\tx\ty\tz\tatlas\treference\titems\tnotes\n";
        maps << "world\tmap\tsites\n";
    }
    for (const MapStatsEntry &entry : data.getMapStats().getEntries())
    {
        if (entry.worldId != "mm7" && entry.worldId != "mm8") continue;
        CAPTURE(entry.worldId);
        CAPTURE(entry.fileName);
        auto map = MapAssetLoader().load(loader.assetFileSystem, entry, data.getMonsterTable(),
            data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
        REQUIRE(map);
        std::string error;
        REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(loader.assetFileSystem, *map, error), error);
        REQUIRE(map->localEventProgram);
        const ScriptedEventProgram &events = *map->localEventProgram;
        const auto sites = map->outdoorMapData ? worldInteractionSites(*map->outdoorMapData)
            : worldInteractionSites(*map->indoorMapData);
        const auto features = buildWorldMapFeatures(WorldDatabase::mapId(entry), sites, events);
        if (maps.is_open()) maps << entry.worldId << '\t' << entry.fileName << '\t' << sites.size() << '\n';
        std::set<std::pair<std::string, uint16_t>> identities;
        for (const WorldInteractionSite &site : sites)
        {
            CAPTURE(site.id);
            CAPTURE(site.eventId);
            CHECK_FALSE(site.id.empty());
            CHECK(site.eventId != 0);
            CHECK(identities.emplace(site.id, site.eventId).second);
            CHECK((std::isfinite(site.x) && std::isfinite(site.y) && std::isfinite(site.z)));
            if (!inventory.is_open()) continue;
            const auto metadata = events.getContextActionMetadata(site.eventId);
            std::string title = events.getHint(site.eventId).value_or("");
            std::replace(title.begin(), title.end(), '\t', ' ');
            std::replace(title.begin(), title.end(), '\n', ' ');
            std::replace(title.begin(), title.end(), '\r', ' ');
            const bool indexed = std::any_of(features.begin(), features.end(), [&](const WorldMapFeature &feature)
                { return feature.site.id == site.id && feature.site.eventId == site.eventId; });
            inventory << entry.worldId << '\t' << entry.fileName << '\t' << site.id << '\t' << site.eventId << '\t'
                << (metadata ? metadata->kind : events.hasEvent(site.eventId) ? "unclassified" : "unregistered")
                << '\t' << (metadata && metadata->hidden) << '\t' << title << '\t'
                << site.x << '\t' << site.y << '\t' << site.z << '\t' << indexed << '\t'
                << events.siteReferences().contains(site.eventId) << '\t' << events.itemRewards().contains(site.eventId)
                << '\t' << events.autonoteReferences().contains(site.eventId) << '\n';
        }
    }
}

TEST_CASE("canonical drink sites expose active effects only in Guide and retain physical discovery identity")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &loader = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = loader.gameDataLoader;
    struct Drink
    {
        const char *pWorld;
        const char *pMap;
        uint16_t event;
        const char *pReference;
    };
    const Drink drinks[] = {
        {"mm7", "7out06.odm", 460, "Random outcomes"},
        {"mm7", "7out13.odm", 214, "Random outcomes"},
        {"mm7", "d02.blv", 451, "+25"},
        {"mm7", "7d12.blv", 451, "Can apply: Paralysis"},
        {"mm7", "7d24.blv", 196, "No attribute or condition change"},
        {"mm7", "7d24.blv", 197, "No attribute or condition change"},
        {"mm7", "7d24.blv", 198, "No attribute or condition change"},
        {"mm7", "7d25.blv", 452, "Not yet used by this character"},
        {"mm8", "d20.blv", 451, "No attribute or condition change"},
        {"mm8", "d35.blv", 451, "No attribute or condition change"}};
    for (const Drink &drink : drinks)
    {
        CAPTURE(std::string(drink.pWorld));
        CAPTURE(std::string(drink.pMap));
        CAPTURE(drink.event);
        const auto &entries = data.getMapStats().getEntries();
        const auto entry = std::find_if(entries.begin(), entries.end(), [&](const MapStatsEntry &map)
            { return map.worldId == drink.pWorld && map.fileName == drink.pMap; });
        REQUIRE(entry != entries.end());
        auto map = MapAssetLoader().load(loader.assetFileSystem, *entry, data.getMonsterTable(),
            data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
        REQUIRE(map);
        std::string error;
        REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(loader.assetFileSystem, *map, error), error);
        REQUIRE(map->localEventProgram);
        const ScriptedEventProgram &events = *map->localEventProgram;
        const auto action = events.getContextActionMetadata(drink.event);
        REQUIRE(action);
        CHECK(action->kind == "drink");
        REQUIRE(events.siteReferences().contains(drink.event));
        CHECK(events.siteReferences().at(drink.event).find(drink.pReference) != std::string::npos);
        WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
            data.getMergedTeacherTopicTable(), drink.pWorld);
        if (map->outdoorMapData) world.indexMapLocations(*map->outdoorMapData, events);
        else world.indexMapLocations(*entry, *map->indoorMapData, events);
        const auto &features = world.features();
        const auto feature = std::find_if(features.begin(), features.end(), [&](const WorldMapFeature &item)
            { return item.site.eventId == drink.event; });
        REQUIRE(feature != features.end());
        CHECK(feature->kind == WorldMapFeatureKind::Water);
        WorldKnowledge knowledge;
        knowledge.visitedMaps.insert(drink.pMap);
        CHECK_FALSE(world.featureVisible(*feature, knowledge, false));
        CHECK(world.featureVisible(*feature, knowledge, true));
        CHECK(world.siteReference(*feature, knowledge, false).empty());
        CHECK(world.siteReference(*feature, knowledge, true) == events.siteReferences().at(drink.event));
        knowledge.interactedSites[drink.pMap].insert(worldSiteDiscoveryKey(drink.event, feature->site.id));
        CHECK(world.featureVisible(*feature, knowledge, false));
        CHECK(world.siteReference(*feature, knowledge, false).empty());
    }
}
