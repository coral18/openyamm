#include "game/outdoor/TerrainDecorationRenderer.h"

#include "engine/ImageAssetLoader.h"
#include "game/outdoor/OutdoorLightingRuntime.h"
#include "game/render/TextureFiltering.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace OpenYAMM::Game
{
namespace
{
struct Vertex
{
    float x, y, z, u, v;
    uint32_t color;
};

std::vector<Vertex> grassMesh()
{
    std::vector<Vertex> vertices;
    for (int axis = 0; axis < 2; ++axis)
    {
        const float x = axis == 0 ? 0.5f : 0.0f;
        const float y = axis == 1 ? 0.5f : 0.0f;
        const Vertex a = {-x, -y, 0.0f, 0.0f, 1.0f, 0xffffffff};
        const Vertex b = {x, y, 0.0f, 1.0f, 1.0f, 0xffffffff};
        const Vertex c = {-x, -y, 1.0f, 0.0f, 0.0f, 0xffffffff};
        const Vertex d = {x, y, 1.0f, 1.0f, 0.0f, 0xffffffff};
        vertices.insert(vertices.end(), {a, b, c, c, b, d});
    }
    return vertices;
}

std::vector<Vertex> stoneMesh()
{
    std::vector<Vertex> vertices;
    for (int side = 0; side < 6; ++side)
    {
        const float a = float(side) * 6.2831853f / 6.0f;
        const float b = float(side + 1) * 6.2831853f / 6.0f;
        const uint32_t shade = 175 + uint32_t((side * 23) % 65);
        const uint32_t color = 0xff000000u | (shade << 16) | (shade << 8) | shade;
        const Vertex lowerA = {std::cos(a) * 0.5f, std::sin(a) * 0.38f, 0.0f, 0.0f, 0.0f, color};
        const Vertex lowerB = {std::cos(b) * 0.5f, std::sin(b) * 0.38f, 0.0f, 0.0f, 0.0f, color};
        const Vertex upperA = {std::cos(a) * 0.27f + 0.06f, std::sin(a) * 0.23f, 0.75f, 0.0f, 0.0f, color};
        const Vertex upperB = {std::cos(b) * 0.27f + 0.06f, std::sin(b) * 0.23f, 0.75f, 0.0f, 0.0f, color};
        const Vertex center = {0.04f, -0.02f, 0.95f, 0.0f, 0.0f, color};
        vertices.insert(vertices.end(), {lowerA, lowerB, upperA, upperA, lowerB, upperB, upperA, upperB, center});
    }
    return vertices;
}
}

bool TerrainDecorationRenderer::initialize(
    const Engine::AssetFileSystem &assets, const TerrainDecorationConfig &config,
    TerrainDecorationPlacement placement, bgfx::ProgramHandle program, bgfx::ProgramHandle shadowProgram)
{
    shutdown(true);
    m_program = program;
    m_shadowProgram = shadowProgram;
    Engine::BinaryAssetCache cache;
    std::optional<Engine::ImagePixelsBgra> texture =
        Engine::loadImageAssetPixelsBgra(assets, config.tuftTexture, cache);
    if (!texture || placement.instances.empty() || !bgfx::isValid(program) || !bgfx::isValid(shadowProgram))
    {
        std::cerr << "Terrain decorations could not initialize: " << config.tuftTexture << '\n';
        shutdown(true);
        return false;
    }
    const int columns = config.tuftAtlasGrid[0];
    const int rows = config.tuftAtlasGrid[1];
    if (texture->width % columns != 0 || texture->height % rows != 0)
    {
        std::cerr << "Terrain decoration atlas dimensions do not match its grid: " << config.tuftTexture << '\n';
        shutdown(true);
        return false;
    }
    const uint16_t cellWidth = uint16_t(texture->width / columns);
    const uint16_t cellHeight = uint16_t(texture->height / rows);
    // bgfx uses a 2D target for one layer; reserve a spare layer to keep the array sampler valid.
    const uint16_t arrayLayers = uint16_t(std::max(2, columns * rows));
    m_texture = bgfx::createTexture2D(cellWidth, cellHeight, true, arrayLayers,
        bgraTextureUploadFormat(), BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
    // Store atlas cells as array layers: independent edge bleed, alpha coverage and complete mip chains.
    for (int layer = 0; layer < columns * rows; ++layer)
    {
        std::vector<uint8_t> pixels(size_t(cellWidth) * cellHeight * 4);
        for (int y = 0; y < cellHeight; ++y)
        {
            const size_t sourceOffset = (size_t(layer / columns * cellHeight + y) * texture->width +
                                         layer % columns * cellWidth) * 4;
            std::copy_n(texture->pixels.data() + sourceOffset, size_t(cellWidth) * 4,
                        pixels.data() + size_t(y) * cellWidth * 4);
        }
        prepareBgraTexturePixelsForUploadInPlace(cellWidth, cellHeight, pixels, TextureFilterProfile::Billboard);
        updateBgraTextureArrayLayer(m_texture, uint16_t(layer), cellWidth, cellHeight, pixels, 102);
    }
    bgfx::VertexLayout meshLayout;
    meshLayout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true).end();
    const std::vector<Vertex> grass = grassMesh();
    const std::vector<Vertex> stone = stoneMesh();
    m_grassMesh = bgfx::createVertexBuffer(
        bgfx::copy(grass.data(), uint32_t(grass.size() * sizeof(Vertex))), meshLayout);
    m_stoneMesh = bgfx::createVertexBuffer(
        bgfx::copy(stone.data(), uint32_t(stone.size() * sizeof(Vertex))), meshLayout);
    bgfx::VertexLayout instanceLayout;
    instanceLayout.begin().add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord5, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float).end();
    m_instances = bgfx::createVertexBuffer(bgfx::copy(placement.instances.data(),
        uint32_t(placement.instances.size() * sizeof(TerrainDecorationInstance))), instanceLayout);
    m_sampler = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    m_params = bgfx::createUniform("u_terrainDecorationParams", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(m_texture) || !bgfx::isValid(m_instances) || !bgfx::isValid(m_grassMesh) ||
        !bgfx::isValid(m_stoneMesh) || !bgfx::isValid(m_sampler) || !bgfx::isValid(m_params))
    {
        shutdown(true);
        return false;
    }
    m_distance = config.distance;
    m_patches = std::move(placement.patches);
    std::cout << "Terrain decorations: " << placement.instances.size() << " instances, " << m_patches.size()
              << " patches, " << placement.instances.size() * sizeof(TerrainDecorationInstance)
              << " instance bytes, " << m_distance << " world-unit range.\n";
    return true;
}

