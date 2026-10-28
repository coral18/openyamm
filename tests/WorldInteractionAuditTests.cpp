#include "doctest/doctest.h"

#include "game/data/GameDataLoader.h"
#include "game/maps/MapAssetLoader.h"
#include "game/indoor/IndoorMapData.h"
#include "game/outdoor/OutdoorMapData.h"
#include "game/world/WorldDatabase.h"
#include "game/world/WorldAtlasPresentation.h"
#include "game/world/WorldGuide.h"
#include "game/party/Party.h"
#include "game/events/EventRuntime.h"
#include "tests/RegressionMapLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>

TEST_CASE("Atlas quest links resolve against fully indexed active MM7 and MM8 worlds")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &fixture = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = fixture.gameDataLoader;
    std::ofstream inventory;
    if (const char *pDirectory = std::getenv("OPENYAMM_ATLAS_INTERACTION_AUDIT"))
    {
        inventory.open(std::filesystem::path(pDirectory) / "quest-links.tsv");
        REQUIRE(inventory.is_open());
        inventory << "world\tquest\tkind\ttarget\tlabel\n";
    }
    size_t maps = 0, quests = 0, links = 0;
    for (const std::string &worldId : {"mm7", "mm8"})
    {
        const WorldAtlasPresentation presentation = loadWorldAtlasPresentation(fixture.assetFileSystem, worldId);
        WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
            data.getMergedTeacherTopicTable(), worldId, presentation.maps);
        for (const MapStatsEntry &entry : data.getMapStats().getEntries())
        {
            if (entry.worldId != worldId) continue;
            CAPTURE(entry.fileName);
            auto map = MapAssetLoader().load(fixture.assetFileSystem, entry, data.getMonsterTable(),
                data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
            REQUIRE(map);
            std::string error;
            REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fixture.assetFileSystem, *map, error), error);
            REQUIRE(map->localEventProgram);
            REQUIRE(map->globalEventProgram);
            const ScriptedEventProgram &events = *map->localEventProgram;
            world.indexMapConnections(entry, events);
            world.indexMapNpcTopics(entry, events);
            world.indexNpcLocations(*map->globalEventProgram);
            world.indexNpcLocations(events);
            if (map->outdoorMapData) world.indexMapLocations(*map->outdoorMapData, events);
            else
            {
                REQUIRE(map->indoorMapData);
                world.indexMapLocations(entry, *map->indoorMapData, events);
            }
            if (worldId == "mm7" && entry.fileName == "7d24.blv")
            {
                const auto action = events.getContextActionMetadata(416);
                REQUIRE(action);
                CHECK(action->kind == "enter_house");
                CHECK(action->houseId == 216);
                const auto sites = world.queryLocations({}, true, WorldDatabase::mapId(entry));
                CHECK(std::any_of(sites.begin(), sites.end(), [](const WorldMapLocation &location)
                    { return location.recordId == "mm7:house/216"; }));
                const WorldRecord *pThrone = world.find("mm7:house/216");
                REQUIRE(pThrone != nullptr);
                CHECK_FALSE(world.visible(*pThrone, {}, false));
                for (bool unlocked : {false, true})
                {
                    Party party;
                    party.setQuestBit(647, unlocked);
                    EventRuntimeState state;
                    REQUIRE(EventRuntime().executeEventById(map->localEventProgram, map->globalEventProgram,
                        416, state, &party));
                    CHECK(state.pendingDialogueContext.has_value() == unlocked);
                    if (unlocked) CHECK(state.pendingDialogueContext->sourceId == 216);
                    else
                    {
                        REQUIRE_FALSE(state.statusMessages.empty());
                        CHECK(state.statusMessages.back() == "The Door is Locked");
                    }
                    CHECK(party.hasQuestBit(647) == unlocked);
                }
            }
            ++maps;
        }
        WorldKnowledge locations;
        WorldGuideKnowledge knowledge;
        WorldGuideSource source;
        source.worldId = worldId;
        source.pQuests = &data.getJournalQuestTable();
        source.pMaps = &data.getMapStats();
        source.pNpcs = &data.getNpcDialogTable();
        source.pHouses = &data.getHouseTable();
        source.pItems = &data.getItemTable();
        source.pWorld = &world;
        source.pWorldKnowledge = &locations;
        CHECK(buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false).empty());
        const auto entries = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, true);
        CHECK(entries.size() == (worldId == "mm7" ? 76 : 69));
        if (worldId == "mm7")
        {
            const HouseEntry *pCouncil = data.getHouseTable().get(221);
            REQUIRE(pCouncil != nullptr);
            const MapStatsEntry *pCastle = data.getMapStats().findById(pCouncil->mapId);
            REQUIRE(pCastle != nullptr);
            CHECK(pCastle->fileName == "d03.blv");
            const WorldRecord *pCouncilContact = world.find("mm7:npc/423/possible-house/221");
            REQUIRE(pCouncilContact != nullptr);
            CHECK(world.recordMap(*pCouncilContact, locations, true) == pCastle);
        }
        for (const WorldGuideEntry &entry : entries)
        {
            CAPTURE(entry.id);
            CHECK_FALSE(entry.links.empty());
            for (const WorldGuideLink &link : entry.links)
            {
                CAPTURE(link.label);
                CAPTURE(link.target);
                REQUIRE_FALSE(link.target.empty());
                if (link.kind == WorldGuideLink::Kind::Item)
                    CHECK(data.getItemTable().get(uint32_t(std::stoul(link.target))) != nullptr);
                else
                {
                    CHECK(link.kind == WorldGuideLink::Kind::Location);
                    const WorldRecord *pTarget = world.find(link.target);
                    REQUIRE(pTarget != nullptr);
                    CHECK(world.visible(*pTarget, locations, true));
                    CHECK(world.recordMap(*pTarget, locations, true) != nullptr);
                }
                if (inventory.is_open()) inventory << worldId << '\t' << entry.id << '\t'
                    << (link.kind == WorldGuideLink::Kind::Item ? "item" : "location") << '\t'
                    << link.target << '\t' << link.label << '\n';
                ++links;
            }
            ++quests;
        }
        CHECK(knowledge.quests.empty());
        CHECK(knowledge.questLocations.empty());
        CHECK(locations.variables.empty());
        CHECK(locations.visitedMaps.empty());
    }
    CHECK(maps == 137);
    CHECK(quests == 145);
    CHECK(links > quests);
}

