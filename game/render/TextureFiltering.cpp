#include "game/render/TextureFiltering.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
bool g_textureFilteringEnabled = true;
TextureFilteringConfig g_textureFilteringConfig = {};

bool profileUsesMipmaps(TextureFilterProfile profile)
{
    return profile == TextureFilterProfile::Terrain
        || profile == TextureFilterProfile::BModel
        || profile == TextureFilterProfile::Sky
        || profile == TextureFilterProfile::UiIllustration;
}

bool profileNeedsTransparentEdgeBleed(TextureFilterProfile profile)
{
    switch (profile)
    {
        case TextureFilterProfile::Billboard:
        case TextureFilterProfile::Ui:
        case TextureFilterProfile::UiIllustration:
            return true;

        case TextureFilterProfile::Terrain:
        case TextureFilterProfile::BModel:
        case TextureFilterProfile::Lightmap:
        case TextureFilterProfile::Sky:
        case TextureFilterProfile::Text:
        case TextureFilterProfile::SmoothText:
            return false;
    }

    return false;
}

TextureFilterMode textureFilterModeForProfile(TextureFilterProfile profile)
{
    switch (profile)
    {
        case TextureFilterProfile::Terrain:
            return g_textureFilteringConfig.terrain;

        case TextureFilterProfile::BModel:
            return g_textureFilteringConfig.bmodel;

        case TextureFilterProfile::Lightmap:
            return g_textureFilteringConfig.bmodel;

        case TextureFilterProfile::Sky:
            return g_textureFilteringConfig.sky;

        case TextureFilterProfile::Billboard:
            return g_textureFilteringConfig.billboard;

        case TextureFilterProfile::Ui:
        case TextureFilterProfile::UiIllustration:
            return g_textureFilteringConfig.ui;

        case TextureFilterProfile::Text:
            return g_textureFilteringConfig.text;

        case TextureFilterProfile::SmoothText:
            return TextureFilterMode::Linear;
    }

    return TextureFilterMode::Linear;
}

uint64_t textureFilterSamplerFlagsForMode(TextureFilterMode mode)
{
    switch (mode)
    {
        case TextureFilterMode::Nearest:
            return BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT;

        case TextureFilterMode::Linear:
            return BGFX_SAMPLER_NONE;

        case TextureFilterMode::Anisotropic:
            return BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC;
    }

    return BGFX_SAMPLER_NONE;
}

std::vector<uint8_t> convertBgraToRgba(const uint8_t *pPixels, uint32_t pixelBytes)
{
    std::vector<uint8_t> rgbaPixels(pixelBytes);

    for (uint32_t byteOffset = 0; byteOffset + 3 < pixelBytes; byteOffset += 4)
    {
        rgbaPixels[byteOffset + 0] = pPixels[byteOffset + 2];
        rgbaPixels[byteOffset + 1] = pPixels[byteOffset + 1];
        rgbaPixels[byteOffset + 2] = pPixels[byteOffset + 0];
        rgbaPixels[byteOffset + 3] = pPixels[byteOffset + 3];
    }

    return rgbaPixels;
}

bool hasMixedTransparency(const uint8_t *pPixels, size_t pixelCount)
{
    bool foundTransparent = false;
    bool foundOpaque = false;

    for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
    {
        const uint8_t alpha = pPixels[pixelIndex * 4 + 3];

        if (alpha == 0)
        {
            foundTransparent = true;
        }
        else
        {
            foundOpaque = true;
        }

        if (foundTransparent && foundOpaque)
        {
            return true;
        }
    }

    return false;
}

