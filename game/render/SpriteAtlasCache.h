#pragma once

#include "engine/AssetFileSystem.h"
#include "engine/SpriteAtlas.h"
#include "game/render/BillboardOpacityMask.h"
#include "game/render/SpriteAtlasCook.h"

#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <algorithm>
#include <map>
#include <cmath>
#include <functional>
#include <unordered_set>

namespace OpenYAMM::Game
{
class SpriteFrameTable;

struct SpriteBillboardTexture
{
    std::string textureName;
    int16_t paletteId = 0;
    float width = 0;
    float height = 0;
    float canvasWidth = 0;
    float canvasHeight = 0;
    int physicalWidth = 0;
    int physicalHeight = 0;
    BillboardOpacityMask opacityMask;
    bgfx::TextureHandle textureHandle = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle maskHandle = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle lookupHandle = BGFX_INVALID_HANDLE;
    std::array<float, 4> atlasRect = {};
    std::array<float, 4> atlasTexel = {};
    std::array<float, 4> chroma = {};
    std::array<float, 4> secondChroma = {};
    std::array<float, 4> thirdChroma = {};
    std::array<float, 4> fourthChroma = {};
    float offsetX = 0;
    float offsetY = 0;
    bool atlas = false;
    bool retained = false;
};

// Coordinates relative to the existing actor's native bottom-anchored logical canvas.
inline bx::Vec3 spriteBillboardCenter(
    float x, float y, float z, const bx::Vec3 &cameraRight, const bx::Vec3 &cameraUp,
    const SpriteBillboardTexture &texture, float scale, bool mirrored)
{
    const float horizontal = texture.offsetX * scale * (mirrored ? -1.0f : 1.0f);
    const float vertical = (texture.height * 0.5f + texture.offsetY) * scale;
    return {x + cameraRight.x * horizontal + cameraUp.x * vertical,
        y + cameraRight.y * horizontal + cameraUp.y * vertical,
        z + cameraRight.z * horizontal + cameraUp.z * vertical};
}

// Atlas outlines are two screen pixels wide, with room for their half-pixel antialiased edge.
// Standalone native bitmaps keep their existing logical-pixel outline geometry.
inline float spriteOutlineWorldPadding(const SpriteBillboardTexture &texture, float spriteScale,
    float viewDepth, float projectionY, int viewportHeight)
{
    if (!texture.atlas)
    {
        return 2.0f * spriteScale;
    }
    const float denominator = std::abs(projectionY) * std::max(viewportHeight, 1);
    return 2.75f * 2.0f * std::abs(viewDepth) / std::max(denominator, 0.001f);
}

class SpriteAtlasCache
{
public:
    struct Statistics
    {
        size_t uploadedPages = 0;
        size_t residentPages = 0;
        size_t residentBytes = 0;
        size_t peakPreparationBytes = 0;
        size_t peakPreparationJobs = 0;
    };
    Statistics statistics() const;
    bool hasProgram() const { return bgfx::isValid(m_program); }
    void beginLevel(const Engine::AssetFileSystem &assets, std::function<void()> progress);
    void setProgram(bgfx::ProgramHandle program);
    void setOutlineProgram(bgfx::ProgramHandle program);
    bool load(const Engine::AssetFileSystem &assets, const std::string &name, int16_t variant,
        SpriteBillboardTexture &texture);
    // Warm every page of packages referenced by this animation during map loading.
    void preload(const Engine::AssetFileSystem &assets, const SpriteFrameTable &frames, uint16_t spriteId);
    void preloadPackage(const Engine::AssetFileSystem &assets, const std::string &name);
    bgfx::ProgramHandle bind(const SpriteBillboardTexture &texture, bgfx::ProgramHandle nativeProgram) const;
    bgfx::ProgramHandle bindOutline(const SpriteBillboardTexture &texture, bgfx::ProgramHandle nativeProgram) const;
    void clear(bool destroyGpu);

private:
    struct Page
    {
        bgfx::TextureHandle base = BGFX_INVALID_HANDLE;
        bgfx::TextureHandle mask = BGFX_INVALID_HANDLE;
        std::unordered_map<std::string, BillboardOpacityMask> opacity;
        std::unordered_map<std::string, std::array<int, 4>> rectangles;
        std::array<int, 2> size = {};
    };
    struct Package
    {
        Engine::SpriteAtlas atlas;
        std::vector<Page> pages;
        std::map<int, bgfx::TextureHandle> lookups;
        bool preloaded = false;
        uint64_t manifestHash = 0;
        std::string root;
        uint64_t lastUse = 0;
        size_t gpuBytes = 0;
    };
    void releasePackage(Package &package, bool destroyGpu);
    void trim(size_t incomingBytes);
    Package &findPackage(const Engine::AssetFileSystem &assets, const std::string &name);
    PreparedSpriteAtlasPage preparePage(const Engine::AssetFileSystem &assets,
        const Package &package, int index, int maxTextureSize) const;
    void publishPage(Package &package, int index, PreparedSpriteAtlasPage prepared);
    std::map<std::string, Package> m_packages;
    std::unordered_set<std::string> m_failed;
    uint64_t m_generation = 0;
    uint64_t m_level = 0;
    size_t m_residentBytes = 0;
    Statistics m_statistics;
    std::function<void()> m_progress;
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_outlineProgram = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_outlineSampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_maskSampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_lookupSampler = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_rectUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_texelUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_chromaUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_secondChromaUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_thirdChromaUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_fourthChromaUniform = BGFX_INVALID_HANDLE;
};
}