TEST_CASE("active conditional house entrances preserve council branches and flooded venue discovery")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &fixture = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = fixture.gameDataLoader;
    struct Entrance
    {
        const char *pWorld;
        const char *pMap;
        uint16_t event;
        uint32_t bit;
        std::vector<uint32_t> houses;
    };
    std::vector<Entrance> entrances = {
        {"mm7", "d03.blv", 5, 710, {219, 221}},
        {"mm7", "7out02.odm", 37, 611, {1165, 1164, 1166}}};
    const std::pair<uint16_t, uint32_t> venues[] = {
        {205, 625}, {213, 629}, {215, 630}, {223, 634}, {225, 635}, {452, 638}, {454, 639},
        {456, 640}, {458, 641}, {464, 6}, {466, 41}, {468, 81}, {470, 115}, {472, 308},
        {474, 1569}, {476, 285}, {478, 188}};
    for (const auto &[event, house] : venues)
        entrances.push_back({"mm8", "d24.blv", event, 23, {event < 464 ? 626u : 627u, house}});
    std::map<std::string, std::optional<MapAssetInfo>> loadedMaps;
    for (const Entrance &entrance : entrances)
    {
        CAPTURE(std::string(entrance.pMap));
        CAPTURE(entrance.event);
        const MapStatsEntry *pEntry = data.getMapStats().findByFileName(entrance.pMap);
        REQUIRE(pEntry != nullptr);
        const auto [cached, inserted] = loadedMaps.try_emplace(entrance.pMap);
        auto &map = cached->second;
        if (inserted) map = MapAssetLoader().load(fixture.assetFileSystem, *pEntry, data.getMonsterTable(),
            data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
        REQUIRE(map);
        std::string error;
        if (inserted)
            REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fixture.assetFileSystem, *map, error), error);
        REQUIRE(map->localEventProgram);
        WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
            data.getMergedTeacherTopicTable(), entrance.pWorld);
        if (map->outdoorMapData) world.indexMapLocations(*map->outdoorMapData, *map->localEventProgram);
        else world.indexMapLocations(*pEntry, *map->indoorMapData, *map->localEventProgram);
        const std::string mapId = WorldDatabase::mapId(*pEntry);
        WorldKnowledge known;
        known.visitedMaps.insert(entrance.pMap);
        const uint32_t venue = entrance.houses.back();
        const std::string venueId = std::string(entrance.pWorld) + ":house/" + std::to_string(venue);
        const auto guide = world.queryLocations(known, true, mapId);
        REQUIRE(std::any_of(guide.begin(), guide.end(), [&](const WorldMapLocation &location)
            { return location.eventId == entrance.event && location.recordId == venueId; }));
        const auto seen = [&](const std::string &id)
        {
            const auto locations = world.queryLocations(known, false, mapId);
            return std::any_of(locations.begin(), locations.end(), [&](const WorldMapLocation &location)
                { return location.eventId == entrance.event && location.recordId == id; });
        };
        CHECK_FALSE(seen(venueId));
        for (size_t branch = 0; branch < entrance.houses.size(); ++branch)
        {
            Party party;
            const bool gate = entrance.event == 37 ? branch == 1 : branch != 0;
            party.setQuestBit(entrance.bit, gate);
            if (entrance.bit == 710 || (entrance.event == 37 && branch == 2)) party.setQuestBit(612, true);
            EventRuntimeState state;
            REQUIRE(EventRuntime().executeEventById(map->localEventProgram, map->globalEventProgram,
                entrance.event, state, &party));
            REQUIRE(state.pendingDialogueContext);
            CHECK(state.pendingDialogueContext->sourceId == entrance.houses[branch]);
            CHECK(party.hasQuestBit(entrance.bit) == gate);
        }
        known.variables[WorldDatabase::houseDiscoveryKey(entrance.houses.front())] = 1;
        CHECK_FALSE(seen(venueId)); // A prior council/flood dialogue does not discover its replacement venue.
        known.variables[WorldDatabase::houseDiscoveryKey(venue)] = 1;
        CHECK(seen(venueId));
        if (entrance.event == 37)
        {
            CHECK(std::any_of(guide.begin(), guide.end(), [](const WorldMapLocation &location)
                { return location.eventId == 37 && location.recordId == "mm7:house/1164"; }));
            CHECK_FALSE(seen("mm7:house/1164"));
        }
    }
}

