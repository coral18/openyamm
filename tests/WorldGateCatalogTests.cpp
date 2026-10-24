#include "doctest/doctest.h"
#include "game/world/WorldGateCatalog.h"
#include "game/world/WorldMapKnowledge.h"
#include "game/world/WorldDatabase.h"
#include "game/party/Party.h"
#include "game/data/GameDataLoader.h"
#include "game/outdoor/OutdoorMapData.h"
#include "game/indoor/IndoorMapData.h"
#include "tests/RegressionMapLoader.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>

using namespace OpenYAMM::Game;

namespace
{
std::string gateSource()
{
    std::ifstream input(std::string(OPENYAMM_SOURCE_DIR) + "/assets_dev/engine/world/gates.yml");
    REQUIRE(input.is_open());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}

TEST_CASE("gate catalog resolves current progression landings through the same data as the native selector")
{
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(), OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &loader = OpenYAMM::Tests::regressionMapLoader().gameDataLoader;
    WorldGateCatalog catalog;
    std::string error;
    REQUIRE_MESSAGE(catalog.loadFromText(gateSource(), loader.getMapStats(),
        loader.getMergedTownPortalSwitchTable(), error), error);
    const WorldGateNetwork *pNetwork = catalog.defaultNetwork();
    REQUIRE(pNetwork);
    CHECK(pNetwork->id == "merge.dimension_doors");
    CHECK_FALSE(catalog.unlocked(*pNetwork, {}));
    CHECK(catalog.unlocked(*pNetwork, {{pNetwork->unlockVariable, 1}}));
    Party party;
    for (bool progressed : {false, true, false})
    {
        CAPTURE(progressed);
        party.setQuestBit(4, progressed);
        party.setQuestBit(519, progressed);
        const auto choices = catalog.choices(*pNetwork, &party);
        REQUIRE(choices.size() == 3);
        CHECK(choices[0].pLanding->mapId == (progressed ? "world.mm8.map.out02" : "world.mm8.map.out01"));
        CHECK(choices[1].pLanding->mapId == (progressed ? "world.mm7.map.7out02" : "world.mm7.map.7out01"));
        CHECK(choices[2].pLanding->mapId == "world.mm6.map.oute3");
        for (const WorldGateChoice &choice : choices)
        {
            const auto historical = catalog.match(pNetwork->id, choice.pDestination->id, choice.pLanding->mapId);
            REQUIRE(historical);
            CHECK(historical->pDestination == choice.pDestination);
            CHECK(historical->pLanding == choice.pLanding);
        }
    }
    CHECK_FALSE(catalog.match("missing.network", "TPGlobal:0", "world.mm8.map.out01"));
    CHECK_FALSE(catalog.match(pNetwork->id, "TPGlobal:2", "world.mm8.map.out01"));
    CHECK_FALSE(catalog.match(pNetwork->id, "TPGlobal:0", "world.mm8.map.missing"));
}

TEST_CASE("gate catalog rejects missing geography ambiguous branches and malformed authoring without stale data")
{
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(), OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &loader = OpenYAMM::Tests::regressionMapLoader().gameDataLoader;
    const std::string source = gateSource();
    const auto altered = [&](const std::string &before, const std::string &after)
    {
        std::string text = source;
        const size_t index = text.find(before);
        REQUIRE(index != std::string::npos);
        text.replace(index, before.size(), after);
        return text;
    };
    const std::vector<std::string> invalid = {
        altered("version: 1", "version: 2"),
        altered("default_network: merge.dimension_doors", "default_network: unknown.network"),
        altered("portal_group: TPGlobal", "portal_group: missing"),
        altered("source_id: 0", "source_id: 9999"),
        altered("source_id: 2", "source_id: 0"),
        altered("world.mm7.map.7out02", "world.mm8.map.7out02"),
        altered("requires_qbit: 519", "requires_qbit: 0"),
        altered("map_start: true", "map_start: false"),
        altered("map_start: true", "map_start: true\n            position: [0, 0, 0]"),
        altered("position: [-9728, -11319, 160]", "position: [0, 1]"),
        altered("position: [-9728, -11319, 160]", "position: [0, 1, 2147483648]"),
        altered("hit_rect: [0, 0, 213, 480]", "hit_rect: [0, 0, -1, 480]"),
        altered("version: 1", "version: 1\nversion: 1"),
        altered("unlock_variable:", "typo:")};
    WorldGateCatalog catalog;
    std::string error;
    for (const std::string &text : invalid)
    {
        REQUIRE(catalog.loadFromText(source, loader.getMapStats(), loader.getMergedTownPortalSwitchTable(), error));
        CHECK_FALSE(catalog.loadFromText(text, loader.getMapStats(), loader.getMergedTownPortalSwitchTable(), error));
        CHECK_FALSE(error.empty());
        CHECK(catalog.defaultNetwork() == nullptr);
    }
}

TEST_CASE("gate offers retain only actual last offers per source event without becoming travelled arrivals")
{
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(), OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &loader = OpenYAMM::Tests::regressionMapLoader().gameDataLoader;
    const WorldGateCatalog &catalog = loader.getWorldGateCatalog();
    const WorldGateNetwork *pNetwork = catalog.defaultNetwork();
    REQUIRE(pNetwork);
    Party party;
    EventRuntimeState state;
    state.mapFileName = "7d29.blv";
    CHECK(worldGateOffers(state).empty());
    recordWorldGateOffers(state, 0, pNetwork->id, catalog.choices(*pNetwork, &party), 1440);
    CHECK(state.namedMapVars.empty());
    recordWorldGateOffers(state, 65031, pNetwork->id, catalog.choices(*pNetwork, &party), 1440);
    auto offers = worldGateOffers(state);
    REQUIRE(offers.size() == 3);
    CHECK(std::any_of(offers.begin(), offers.end(), [](const WorldGateOffer &offer)
        { return offer.mapId == "world.mm7.map.7out01"; }));
    CHECK(worldMapArrivals(state, state.mapFileName).empty());
    party.setQuestBit(519, true);
    recordWorldGateOffers(state, 65031, pNetwork->id, catalog.choices(*pNetwork, &party), 2880);
    offers = worldGateOffers(state);
    REQUIRE(offers.size() == 3);
    CHECK_FALSE(std::any_of(offers.begin(), offers.end(), [](const WorldGateOffer &offer)
        { return offer.mapId == "world.mm7.map.7out01"; }));
    CHECK(std::any_of(offers.begin(), offers.end(), [](const WorldGateOffer &offer)
        { return offer.mapId == "world.mm7.map.7out02" && offer.gameMinutes == 2880; }));
    EventRuntimeState refilled;
    refilled.mapFileName = state.mapFileName;
    restoreWorldMapKnowledgeAfterRefill(state, refilled);
    CHECK(worldGateOffers(refilled).size() == 3);
    state.namedMapVars["openyamm.discovery.gate/65031/merge.dimension_doors/TPGlobal:2/world.mm7.map.7out02/time"] = -1;
    CHECK(worldGateOffers(state).size() == 2);
    state.namedMapVars["openyamm.discovery.gate/bad/network/choice/world.mm8.map.out01/seen"] = 1;
    CHECK(worldGateOffers(state).size() == 2);
}

TEST_CASE("gate references distinguish authored possibilities actual offers and per-world travel history")
{
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(), OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &loader = OpenYAMM::Tests::regressionMapLoader().gameDataLoader;
    const WorldGateCatalog &gates = loader.getWorldGateCatalog();
    WorldDatabase world(loader.getMapStats(), loader.getHouseTable(), loader.getNpcDialogTable(),
        loader.getMergedTeacherTopicTable(), "mm7");
    WorldMapFeature gate;
    gate.kind = WorldMapFeatureKind::Connection;
    gate.mapId = "world.mm7.map.7d29";
    gate.site = {"surface/1668", 65031};
    gate.interactionReference.emplace();
    gate.interactionReference->kind = "dimension_door";
    gate.interactionReference->network = "merge.dimension_doors";
    WorldKnowledge knowledge;
    knowledge.visitedMaps.insert("7d29.blv");
    CHECK(world.gateDestinations(gate, knowledge, gates, nullptr, false).empty());
    CHECK(world.gateDestinations(gate, knowledge, gates, nullptr, true).size() == 3);
    knowledge.interactedSites["7d29.blv"].insert(worldSiteDiscoveryKey(65031, gate.site.id));
    CHECK(world.gateDestinations(gate, knowledge, gates, nullptr, false).empty());
    EventRuntimeState source;
    source.mapFileName = "7d29.blv";
    recordWorldGateOffers(source, 65031, gates.defaultNetwork()->id,
        gates.choices(*gates.defaultNetwork(), nullptr), 1440);
    knowledge.gateOffers[gate.mapId] = worldGateOffers(source);
    CHECK(world.gateDestinations(gate, knowledge, gates, nullptr, false).size() == 3);
    CHECK(world.gateOfferText(gate, knowledge, gates).find("Previously offered") != std::string::npos);
    knowledge.gateOffers[gate.mapId].push_back({65031, "merge.dimension_doors", "TPGlobal:2",
        "world.mm8.map.out03", 1440});
    CHECK(world.gateDestinations(gate, knowledge, gates, nullptr, false).size() == 3);
    CHECK(world.gateOfferText(gate, knowledge, gates).find("Ravenshore") == std::string::npos);
    CHECK(world.connectionArrivals(gate, knowledge).empty());
    WorldMapFeature other = gate;
    other.site.eventId = 6;
    knowledge.interactedSites["7d29.blv"].insert(worldSiteDiscoveryKey(6, other.site.id));
    CHECK(world.gateDestinations(other, knowledge, gates, nullptr, false).empty());
    EventRuntimeState destination;
    destination.mapFileName = "out01.odm";
    REQUIRE(recordWorldMapArrival(destination, gate.mapId, 65031, 10, 20, 30, 1500));
    knowledge.arrivals["world.mm8.map.out01"] = worldMapArrivals(destination, destination.mapFileName);
    knowledge.visitedMaps.insert(destination.mapFileName);
    world.indexMapArrivals(knowledge);
    const auto arrivals = world.connectionArrivals(gate, knowledge);
    REQUIRE(arrivals.size() == 1);
    CHECK(arrivals[0]->mapId == "world.mm8.map.out01");
    CHECK(arrivals[0]->targetMapId == gate.mapId);
    CHECK(world.queryFeatures(knowledge, false).empty()); // Foreign arrivals do not become local map pins.
    WorldDatabase jadame(loader.getMapStats(), loader.getHouseTable(), loader.getNpcDialogTable(),
        loader.getMergedTeacherTopicTable(), "mm8");
    jadame.indexMapArrivals(knowledge);
    const auto local = jadame.queryFeatures(knowledge, false);
    REQUIRE(local.size() == 1);
    CHECK(local[0]->arrival);
    CHECK(local[0]->title.find("Castle Harmondale") != std::string::npos);
}

TEST_CASE("gate catalogs bind all five physical MM7 and MM8 sources without leaking destinations into discovery")
{
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(), OpenYAMM::Tests::regressionMapLoaderFailure());
    const auto &fixture = OpenYAMM::Tests::regressionMapLoader();
    const auto &data = fixture.gameDataLoader;
    const auto &gates = data.getWorldGateCatalog();
    struct Source { const char *pWorld; const char *pMap; uint16_t event; };
    const Source sources[] = {{"mm7", "7d29.blv", 65031}, {"mm7", "7out04.odm", 504},
        {"mm7", "out09.odm", 6}, {"mm8", "out02.odm", 104}, {"mm8", "out07.odm", 500}};
    for (const Source &source : sources)
    {
        CAPTURE(source.pWorld);
        CAPTURE(source.pMap);
        const auto &entries = data.getMapStats().getEntries();
        const auto entry = std::find_if(entries.begin(), entries.end(), [&](const MapStatsEntry &map)
            { return map.worldId == source.pWorld && map.fileName == source.pMap; });
        REQUIRE(entry != entries.end());
        auto map = MapAssetLoader().load(fixture.assetFileSystem, *entry, data.getMonsterTable(),
            data.getObjectTable(), MapLoadPurpose::HeadlessGameplay);
        REQUIRE(map);
        std::string error;
        REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fixture.assetFileSystem, *map, error), error);
        REQUIRE(map->localEventProgram);
        WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
            data.getMergedTeacherTopicTable(), source.pWorld);
        if (map->outdoorMapData) world.indexMapLocations(*map->outdoorMapData, *map->localEventProgram);
        else world.indexMapLocations(*entry, *map->indoorMapData, *map->localEventProgram);
        world.indexMapInteractionReferences(*entry, data.getItemTable(), data.getSpellTable());
        world.indexMapGateReferences(*entry, gates, nullptr);
        const auto feature = std::find_if(world.features().begin(), world.features().end(),
            [&](const WorldMapFeature &value) { return value.site.eventId == source.event; });
        REQUIRE(feature != world.features().end());
        REQUIRE(feature->interactionReference);
        CHECK(feature->interactionReference->network == gates.defaultNetwork()->id);
        CHECK(feature->kind == WorldMapFeatureKind::Connection);
        CHECK(feature->title == gates.defaultNetwork()->title);
        CHECK(std::count_if(world.features().begin(), world.features().end(),
            [&](const WorldMapFeature &value) { return value.site.eventId == source.event; }) == 1);
        WorldKnowledge knowledge;
        knowledge.visitedMaps.insert(source.pMap);
        CHECK_FALSE(world.featureVisible(*feature, knowledge, false));
        CHECK(world.siteReference(*feature, knowledge, true).find("Jadame") != std::string::npos);
        knowledge.interactedSites[source.pMap].insert(worldSiteDiscoveryKey(source.event, feature->site.id));
        CHECK(world.featureVisible(*feature, knowledge, false));
        CHECK(world.siteReference(*feature, knowledge, false).empty());
        CHECK(world.gateOfferText(*feature, knowledge, gates).empty());
        CHECK(world.gateDestinations(*feature, knowledge, gates, nullptr, false).empty());
        CHECK(world.gateDestinations(*feature, knowledge, gates, nullptr, true).size() == 3);
    }
}
