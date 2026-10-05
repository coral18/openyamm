#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace OpenYAMM::Engine
{
enum class ImageMipSemantic
{
    Linear,
    Srgb,
    Normal,
};

struct BgraMipLevel
{
    uint16_t width = 0;
    uint16_t height = 0;
    std::vector<uint8_t> pixels;
};

float srgbToLinear(float value);
float linearToSrgb(float value);
void preserveBgraCutoutCoverage(
    std::vector<uint8_t> &pixels, std::span<const uint8_t> referencePixels, uint8_t alphaCutoff);
std::vector<BgraMipLevel> prepareBgraMipChain(
    uint16_t width, uint16_t height, std::span<const uint8_t> pixels, uint8_t alphaCutoff = 0,
    ImageMipSemantic semantic = ImageMipSemantic::Linear);
}
