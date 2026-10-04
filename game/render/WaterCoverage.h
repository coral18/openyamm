#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace OpenYAMM::Game
{
constexpr uint16_t WaterCoverageSize = 128;

// The native shoreline overlay contains land alpha over an animated water base.
// Preserve that authoring mask independently of the composited colour/animation.
inline std::vector<uint8_t> waterCoverageFromOverlay(std::span<const uint8_t> bgra, int width, int height,
    uint16_t size = WaterCoverageSize)
{
    if (width <= 0 || height <= 0 || size == 0 || bgra.size() != size_t(width) * size_t(height) * 4)
    {
        throw std::invalid_argument("Invalid water shoreline overlay dimensions or pixel data");
    }
    std::vector<uint8_t> result(size_t(size) * size);
    for (int y = 0; y < size; ++y)
    {
        const int y0 = y * height / size;
        const int y1 = std::min(height, std::max(y0 + 1, (y + 1) * height / size));
        for (int x = 0; x < size; ++x)
        {
            const int x0 = x * width / size;
            const int x1 = std::min(width, std::max(x0 + 1, (x + 1) * width / size));
            uint32_t sum = 0;
            uint32_t count = 0;
            for (int sy = y0; sy < y1; ++sy)
            {
                for (int sx = x0; sx < x1; ++sx)
                {
                    sum += 255 - bgra[(size_t(sy) * width + sx) * 4 + 3];
                    ++count;
                }
            }
            result[size_t(y) * size + x] = uint8_t((sum + count / 2) / count);
        }
    }
    return result;
}
}