TEST_CASE("Atlas dungeon parents agree with active physical outdoor entrances")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &fixture = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = fixture.gameDataLoader;
    size_t auditedMaps = 0, singleParents = 0, multipleParents = 0, checkedExplicitParents = 0;
    for (const std::string &worldId : {"mm7", "mm8"})
    {
        const WorldAtlasPresentation presentation = loadWorldAtlasPresentation(fixture.assetFileSystem, worldId);
        WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
            data.getMergedTeacherTopicTable(), worldId, presentation.maps);
        std::map<std::string, std::set<std::string>> entrances;
        for (const MapStatsEntry &entry : data.getMapStats().getEntries())
        {
            if (entry.worldId != worldId || !entry.fileName.ends_with(".odm")) continue;
            CAPTURE(entry.fileName);
            auto map = MapAssetLoader().load(fixture.assetFileSystem, entry, data.getMonsterTable(),
                data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
            REQUIRE(map);
            REQUIRE(map->outdoorMapData);
            std::string error;
            REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fixture.assetFileSystem, *map, error), error);
            REQUIRE(map->localEventProgram);
            world.indexMapLocations(*map->outdoorMapData, *map->localEventProgram);
            ++auditedMaps;
            for (const WorldMapLocation &location : world.queryLocations({}, true, WorldDatabase::mapId(entry)))
            {
                const WorldRecord *pTarget = world.find(location.recordId);
                REQUIRE(pTarget != nullptr);
                // Celeste, the Pit and the Plane of Earth have their own explicit overview anchors.
                if (pTarget->kind == WorldRecordKind::Dungeon && !pTarget->position)
                    entrances[pTarget->id].insert(location.mapId);
            }
        }
        for (const auto &[id, sources] : entrances)
        {
            const WorldRecord *pTarget = world.find(id);
            REQUIRE(pTarget != nullptr);
            CAPTURE(pTarget->map->fileName);
            CAPTURE(pTarget->parentId);
            if (sources.size() == 1)
            {
                ++singleParents;
                CAPTURE(*sources.begin());
                CHECK(pTarget->parentId == *sources.begin());
            }
            else
            {
                ++multipleParents;
                if (!pTarget->parentId.empty()) CHECK(sources.contains(pTarget->parentId));
            }
        }
        for (const MapStatsEntry &entry : data.getMapStats().getEntries())
        {
            if (entry.worldId != worldId) continue;
            MapAssetInfo metadata;
            metadata.map = entry;
            std::string error;
            REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fixture.assetFileSystem, metadata, error), error);
            REQUIRE(metadata.localEventProgram);
            world.indexMapConnections(entry, *metadata.localEventProgram);
        }
        WorldKnowledge unknown;
        for (const WorldMapPresentation &entry : presentation.maps)
        {
            if (entry.parentFileName.empty()) continue;
            CAPTURE(entry.fileName);
            const MapStatsEntry *pEntry = data.getMapStats().findByFileName(entry.fileName);
            REQUIRE(pEntry != nullptr);
            const WorldRecord *pChild = world.find(WorldDatabase::mapId(*pEntry));
            REQUIRE(pChild != nullptr);
            const auto graph = buildWorldConnectionGraph(world, unknown, pChild->id, true);
            CHECK(std::any_of(graph.nodes.begin(), graph.nodes.end(), [&](const MapStatsEntry *pNode)
                { return WorldDatabase::mapId(*pNode) == pChild->parentId; }));
            CHECK_FALSE(world.visible(*pChild, unknown, false));
            ++checkedExplicitParents;
        }
        if (worldId == "mm7")
        {
            for (const WorldRecord &record : world.records())
            {
                if (record.kind != WorldRecordKind::Dungeon || record.position) continue;
                const std::string &file = record.map->fileName;
                CAPTURE(file);
                // Arena/portable Temple travel is not geographic; the tunnel/house bridge distinct roots.
                if (file == "7d05.blv" || file == "7nwc.blv" || file == "7d35.blv" || file == "mdt15.blv")
                    CHECK(record.parentId.empty());
                else CHECK_FALSE(record.parentId.empty());
            }
            WorldKnowledge regionKnown;
            regionKnown.visitedMaps.insert("out11.odm");
            const std::string parent = WorldDatabase::mapId(*data.getMapStats().findByFileName("out11.odm"));
            CHECK(world.query(regionKnown, false, "", WorldRecordKind::Dungeon, parent).empty());
            CHECK(world.query(regionKnown, true, "Barrow", WorldRecordKind::Dungeon, parent).size() == 15);
            CHECK(regionKnown.floorPlans.empty());
            CHECK(regionKnown.variables.empty());
        }
    }
    CHECK(auditedMaps == 27);
    CHECK(singleParents > 40);
    MESSAGE("Active outdoor entrance hierarchy: maps=", auditedMaps, ", one outdoor entry region=", singleParents,
        ", multiple outdoor entry regions=", multipleParents, ", explicit parents=", checkedExplicitParents);
}