void TerrainDecorationRenderer::shutdown(bool destroyResources)
{
    if (destroyResources)
    {
        if (bgfx::isValid(m_program))
        {
            bgfx::destroy(m_program);
        }
        if (bgfx::isValid(m_shadowProgram))
        {
            bgfx::destroy(m_shadowProgram);
        }
        if (bgfx::isValid(m_instances))
        {
            bgfx::destroy(m_instances);
        }
        if (bgfx::isValid(m_grassMesh))
        {
            bgfx::destroy(m_grassMesh);
        }
        if (bgfx::isValid(m_stoneMesh))
        {
            bgfx::destroy(m_stoneMesh);
        }
        if (bgfx::isValid(m_texture))
        {
            bgfx::destroy(m_texture);
        }
        if (bgfx::isValid(m_sampler))
        {
            bgfx::destroy(m_sampler);
        }
        if (bgfx::isValid(m_params))
        {
            bgfx::destroy(m_params);
        }
    }
    m_program = BGFX_INVALID_HANDLE;
    m_shadowProgram = BGFX_INVALID_HANDLE;
    m_instances = BGFX_INVALID_HANDLE;
    m_grassMesh = BGFX_INVALID_HANDLE;
    m_stoneMesh = BGFX_INVALID_HANDLE;
    m_texture = BGFX_INVALID_HANDLE;
    m_sampler = BGFX_INVALID_HANDLE;
    m_params = BGFX_INVALID_HANDLE;
    m_patches.clear();
    m_viewValid = false;
}

