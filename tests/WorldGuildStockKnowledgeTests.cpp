#include "doctest/doctest.h"
#include "tests/RegressionMapLoader.h"
#include "game/events/EventRuntime.h"
#include "game/gameplay/GuildSpellbookRules.h"
#include "game/world/WorldDatabase.h"
#include "game/world/WorldGuide.h"
#include "game/world/WorldGuildStockKnowledge.h"

#include <algorithm>
#include <limits>

using namespace OpenYAMM::Game;

namespace
{
const GameDataLoader &guildData()
{
    REQUIRE_MESSAGE(OpenYAMM::Tests::regressionMapLoaderLoaded(),
        OpenYAMM::Tests::regressionMapLoaderFailure());
    return OpenYAMM::Tests::regressionMapLoader().gameDataLoader;
}

InventoryItem guildBook(const ItemTable &items, uint32_t spellId)
{
    const auto book = std::find_if(items.entries().begin(), items.entries().end(),
        [&](const ItemDefinition &item) { return spellbookSpellId(item) == spellId; });
    REQUIRE(book != items.entries().end());
    InventoryItem result;
    result.objectDescriptionId = book->itemId;
    result.identified = true;
    return result;
}
}

TEST_CASE("guild shelf observations replace last stock but retain encountered book knowledge")
{
    const ItemTable &items = guildData().getItemTable();
    const InventoryItem first = guildBook(items, 1), second = guildBook(items, 2);
    EventRuntimeState state;
    CHECK_FALSE(worldGuildStockObservation(state.namedGlobalVars, 129));
    CHECK_FALSE(worldGuildBookKnown(state.namedGlobalVars, first.objectDescriptionId));
    recordWorldGuildStockObservation(state, 129, {second, {}, first, second}, items, 1440.9f);
    const auto seen = worldGuildStockObservation(state.namedGlobalVars, 129);
    REQUIRE(seen);
    CHECK(seen->gameMinutes == 1440);
    CHECK(seen->books.size() == 2);
    CHECK(std::is_sorted(seen->books.begin(), seen->books.end()));
    CHECK(worldGuildBookKnown(state.namedGlobalVars, first.objectDescriptionId));
    CHECK(worldGuildBookKnown(state.namedGlobalVars, second.objectDescriptionId));
    CHECK(state.namedGlobalVars.at(WorldDatabase::houseDiscoveryKey(129)) == 1);
    CHECK_FALSE(worldGuildStockObservation(state.namedGlobalVars, 182));
    const auto unchanged = state.namedGlobalVars;
    recordWorldGuildStockObservation(state, 129, {first, second}, items, 1440.9f);
    CHECK(state.namedGlobalVars == unchanged); // Repeated visible frames do not append history or mutate time.
    recordWorldGuildStockObservation(state, 129, {second}, items, 2880);
    const auto later = worldGuildStockObservation(state.namedGlobalVars, 129);
    REQUIRE(later);
    CHECK(later->books == std::vector<uint32_t>{second.objectDescriptionId});
    CHECK(state.namedGlobalVars.at(worldGuildStockKey(129) + "slot/1") == 0);
    CHECK(worldGuildBookKnown(state.namedGlobalVars, first.objectDescriptionId));
    recordWorldGuildStockObservation(state, 129, {}, items, 4320);
    const auto empty = worldGuildStockObservation(state.namedGlobalVars, 129);
    REQUIRE(empty);
    CHECK(empty->books.empty());
    CHECK(empty->gameMinutes == 4320);
    CHECK(worldGuildBookKnown(state.namedGlobalVars, second.objectDescriptionId));
    CHECK(state.grantedItems.empty());
    CHECK(state.grantedItemIds.empty());
}