TEST_CASE("active scripted monster spawn catalog resolves current MM7 and MM8 encounters without discoveries")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &fixture = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = fixture.gameDataLoader;
    std::map<std::string, size_t> counts;
    for (const MapStatsEntry &entry : data.getMapStats().getEntries())
    {
        if (entry.worldId != "mm7" && entry.worldId != "mm8") continue;
        CAPTURE(entry.fileName);
        MapAssetInfo map;
        map.map = entry;
        std::string error;
        REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fixture.assetFileSystem, map, error), error);
        REQUIRE(map.localEventProgram);
        const ScriptedEventProgram &events = *map.localEventProgram;
        WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
            data.getMergedTeacherTopicTable(), entry.worldId);
        world.indexMapMonsterSpawns(entry, events, data.getMonsterTable());
        size_t declarations = 0;
        for (const auto &[eventId, spawns] : events.monsterSpawns())
        {
            CHECK(events.hasEvent(eventId));
            declarations += spawns.size();
        }
        WorldKnowledge knowledge;
        knowledge.visitedMaps.insert(entry.fileName);
        CHECK(world.queryFeatures(knowledge, true).size() == declarations);
        CHECK(world.queryFeatures(knowledge, false).empty());
        CHECK(knowledge.variables.empty());
        CHECK(knowledge.monsterObservations.empty());
        for (const WorldMapFeature &feature : world.features())
        {
            REQUIRE(feature.monster);
            CHECK(feature.monster->scripted);
            CHECK_FALSE(feature.monster->observed);
            REQUIRE(feature.monster->definitions.size() == 1);
            CHECK(data.getMonsterTable().findStatsById(feature.monster->definitions.front()) != nullptr);
            CHECK(feature.site.eventId > 0);
            CHECK(feature.mapId == WorldDatabase::mapId(entry));
        }
        counts[entry.worldId] += declarations;
    }
    CHECK(counts["mm7"] > 0);
    CHECK(counts["mm8"] > 0);
    MESSAGE("Active spawn references: MM7=", counts["mm7"], ", MM8=", counts["mm8"]);
}

TEST_CASE("canonical companion contacts link Fredrick dialogue and Cauri rescue without discovering either")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &fixture = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = fixture.gameDataLoader;
    const MapStatsEntry *pMap = data.getMapStats().findByFileName("out07.odm");
    REQUIRE(pMap);
    auto map = MapAssetLoader().load(fixture.assetFileSystem, *pMap, data.getMonsterTable(),
        data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
    REQUIRE(map);
    REQUIRE(map->outdoorMapData);
    std::string error;
    REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fixture.assetFileSystem, *map, error), error);
    REQUIRE(map->globalEventProgram);
    REQUIRE(map->localEventProgram);
    const auto &global = *map->globalEventProgram;
    REQUIRE(global.qbitGrants().contains(25));
    CHECK(global.qbitGrants().at(25) == std::vector<uint32_t>{430});
    REQUIRE(global.npcTopicReferences().contains(36));
    CHECK(global.npcTopicReferences().at(36)
        == std::vector<ScriptedEventProgram::NpcTopicReference>{{43, 612}});
    WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
        data.getMergedTeacherTopicTable(), "mm8");
    world.indexNpcLocations(global);
    world.indexMapLocations(*map->outdoorMapData, *map->localEventProgram);
    world.indexMapInteractionReferences(*pMap, data.getItemTable(), data.getSpellTable());
    WorldKnowledge knowledge;
    Party party;
    WorldGuideSource source;
    source.worldId = "mm8";
    source.pRoster = &data.getRosterTable();
    source.pNpcs = &data.getNpcDialogTable();
    source.pWorld = &world;
    source.pWorldKnowledge = &knowledge;
    source.pParty = &party;
    source.pGlobalEvents = &global;
    source.pLocalEvents = &*map->localEventProgram;
    const auto entries = buildWorldGuide(source, WorldGuideSection::Companions, {}, true);
    const auto find = [&](const std::string &id) -> const WorldGuideEntry &
    {
        const auto entry = std::find_if(entries.begin(), entries.end(), [&](const WorldGuideEntry &value)
            { return value.id == id; });
        REQUIRE(entry != entries.end());
        return *entry;
    };
    const auto &fredrick = find("2");
    CHECK(std::any_of(fredrick.links.begin(), fredrick.links.end(), [](const WorldGuideLink &link)
        { return link.kind == WorldGuideLink::Kind::Location && link.target == "mm8:npc/28"; }));
    const auto &cauri = find("30");
    CHECK(cauri.details.find("Inn unlock not recorded yet") != std::string::npos);
    CHECK(std::any_of(cauri.links.begin(), cauri.links.end(), [&](const WorldGuideLink &link)
    {
        if (link.kind != WorldGuideLink::Kind::Feature) return false;
        const auto feature = std::find_if(world.features().begin(), world.features().end(),
            [&](const WorldMapFeature &value) { return value.id == link.target; });
        return feature != world.features().end() && feature->site.eventId == 132
            && feature->interactionReference && feature->interactionReference->npcId == 42;
    }));
    CHECK(buildWorldGuide(source, WorldGuideSection::Companions, {}, false).empty());
    CHECK(knowledge.variables.empty());
    CHECK(knowledge.visitedMaps.empty());
    CHECK_FALSE(party.hasQuestBit(430));
    CHECK(party.memberCount() == 0);
    CHECK(party.adventurersInnMembers().empty());
}

