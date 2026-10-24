#include "doctest/doctest.h"

#include "game/data/GameDataLoader.h"
#include "game/maps/MapAssetLoader.h"
#include "game/indoor/IndoorMapData.h"
#include "game/outdoor/OutdoorMapData.h"
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
                { return worldMapFeatureContainsSite(feature, site.id) && feature.site.eventId == site.eventId; });
            inventory << entry.worldId << '\t' << entry.fileName << '\t' << site.id << '\t' << site.eventId << '\t'
                << (metadata ? metadata->kind : events.hasEvent(site.eventId) ? "unclassified" : "unregistered")
                << '\t' << (metadata && metadata->hidden) << '\t' << title << '\t'
                << site.x << '\t' << site.y << '\t' << site.z << '\t' << indexed << '\t'
                << (events.siteReferences().contains(site.eventId)
                    || events.interactionReferences().contains(site.eventId))
                << '\t' << events.itemRewards().contains(site.eventId)
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

TEST_CASE("canonical interaction references describe active statue rescues and reusable keyed entrances")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &loader = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = loader.gameDataLoader;
    const std::pair<std::string, std::string> maps[] = {
        {"mm7", "7d34.blv"}, {"mm8", "out07.odm"}, {"mm8", "out02.odm"}, {"mm8", "pbp.odm"}};
    size_t references = 0;
    for (const auto &[worldId, file] : maps)
    {
        CAPTURE(file);
        const auto &entries = data.getMapStats().getEntries();
        const auto entry = std::find_if(entries.begin(), entries.end(), [&](const MapStatsEntry &map)
            { return map.worldId == worldId && map.fileName == file; });
        REQUIRE(entry != entries.end());
        auto map = MapAssetLoader().load(loader.assetFileSystem, *entry, data.getMonsterTable(),
            data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
        REQUIRE(map);
        std::string error;
        REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(loader.assetFileSystem, *map, error), error);
        REQUIRE(map->localEventProgram);
        const ScriptedEventProgram &events = *map->localEventProgram;
        WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
            data.getMergedTeacherTopicTable(), worldId);
        if (map->outdoorMapData) world.indexMapLocations(*map->outdoorMapData, events);
        else world.indexMapLocations(*entry, *map->indoorMapData, events);
        world.indexMapInteractionReferences(*entry, data.getItemTable(), data.getSpellTable());
        for (const auto &[event, reference] : events.interactionReferences())
        {
            CAPTURE(event);
            if (reference.kind == "dimension_door") continue;
            ++references;
            REQUIRE(events.getContextActionMetadata(event));
            CHECK(events.getContextActionMetadata(event)->kind == reference.kind);
            size_t sites = 0;
            for (const WorldMapFeature &feature : world.features())
            {
                if (feature.site.eventId != event) continue;
                ++sites;
                WorldKnowledge knowledge;
                knowledge.visitedMaps.insert(file);
                CHECK_FALSE(world.featureVisible(feature, knowledge, false));
                CHECK(world.featureVisible(feature, knowledge, true));
                CHECK(world.siteReference(feature, knowledge, false).empty());
                const std::string guide(world.siteReference(feature, knowledge, true));
                REQUIRE_FALSE(guide.empty());
                CHECK(guide.find(data.getItemTable().get(reference.itemId)->name) != std::string::npos);
                if (reference.kind == "enter_dungeon")
                {
                    CHECK(feature.kind == WorldMapFeatureKind::Connection);
                    CHECK(feature.targetMap == reference.targetMap);
                    CHECK(guide.find("reused without the key") != std::string::npos);
                    CHECK(std::any_of(world.locations().begin(), world.locations().end(),
                        [&](const WorldMapLocation &location) { return location.eventId == event; }));
                }
                else
                {
                    CHECK(feature.kind == WorldMapFeatureKind::Decoration);
                    REQUIRE(feature.site.id.starts_with("decoration/"));
                    const size_t index = std::stoul(feature.site.id.substr(11));
                    if (map->outdoorMapData)
                    {
                        REQUIRE(index < map->outdoorMapData->entities.size());
                        CHECK(map->outdoorMapData->entities[index].eventIdPrimary == reference.spriteId);
                    }
                    else
                    {
                        REQUIRE(index < map->indoorMapData->entities.size());
                        CHECK(map->indoorMapData->entities[index].eventIdPrimary == reference.spriteId);
                    }
                    CHECK(guide.find(data.getNpcDialogTable().getNpc(reference.npcId)->name) != std::string::npos);
                    if (reference.spellId != 0)
                        CHECK(guide.find(data.getSpellTable().findById(int(reference.spellId))->name)
                            != std::string::npos);
                }
                knowledge.interactedSites[file].insert(worldSiteDiscoveryKey(event, feature.site.id));
                CHECK(world.featureVisible(feature, knowledge, false));
                CHECK(world.siteReference(feature, knowledge, false).empty());
            }
            CHECK(sites > 0);
        }
    }
    CHECK(references == 17);
}

