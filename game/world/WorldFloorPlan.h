#pragma once

#include <cstdint>
#include <vector>
#include <utility>
#include <optional>

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
    double centerX = 0, centerY = 0, pixelsPerWorldUnit = 0;
    std::pair<float, float> worldToUv(float x, float y) const;
    std::pair<float, float> uvToWorld(float u, float v) const;
    std::vector<uint8_t> pixelsBgra;
    std::vector<int32_t> layers;
};

// Height bands are 512 world units wide and centred at integer multiples of 512.
std::optional<int32_t> worldFloorPlanLayer(float z);
bool worldFloorPlanLayerMatches(std::optional<int32_t> layer, float z);
WorldFloorPlanRaster buildWorldFloorPlan(const IndoorMapData &map,
    const WorldFloorPlanKnowledge &knowledge, bool guide,
    const std::vector<std::pair<float, float>> &knownPositions = {},
    std::optional<int32_t> layer = std::nullopt);
} // namespace OpenYAMM::Game