TEST_CASE("canonical journal well references bind all newly explained sites without leaking into Discovery")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &fixture = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = fixture.gameDataLoader;
    struct Map
    {
        const char *pWorld;
        const char *pFile;
        std::vector<uint16_t> events;
    };
    const Map maps[] = {
        {"mm7", "7out03.odm", {203, 204, 208, 209, 210}},
        {"mm7", "7out04.odm", {204}}, {"mm7", "7out05.odm", {203, 204, 205, 207}},
        {"mm7", "7out06.odm", {461}}, {"mm7", "7out13.odm", {202, 203, 204}},
        {"mm7", "out10.odm", {203, 204, 206}}, {"mm7", "out11.odm", {321}},
        {"mm7", "out14.odm", {204, 205}}, {"mm8", "out07.odm", {101}}};
    size_t references = 0;
    for (const Map &target : maps)
    {
        CAPTURE(std::string(target.pFile));
        const MapStatsEntry *pDefinition = data.getMapStats().findByFileName(target.pFile);
        REQUIRE(pDefinition);
        auto map = MapAssetLoader().load(fixture.assetFileSystem, *pDefinition, data.getMonsterTable(),
            data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
        REQUIRE(map);
        REQUIRE(map->outdoorMapData);
        std::string error;
        REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fixture.assetFileSystem, *map, error), error);
        REQUIRE(map->localEventProgram);
        const ScriptedEventProgram &events = *map->localEventProgram;
        WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
            data.getMergedTeacherTopicTable(), target.pWorld);
        world.indexMapLocations(*map->outdoorMapData, events);
        for (uint16_t event : target.events)
        {
            CAPTURE(event);
            REQUIRE(events.siteReferences().contains(event));
            const auto feature = std::find_if(world.features().begin(), world.features().end(),
                [&](const WorldMapFeature &value) { return value.site.eventId == event; });
            REQUIRE(feature != world.features().end());
            CHECK(feature->kind == WorldMapFeatureKind::Water);
            WorldKnowledge knowledge;
            knowledge.visitedMaps.insert(target.pFile);
            CHECK_FALSE(world.featureVisible(*feature, knowledge, false));
            CHECK(world.siteReference(*feature, knowledge, false).empty());
            const std::string text(world.siteReference(*feature, knowledge, true));
            CHECK(text.find("Outcome 1") != std::string::npos);
            CHECK(text.find("Outcome 2") == std::string::npos);
            CHECK(text.find("Journal note") == std::string::npos);
            knowledge.interactedSites[target.pFile].insert(worldSiteDiscoveryKey(event, feature->site.id));
            CHECK(world.featureVisible(*feature, knowledge, false));
            CHECK(world.siteReference(*feature, knowledge, false).empty());
            CHECK(knowledge.variables.empty());
            CHECK(knowledge.autonoteIds.empty());
            ++references;
        }
    }
    CHECK(references == 21);
}

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

