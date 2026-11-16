#include "game/world/WorldGuildStockKnowledge.h"
#include "game/world/WorldDatabase.h"
#include "game/events/EventRuntime.h"
#include "game/gameplay/GuildSpellbookRules.h"
#include "game/items/ItemRuntime.h"
#include "game/tables/ItemTable.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace OpenYAMM::Game
{
std::string worldGuildStockKey(uint32_t houseId)
{
    if (houseId == 0) throw std::invalid_argument("Invalid observed guild ID");
    return "openyamm.discovery.guild-stock.v1/house/" + std::to_string(houseId) + "/";
}

std::string worldGuildBookKey(uint32_t itemId)
{
    if (itemId == 0) throw std::invalid_argument("Invalid observed guild book ID");
    return "openyamm.discovery.guild-stock.v1/book/" + std::to_string(itemId);
}

void recordWorldGuildStockObservation(EventRuntimeState &state, uint32_t houseId,
    const std::vector<InventoryItem> &stock, const ItemTable &items, float gameMinutes)
{
    if (!std::isfinite(gameMinutes) || gameMinutes < 0
        || double(gameMinutes) > std::numeric_limits<int32_t>::max() || stock.size() > 64)
        throw std::invalid_argument("Invalid observed guild shelf");
    const std::string prefix = worldGuildStockKey(houseId);
    std::vector<uint32_t> books;
    for (const InventoryItem &item : stock)
    {
        const ItemDefinition *pItem = items.get(item.objectDescriptionId);
        if (pItem == nullptr || !spellbookSpellId(*pItem)
            || (!item.identified && ItemRuntime::requiresIdentification(*pItem))) continue;
        if (item.objectDescriptionId > uint32_t(std::numeric_limits<int32_t>::max()))
            throw std::invalid_argument("Observed guild book ID is out of range");
        books.push_back(item.objectDescriptionId);
    }
    std::sort(books.begin(), books.end());
    books.erase(std::unique(books.begin(), books.end()), books.end());
    const int32_t time = int32_t(gameMinutes);
    const auto previous = worldGuildStockObservation(state.namedGlobalVars, houseId);
    if (previous && previous->gameMinutes == time && previous->books == books) return;
    // Explicit zeroes retire old slots through the session's ordinary global-variable merge.
    if (previous)
        for (size_t i = books.size(); i < previous->books.size(); ++i)
            state.namedGlobalVars[prefix + "slot/" + std::to_string(i)] = 0;
    state.namedGlobalVars[prefix + "seen"] = 1;
    state.namedGlobalVars[prefix + "time"] = time;
    state.namedGlobalVars[prefix + "count"] = int32_t(books.size());
    for (size_t i = 0; i < books.size(); ++i)
    {
        state.namedGlobalVars[prefix + "slot/" + std::to_string(i)] = int32_t(books[i]);
        state.namedGlobalVars[worldGuildBookKey(books[i])] = 1;
    }
    state.namedGlobalVars[WorldDatabase::houseDiscoveryKey(houseId)] = 1;
}

std::optional<WorldGuildStockObservation> worldGuildStockObservation(
    const std::unordered_map<std::string, int32_t> &variables, uint32_t houseId)
{
    const std::string prefix = worldGuildStockKey(houseId);
    const auto seen = variables.find(prefix + "seen");
    if (seen == variables.end() || seen->second == 0) return std::nullopt;
    const auto field = [&](const std::string &key)
    {
        const auto value = variables.find(prefix + key);
        if (value == variables.end()) throw std::invalid_argument("Incomplete observed guild shelf");
        return value->second;
    };
    WorldGuildStockObservation result;
    result.gameMinutes = field("time");
    const int32_t count = field("count");
    if (seen->second != 1 || result.gameMinutes < 0 || count < 0 || count > 64)
        throw std::invalid_argument("Invalid saved guild shelf");
    for (int32_t i = 0; i < count; ++i)
    {
        const int32_t itemId = field("slot/" + std::to_string(i));
        if (itemId <= 0 || (!result.books.empty() && uint32_t(itemId) <= result.books.back()))
            throw std::invalid_argument("Invalid saved guild book sequence");
        result.books.push_back(uint32_t(itemId));
    }
    return result;
}

bool worldGuildBookKnown(const std::unordered_map<std::string, int32_t> &variables, uint32_t itemId)
{
    const auto found = variables.find(worldGuildBookKey(itemId));
    if (found == variables.end() || found->second == 0) return false;
    if (found->second != 1) throw std::invalid_argument("Invalid saved guild book discovery");
    return true;
}
}
