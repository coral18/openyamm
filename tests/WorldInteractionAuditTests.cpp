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
