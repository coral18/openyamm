#pragma once

#include <cstdint>
#include <vector>
#include <utility>
#include <optional>
#include <compare>
#include <string>

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

struct WorldFloorPlanLayer
{
    // A coarse 512-unit band, optionally one of its four 128-unit subdivisions.
    WorldFloorPlanLayer(int32_t index = 0, uint8_t subdivision = 0)
        : index(index), subdivision(subdivision)
    {
    }
    int32_t index = 0;
    uint8_t subdivision = 0;
    auto operator<=>(const WorldFloorPlanLayer &) const = default;
    std::string name() const;
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
    std::vector<WorldFloorPlanLayer> layers;
    std::unordered_set<int32_t> refinedBands;
    std::optional<WorldFloorPlanLayer> selectedLayer;
    std::optional<WorldFloorPlanLayer> layerAtHeight(float z) const;
};

// Height bands are 512 world units wide and centred at integer multiples of 512.
std::optional<int32_t> worldFloorPlanLayer(float z);
std::optional<WorldFloorPlanLayer> worldFloorPlanLayerForHeight(float z, bool subdivide = false);
std::optional<int32_t> worldFloorPlanLayerElevation(WorldFloorPlanLayer layer);
bool worldFloorPlanLayerMatches(std::optional<WorldFloorPlanLayer> layer, float z);
WorldFloorPlanRaster buildWorldFloorPlan(const IndoorMapData &map,
    const WorldFloorPlanKnowledge &knowledge, bool guide,
    const std::vector<std::pair<float, float>> &knownPositions = {},
    std::optional<WorldFloorPlanLayer> layer = std::nullopt);
} // namespace OpenYAMM::Game
