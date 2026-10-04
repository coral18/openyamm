#include "engine/render/ModelRenderer.h"

#include "engine/BgfxContext.h"
#include "engine/ImageAssetLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace OpenYAMM::Engine
{
namespace
{
static_assert(sizeof(ModelVertex) == sizeof(float) * 8);

constexpr uint64_t OpaqueState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
    BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_MSAA;
constexpr uint64_t BlendState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LEQUAL |
    BGFX_STATE_BLEND_ALPHA | BGFX_STATE_MSAA;
// Inspector markers intentionally ignore scene depth so empty and occluded attachment nodes remain inspectable.
constexpr uint64_t MarkerState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_PT_LINES | BGFX_STATE_MSAA;
constexpr float MarkerAxisLength = 0.4f;

bgfx::VertexLayout modelVertexLayout()
{
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    return layout;
}

float determinant3x3(const ModelMatrix &matrix)
{
    return matrix[0] * (matrix[5] * matrix[10] - matrix[9] * matrix[6]) -
        matrix[4] * (matrix[1] * matrix[10] - matrix[9] * matrix[2]) +
        matrix[8] * (matrix[1] * matrix[6] - matrix[5] * matrix[2]);
}

ModelMatrix normalMatrix(const ModelMatrix &matrix)
{
    const float determinant = determinant3x3(matrix);
    if (std::abs(determinant) <= std::numeric_limits<float>::epsilon())
    {
        return identityModelMatrix();
    }

    const float inverseDeterminant = 1.0f / determinant;
    ModelMatrix result = {};
    result[0] = (matrix[5] * matrix[10] - matrix[9] * matrix[6]) * inverseDeterminant;
    result[1] = (matrix[8] * matrix[6] - matrix[4] * matrix[10]) * inverseDeterminant;
    result[2] = (matrix[4] * matrix[9] - matrix[8] * matrix[5]) * inverseDeterminant;
    result[4] = (matrix[9] * matrix[2] - matrix[1] * matrix[10]) * inverseDeterminant;
    result[5] = (matrix[0] * matrix[10] - matrix[8] * matrix[2]) * inverseDeterminant;
    result[6] = (matrix[8] * matrix[1] - matrix[0] * matrix[9]) * inverseDeterminant;
    result[8] = (matrix[1] * matrix[6] - matrix[5] * matrix[2]) * inverseDeterminant;
    result[9] = (matrix[4] * matrix[2] - matrix[0] * matrix[6]) * inverseDeterminant;
    result[10] = (matrix[0] * matrix[5] - matrix[4] * matrix[1]) * inverseDeterminant;
    result[15] = 1.0f;
    return result;
}

std::array<float, 3> transformPoint(const ModelMatrix &matrix, const std::array<float, 3> &point)
{
    return {
        matrix[0] * point[0] + matrix[4] * point[1] + matrix[8] * point[2] + matrix[12],
        matrix[1] * point[0] + matrix[5] * point[1] + matrix[9] * point[2] + matrix[13],
        matrix[2] * point[0] + matrix[6] * point[1] + matrix[10] * point[2] + matrix[14],
    };
}

bgfx::TextureHandle createTexture(const ModelImage &image)
{
    const std::optional<ImagePixelsBgra> decoded = decodeImagePixelsBgra(image.pngBytes, image.sourcePath);
    if (!decoded || decoded->width <= 0 || decoded->height <= 0 ||
        decoded->width > std::numeric_limits<uint16_t>::max() ||
        decoded->height > std::numeric_limits<uint16_t>::max())
    {
        return BGFX_INVALID_HANDLE;
    }

    std::vector<uint8_t> rgba = decoded->pixels;
    for (size_t offset = 0; offset + 3 < rgba.size(); offset += 4)
    {
        std::swap(rgba[offset], rgba[offset + 2]);
    }
    return bgfx::createTexture2D(
        static_cast<uint16_t>(decoded->width),
        static_cast<uint16_t>(decoded->height),
        true,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        bgfx::copy(rgba.data(), static_cast<uint32_t>(rgba.size())));
}

uint64_t cullState(const ModelMaterial &material, const ModelMatrix &matrix)
{
    if (material.doubleSided)
    {
        return 0;
    }
    return determinant3x3(matrix) < 0.0f ? BGFX_STATE_CULL_CCW : BGFX_STATE_CULL_CW;
}

}

struct ModelRenderer::Draw
{
    const PrimitiveResources *pPrimitive = nullptr;
    const AssetResources *pResources = nullptr;
    const ModelMaterial *pMaterial = nullptr;
    const ModelMatrix *pMatrix = nullptr;
    float distanceSquared = 0.0f;
};

void ModelRenderer::submit(const Draw &draw, uint16_t viewId, const ModelRenderLighting &lighting) const
{
    const ModelMaterial &material = *draw.pMaterial;
    const float materialValues[8] = {
        material.baseColor[0], material.baseColor[1], material.baseColor[2], material.baseColor[3],
        material.alphaCutoff,
        material.alphaMode == ModelAlphaMode::Mask ? 1.0f : 0.0f,
        material.unlit ? 1.0f : 0.0f,
        0.0f,
    };
    const float lightingValues[8] = {
        lighting.lightDirection[0], lighting.lightDirection[1], lighting.lightDirection[2], lighting.ambient,
        lighting.direct, 0.0f, 0.0f, 0.0f,
    };
    const ModelMatrix transformedNormals = normalMatrix(*draw.pMatrix);
    bgfx::TextureHandle texture = m_whiteTextureHandle;
    if (material.imageIndex >= 0 && static_cast<size_t>(material.imageIndex) < draw.pResources->textures.size())
    {
        const bgfx::TextureHandle candidate = draw.pResources->textures[material.imageIndex];
        if (bgfx::isValid(candidate))
        {
            texture = candidate;
        }
    }

    bgfx::setTransform(draw.pMatrix->data());
    bgfx::setVertexBuffer(0, draw.pPrimitive->vertexBuffer);
    bgfx::setIndexBuffer(draw.pPrimitive->indexBuffer, 0, draw.pPrimitive->indexCount);
    bgfx::setTexture(0, m_textureSamplerHandle, texture);
    bgfx::setUniform(m_materialUniformHandle, materialValues, 2);
    bgfx::setUniform(m_lightingUniformHandle, lightingValues, 2);
    bgfx::setUniform(m_normalMatrixUniformHandle, transformedNormals.data());
    const uint64_t state = material.alphaMode == ModelAlphaMode::Blend ? BlendState : OpaqueState;
    bgfx::setState(state | cullState(material, *draw.pMatrix));
    bgfx::submit(viewId, m_programHandle);
}

void ModelRenderer::submitNodeMarkers(const ModelPose &pose, uint16_t viewId) const
{
    const bgfx::VertexLayout layout = modelVertexLayout();
    for (const ModelMatrix &matrix : pose.globalMatrices)
    {
        constexpr uint32_t vertexCount = 6;
        if (bgfx::getAvailTransientVertexBuffer(vertexCount, layout) < vertexCount)
        {
            return;
        }

        bgfx::TransientVertexBuffer vertices = {};
        bgfx::allocTransientVertexBuffer(&vertices, vertexCount, layout);
        ModelVertex *pVertices = static_cast<ModelVertex *>(static_cast<void *>(vertices.data));
        for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
        {
            pVertices[vertexIndex] = {};
        }
        pVertices[0].position = {0.0f, 0.0f, 0.0f};
        pVertices[1].position = {MarkerAxisLength, 0.0f, 0.0f};
        pVertices[2].position = {0.0f, 0.0f, 0.0f};
        pVertices[3].position = {0.0f, MarkerAxisLength, 0.0f};
        pVertices[4].position = {0.0f, 0.0f, 0.0f};
        pVertices[5].position = {0.0f, 0.0f, MarkerAxisLength};

        static constexpr std::array<std::array<float, 4>, 3> axisColors = {{
            {1.0f, 0.05f, 0.05f, 1.0f},
            {0.05f, 1.0f, 0.05f, 1.0f},
            {0.05f, 0.35f, 1.0f, 1.0f},
        }};
        const float lightingValues[8] = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        const ModelMatrix identity = identityModelMatrix();
        for (uint32_t axis = 0; axis < 3; ++axis)
        {
            const float materialValues[8] = {
                axisColors[axis][0], axisColors[axis][1], axisColors[axis][2], axisColors[axis][3],
                0.5f, 0.0f, 1.0f, 0.0f,
            };
            bgfx::setTransform(matrix.data());
            bgfx::setVertexBuffer(0, &vertices, axis * 2, 2);
            bgfx::setTexture(0, m_textureSamplerHandle, m_whiteTextureHandle);
            bgfx::setUniform(m_materialUniformHandle, materialValues, 2);
            bgfx::setUniform(m_lightingUniformHandle, lightingValues, 2);
            bgfx::setUniform(m_normalMatrixUniformHandle, identity.data());
            bgfx::setState(MarkerState);
            bgfx::submit(viewId, m_programHandle);
        }
    }
}

bool ModelRenderer::initialize(bgfx::ProgramHandle programHandle)
{
    if (!bgfx::isValid(programHandle) || !BgfxContext::isBgfxInitialized())
    {
        return false;
    }
    shutdown(true);
    m_programHandle = programHandle;
    m_textureSamplerHandle = bgfx::createUniform("s_modelTexture", bgfx::UniformType::Sampler);
    m_materialUniformHandle = bgfx::createUniform("u_modelMaterial", bgfx::UniformType::Vec4, 2);
    m_lightingUniformHandle = bgfx::createUniform("u_modelLighting", bgfx::UniformType::Vec4, 2);
    m_normalMatrixUniformHandle = bgfx::createUniform("u_modelNormalMatrix", bgfx::UniformType::Mat4);
    const uint32_t white = 0xffffffffu;
    m_whiteTextureHandle = bgfx::createTexture2D(
        1,
        1,
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        bgfx::copy(&white, sizeof(white)));
    const bool initialized = bgfx::isValid(m_textureSamplerHandle) && bgfx::isValid(m_materialUniformHandle) &&
        bgfx::isValid(m_lightingUniformHandle) && bgfx::isValid(m_normalMatrixUniformHandle) &&
        bgfx::isValid(m_whiteTextureHandle);
    if (!initialized)
    {
        shutdown(true);
    }
    return initialized;
}

void ModelRenderer::shutdown(bool destroyGpu)
{
    if (destroyGpu && BgfxContext::isBgfxInitialized())
    {
        for (auto &[pAsset, resources] : m_assets)
        {
            destroy(resources);
        }
        if (bgfx::isValid(m_whiteTextureHandle))
        {
            bgfx::destroy(m_whiteTextureHandle);
        }
        if (bgfx::isValid(m_textureSamplerHandle))
        {
            bgfx::destroy(m_textureSamplerHandle);
        }
        if (bgfx::isValid(m_materialUniformHandle))
        {
            bgfx::destroy(m_materialUniformHandle);
        }
        if (bgfx::isValid(m_lightingUniformHandle))
        {
            bgfx::destroy(m_lightingUniformHandle);
        }
        if (bgfx::isValid(m_normalMatrixUniformHandle))
        {
            bgfx::destroy(m_normalMatrixUniformHandle);
        }
        if (bgfx::isValid(m_programHandle))
        {
            bgfx::destroy(m_programHandle);
        }
    }
    m_assets.clear();
    m_programHandle = BGFX_INVALID_HANDLE;
    m_textureSamplerHandle = BGFX_INVALID_HANDLE;
    m_materialUniformHandle = BGFX_INVALID_HANDLE;
    m_lightingUniformHandle = BGFX_INVALID_HANDLE;
    m_normalMatrixUniformHandle = BGFX_INVALID_HANDLE;
    m_whiteTextureHandle = BGFX_INVALID_HANDLE;
}

void ModelRenderer::preload(const ModelInstanceSystem &instances)
{
    if (!bgfx::isValid(m_programHandle))
    {
        return;
    }
    for (const ModelInstanceHandle handle : instances.handles())
    {
        prepare(instances.sharedAsset(handle));
    }
}

void ModelRenderer::render(
    const ModelInstanceSystem &instances,
    uint16_t viewId,
    const std::array<float, 3> &cameraPosition,
    const ModelRenderLighting &lighting)
{
    if (!bgfx::isValid(m_programHandle))
    {
        return;
    }

    static const ModelMaterial defaultMaterial;
    std::vector<Draw> opaqueDraws;
    std::vector<Draw> transparentDraws;
    std::vector<const ModelPose *> markerPoses;
    for (const ModelInstanceHandle handle : instances.handles())
    {
        if (!instances.isVisible(handle))
        {
            continue;
        }
        const std::shared_ptr<const ModelAsset> asset = instances.sharedAsset(handle);
        const ModelPose *pPose = instances.pose(handle);
        const AssetResources *pResources = prepare(asset);
        if (asset == nullptr || pPose == nullptr || pResources == nullptr)
        {
            continue;
        }
        if (instances.areNodeMarkersVisible(handle))
        {
            markerPoses.push_back(pPose);
        }
        for (size_t nodeIndex = 0; nodeIndex < asset->nodes.size(); ++nodeIndex)
        {
            const ModelNode &node = asset->nodes[nodeIndex];
            if (node.meshIndex < 0 || static_cast<size_t>(node.meshIndex) >= pResources->meshes.size() ||
                nodeIndex >= pPose->globalMatrices.size())
            {
                continue;
            }
            const ModelMatrix &matrix = pPose->globalMatrices[nodeIndex];
            const MeshResources &mesh = pResources->meshes[node.meshIndex];
            for (const PrimitiveResources &primitive : mesh.primitives)
            {
                if (!bgfx::isValid(primitive.vertexBuffer) || !bgfx::isValid(primitive.indexBuffer) ||
                    primitive.indexCount == 0)
                {
                    continue;
                }
                const ModelMaterial *pMaterial = &defaultMaterial;
                if (primitive.materialIndex >= 0 &&
                    static_cast<size_t>(primitive.materialIndex) < asset->materials.size())
                {
                    pMaterial = &asset->materials[primitive.materialIndex];
                }
                const std::array<float, 3> center = transformPoint(matrix, primitive.center);
                const float dx = center[0] - cameraPosition[0];
                const float dy = center[1] - cameraPosition[1];
                const float dz = center[2] - cameraPosition[2];
                Draw draw = {
                    &primitive,
                    pResources,
                    pMaterial,
                    &matrix,
                    dx * dx + dy * dy + dz * dz,
                };
                if (pMaterial->alphaMode == ModelAlphaMode::Blend)
                {
                    transparentDraws.push_back(draw);
                }
                else
                {
                    opaqueDraws.push_back(draw);
                }
            }
        }
    }

    std::stable_sort(
        opaqueDraws.begin(),
        opaqueDraws.end(),
        [](const Draw &left, const Draw &right)
        {
            return left.distanceSquared < right.distanceSquared;
        });
    std::stable_sort(
        transparentDraws.begin(),
        transparentDraws.end(),
        [](const Draw &left, const Draw &right)
        {
            return left.distanceSquared > right.distanceSquared;
        });
    for (const Draw &draw : opaqueDraws)
    {
        submit(draw, viewId, lighting);
    }
    for (const Draw &draw : transparentDraws)
    {
        submit(draw, viewId, lighting);
    }
    for (const ModelPose *pPose : markerPoses)
    {
        submitNodeMarkers(*pPose, viewId);
    }
}

const ModelRenderer::AssetResources *ModelRenderer::prepare(std::shared_ptr<const ModelAsset> asset)
{
    if (asset == nullptr)
    {
        return nullptr;
    }
    const auto existing = m_assets.find(asset.get());
    if (existing != m_assets.end())
    {
        return &existing->second;
    }

    AssetResources resources;
    resources.asset = asset;
    resources.textures.reserve(asset->images.size());
    for (const ModelImage &image : asset->images)
    {
        resources.textures.push_back(createTexture(image));
    }
    const bgfx::VertexLayout layout = modelVertexLayout();
    resources.meshes.reserve(asset->meshes.size());
    for (const ModelMesh &mesh : asset->meshes)
    {
        MeshResources meshResources;
        meshResources.primitives.reserve(mesh.primitives.size());
        for (const ModelPrimitive &primitive : mesh.primitives)
        {
            PrimitiveResources primitiveResources;
            primitiveResources.materialIndex = primitive.materialIndex;
            primitiveResources.indexCount = static_cast<uint32_t>(primitive.indices.size());
            if (!primitive.vertices.empty() && !primitive.indices.empty())
            {
                primitiveResources.vertexBuffer = bgfx::createVertexBuffer(
                    bgfx::copy(
                        primitive.vertices.data(),
                        static_cast<uint32_t>(primitive.vertices.size() * sizeof(ModelVertex))),
                    layout);
                primitiveResources.indexBuffer = bgfx::createIndexBuffer(
                    bgfx::copy(
                        primitive.indices.data(),
                        static_cast<uint32_t>(primitive.indices.size() * sizeof(uint32_t))),
                    BGFX_BUFFER_INDEX32);
                std::array<float, 3> min = primitive.vertices.front().position;
                std::array<float, 3> max = min;
                for (const ModelVertex &vertex : primitive.vertices)
                {
                    for (size_t axis = 0; axis < 3; ++axis)
                    {
                        min[axis] = std::min(min[axis], vertex.position[axis]);
                        max[axis] = std::max(max[axis], vertex.position[axis]);
                    }
                }
                for (size_t axis = 0; axis < 3; ++axis)
                {
                    primitiveResources.center[axis] = (min[axis] + max[axis]) * 0.5f;
                }
            }
            meshResources.primitives.push_back(primitiveResources);
        }
        resources.meshes.push_back(std::move(meshResources));
    }
    return &m_assets.emplace(asset.get(), std::move(resources)).first->second;
}

void ModelRenderer::destroy(AssetResources &resources)
{
    for (const bgfx::TextureHandle texture : resources.textures)
    {
        if (bgfx::isValid(texture))
        {
            bgfx::destroy(texture);
        }
    }
    for (MeshResources &mesh : resources.meshes)
    {
        for (PrimitiveResources &primitive : mesh.primitives)
        {
            if (bgfx::isValid(primitive.vertexBuffer))
            {
                bgfx::destroy(primitive.vertexBuffer);
            }
            if (bgfx::isValid(primitive.indexBuffer))
            {
                bgfx::destroy(primitive.indexBuffer);
            }
        }
    }
}
}
