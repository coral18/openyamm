#pragma once

#include <cstdint>
#include <vector>

#include <unordered_set>

namespace OpenYAMM::Game
{
struct IndoorMapData;

// Read-only projection of the same reveal state used by the dungeon Map Book.
struct WorldFloorPlanKnowledge
{
    std::vector<uint8_t> visibleOutlines;
    std::vector<uint32_t> faceAttributes;
    std::unordered_set<uint32_t> invisibleFaces;
};

struct WorldFloorPlanRaster
{
    int width = 1024;
    int height = 768;
    size_t outlineCount = 0;
    std::vector<uint8_t> pixelsBgra;
};

WorldFloorPlanRaster buildWorldFloorPlan(const IndoorMapData &map,
    const WorldFloorPlanKnowledge &knowledge, bool guide);
} // namespace OpenYAMM::Game
