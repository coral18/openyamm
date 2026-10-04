#pragma once

#include "engine/ImageAssetLoader.h"
#include "game/render/BillboardOpacityMask.h"
#include "game/render/SpriteAtlasMipmaps.h"

#include <span>
#include <bgfx/bgfx.h>

namespace OpenYAMM::Game
{
constexpr uint32_t SpriteAtlasCookVersion = 2;
constexpr int SpriteAtlasCookTextureLimit = 4096;
constexpr size_t SpriteAtlasCookHeaderBytes = 72;
constexpr size_t SpriteAtlasCookByteLimit = 256 * 1024 * 1024;

// Stable on-disk ids; never serialize bgfx's enum ordinals.
enum class SpriteAtlasCodec : uint64_t
{
    Bc7 = 1,
    Bc4 = 2,
    Bc5 = 3,
    Etc2Rgba = 4,
    EacR = 5,
    EacRg = 6
};

struct SpriteAtlasSourcePage
{
    SpriteAtlasMipPage mips;
    std::unordered_map<std::string, BillboardOpacityMask> opacity;
};

struct SpriteAtlasTextureLevel
{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> baseBlocks;
    std::vector<uint8_t> maskBlocks;
};

struct PreparedSpriteAtlasPage
{
    SpriteAtlasCodec baseCodec = SpriteAtlasCodec::Bc7;
    SpriteAtlasCodec maskCodec = SpriteAtlasCodec::Bc4;
    std::vector<SpriteAtlasTextureLevel> levels;
    std::unordered_map<std::string, std::array<int, 4>> rectangles;
    std::unordered_map<std::string, BillboardOpacityMask> opacity;
};

bgfx::TextureFormat::Enum spriteAtlasTextureFormat(SpriteAtlasCodec codec);
size_t spriteAtlasBlockBytes(SpriteAtlasCodec codec, int width, int height);
// Peak compressed input + decompression buffer + parsed page; bounded before scheduling worker jobs.
size_t cookedSpriteAtlasPreparationBytes(std::span<const uint8_t> header, size_t fileBytes);
uint64_t spriteAtlasContentHash(std::span<const uint8_t> bytes);
std::string cookedSpriteAtlasPageName(int pageIndex);
SpriteAtlasSourcePage prepareSpriteAtlasPage(const Engine::SpriteAtlas &atlas, int pageIndex,
    const Engine::ImagePixelsBgra &base, const Engine::ImagePixelsBgra &mask, int maxTextureSize);
std::vector<uint8_t> encodeCookedSpriteAtlasPage(const Engine::SpriteAtlas &atlas, int pageIndex,
    uint64_t manifestHash, const PreparedSpriteAtlasPage &page);
// Throws on stale, truncated, corrupt or incompatible data. Never silently rebuilds stale runtime assets.
PreparedSpriteAtlasPage decodeCookedSpriteAtlasPage(std::span<const uint8_t> bytes,
    const Engine::SpriteAtlas &atlas, int pageIndex, uint64_t manifestHash, int maxTextureSize);
}
