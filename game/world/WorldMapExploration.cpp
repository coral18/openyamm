#include "game/world/WorldMapExploration.h"
#include "game/ui/GameplayMinimapTransform.h"

#include <cmath>

namespace OpenYAMM::Game
{
namespace
{
bool revealed(const std::vector<uint8_t> &bits, size_t index)
{
    return index / 8 < bits.size() && (bits[index / 8] & (1u << (7u - index % 8))) != 0;
}
}

bool worldRegionPointExplored(const WorldRegionMapKnowledge &knowledge, float worldX, float worldY)
{
    if (!std::isfinite(worldX) || !std::isfinite(worldY)
        || worldX < -32768 || worldX > 32768 || worldY < -32768 || worldY > 32768)
        return false;
    const int x = std::min(int((worldX + 32768) / 65536 * WorldRegionFog::Size), WorldRegionFog::Size - 1);
    const int y = std::min(int((32768 - worldY) / 65536 * WorldRegionFog::Size), WorldRegionFog::Size - 1);
    return revealed(knowledge.fullyRevealedCells, size_t(y * WorldRegionFog::Size + x));
}

WorldRegionFog buildWorldRegionFog(const WorldRegionMapKnowledge &knowledge,
    const GameplayMinimapState &projection)
{
    WorldRegionFog fog;
    fog.pixelsBgra.resize(WorldRegionFog::Size * WorldRegionFog::Size * 4, 0);
    for (int y = 0; y < WorldRegionFog::Size; ++y)
        for (int x = 0; x < WorldRegionFog::Size; ++x)
        {
            const auto uv = gameplayMinimapUvToOutdoorRevealUv(projection,
                (x + 0.5f) / WorldRegionFog::Size, (y + 0.5f) / WorldRegionFog::Size);
            const size_t index = size_t(int(std::floor(uv.y * WorldRegionFog::Size)) * WorldRegionFog::Size
                                     + int(std::floor(uv.x * WorldRegionFog::Size)));
            const size_t pixel = size_t(y * WorldRegionFog::Size + x) * 4;
            fog.pixelsBgra[pixel + 3] = revealed(knowledge.fullyRevealedCells, index) ? 0
                : revealed(knowledge.partiallyRevealedCells, index) ? 144 : 255;
        }
    return fog;
}
} // namespace OpenYAMM::Game
