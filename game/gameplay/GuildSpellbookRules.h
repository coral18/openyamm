#pragma once

#include <cstdint>
#include <optional>

namespace OpenYAMM::Game
{
struct HouseEntry;
struct ItemDefinition;

std::optional<uint32_t> spellbookSpellId(const ItemDefinition &item);
// Positive weight means a candidate for the guild's random pool, not guaranteed current stock.
int guildSpellbookStockWeight(const HouseEntry &house, const ItemDefinition &item);
}
