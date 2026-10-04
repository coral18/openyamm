#pragma once

#include "engine/models/ModelInstance.h"

#include <bgfx/bgfx.h>

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Engine
{
struct ModelRenderLighting
{
    std::array<float, 3> lightDirection = {-0.35f, 0.55f, 0.76f};
    float ambient = 0.35f;
    float direct = 0.65f;
};

class ModelRenderer
{
public:
    bool initialize(bgfx::ProgramHandle programHandle);
    void shutdown(bool destroyGpu);
    void preload(const ModelInstanceSystem &instances);
    void render(
        const ModelInstanceSystem &instances,
        uint16_t viewId,
        const std::array<float, 3> &cameraPosition,
        const ModelRenderLighting &lighting = {});

private:
    struct PrimitiveResources
    {
        bgfx::VertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle indexBuffer = BGFX_INVALID_HANDLE;
        uint32_t indexCount = 0;
        int materialIndex = -1;
        std::array<float, 3> center = {};
    };

    struct MeshResources
    {
        std::vector<PrimitiveResources> primitives;
    };

    struct AssetResources
    {
        std::shared_ptr<const ModelAsset> asset;
        std::vector<bgfx::TextureHandle> textures;
        std::vector<MeshResources> meshes;
    };

    struct Draw;

    const AssetResources *prepare(std::shared_ptr<const ModelAsset> asset);
    void destroy(AssetResources &resources);
    void submit(const Draw &draw, uint16_t viewId, const ModelRenderLighting &lighting) const;
    void submitNodeMarkers(const ModelPose &pose, uint16_t viewId) const;

    bgfx::ProgramHandle m_programHandle = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_textureSamplerHandle = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_materialUniformHandle = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_lightingUniformHandle = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_normalMatrixUniformHandle = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_whiteTextureHandle = BGFX_INVALID_HANDLE;
    std::unordered_map<const ModelAsset *, AssetResources> m_assets;
};
}