TEST_CASE("canonical Lincoln and submarine references match active party scope and persistent unlock callbacks")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &loader = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = loader.gameDataLoader;
    for (const bool lincoln : {true, false})
    {
        const std::string worldId = lincoln ? "mm7" : "mm8";
        const std::string file = lincoln ? "7d23.blv" : "d06.blv";
        const uint16_t event = lincoln ? 501 : 451;
        const uint32_t itemId = lincoln ? 1406 : 619;
        CAPTURE(file);
        const MapStatsEntry *pEntry = data.getMapStats().findByFileName(file);
        REQUIRE(pEntry);
        auto map = MapAssetLoader().load(loader.assetFileSystem, *pEntry, data.getMonsterTable(),
            data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
        REQUIRE(map);
        REQUIRE(map->indoorMapData);
        std::string error;
        REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(loader.assetFileSystem, *map, error), error);
        REQUIRE(map->localEventProgram);
        const ScriptedEventProgram &events = *map->localEventProgram;
        REQUIRE(events.interactionReferences().contains(event));
        const SiteInteractionReference &reference = events.interactionReferences().at(event);
        CHECK(reference.itemScope == (lincoln ? "each_member" : "")); // Empty retains the party-wide default.
        CHECK(reference.openedQBit == (lincoln ? 0 : 214));
        WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
            data.getMergedTeacherTopicTable(), worldId);
        world.indexMapLocations(*pEntry, *map->indoorMapData, events);
        world.indexMapInteractionReferences(*pEntry, data.getItemTable(), data.getSpellTable());
        size_t sites = 0;
        for (const WorldMapFeature &feature : world.features())
        {
            if (feature.site.eventId != event) continue;
            ++sites;
            CHECK(feature.kind == WorldMapFeatureKind::Connection);
            CHECK(feature.targetMap == reference.targetMap);
            WorldKnowledge knowledge;
            knowledge.visitedMaps.insert(file);
            CHECK_FALSE(world.featureVisible(feature, knowledge, false));
            CHECK(world.featureVisible(feature, knowledge, true));
            const std::string guide(world.siteReference(feature, knowledge, true));
            CHECK(guide.find(data.getItemTable().get(itemId)->name) != std::string::npos);
            CHECK(guide.find(lincoln ? "Every party member" : "A party member") != std::string::npos);
            CHECK((guide.find("campaign record") != std::string::npos) == !lincoln);
            CHECK(guide.find("while this location") == std::string::npos);
            CHECK(guide.find("wearing") == std::string::npos); // The active script accepts inventory too.
            knowledge.interactedSites[file].insert(worldSiteDiscoveryKey(event, feature.site.id));
            CHECK(world.siteReference(feature, knowledge, false).empty());
        }
        CHECK(sites > 0);
        EventRuntime runtime;
        for (const size_t members : {1u, 2u, 5u})
            for (const bool carries : {false, true})
                for (const bool unlocked : {false, true})
                {
                    Party::Snapshot seed;
                    seed.members.resize(members);
                    Party party;
                    party.restoreSnapshot(seed);
                    InventoryItem item;
                    item.objectDescriptionId = itemId;
                    if (lincoln)
                    {
                        // One missing member must block travel even when another holds duplicate suits.
                        for (size_t index = 0; index < members; ++index)
                            if (carries || index + 1 < members)
                                REQUIRE(party.member(index)->addInventoryItem(item));
                        if (!carries && members > 1) REQUIRE(party.member(0)->addInventoryItem(item));
                    }
                    else if (carries) REQUIRE(party.member(members - 1)->addInventoryItem(item));
                    party.setQuestBit(214, unlocked);
                    const int originalCount = party.inventoryItemCount(itemId);
                    EventRuntimeState state;
                    state.mapFileName = file;
                    REQUIRE(runtime.executeEventById(map->localEventProgram, map->globalEventProgram,
                        event, state, &party));
                    const EventRuntimeState::PendingMapMove *pMove = state.pendingMapMove
                        ? &*state.pendingMapMove : state.pendingDialogueContext
                            && state.pendingDialogueContext->transitionMapMove
                            ? &*state.pendingDialogueContext->transitionMapMove : nullptr;
                    CHECK((pMove != nullptr) == (carries || (!lincoln && unlocked)));
                    if (pMove != nullptr)
                    {
                        REQUIRE(pMove->mapName);
                        CHECK(*pMove->mapName == reference.targetMap);
                    }
                    CHECK(party.inventoryItemCount(itemId) == originalCount);
                    CHECK(party.hasQuestBit(214) == (unlocked || (!lincoln && carries)));
                }
    }
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
            const auto knownSites = world.featureSites(**feature, knowledge, false);
            REQUIRE(knownSites.size() == 1);
            CHECK(knownSites.front()->id == site.id);
            const auto locations = world.queryLocations(knowledge, false, WorldDatabase::mapId(*entry));
            REQUIRE(locations.size() == 1);
            CHECK(locations.front().x == site.x);
            CHECK(locations.front().y == site.y);
            CHECK(locations.front().z == site.z);
            CHECK(locations.front().id == (*feature)->id);
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

TEST_CASE("canonical connection requirement references stay Guide-only after discovering their physical sites")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &loader = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = loader.gameDataLoader;
    struct Route
    {
        const char *pWorld;
        const char *pMap;
        uint16_t event;
        const char *pRequirement;
    };
    const Route routes[] = {
        {"mm7", "t04.blv", 502, "Any one of these alternatives:"},
        {"mm7", "mdk01.blv", 502, "Controlling lever: second position"},
        {"mm7", "7d13.blv", 501, "at random; probabilities are not shown"},
        {"mm8", "pbp.odm", 507, "Selected character carries: Flute"}};
    for (const Route &route : routes)
    {
        CAPTURE(route.pWorld);
        CAPTURE(route.pMap);
        const auto &entries = data.getMapStats().getEntries();
        const auto entry = std::find_if(entries.begin(), entries.end(), [&](const MapStatsEntry &map)
            { return map.worldId == route.pWorld && map.fileName == route.pMap; });
        REQUIRE(entry != entries.end());
        auto map = MapAssetLoader().load(loader.assetFileSystem, *entry, data.getMonsterTable(),
            data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
        REQUIRE(map);
        std::string error;
        REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(loader.assetFileSystem, *map, error), error);
        REQUIRE(map->localEventProgram);
        WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
            data.getMergedTeacherTopicTable(), route.pWorld);
        if (map->outdoorMapData) world.indexMapLocations(*map->outdoorMapData, *map->localEventProgram);
        else world.indexMapLocations(*entry, *map->indoorMapData, *map->localEventProgram);
        const auto feature = std::find_if(world.features().begin(), world.features().end(),
            [&](const WorldMapFeature &value) { return value.site.eventId == route.event; });
        REQUIRE(feature != world.features().end());
        CHECK(feature->kind == WorldMapFeatureKind::Connection);
        WorldKnowledge knowledge;
        knowledge.visitedMaps.insert(route.pMap);
        CHECK(world.siteReference(*feature, knowledge, true).find(route.pRequirement) != std::string::npos);
        CHECK(world.siteReference(*feature, knowledge, false).empty());
        CHECK(knowledge.interactedSites.empty());
        CHECK(knowledge.variables.empty());
        knowledge.interactedSites[route.pMap].insert(worldSiteDiscoveryKey(route.event, feature->site.id));
        CHECK(world.featureVisible(*feature, knowledge, false));
        CHECK(world.siteReference(*feature, knowledge, false).empty());
        CHECK(knowledge.visitedMaps.size() == 1);
    }
}