TEST_CASE("canonical elemental prison passages group their physical components and preserve independent knowledge")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &loader = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = loader.gameDataLoader;
    const auto &entries = data.getMapStats().getEntries();
    const auto entry = std::find_if(entries.begin(), entries.end(), [](const MapStatsEntry &map)
        { return map.worldId == "mm8" && map.fileName == "pbp.odm"; });
    REQUIRE(entry != entries.end());
    auto map = MapAssetLoader().load(loader.assetFileSystem, *entry, data.getMonsterTable(),
        data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
    REQUIRE(map);
    REQUIRE(map->outdoorMapData);
    std::string error;
    REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(loader.assetFileSystem, *map, error), error);
    REQUIRE(map->localEventProgram);
    WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
        data.getMergedTeacherTopicTable(), "mm8");
    world.indexMapLocations(*map->outdoorMapData, *map->localEventProgram);
    world.indexMapInteractionReferences(*entry, data.getItemTable(), data.getSpellTable());
    const auto &groups = map->localEventProgram->siteGroups();
    REQUIRE(groups.size() == 4);
    size_t components = 0;
    for (const auto &[event, declared] : groups)
    {
        CAPTURE(event);
        REQUIRE(declared.size() == 1);
        const auto features = world.queryFeatures({}, true, "", WorldDatabase::mapId(*entry));
        const auto feature = std::find_if(features.begin(), features.end(), [&](const WorldMapFeature *pFeature)
            { return pFeature->site.eventId == event; });
        REQUIRE(feature != features.end());
        CHECK(std::count_if(features.begin(), features.end(), [&](const WorldMapFeature *pFeature)
            { return pFeature->site.eventId == event; }) == 1);
        CHECK(std::count_if(world.locations().begin(), world.locations().end(), [&](const WorldMapLocation &location)
            { return location.eventId == event; }) == 1);
        CHECK((*feature)->components.size() == declared.front().size());
        components += (*feature)->components.size();
        for (const WorldInteractionSite &site : (*feature)->components)
        {
            CAPTURE(site.id);
            WorldKnowledge knowledge;
            knowledge.visitedMaps.insert(entry->fileName);
            CHECK(world.queryFeatures(knowledge, false).empty());
            knowledge.interactedSites[entry->fileName].insert(worldSiteDiscoveryKey(event, site.id));
            const auto known = world.queryFeatures(knowledge, false);
            REQUIRE(known.size() == 1);
            CHECK(known.front()->id == (*feature)->id);
            CHECK(world.siteReference(**feature, knowledge, false).empty());
            CHECK(world.locationVisible(*std::find_if(world.locations().begin(), world.locations().end(),
                [&](const WorldMapLocation &location) { return location.eventId == event; }), knowledge, false));
            CHECK(knowledge.interactedSites.at(entry->fileName).size() == 1);
        }
    }
    CHECK(components == 21);
}