void bleedTransparentEdgeColors(uint16_t width, uint16_t height, std::vector<uint8_t> &pixels)
{
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);

    if (pixelCount == 0 || pixels.size() < pixelCount * 4)
    {
        return;
    }

    std::vector<int32_t> owner(pixelCount, -1);
    std::vector<uint32_t> queue;
    queue.reserve(pixelCount);

    for (uint32_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
    {
        if (pixels[static_cast<size_t>(pixelIndex) * 4 + 3] != 0)
        {
            owner[pixelIndex] = static_cast<int32_t>(pixelIndex);
            queue.push_back(pixelIndex);
        }
    }

    size_t queueHead = 0;

    while (queueHead < queue.size())
    {
        const uint32_t currentIndex = queue[queueHead++];
        const uint32_t x = currentIndex % width;
        const uint32_t y = currentIndex / width;

        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx == 0 && dy == 0)
                {
                    continue;
                }

                const int neighborX = static_cast<int>(x) + dx;
                const int neighborY = static_cast<int>(y) + dy;

                if (neighborX < 0
                    || neighborY < 0
                    || neighborX >= static_cast<int>(width)
                    || neighborY >= static_cast<int>(height))
                {
                    continue;
                }

                const uint32_t neighborIndex =
                    static_cast<uint32_t>(neighborY) * static_cast<uint32_t>(width) + static_cast<uint32_t>(neighborX);
                const size_t neighborOffset = static_cast<size_t>(neighborIndex) * 4;

                if (pixels[neighborOffset + 3] != 0 || owner[neighborIndex] != -1)
                {
                    continue;
                }

                owner[neighborIndex] = owner[currentIndex];
                queue.push_back(neighborIndex);
            }
        }
    }

    for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
    {
        const size_t pixelOffset = pixelIndex * 4;

        if (pixels[pixelOffset + 3] != 0)
        {
            continue;
        }

        const int32_t sourceIndex = owner[pixelIndex];

        if (sourceIndex < 0)
        {
            continue;
        }

        const size_t sourceOffset = static_cast<size_t>(sourceIndex) * 4;
        pixels[pixelOffset + 0] = pixels[sourceOffset + 0];
        pixels[pixelOffset + 1] = pixels[sourceOffset + 1];
        pixels[pixelOffset + 2] = pixels[sourceOffset + 2];
    }
}

bgfx::TextureHandle createBgraTextureWithMipChain(
    uint16_t width,
    uint16_t height,
    const uint8_t *pPixels,
    uint32_t pixelBytes,
    TextureFilterProfile profile,
    uint64_t extraFlags)
{
    if (pPixels == nullptr || pixelBytes == 0)
    {
        return BGFX_INVALID_HANDLE;
    }

    const std::vector<BgraMipLevel> mipLevels = prepareBgraMipChain(width, height, std::span<const uint8_t>(pPixels, pixelBytes));

    if (mipLevels.empty())
    {
        return BGFX_INVALID_HANDLE;
    }

    const bgfx::TextureHandle textureHandle = bgfx::createTexture2D(
        width,
        height,
        true,
        1,
        bgraTextureUploadFormat(),
        textureFilterSamplerFlags(profile) | extraFlags);

    if (!bgfx::isValid(textureHandle))
    {
        return BGFX_INVALID_HANDLE;
    }

    for (uint8_t mipLevelIndex = 0; mipLevelIndex < mipLevels.size(); ++mipLevelIndex)
    {
        const BgraMipLevel &mipLevel = mipLevels[mipLevelIndex];
        bgfx::updateTexture2D(
            textureHandle,
            0,
            mipLevelIndex,
            0,
            0,
            mipLevel.width,
            mipLevel.height,
            copyBgraTextureUploadMemory(mipLevel.pixels.data(), static_cast<uint32_t>(mipLevel.pixels.size())));
    }

    return textureHandle;
}
}

uint64_t textureFilterSamplerFlags(TextureFilterProfile profile)
{
    return textureFilterSamplerFlagsForMode(textureFilterModeForProfile(profile));
}

bool textureFilteringEnabled()
{
    return g_textureFilteringEnabled;
}

void setTextureFilteringEnabled(bool enabled)
{
    g_textureFilteringEnabled = enabled;
    g_textureFilteringConfig.enabled = enabled;
}

void setTextureFilteringConfig(const TextureFilteringConfig &config)
{
    g_textureFilteringConfig = config;
    g_textureFilteringEnabled = config.enabled;
}

bool toggleTextureFilteringEnabled()
{
    g_textureFilteringEnabled = !g_textureFilteringEnabled;
    g_textureFilteringConfig.enabled = g_textureFilteringEnabled;
    return g_textureFilteringEnabled;
}

uint32_t textureBindingSamplerFlags(TextureFilterProfile profile, uint32_t extraFlags)
{
    uint64_t profileFlags = textureFilterSamplerFlags(profile);

    if (!g_textureFilteringEnabled)
    {
        profileFlags = BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT;
    }

    return uint32_t(profileFlags) | extraFlags;
}

bgfx::TextureFormat::Enum bgraTextureUploadFormat()
{
#if defined(__ANDROID__)
    return bgfx::TextureFormat::RGBA8;
#else
    return bgfx::TextureFormat::BGRA8;
#endif
}