TEST_CASE("canonical Barrow control references have physical exits and levers across the full authored network")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &loader = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = loader.gameDataLoader;
    size_t references = 0, levers = 0, maps = 0;
    for (const MapStatsEntry &entry : data.getMapStats().getEntries())
    {
        if (entry.worldId != "mm7") continue;
        MapAssetInfo metadata;
        metadata.map = entry;
        std::string error;
        REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(loader.assetFileSystem, metadata, error), error);
        REQUIRE(metadata.localEventProgram);
        const ScriptedEventProgram &events = *metadata.localEventProgram;
        const auto &descriptions = events.siteReferences();
        if (std::none_of(descriptions.begin(), descriptions.end(), [](const auto &item)
            { return item.second.find("Controlling lever:") != std::string::npos; })) continue;
        CAPTURE(entry.fileName);
        ++maps;
        auto map = MapAssetLoader().load(loader.assetFileSystem, entry, data.getMonsterTable(),
            data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
        REQUIRE(map);
        REQUIRE(map->indoorMapData);
        const auto sites = worldInteractionSites(*map->indoorMapData);
        size_t localReferences = 0, localLevers = 0;
        for (const auto &[event, description] : descriptions)
        {
            if (description.find("Controlling lever:") == std::string::npos) continue;
            ++localReferences;
            CAPTURE(event);
            CHECK(std::any_of(sites.begin(), sites.end(), [&](const WorldInteractionSite &site)
                { return site.eventId == event; }));
            const auto action = events.getContextActionMetadata(event);
            REQUIRE(action);
            CHECK(action->targetMaps.size() == 2);
        }
        for (uint16_t event : events.eventIds())
        {
            const auto action = events.getContextActionMetadata(event);
            if (!action || action->kind != "use_lever") continue;
            ++localLevers;
            CAPTURE(event);
            CHECK(std::any_of(sites.begin(), sites.end(), [&](const WorldInteractionSite &site)
                { return site.eventId == event; }));
        }
        CHECK(localReferences == localLevers);
        references += localReferences;
        levers += localLevers;
    }
    CHECK(maps == 15);
    CHECK(references == 27);
    CHECK(levers == 27);
}


TEST_CASE("site item observations resolve active names and identification gates in native feature search")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &fixture = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = fixture.gameDataLoader;
    const MapStatsEntry *pMap = data.getMapStats().findByFileName("out07.odm");
    REQUIRE(pMap);
    auto map = MapAssetLoader().load(fixture.assetFileSystem, *pMap, data.getMonsterTable(),
        data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
    REQUIRE(map);
    REQUIRE(map->outdoorMapData);
    std::string error;
    REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fixture.assetFileSystem, *map, error), error);
    REQUIRE(map->localEventProgram);
    WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
        data.getMergedTeacherTopicTable(), "mm8");
    world.indexMapLocations(*map->outdoorMapData, *map->localEventProgram);
    const auto feature = std::find_if(world.features().begin(), world.features().end(),
        [](const WorldMapFeature &value) { return value.site.eventId == 132; });
    REQUIRE(feature != world.features().end());
    std::vector<std::string> row(17);
    row[0] = "42"; row[2] = "Custom secret blade"; row[9] = "5"; row[10] = "Unidentified blade";
    ItemTable items;
    REQUIRE(items.load(fixture.assetFileSystem, {row}, {}));
    WorldSiteValueSnapshot before, after;
    before.members = after.members = {"Hero"};
    before.items[42] = 1;
    EventRuntimeState state;
    REQUIRE(recordWorldSiteObservation(state, feature->site.id, 132, before, after, 10));
    WorldKnowledge knowledge;
    knowledge.siteObservations[feature->mapId] = worldSiteObservations(state);
    CHECK(world.queryFeatures(knowledge, false, "Unidentified blade", "", nullptr, &items).empty());
    knowledge.interactedSites["out07.odm"].insert(worldSiteDiscoveryKey(132, feature->site.id));
    CHECK(world.queryFeatures(knowledge, false, "Unidentified blade", "", nullptr, &items).size() == 1);
    CHECK(world.queryFeatures(knowledge, false, "Custom secret blade", "", nullptr, &items).empty());
    CHECK(world.queryFeatures(knowledge, true, "Custom secret blade", "", nullptr, &items).empty());
    CHECK(world.siteObservationText(*feature, knowledge, nullptr, &items).find("Unidentified blade -1")
        != std::string::npos);
    knowledge.identifiedItems.insert(42);
    CHECK(world.queryFeatures(knowledge, false, "Custom secret blade", "", nullptr, &items).size() == 1);
    CHECK(world.siteObservationText(*feature, knowledge, nullptr, &items).find("Custom secret blade -1")
        != std::string::npos);
    CHECK(knowledge.autonoteIds.empty());
    CHECK(knowledge.visitedMaps.empty());
}


