#pragma once

#include <cstdint>
#include <vector>

namespace OpenYAMM::Game
{
struct ArenaPosition
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
};

struct MapArenaDefinition
{
    // IDs belong to the merged base table; each world's Arena declares its own opponent pool.
    int16_t minimumMonsterId = 0;
    int16_t maximumMonsterId = 0;
    ArenaPosition partyPosition = {};
    int32_t partyDirectionDegrees = 0;
    std::vector<ArenaPosition> monsterPositions;
};
}
