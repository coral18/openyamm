#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
struct GameplayMinimapState;

struct WorldRegionMapKnowledge
{
    std::vector<uint8_t> fullyRevealedCells;
    std::vector<uint8_t> partiallyRevealedCells;
};

struct WorldPartyMapPosition
{
    std::string mapFileName;
    float x = 0, y = 0, yawRadians = 0;
};

// Same native 88x88 exploration grid as the outdoor Map Book. Empty masks stay hidden.
struct WorldRegionFog
{
    static constexpr int Size = 88;
    std::vector<uint8_t> pixelsBgra;
};

WorldRegionFog buildWorldRegionFog(const WorldRegionMapKnowledge &knowledge,
    const GameplayMinimapState &projection);
} // namespace OpenYAMM::Game
