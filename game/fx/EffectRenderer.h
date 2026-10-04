#pragma once

#include "engine/ImageAssetLoader.h"

#include <bgfx/bgfx.h>

#include <cstddef>
#include <string>
#include <unordered_map>

namespace bx
{
struct Vec3;
}

namespace OpenYAMM::Engine
{
class AssetFileSystem;
}

namespace OpenYAMM::Game
{
class EffectResourceLibrary;
class EffectSystem;
class WorldFxRenderResources;

class EffectRenderer
{
public:
    struct Diagnostics
    {
        size_t sourceParticles = 0;
        size_t sourceFixedSprites = 0;
        size_t visibleQuads = 0;
        size_t drawCalls = 0;
        size_t textureSwitches = 0;
        size_t loadedTextures = 0;
        size_t estimatedTextureBytes = 0;
        double worldQuadArea = 0.0;
    };

    struct Texture
    {
        bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
        int width = 0;
        int height = 0;
    };

    void render(
        WorldFxRenderResources &resources,
        const EffectSystem &effects,
        const EffectResourceLibrary &effectResources,
        const Engine::AssetFileSystem &assetFileSystem,
        uint16_t viewId,
        const float *pViewMatrix,
        const bx::Vec3 &cameraPosition);
    void shutdown(bool destroyGpu);
    void preload(const EffectSystem &effects, const EffectResourceLibrary &effectResources,
        const Engine::AssetFileSystem &assetFileSystem);
    const Diagnostics &diagnostics() const;

private:
    const Texture *loadTexture(const Engine::AssetFileSystem &assetFileSystem, const std::string &path);

    std::unordered_map<std::string, Texture> m_textures;
    Engine::BinaryAssetCache m_binaryAssets;
    Diagnostics m_diagnostics;
};
}