TEST_CASE("canonical Harmondale dimensional fountain uses all six packaged water bindings in the Atlas")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &loader = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = loader.gameDataLoader;
    const auto &entries = data.getMapStats().getEntries();
    const auto entry = std::find_if(entries.begin(), entries.end(), [](const MapStatsEntry &map)
        { return map.worldId == "mm7" && map.fileName == "7d29.blv"; });
    REQUIRE(entry != entries.end());
    auto map = MapAssetLoader().load(loader.assetFileSystem, *entry, data.getMonsterTable(),
        data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
    REQUIRE(map);
    REQUIRE(map->indoorMapData);
    std::string error;
    REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(loader.assetFileSystem, *map, error), error);
    REQUIRE(map->localEventProgram);
    WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
        data.getMergedTeacherTopicTable(), "mm7");
    world.indexMapLocations(*entry, *map->indoorMapData, *map->localEventProgram);
    const auto feature = std::find_if(world.features().begin(), world.features().end(),
        [](const WorldMapFeature &value) { return value.site.eventId == 65031; });
    REQUIRE(feature != world.features().end());
    CHECK(feature->kind == WorldMapFeatureKind::Connection);
    CHECK(std::count_if(world.features().begin(), world.features().end(),
        [](const WorldMapFeature &value) { return value.site.eventId == 65031; }) == 1);
    CHECK(feature->site.faceIds.size() == 6);
    for (uint32_t face : {1668, 1669, 1689, 1697, 4669, 4670})
    {
        CAPTURE(face);
        CHECK(worldMapFeatureContainsSite(*feature, "surface/" + std::to_string(face)));
        WorldKnowledge knowledge;
        knowledge.visitedMaps.insert(entry->fileName);
        CHECK_FALSE(world.featureVisible(*feature, knowledge, false));
        CHECK(world.featureVisible(*feature, knowledge, true));
        knowledge.interactedSites[entry->fileName].insert(worldSiteDiscoveryKey(65031, "surface/"
            + std::to_string(face)));
        CHECK(world.featureVisible(*feature, knowledge, false));
        CHECK(knowledge.interactedSites.at(entry->fileName).size() == 1);
        CHECK(world.connectionArrivals(*feature, knowledge).empty());
    }
    WorldKnowledge unrelated;
    unrelated.interactedSites[entry->fileName].insert(worldSiteDiscoveryKey(65031, "surface/4229"));
    CHECK_FALSE(world.featureVisible(*feature, unrelated, false));
}


TEST_CASE("connection target inventory resolves every active MM6 MM7 MM8 declaration inside its owning world")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(), OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &fixture = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = fixture.gameDataLoader;
    const auto &entries = data.getMapStats().getEntries();
    size_t maps = 0, connections = 0, multiple = 0;
    for (const MapStatsEntry &entry : entries)
    {
        if (entry.worldId != "mm6" && entry.worldId != "mm7" && entry.worldId != "mm8") continue;
        CAPTURE(entry.worldId);
        CAPTURE(entry.fileName);
        MapAssetInfo map;
        map.map = entry;
        std::string error;
        REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fixture.assetFileSystem, map, error), error);
        REQUIRE(map.localEventProgram);
        ++maps;
        for (const auto &[event, targets] : map.localEventProgram->connectionTargets())
        {
            CAPTURE(event);
            ++connections;
            if (targets.size() > 1) ++multiple;
            CHECK(map.localEventProgram->hasEvent(event));
            for (const std::string &target : targets)
            {
                CAPTURE(target);
                CHECK(std::any_of(entries.begin(), entries.end(), [&](const MapStatsEntry &destination)
                    { return destination.worldId == entry.worldId && destination.fileName == target; }));
            }
        }
        if (entry.worldId == "mm7" && entry.fileName == "mdk01.blv")
            CHECK(map.localEventProgram->connectionTargets().at(502).size() == 2);
        if (entry.worldId == "mm8" && entry.fileName == "d34.blv")
            CHECK(map.localEventProgram->connectionTargets().at(451) == std::vector<std::string>{"d06.blv"});
        if (entry.worldId == "mm8" && entry.fileName == "d06.blv")
            CHECK(map.localEventProgram->connectionTargets().at(451) == std::vector<std::string>{"d34.blv"});
    }
    CHECK(maps > 150);
    CHECK(connections > maps);
    CHECK(multiple > 10);
}
