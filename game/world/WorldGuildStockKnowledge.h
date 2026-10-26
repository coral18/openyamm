#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Game
{
struct EventRuntimeState;
struct InventoryItem;
class ItemTable;

struct WorldGuildStockObservation
{
    int32_t gameMinutes = 0;
    std::vector<uint32_t> books;
};

std::string worldGuildStockKey(uint32_t houseId);
std::string worldGuildBookKey(uint32_t itemId);
// Call only for an actually displayed guild shelf, after applying its visible transactions.
void recordWorldGuildStockObservation(EventRuntimeState &state, uint32_t houseId,
    const std::vector<InventoryItem> &stock, const ItemTable &items, float gameMinutes);
std::optional<WorldGuildStockObservation> worldGuildStockObservation(
    const std::unordered_map<std::string, int32_t> &variables, uint32_t houseId);
bool worldGuildBookKnown(const std::unordered_map<std::string, int32_t> &variables, uint32_t itemId);
}