TEST_CASE("inherited site deliveries join only confirmed receipts at the exact object and preserve identification")
{
    using namespace OpenYAMM::Game;
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &fixture = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = fixture.gameDataLoader;
    const MapStatsEntry *pMap = data.getMapStats().findByFileName("7out02.odm");
    REQUIRE(pMap);
    auto map = MapAssetLoader().load(fixture.assetFileSystem, *pMap, data.getMonsterTable(),
        data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
    REQUIRE(map);
    std::string error;
    REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fixture.assetFileSystem, *map, error), error);
    REQUIRE(map->globalEventProgram);
    WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
        data.getMergedTeacherTopicTable(), "mm7");
    WorldDecorationSite first, other;
    first.site = {"inherited-decoration/0", 550, 100, 200, 300};
    first.eventCount = 2; first.initialEventId = 551; first.name = "Beacon fire";
    other = first; other.site.id = "inherited-decoration/1";
    world.indexInheritedDecorations(*pMap, {first, other}, *map->globalEventProgram);
    const auto features = world.queryFeatures({}, true);
    REQUIRE(features.size() == 2);
    const auto feature = std::find_if(features.begin(), features.end(), [](const WorldMapFeature *pFeature)
        { return pFeature->site.id == "inherited-decoration/0"; });
    REQUIRE(feature != features.end());
    WorldKnowledge knowledge;
    knowledge.interactedSites["7out02.odm"].insert(worldSiteDiscoveryKey(551, first.site.id));
    EventRuntimeState state;
    state.activeDecorationContext = EventRuntimeState::ActiveDecorationContext{
        .decorVarIndex = 0, .baseEventId = 550, .currentEventId = 551, .eventCount = 2};
    state.grantedItemIds = {1};
    stageWorldItemReward(state, false, 0, 551, true, 1440);
    state.activeDecorationContext.reset();
    knowledge.itemObservations["7out02.odm"] = worldObservedItemSources(state);
    CHECK(world.siteItemDeliveries(**feature, knowledge).empty());
    confirmWorldItemReward(state, false, 0);
    knowledge.itemObservations["7out02.odm"] = worldObservedItemSources(state);
    const auto delivered = world.siteItemDeliveries(**feature, knowledge);
    REQUIRE(delivered.size() == 1);
    CHECK(delivered[0]->itemId == 1);
    CHECK(delivered[0]->sourceIndex == 551);
    WorldSiteValueSnapshot unchanged;
    REQUIRE(recordWorldSiteObservation(state, first.site.id, 551, unchanged, unchanged, 1440));
    knowledge.siteObservations[(*feature)->mapId] = worldSiteObservations(state);
    CHECK_FALSE(world.siteObservations(**feature, knowledge)[0]->lastChanged);
    CHECK(world.siteObservationText(**feature, knowledge).find("No additional changes") == std::string::npos);
    CHECK_FALSE(world.siteObservations(**feature, knowledge)[0]->lastChanged);
    const auto ownSources = knowledge.itemObservations;
    knowledge.itemObservations.clear();
    knowledge.itemObservations["out01.odm"] = ownSources.at("7out02.odm");
    CHECK(world.siteItemDeliveries(**feature, knowledge).empty());
    knowledge.itemObservations = ownSources;
    knowledge.itemObservations["7out02.odm"][0].observed = false;
    CHECK(world.siteItemDeliveries(**feature, knowledge).empty());
    knowledge.itemObservations = ownSources;
    REQUIRE(data.getItemTable().get(1));
    const auto &item = *data.getItemTable().get(1);
    const std::string received = world.siteObservationText(**feature, knowledge, nullptr, &data.getItemTable());
    CHECK(received.find("Items received here:") != std::string::npos);
    CHECK(received.find(item.unidentifiedName + " | Recorded: day 2") != std::string::npos);
    CHECK(received.find(item.name) == std::string::npos);
    CHECK(world.queryFeatures(knowledge, false, item.unidentifiedName, "", nullptr, &data.getItemTable()).size() == 1);
    CHECK(world.queryFeatures(knowledge, false, item.name, "", nullptr, &data.getItemTable()).empty());
    const auto neighbor = std::find_if(features.begin(), features.end(), [](const WorldMapFeature *pFeature)
        { return pFeature->site.id == "inherited-decoration/1"; });
    REQUIRE(neighbor != features.end());
    knowledge.interactedSites["7out02.odm"].insert(worldSiteDiscoveryKey(551, other.site.id));
    CHECK(world.siteItemDeliveries(**neighbor, knowledge).empty());
    // A second real callback variant of this slot must not duplicate the same item in the view.
    state.activeDecorationContext = EventRuntimeState::ActiveDecorationContext{
        .decorVarIndex = 0, .baseEventId = 550, .currentEventId = 550, .eventCount = 2};
    recordWorldItemReward(state, 1, 550, true, 2880);
    recordWorldItemReward(state, 2, 550, true, 2880);
    knowledge.itemObservations["7out02.odm"] = worldObservedItemSources(state);
    REQUIRE(world.siteItemDeliveries(**feature, knowledge).size() == 2);
    CHECK(world.siteItemDeliveries(**feature, knowledge)[0]->gameMinutes == 2880);
    CHECK(world.siteObservationText(**feature, knowledge, nullptr, &data.getItemTable())
        .find("Recorded: day 3") != std::string::npos);
    knowledge.identifiedItems.insert(1);
    CHECK(world.siteObservationText(**feature, knowledge, nullptr, &data.getItemTable()).find(item.name)
        != std::string::npos);
    knowledge.interactedSites.clear();
    CHECK(world.siteItemDeliveries(**feature, knowledge).empty());
    CHECK(world.siteObservationText(**feature, knowledge, nullptr, &data.getItemTable()).empty());
    CHECK(knowledge.visitedMaps.empty());
    CHECK(knowledge.autonoteIds.empty());
}