TEST_CASE("guild shelf observations reject malformed saved snapshots and invalid observation times")
{
    const ItemTable &items = guildData().getItemTable();
    const InventoryItem book = guildBook(items, 1);
    EventRuntimeState state;
    for (const float time : {-1.0f, std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN(), float(std::numeric_limits<int32_t>::max())})
        CHECK_THROWS_AS(recordWorldGuildStockObservation(state, 129, {book}, items, time), std::invalid_argument);
    CHECK(state.namedGlobalVars.empty());
    CHECK_THROWS_AS(recordWorldGuildStockObservation(state, 0, {book}, items, 0), std::invalid_argument);
    CHECK_THROWS_AS(recordWorldGuildStockObservation(state, 129,
        std::vector<InventoryItem>(65, book), items, 0), std::invalid_argument);
    recordWorldGuildStockObservation(state, 129, {book}, items, 0);
    const auto original = state.namedGlobalVars;
    const std::string prefix = worldGuildStockKey(129);
    for (const auto &[field, value] : std::vector<std::pair<std::string, int32_t>>{
        {"seen", 2}, {"time", -1}, {"count", -1}, {"count", 65}, {"slot/0", 0}, {"slot/0", -1}})
    {
        state.namedGlobalVars = original;
        state.namedGlobalVars[prefix + field] = value;
        CHECK_THROWS_AS(worldGuildStockObservation(state.namedGlobalVars, 129), std::invalid_argument);
    }
    for (const char *pField : {"time", "count", "slot/0"})
    {
        state.namedGlobalVars = original;
        state.namedGlobalVars.erase(prefix + pField);
        CHECK_THROWS_AS(worldGuildStockObservation(state.namedGlobalVars, 129), std::invalid_argument);
    }
    state.namedGlobalVars = original;
    state.namedGlobalVars[prefix + "count"] = 2;
    state.namedGlobalVars[prefix + "slot/1"] = int32_t(book.objectDescriptionId);
    CHECK_THROWS_AS(worldGuildStockObservation(state.namedGlobalVars, 129), std::invalid_argument);
    state.namedGlobalVars[worldGuildBookKey(book.objectDescriptionId)] = 2;
    CHECK_THROWS_AS(worldGuildBookKnown(state.namedGlobalVars, book.objectDescriptionId), std::invalid_argument);
}

TEST_CASE("guild shelf links distinguish actual observations from possible pools in both worlds")
{
    const auto &data = guildData();
    for (const int spellId : {40, 49, 53, 61, 64, 72, 74})
    {
        const SpellEntry *pSpell = data.getSpellTable().findById(spellId);
        REQUIRE(pSpell);
        CHECK(pSpell->description.find("\"point of no return\"") != std::string::npos);
    }
    const InventoryItem book = guildBook(data.getItemTable(), 1);
    for (const auto &[worldId, houseId] : std::vector<std::pair<std::string, uint32_t>>{
        {"mm7", 129}, {"mm8", 182}})
    {
        WorldDatabase world(data.getMapStats(), data.getHouseTable(), data.getNpcDialogTable(),
            data.getMergedTeacherTopicTable(), worldId);
        WorldKnowledge discoveries;
        WorldGuideSource source;
        source.pItems = &data.getItemTable(); source.pSpells = &data.getSpellTable();
        source.pWorld = &world; source.pWorldKnowledge = &discoveries; source.worldId = worldId;
        WorldGuideKnowledge knowledge;
        knowledge.items.insert(book.objectDescriptionId);
        knowledge.identifiedItems.insert(book.objectDescriptionId);
        const auto entry = [&](bool guide)
        {
            const auto entries = buildWorldGuide(source, WorldGuideSection::Spells, knowledge, guide);
            const auto found = std::find_if(entries.begin(), entries.end(),
                [](const WorldGuideEntry &value) { return value.id == "1"; });
            REQUIRE(found != entries.end());
            return *found;
        };
        CHECK(entry(false).links.size() == 1); // Known book only; no unexplored guild address.
        EventRuntimeState state;
        recordWorldGuildStockObservation(state, houseId, {book}, data.getItemTable(), 1440);
        discoveries.variables = state.namedGlobalVars;
        CHECK(entry(false).links.size() == 1); // Even an observation cannot invent the containing map's address.
        const HouseEntry *pGuild = data.getHouseTable().get(houseId);
        REQUIRE(pGuild);
        const MapStatsEntry *pMap = data.getMapStats().findById(pGuild->mapId);
        REQUIRE(pMap);
        discoveries.visitedMaps.insert(pMap->fileName);
        const auto observed = entry(false);
        const std::string target = worldId + ":house/" + std::to_string(houseId);
        const auto link = std::find_if(observed.links.begin(), observed.links.end(),
            [&](const WorldGuideLink &value) { return value.target == target; });
        REQUIRE(link != observed.links.end());
        CHECK(link->label.starts_with("Last seen:"));
        CHECK(link->label.ends_with("Day 2"));
        CHECK(observed.details.find("actually displayed") != std::string::npos);
        const auto beforeGuide = discoveries.variables;
        CHECK_FALSE(entry(true).links.empty());
        CHECK(discoveries.variables == beforeGuide);
        recordWorldGuildStockObservation(state, houseId, {}, data.getItemTable(), 2880);
        discoveries.variables = state.namedGlobalVars;
        const auto refreshed = entry(false);
        CHECK(std::none_of(refreshed.links.begin(), refreshed.links.end(),
            [](const WorldGuideLink &value) { return value.label.starts_with("Last seen:"); }));
        CHECK(worldGuildBookKnown(discoveries.variables, book.objectDescriptionId));
    }
}