bool TerrainDecorationRenderer::setView(
    const bx::Vec3 &camera, const bx::Vec3 &forward, const bx::Vec3 &right, const bx::Vec3 &up,
    float aspect, float verticalFovRadians)
{
    const auto sameVector = [](const bx::Vec3 &a, const bx::Vec3 &b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    };
    if (m_viewValid && sameVector(camera, m_camera) && sameVector(forward, m_forward) &&
        sameVector(right, m_right) && sameVector(up, m_up) && aspect == m_aspect && verticalFovRadians == m_verticalFov)
    {
        return false;
    }
    m_viewValid = true;
    m_aspect = aspect;
    m_verticalFov = verticalFovRadians;
    m_camera = camera;
    m_forward = forward;
    m_right = right;
    m_up = up;
    m_tanY = std::tan(verticalFovRadians * 0.5f);
    m_tanX = m_tanY * aspect;
    m_radiusScaleX = std::sqrt(1.0f + m_tanX * m_tanX);
    m_radiusScaleY = std::sqrt(1.0f + m_tanY * m_tanY);
    return true;
}

bool TerrainDecorationRenderer::visible(const TerrainDecorationPatch &patch) const
{
    const bx::Vec3 center = {(patch.min[0] + patch.max[0]) * 0.5f, (patch.min[1] + patch.max[1]) * 0.5f,
                            (patch.min[2] + patch.max[2]) * 0.5f};
    const bx::Vec3 extent = {(patch.max[0] - patch.min[0]) * 0.5f, (patch.max[1] - patch.min[1]) * 0.5f,
                            (patch.max[2] - patch.min[2]) * 0.5f};
    const float radius = bx::length(extent);
    const bx::Vec3 delta = bx::sub(center, m_camera);
    const float detailDistance = patch.stone ? std::min(m_distance, 2048.0f) : m_distance;
    const float maximumDistance = detailDistance + radius;
    if (bx::dot(delta, delta) > maximumDistance * maximumDistance)
    {
        return false;
    }
    const float depth = bx::dot(delta, m_forward);
    return depth >= -radius &&
        std::abs(bx::dot(delta, m_right)) <= depth * m_tanX + radius * m_radiusScaleX &&
        std::abs(bx::dot(delta, m_up)) <= depth * m_tanY + radius * m_radiusScaleY;
}

bool TerrainDecorationRenderer::canMerge(
    const TerrainDecorationPatch &batch, const OutdoorSelectedFxLights &batchLights,
    const TerrainDecorationPatch &next, const OutdoorSelectedFxLights &nextLights)
{
    return batch.count != 0 && next.count != 0 && batch.stone == next.stone &&
        batch.first + batch.count == next.first && batchLights.positions == nextLights.positions &&
        batchLights.colors == nextLights.colors && batchLights.params == nextLights.params;
}

void TerrainDecorationRenderer::submit(uint16_t viewId, const TerrainDecorationPatch &patch, float elapsedTime,
    bool sunShadows)
{
    const float detailDistance = patch.stone ? std::min(m_distance, 2048.0f) : m_distance;
    const float params[4] = {elapsedTime, detailDistance * 0.5f, detailDistance, 0.0f};
    bgfx::setUniform(m_params, params);
    bgfx::setVertexBuffer(0, patch.stone ? m_stoneMesh : m_grassMesh);
    bgfx::setInstanceDataBuffer(m_instances, patch.first, patch.count);
    bgfx::setTexture(0, m_sampler, m_texture);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
    bgfx::submit(viewId, sunShadows ? m_shadowProgram : m_program);
}
} // namespace OpenYAMM::Game
