#include "engine/ImageMipmaps.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace OpenYAMM::Engine
{
float srgbToLinear(float value)
{
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

float linearToSrgb(float value)
{
    value = std::max(value, 0.0f);
    return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

void preserveBgraCutoutCoverage(
    std::vector<uint8_t> &pixels, std::span<const uint8_t> referencePixels, uint8_t alphaCutoff)
{
    if (alphaCutoff == 0 || pixels.empty() || referencePixels.empty() ||
        pixels.size() % 4 != 0 || referencePixels.size() % 4 != 0)
    {
        return;
    }
    uint64_t referenceCovered = 0;
    for (size_t i = 3; i < referencePixels.size(); i += 4)
    {
        referenceCovered += referencePixels[i] >= alphaCutoff ? 1 : 0;
    }
    std::array<uint64_t, 256> histogram = {};
    for (size_t i = 3; i < pixels.size(); i += 4)
    {
        ++histogram[pixels[i]];
    }
    const uint64_t referenceCount = referencePixels.size() / 4;
    const uint64_t desired = referenceCovered * (pixels.size() / 4);
    const auto errorFor = [&](uint64_t covered)
    {
        const uint64_t actual = covered * referenceCount;
        return actual > desired ? actual - desired : desired - actual;
    };
    uint64_t covered = 0;
    for (int alpha = alphaCutoff; alpha <= 255; ++alpha)
    {
        covered += histogram[alpha];
    }
    uint64_t bestError = errorFor(covered);
    int bestThreshold = alphaCutoff;
    if (errorFor(0) < bestError)
    {
        bestError = errorFor(0);
        bestThreshold = 256;
    }
    covered = 0;
    for (int alpha = 255; alpha >= 1; --alpha)
    {
        covered += histogram[alpha];
        const uint64_t error = errorFor(covered);
        if (error < bestError)
        {
            bestError = error;
            bestThreshold = alpha;
        }
    }
    if (bestThreshold == alphaCutoff)
    {
        return;
    }
    const float scale = (float(alphaCutoff) - 0.5f) / (float(bestThreshold) - 0.5f);
    for (size_t i = 3; i < pixels.size(); i += 4)
    {
        pixels[i] = uint8_t(std::clamp(std::lround(pixels[i] * scale), 0L, 255L));
    }
}

std::vector<BgraMipLevel> prepareBgraMipChain(
    uint16_t width, uint16_t height, std::span<const uint8_t> pixels, uint8_t alphaCutoff,
    ImageMipSemantic semantic)
{
    const size_t bytes = size_t(width) * height * 4;
    if (width == 0 || height == 0 || pixels.size() < bytes)
    {
        return {};
    }
    const std::array<float, 256> srgbValues = [semantic]()
    {
        std::array<float, 256> values = {};
        if (semantic == ImageMipSemantic::Srgb)
        {
            for (size_t index = 0; index < values.size(); ++index)
            {
                values[index] = srgbToLinear(float(index) / 255.0f);
            }
        }
        return values;
    }();
    std::vector<BgraMipLevel> levels;
    levels.push_back({width, height, {pixels.begin(), pixels.begin() + bytes}});
    while (levels.back().width > 1 || levels.back().height > 1)
    {
        const BgraMipLevel &source = levels.back();
        BgraMipLevel target;
        target.width = std::max<uint16_t>(1, source.width / 2);
        target.height = std::max<uint16_t>(1, source.height / 2);
        target.pixels.resize(size_t(target.width) * target.height * 4);
        for (uint16_t y = 0; y < target.height; ++y)
        {
            for (uint16_t x = 0; x < target.width; ++x)
            {
                // Preserve legacy display-space averaging for existing world/UI consumers.
                // Material mips include every texel of non-power-of-two images.
                const bool legacy = semantic == ImageMipSemantic::Linear;
                const uint16_t beginX = legacy ? x * 2 : uint32_t(x) * source.width / target.width;
                const uint16_t beginY = legacy ? y * 2 : uint32_t(y) * source.height / target.height;
                const uint16_t endX = legacy ? beginX + 2 : uint32_t(x + 1) * source.width / target.width;
                const uint16_t endY = legacy ? beginY + 2 : uint32_t(y + 1) * source.height / target.height;
                std::array<float, 4> sum = {};
                uint32_t count = 0;
                for (uint16_t sy = beginY; sy < endY; ++sy)
                {
                    for (uint16_t sx = beginX; sx < endX; ++sx)
                    {
                        const size_t offset = (size_t(std::min<uint16_t>(sy, source.height - 1)) * source.width
                            + std::min<uint16_t>(sx, source.width - 1)) * 4;
                        for (size_t channel = 0; channel < 4; ++channel)
                        {
                            float value = source.pixels[offset + channel] / (legacy ? 1.0f : 255.0f);
                            if (channel < 3 && semantic == ImageMipSemantic::Srgb)
                            {
                                value = srgbValues[source.pixels[offset + channel]];
                            }
                            else if (channel < 3 && semantic == ImageMipSemantic::Normal)
                            {
                                value = value * 2.0f - 1.0f;
                            }
                            sum[channel] += value;
                        }
                        ++count;
                    }
                }
                for (float &value : sum)
                {
                    value /= count;
                }
                if (semantic == ImageMipSemantic::Normal)
                {
                    const float length = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
                    if (length < 1.0e-6f)
                    {
                        sum[0] = 1.0f;
                        sum[1] = sum[2] = 0.0f;
                    }
                    else
                    {
                        for (size_t channel = 0; channel < 3; ++channel)
                        {
                            sum[channel] /= length;
                        }
                    }
                }
                const size_t offset = (size_t(y) * target.width + x) * 4;
                for (size_t channel = 0; channel < 4; ++channel)
                {
                    float value = sum[channel];
                    if (channel < 3 && semantic == ImageMipSemantic::Srgb)
                    {
                        value = linearToSrgb(value);
                    }
                    else if (channel < 3 && semantic == ImageMipSemantic::Normal)
                    {
                        value = value * 0.5f + 0.5f;
                    }
                    target.pixels[offset + channel] =
                        uint8_t(std::clamp(std::lround(value * (legacy ? 1.0f : 255.0f)), 0L, 255L));
                }
            }
        }
        if (alphaCutoff > 0)
        {
            preserveBgraCutoutCoverage(target.pixels, pixels.first(bytes), alphaCutoff);
        }
        levels.push_back(std::move(target));
    }
    return levels;
}
}