const bgfx::Memory *copyBgraTextureUploadMemory(const uint8_t *pPixels, uint32_t pixelBytes)
{
    if (pPixels == nullptr || pixelBytes == 0)
    {
        return nullptr;
    }

#if defined(__ANDROID__)
    const std::vector<uint8_t> rgbaPixels = convertBgraToRgba(pPixels, pixelBytes);
    return bgfx::copy(rgbaPixels.data(), static_cast<uint32_t>(rgbaPixels.size()));
#else
    return bgfx::copy(pPixels, pixelBytes);
#endif
}

bool prepareBgraTexturePixelsForUploadInPlace(
    uint16_t width,
    uint16_t height,
    std::vector<uint8_t> &pixels,
    TextureFilterProfile profile)
{
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t expectedPixelBytes = pixelCount * 4;

    if (!profileNeedsTransparentEdgeBleed(profile)
        || pixelCount == 0
        || pixels.size() < expectedPixelBytes
        || !hasMixedTransparency(pixels.data(), pixelCount))
    {
        return false;
    }

    bleedTransparentEdgeColors(width, height, pixels);
    return true;
}

void bindTexture(
    uint8_t stage,
    bgfx::UniformHandle sampler,
    bgfx::TextureHandle textureHandle,
    TextureFilterProfile profile,
    uint32_t extraFlags)
{
    bgfx::setTexture(stage, sampler, textureHandle, textureBindingSamplerFlags(profile, extraFlags));
}

bgfx::TextureHandle createBgraTexture2D(
    uint16_t width,
    uint16_t height,
    const uint8_t *pPixels,
    uint32_t pixelBytes,
    TextureFilterProfile profile,
    uint64_t extraFlags,
    BgraTexturePixelPreparation pixelPreparation)
{
    if (pPixels == nullptr || pixelBytes == 0)
    {
        return BGFX_INVALID_HANDLE;
    }

    std::vector<uint8_t> uploadPixels;

    if (pixelPreparation == BgraTexturePixelPreparation::Required
        && profileNeedsTransparentEdgeBleed(profile))
    {
        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);

        if (pixelBytes >= pixelCount * 4 && hasMixedTransparency(pPixels, pixelCount))
        {
            uploadPixels.assign(pPixels, pPixels + pixelBytes);
            bleedTransparentEdgeColors(width, height, uploadPixels);
        }
    }

    const uint8_t *pUploadPixels = uploadPixels.empty() ? pPixels : uploadPixels.data();

    if (profileUsesMipmaps(profile))
    {
        const bgfx::TextureHandle mipTextureHandle =
            createBgraTextureWithMipChain(width, height, pUploadPixels, pixelBytes, profile, extraFlags);

        if (bgfx::isValid(mipTextureHandle))
        {
            return mipTextureHandle;
        }
    }

    return bgfx::createTexture2D(
        width,
        height,
        false,
        1,
        bgraTextureUploadFormat(),
        textureFilterSamplerFlags(profile) | extraFlags,
        copyBgraTextureUploadMemory(pUploadPixels, pixelBytes));
}

void updateBgraTextureArrayLayer(
    bgfx::TextureHandle texture, uint16_t layer, const std::vector<BgraMipLevel> &levels)
{
    if (!bgfx::isValid(texture))
    {
        return;
    }
    for (uint8_t index = 0; index < levels.size(); ++index)
    {
        const BgraMipLevel &level = levels[index];
        bgfx::updateTexture2D(texture, layer, index, 0, 0, level.width, level.height,
            copyBgraTextureUploadMemory(level.pixels.data(), level.pixels.size()));
    }
}

void updateBgraTextureArrayLayer(
    bgfx::TextureHandle texture,
    uint16_t layer,
    uint16_t width,
    uint16_t height,
    const std::vector<uint8_t> &pixels,
    uint8_t alphaCutoff)
{
    if (bgfx::isValid(texture))
    {
        updateBgraTextureArrayLayer(texture, layer, prepareBgraMipChain(width, height, pixels, alphaCutoff));
    }
}

bgfx::TextureHandle createEmptyBgraTexture2D(
    uint16_t width,
    uint16_t height,
    TextureFilterProfile profile,
    uint64_t extraFlags)
{
    return bgfx::createTexture2D(
        width,
        height,
        false,
        1,
        bgraTextureUploadFormat(),
        textureFilterSamplerFlags(profile) | extraFlags);
}
}
