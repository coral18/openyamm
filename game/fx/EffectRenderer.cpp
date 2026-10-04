#include "game/fx/EffectRenderer.h"

#include "engine/AssetFileSystem.h"
#include "engine/BgfxContext.h"
#include "game/fx/EffectSystem.h"
#include "game/fx/WorldFxRenderResources.h"
#include "game/fx/import/EffectResourceLibrary.h"
#include "game/render/TextureFiltering.h"

#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace OpenYAMM::Game
{
namespace
{
constexpr uint64_t AlphaRenderState =
    BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_BLEND_ALPHA;
constexpr uint64_t AdditiveRenderState =
    BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_BLEND_ADD;
constexpr uint64_t MultiplyRenderState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
    BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_DST_COLOR, BGFX_STATE_BLEND_ZERO);
constexpr float CullDistance = 12000.0f;

struct DrawParticle
{
    std::array<float, 3> position = {};
    std::array<float, 4> color = {};
    float scale = 1.0f;
    float stretchU = 1.0f;
    float stretchV = 1.0f;
    float spriteAnimationSeconds = 0.0f;
    EffectBlendMode blendMode = EffectBlendMode::Alpha;
    EffectSpriteAlignment alignment = EffectSpriteAlignment::CameraFacing;
    std::array<float, 3> planeRight = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> planeUp = {0.0f, 0.0f, 1.0f};
    std::string spriteResource;
    const EffectRenderer::Texture *pTexture = nullptr;
    std::string texturePath;
    float logicalWidth = 0.0f;
    float logicalHeight = 0.0f;
    float depth = 0.0f;
};

uint8_t colorChannel(float value)
{
    return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

uint32_t particleColor(const DrawParticle &particle)
{
    return static_cast<uint32_t>(colorChannel(particle.color[0])) |
        (static_cast<uint32_t>(colorChannel(particle.color[1])) << 8) |
        (static_cast<uint32_t>(colorChannel(particle.color[2])) << 16) |
        (static_cast<uint32_t>(colorChannel(particle.color[3])) << 24);
}

uint64_t renderState(EffectBlendMode blendMode)
{
    switch (blendMode)
    {
    case EffectBlendMode::Alpha:
        return AlphaRenderState;
    case EffectBlendMode::Additive:
        return AdditiveRenderState;
    case EffectBlendMode::Multiply:
        return MultiplyRenderState;
    }
    return AlphaRenderState;
}

bx::Vec3 normalizedOrFallback(const bx::Vec3 &value, const bx::Vec3 &fallback)
{
    const float lengthSquared = bx::dot(value, value);
    return lengthSquared > 1.0e-12f ? bx::mul(value, 1.0f / std::sqrt(lengthSquared)) : fallback;
}

void writeQuad(
    std::vector<WorldFxParticleVertex> &vertices,
    const DrawParticle &particle,
    const bx::Vec3 &cameraRight,
    const bx::Vec3 &cameraUp)
{
    const bx::Vec3 center = {particle.position[0], particle.position[1], particle.position[2]};
    const bx::Vec3 rightDirection = particle.alignment == EffectSpriteAlignment::WorldPlane
        ? normalizedOrFallback(
            {particle.planeRight[0], particle.planeRight[1], particle.planeRight[2]},
            cameraRight)
        : cameraRight;
    const bx::Vec3 upDirection = particle.alignment == EffectSpriteAlignment::WorldPlane
        ? normalizedOrFallback({particle.planeUp[0], particle.planeUp[1], particle.planeUp[2]}, cameraUp)
        : cameraUp;
    const bx::Vec3 right = bx::mul(
        rightDirection,
        particle.logicalWidth * particle.scale * particle.stretchU * 0.5f);
    const bx::Vec3 up = bx::mul(
        upDirection,
        particle.logicalHeight * particle.scale * particle.stretchV * 0.5f);
    const uint32_t color = particleColor(particle);
    vertices.push_back({center.x - right.x - up.x, center.y - right.y - up.y,
        center.z - right.z - up.z, 0.0f, 1.0f, color});
    vertices.push_back({center.x - right.x + up.x, center.y - right.y + up.y,
        center.z - right.z + up.z, 0.0f, 0.0f, color});
    vertices.push_back({center.x + right.x + up.x, center.y + right.y + up.y,
        center.z + right.z + up.z, 1.0f, 0.0f, color});
    vertices.push_back(vertices[vertices.size() - 3]);
    vertices.push_back(vertices[vertices.size() - 2]);
    vertices.push_back({center.x + right.x - up.x, center.y + right.y - up.y,
        center.z + right.z - up.z, 1.0f, 1.0f, color});
}

void submitBatch(
    WorldFxRenderResources &resources,
    const EffectRenderer::Texture &texture,
    const std::vector<WorldFxParticleVertex> &vertices,
    EffectBlendMode blendMode,
    uint16_t viewId)
{
    if (vertices.empty() ||
        bgfx::getAvailTransientVertexBuffer(
            static_cast<uint32_t>(vertices.size()),
            WorldFxParticleVertex::ms_layout) < vertices.size())
    {
        return;
    }
    bgfx::TransientVertexBuffer vertexBuffer = {};
    bgfx::allocTransientVertexBuffer(
        &vertexBuffer,
        static_cast<uint32_t>(vertices.size()),
        WorldFxParticleVertex::ms_layout);
    std::memcpy(vertexBuffer.data, vertices.data(), vertices.size() * sizeof(WorldFxParticleVertex));
    const float particleParams[4] = {
        blendMode == EffectBlendMode::Additive ? 1.0f : 0.0f,
        0.0f,
        0.0f,
        0.0f,
    };
    float identity[16] = {};
    bx::mtxIdentity(identity);
    bgfx::setTransform(identity);
    bgfx::setVertexBuffer(0, &vertexBuffer, 0, static_cast<uint32_t>(vertices.size()));
    bindTexture(
        0,
        resources.textureSamplerHandle(),
        texture.handle,
        TextureFilterProfile::Billboard,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    bgfx::setUniform(resources.particleParamsUniformHandle(), particleParams);
    bgfx::setState(renderState(blendMode));
    bgfx::submit(viewId, resources.particleProgramHandle());
}
}

void EffectRenderer::render(
    WorldFxRenderResources &resources,
    const EffectSystem &effects,
    const EffectResourceLibrary &effectResources,
    const Engine::AssetFileSystem &assetFileSystem,
    uint16_t viewId,
    const float *pViewMatrix,
    const bx::Vec3 &cameraPosition)
{
    m_diagnostics = {};
    m_diagnostics.sourceParticles = effects.particles().size();
    m_diagnostics.sourceFixedSprites = effects.fixedSprites().size();
    const auto recordTextureMemory = [this]()
    {
        m_diagnostics.loadedTextures = m_textures.size();
        m_diagnostics.estimatedTextureBytes = 0;
        for (const auto &[path, texture] : m_textures)
        {
            (void)path;
            m_diagnostics.estimatedTextureBytes +=
                static_cast<size_t>(texture.width) * static_cast<size_t>(texture.height) * 4u * 4u / 3u;
        }
    };
    if (!resources.isReady() || (effects.particles().empty() && effects.fixedSprites().empty()))
    {
        recordTextureMemory();
        return;
    }
    const bx::Vec3 cameraRight = {pViewMatrix[0], pViewMatrix[4], pViewMatrix[8]};
    const bx::Vec3 cameraUp = {pViewMatrix[1], pViewMatrix[5], pViewMatrix[9]};
    const bx::Vec3 cameraForward = bx::mul(bx::cross(cameraRight, cameraUp), -1.0f);
    std::vector<DrawParticle> draws;
    draws.reserve(effects.particles().size() + effects.fixedSprites().size());
    const auto addDraw = [&](const std::string &spriteResource,
                             const std::array<float, 3> &position,
                             const std::array<float, 4> &color,
                             float scale,
                             float stretchU,
                             float stretchV,
                             float spriteAnimationSeconds,
                             EffectBlendMode blendMode,
                             EffectSpriteAlignment alignment,
                             const std::array<float, 3> &planeRight,
                             const std::array<float, 3> &planeUp)
    {
        const EffectSpriteResource *pSprite = effectResources.findSprite(spriteResource);
        if (pSprite == nullptr || pSprite->framePaths.empty())
        {
            return;
        }
        const size_t frameIndex = static_cast<size_t>(
            spriteAnimationSeconds * static_cast<float>(pSprite->framesPerSecond)) %
            pSprite->framePaths.size();
        const std::string &texturePath = pSprite->framePaths[frameIndex];
        const Texture *pTexture = loadTexture(assetFileSystem, texturePath);
        if (pTexture == nullptr)
        {
            return;
        }
        const bx::Vec3 offset = {
            position[0] - cameraPosition.x,
            position[1] - cameraPosition.y,
            position[2] - cameraPosition.z,
        };
        const float distanceSquared = bx::dot(offset, offset);
        const float depth = bx::dot(offset, cameraForward);
        if (distanceSquared > CullDistance * CullDistance || depth <= 0.0f)
        {
            return;
        }
        draws.push_back({
            .position = position,
            .color = color,
            .scale = scale,
            .stretchU = stretchU,
            .stretchV = stretchV,
            .spriteAnimationSeconds = spriteAnimationSeconds,
            .blendMode = blendMode,
            .alignment = alignment,
            .planeRight = planeRight,
            .planeUp = planeUp,
            .spriteResource = spriteResource,
            .pTexture = pTexture,
            .texturePath = texturePath,
            .logicalWidth = static_cast<float>(pSprite->logicalWidth),
            .logicalHeight = static_cast<float>(pSprite->logicalHeight),
            .depth = depth,
        });
        m_diagnostics.worldQuadArea += static_cast<double>(pSprite->logicalWidth) * pSprite->logicalHeight
            * scale * scale * std::abs(stretchU * stretchV);
    };
    for (const EffectParticle &particle : effects.particles())
    {
        addDraw(
            particle.spriteResource,
            particle.position,
            particle.color,
            particle.scale,
            particle.stretchU,
            particle.stretchV,
            particle.spriteAnimationSeconds,
            particle.blendMode,
            particle.alignment,
            particle.planeRight,
            particle.planeUp);
    }
    for (const EffectFixedSprite &sprite : effects.fixedSprites())
    {
        addDraw(
            sprite.spriteResource,
            sprite.position,
            sprite.color,
            sprite.scale,
            sprite.stretchU,
            sprite.stretchV,
            sprite.spriteAnimationSeconds,
            sprite.blendMode,
            sprite.alignment,
            sprite.planeRight,
            sprite.planeUp);
    }
    std::stable_sort(
        draws.begin(),
        draws.end(),
        [](const DrawParticle &left, const DrawParticle &right)
        {
            return left.depth > right.depth;
        });

    std::vector<WorldFxParticleVertex> vertices;
    m_diagnostics.visibleQuads = draws.size();
    size_t drawIndex = 0;
    while (drawIndex < draws.size())
    {
        const DrawParticle &first = draws[drawIndex];
        vertices.clear();
        size_t batchEnd = drawIndex;
        while (batchEnd < draws.size() && draws[batchEnd].texturePath == first.texturePath &&
            draws[batchEnd].blendMode == first.blendMode)
        {
            writeQuad(vertices, draws[batchEnd], cameraRight, cameraUp);
            ++batchEnd;
        }
        submitBatch(resources, *first.pTexture, vertices, first.blendMode, viewId);
        ++m_diagnostics.drawCalls;
        drawIndex = batchEnd;
    }
    m_diagnostics.textureSwitches = m_diagnostics.drawCalls;
    recordTextureMemory();
}

void EffectRenderer::shutdown(bool destroyGpu)
{
    if (destroyGpu && Engine::BgfxContext::isBgfxInitialized())
    {
        for (const auto &[path, texture] : m_textures)
        {
            if (bgfx::isValid(texture.handle))
            {
                bgfx::destroy(texture.handle);
            }
        }
    }
    m_textures.clear();
    m_binaryAssets.clear();
    m_diagnostics = {};
}

const EffectRenderer::Diagnostics &EffectRenderer::diagnostics() const
{
    return m_diagnostics;
}

void EffectRenderer::preload(const EffectSystem &effects, const EffectResourceLibrary &effectResources,
    const Engine::AssetFileSystem &assetFileSystem)
{
    for (const std::string &id : effects.activeSpriteResources())
    {
        const EffectSpriteResource *pSprite = effectResources.findSprite(id);
        if (pSprite == nullptr)
        {
            continue;
        }
        for (const std::string &path : pSprite->framePaths)
        {
            loadTexture(assetFileSystem, path);
        }
    }
}

const EffectRenderer::Texture *EffectRenderer::loadTexture(
    const Engine::AssetFileSystem &assetFileSystem,
    const std::string &path)
{
    const auto existing = m_textures.find(path);
    if (existing != m_textures.end())
    {
        return &existing->second;
    }
    const std::optional<Engine::ImagePixelsBgra> image =
        Engine::loadImageAssetPixelsBgra(assetFileSystem, path, m_binaryAssets);
    if (!image)
    {
        return nullptr;
    }
    Texture texture;
    texture.width = image->width;
    texture.height = image->height;
    texture.handle = createBgraTexture2D(
        static_cast<uint16_t>(image->width),
        static_cast<uint16_t>(image->height),
        image->pixels.data(),
        static_cast<uint32_t>(image->pixels.size()),
        TextureFilterProfile::Billboard,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    if (!bgfx::isValid(texture.handle))
    {
        return nullptr;
    }
    return &m_textures.emplace(path, texture).first->second;
}
}
