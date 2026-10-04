#include "engine/models/GltfModelLoader.h"

#include "engine/AssetFileSystem.h"

#include <cgltf/cgltf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string_view>
#include <unordered_set>

namespace OpenYAMM::Engine
{
namespace
{
struct CgltfDeleter
{
    void operator()(cgltf_data *pData) const
    {
        cgltf_free(pData);
    }
};

struct AssetFileContext
{
    const AssetFileSystem *pAssetFileSystem = nullptr;
};

void *cgltfAllocate(const cgltf_memory_options *pMemoryOptions, size_t size)
{
    if (pMemoryOptions->alloc_func != nullptr)
    {
        return pMemoryOptions->alloc_func(pMemoryOptions->user_data, size);
    }
    return std::malloc(size);
}

void cgltfReleaseMemory(const cgltf_memory_options *pMemoryOptions, void *pData)
{
    if (pMemoryOptions->free_func != nullptr)
    {
        pMemoryOptions->free_func(pMemoryOptions->user_data, pData);
        return;
    }
    std::free(pData);
}

cgltf_result readAssetFile(
    const cgltf_memory_options *pMemoryOptions,
    const cgltf_file_options *pFileOptions,
    const char *pPath,
    cgltf_size *pSize,
    void **ppData
)
{
    const AssetFileContext *pContext = static_cast<const AssetFileContext *>(pFileOptions->user_data);
    const std::optional<std::vector<uint8_t>> bytes = pContext->pAssetFileSystem->readBinaryFile(pPath);
    if (!bytes)
    {
        return cgltf_result_file_not_found;
    }
    if (pSize != nullptr && *pSize != 0 && bytes->size() < *pSize)
    {
        return cgltf_result_data_too_short;
    }

    void *pCopy = cgltfAllocate(pMemoryOptions, bytes->size());
    if (pCopy == nullptr && !bytes->empty())
    {
        return cgltf_result_out_of_memory;
    }
    if (!bytes->empty())
    {
        std::memcpy(pCopy, bytes->data(), bytes->size());
    }
    if (pSize != nullptr)
    {
        *pSize = bytes->size();
    }
    *ppData = pCopy;
    return cgltf_result_success;
}

void releaseAssetFile(
    const cgltf_memory_options *pMemoryOptions,
    const cgltf_file_options *,
    void *pData,
    cgltf_size
)
{
    cgltfReleaseMemory(pMemoryOptions, pData);
}

std::string cgltfResultName(cgltf_result result)
{
    switch (result)
    {
    case cgltf_result_success:
        return "success";
    case cgltf_result_data_too_short:
        return "data is truncated";
    case cgltf_result_unknown_format:
        return "unknown format";
    case cgltf_result_invalid_json:
        return "invalid JSON";
    case cgltf_result_invalid_gltf:
        return "invalid glTF";
    case cgltf_result_invalid_options:
        return "invalid loader options";
    case cgltf_result_file_not_found:
        return "referenced file was not found";
    case cgltf_result_io_error:
        return "I/O error";
    case cgltf_result_out_of_memory:
        return "out of memory";
    case cgltf_result_legacy_gltf:
        return "legacy glTF is unsupported";
    default:
        return "unknown cgltf error";
    }
}

bool isSafeRelativeUri(const char *pUri)
{
    if (pUri == nullptr || std::string_view(pUri).starts_with("data:"))
    {
        return true;
    }
    const std::string uri = pUri;
    if (uri.empty() || uri.find("://") != std::string::npos || uri[0] == '/' || uri[0] == '\\')
    {
        return false;
    }
    for (const std::filesystem::path &component : std::filesystem::path(uri))
    {
        if (component == "..")
        {
            return false;
        }
    }
    return true;
}

std::string joinedVirtualPath(const std::string &modelPath, const std::string &relativePath)
{
    const std::filesystem::path parent = std::filesystem::path(modelPath).parent_path();
    return (parent / relativePath).lexically_normal().generic_string();
}

bool hasPngSignature(const std::vector<uint8_t> &bytes)
{
    constexpr std::array<uint8_t, 8> Signature = {137, 80, 78, 71, 13, 10, 26, 10};
    return bytes.size() >= Signature.size() && std::equal(Signature.begin(), Signature.end(), bytes.begin());
}

const cgltf_accessor *findAttribute(const cgltf_primitive &primitive, cgltf_attribute_type type, int index = 0)
{
    for (size_t attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex)
    {
        const cgltf_attribute &attribute = primitive.attributes[attributeIndex];
        if (attribute.type == type && attribute.index == index)
        {
            return attribute.data;
        }
    }
    return nullptr;
}

bool readAccessor(const cgltf_accessor &accessor, size_t index, size_t count, float *pValues)
{
    std::array<float, 4> values = {};
    if (count > values.size() || cgltf_accessor_read_float(&accessor, index, values.data(), count) == 0)
    {
        return false;
    }
    std::copy_n(values.data(), count, pValues);
    return true;
}

void expandBounds(ModelBounds &bounds, const std::array<float, 3> &point)
{
    if (!bounds.valid)
    {
        bounds.min = point;
        bounds.max = point;
        bounds.valid = true;
        return;
    }
    for (size_t axis = 0; axis < 3; ++axis)
    {
        bounds.min[axis] = std::min(bounds.min[axis], point[axis]);
        bounds.max[axis] = std::max(bounds.max[axis], point[axis]);
    }
}

std::array<float, 3> transformPoint(const ModelMatrix &matrix, const std::array<float, 3> &point)
{
    return {
        matrix[0] * point[0] + matrix[4] * point[1] + matrix[8] * point[2] + matrix[12],
        matrix[1] * point[0] + matrix[5] * point[1] + matrix[9] * point[2] + matrix[13],
        matrix[2] * point[0] + matrix[6] * point[1] + matrix[10] * point[2] + matrix[14],
    };
}

bool visitHierarchy(
    const ModelAsset &asset,
    uint32_t nodeIndex,
    std::vector<uint8_t> &states,
    std::vector<uint32_t> &order,
    std::string &error
)
{
    if (states[nodeIndex] == 1)
    {
        error = "model node hierarchy contains a cycle at node " + std::to_string(nodeIndex);
        return false;
    }
    if (states[nodeIndex] == 2)
    {
        return true;
    }

    states[nodeIndex] = 1;
    order.push_back(nodeIndex);
    for (uint32_t childIndex : asset.nodes[nodeIndex].childIndices)
    {
        if (!visitHierarchy(asset, childIndex, states, order, error))
        {
            return false;
        }
    }
    states[nodeIndex] = 2;
    return true;
}

bool buildHierarchy(ModelAsset &asset, std::string &error)
{
    std::vector<uint8_t> states(asset.nodes.size(), 0);
    for (uint32_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex)
    {
        if (asset.nodes[nodeIndex].parentIndex < 0 &&
            !visitHierarchy(asset, nodeIndex, states, asset.hierarchyOrder, error))
        {
            return false;
        }
    }
    if (asset.hierarchyOrder.size() != asset.nodes.size())
    {
        error = "model node hierarchy has no root for one or more nodes";
        return false;
    }
    return true;
}

bool loadImages(
    const AssetFileSystem &assetFileSystem,
    const std::string &virtualPath,
    const cgltf_data &data,
    ModelAsset &asset,
    std::string &error
)
{
    asset.images.reserve(data.images_count);
    for (size_t imageIndex = 0; imageIndex < data.images_count; ++imageIndex)
    {
        const cgltf_image &source = data.images[imageIndex];
        ModelImage image;
        image.name = source.name != nullptr ? source.name : "";
        if (source.buffer_view != nullptr)
        {
            if (source.mime_type == nullptr || std::string_view(source.mime_type) != "image/png")
            {
                error = "embedded model image " + std::to_string(imageIndex) + " is not PNG";
                return false;
            }
            const uint8_t *pBytes = cgltf_buffer_view_data(source.buffer_view);
            if (pBytes == nullptr)
            {
                error = "embedded model image " + std::to_string(imageIndex) + " has no buffer data";
                return false;
            }
            image.pngBytes.assign(pBytes, pBytes + source.buffer_view->size);
            image.sourcePath = virtualPath + "#image" + std::to_string(imageIndex);
        }
        else if (source.uri != nullptr)
        {
            if (!isSafeRelativeUri(source.uri) || std::string_view(source.uri).starts_with("data:"))
            {
                error = "model image URI must be a package-relative PNG path: " + std::string(source.uri);
                return false;
            }
            image.sourcePath = joinedVirtualPath(virtualPath, source.uri);
            const std::optional<std::vector<uint8_t>> bytes = assetFileSystem.readBinaryFile(image.sourcePath);
            if (!bytes)
            {
                error = "model image was not found: " + image.sourcePath;
                return false;
            }
            image.pngBytes = *bytes;
        }
        else
        {
            error = "model image " + std::to_string(imageIndex) + " has neither URI nor buffer view";
            return false;
        }
        if (!hasPngSignature(image.pngBytes))
        {
            error = "model image is not a PNG: " + image.sourcePath;
            return false;
        }
        asset.images.push_back(std::move(image));
    }
    return true;
}

bool rejectUnsupportedFeatures(const cgltf_data &data, std::string &error)
{
    for (size_t animationIndex = 0; animationIndex < data.animations_count; ++animationIndex)
    {
        const cgltf_animation &animation = data.animations[animationIndex];
        for (size_t samplerIndex = 0; samplerIndex < animation.samplers_count; ++samplerIndex)
        {
            if (animation.samplers[samplerIndex].interpolation == cgltf_interpolation_type_cubic_spline)
            {
                const std::string name = animation.name != nullptr ? animation.name : std::to_string(animationIndex);
                error = "animation " + name + " uses unsupported CUBICSPLINE interpolation";
                return false;
            }
        }
    }
    return true;
}

bool loadMaterials(const cgltf_data &data, ModelAsset &asset, std::string &error)
{
    asset.materials.reserve(data.materials_count);
    for (size_t materialIndex = 0; materialIndex < data.materials_count; ++materialIndex)
    {
        const cgltf_material &source = data.materials[materialIndex];
        if (source.has_pbr_specular_glossiness || source.has_clearcoat || source.has_transmission ||
            source.has_volume ||
            source.has_ior || source.has_specular || source.has_sheen || source.has_iridescence ||
            source.has_diffuse_transmission || source.has_anisotropy || source.has_dispersion)
        {
            error = "material " + std::to_string(materialIndex) + " uses an unsupported material extension";
            return false;
        }

        ModelMaterial material;
        material.name = source.name != nullptr ? source.name : "";
        material.doubleSided = source.double_sided != 0;
        material.unlit = source.unlit != 0;
        material.alphaCutoff = source.alpha_cutoff;
        switch (source.alpha_mode)
        {
        case cgltf_alpha_mode_opaque:
            material.alphaMode = ModelAlphaMode::Opaque;
            break;
        case cgltf_alpha_mode_mask:
            material.alphaMode = ModelAlphaMode::Mask;
            break;
        case cgltf_alpha_mode_blend:
            material.alphaMode = ModelAlphaMode::Blend;
            break;
        default:
            error = "material " + std::to_string(materialIndex) + " has an invalid alpha mode";
            return false;
        }

        if (source.has_pbr_metallic_roughness)
        {
            std::copy_n(source.pbr_metallic_roughness.base_color_factor, 4, material.baseColor.begin());
            const cgltf_texture *pTexture = source.pbr_metallic_roughness.base_color_texture.texture;
            if (pTexture != nullptr)
            {
                if (source.pbr_metallic_roughness.base_color_texture.texcoord != 0 || pTexture->image == nullptr)
                {
                    error = "material " + std::to_string(materialIndex) +
                        " uses an unsupported base-color texture binding";
                    return false;
                }
                material.imageIndex = static_cast<int>(cgltf_image_index(&data, pTexture->image));
            }
        }
        asset.materials.push_back(std::move(material));
    }
    return true;
}

bool loadMeshes(const cgltf_data &data, ModelAsset &asset, std::string &error)
{
    asset.meshes.reserve(data.meshes_count);
    for (size_t meshIndex = 0; meshIndex < data.meshes_count; ++meshIndex)
    {
        const cgltf_mesh &sourceMesh = data.meshes[meshIndex];
        if (sourceMesh.weights_count != 0)
        {
            error = "mesh " + std::to_string(meshIndex) + " uses unsupported morph weights";
            return false;
        }
        ModelMesh mesh;
        mesh.name = sourceMesh.name != nullptr ? sourceMesh.name : "";
        mesh.primitives.reserve(sourceMesh.primitives_count);
        for (size_t primitiveIndex = 0; primitiveIndex < sourceMesh.primitives_count; ++primitiveIndex)
        {
            const cgltf_primitive &source = sourceMesh.primitives[primitiveIndex];
            if (source.type != cgltf_primitive_type_triangles)
            {
                error = "mesh " + std::to_string(meshIndex) + " primitive " + std::to_string(primitiveIndex) +
                    " is not a triangle list";
                return false;
            }
            if (source.targets_count != 0)
            {
                error = "mesh " + std::to_string(meshIndex) + " primitive " + std::to_string(primitiveIndex) +
                    " uses unsupported morph targets";
                return false;
            }

            const cgltf_accessor *pPosition = findAttribute(source, cgltf_attribute_type_position);
            const cgltf_accessor *pNormal = findAttribute(source, cgltf_attribute_type_normal);
            const cgltf_accessor *pTexCoord = findAttribute(source, cgltf_attribute_type_texcoord);
            if (pPosition == nullptr || pPosition->type != cgltf_type_vec3)
            {
                error = "mesh " + std::to_string(meshIndex) + " primitive " + std::to_string(primitiveIndex) +
                    " has no VEC3 POSITION attribute";
                return false;
            }
            if (pNormal != nullptr && (pNormal->type != cgltf_type_vec3 || pNormal->count != pPosition->count))
            {
                error = "mesh " + std::to_string(meshIndex) + " has an invalid NORMAL attribute";
                return false;
            }
            if (pTexCoord != nullptr && (pTexCoord->type != cgltf_type_vec2 || pTexCoord->count != pPosition->count))
            {
                error = "mesh " + std::to_string(meshIndex) + " has an invalid TEXCOORD_0 attribute";
                return false;
            }
            if (findAttribute(source, cgltf_attribute_type_joints) != nullptr ||
                findAttribute(source, cgltf_attribute_type_weights) != nullptr)
            {
                error = "mesh " + std::to_string(meshIndex) + " contains unsupported skinning attributes";
                return false;
            }

            ModelPrimitive primitive;
            primitive.materialIndex = source.material != nullptr
                ? static_cast<int>(cgltf_material_index(&data, source.material)) : -1;
            primitive.vertices.resize(pPosition->count);
            for (size_t vertexIndex = 0; vertexIndex < pPosition->count; ++vertexIndex)
            {
                ModelVertex &vertex = primitive.vertices[vertexIndex];
                if (!readAccessor(*pPosition, vertexIndex, 3, vertex.position.data()) ||
                    (pNormal != nullptr && !readAccessor(*pNormal, vertexIndex, 3, vertex.normal.data())) ||
                    (pTexCoord != nullptr && !readAccessor(*pTexCoord, vertexIndex, 2, vertex.texCoord.data())))
                {
                    error = "mesh " + std::to_string(meshIndex) + " contains unreadable vertex data";
                    return false;
                }
            }

            const size_t indexCount = source.indices != nullptr ? source.indices->count : pPosition->count;
            if (indexCount % 3 != 0)
            {
                error = "mesh " + std::to_string(meshIndex) + " triangle index count is not divisible by three";
                return false;
            }
            primitive.indices.reserve(indexCount);
            for (size_t index = 0; index < indexCount; ++index)
            {
                const size_t vertexIndex = source.indices != nullptr
                    ? cgltf_accessor_read_index(source.indices, index) : index;
                if (vertexIndex >= primitive.vertices.size() || vertexIndex > std::numeric_limits<uint32_t>::max())
                {
                    error = "mesh " + std::to_string(meshIndex) + " contains an out-of-range index";
                    return false;
                }
                primitive.indices.push_back(static_cast<uint32_t>(vertexIndex));
            }
            mesh.primitives.push_back(std::move(primitive));
        }
        asset.meshes.push_back(std::move(mesh));
    }
    return true;
}

bool loadNodes(const cgltf_data &data, ModelAsset &asset, std::string &error)
{
    asset.nodes.resize(data.nodes_count);
    for (size_t nodeIndex = 0; nodeIndex < data.nodes_count; ++nodeIndex)
    {
        const cgltf_node &source = data.nodes[nodeIndex];
        if (source.skin != nullptr && source.mesh != nullptr)
        {
            error = "node " + std::to_string(nodeIndex) + " uses unsupported visible mesh skinning";
            return false;
        }
        if (source.has_mesh_gpu_instancing)
        {
            error = "node " + std::to_string(nodeIndex) + " uses unsupported GPU instancing";
            return false;
        }

        ModelNode &node = asset.nodes[nodeIndex];
        node.name = source.name != nullptr ? source.name : "";
        node.parentIndex = source.parent != nullptr ? static_cast<int>(cgltf_node_index(&data, source.parent)) : -1;
        node.meshIndex = source.mesh != nullptr ? static_cast<int>(cgltf_mesh_index(&data, source.mesh)) : -1;
        node.usesMatrix = source.has_matrix != 0;
        if (node.usesMatrix)
        {
            std::copy_n(source.matrix, 16, node.matrix.begin());
        }
        else
        {
            std::copy_n(source.translation, 3, node.transform.translation.begin());
            std::copy_n(source.rotation, 4, node.transform.rotation.begin());
            std::copy_n(source.scale, 3, node.transform.scale.begin());
            node.matrix = composeModelTransform(node.transform);
        }
        node.childIndices.reserve(source.children_count);
        for (size_t childIndex = 0; childIndex < source.children_count; ++childIndex)
        {
            node.childIndices.push_back(static_cast<uint32_t>(cgltf_node_index(&data, source.children[childIndex])));
        }
        if (!node.name.empty())
        {
            const auto [iterator, inserted] = asset.nodeIndicesByName.emplace(node.name, nodeIndex);
            if (!inserted)
            {
                error = "duplicate model node name: " + node.name + " (indices " +
                    std::to_string(iterator->second) + " and " + std::to_string(nodeIndex) + ")";
                return false;
            }
        }
    }
    return buildHierarchy(asset, error);
}

bool loadAnimations(const cgltf_data &data, ModelAsset &asset, std::string &error)
{
    asset.clips.reserve(data.animations_count);
    for (size_t animationIndex = 0; animationIndex < data.animations_count; ++animationIndex)
    {
        const cgltf_animation &sourceAnimation = data.animations[animationIndex];
        ModelAnimationClip clip;
        clip.name = sourceAnimation.name != nullptr && sourceAnimation.name[0] != '\0'
            ? sourceAnimation.name : "clip_" + std::to_string(animationIndex);
        if (!asset.clipIndicesByName.emplace(clip.name, animationIndex).second)
        {
            error = "duplicate model animation name: " + clip.name;
            return false;
        }
        std::unordered_set<uint64_t> animatedTargets;
        clip.channels.reserve(sourceAnimation.channels_count);
        for (size_t channelIndex = 0; channelIndex < sourceAnimation.channels_count; ++channelIndex)
        {
            const cgltf_animation_channel &sourceChannel = sourceAnimation.channels[channelIndex];
            if (sourceChannel.target_node == nullptr || sourceChannel.sampler == nullptr)
            {
                error = "animation " + clip.name + " contains an unbound channel";
                return false;
            }
            if (sourceChannel.sampler->interpolation == cgltf_interpolation_type_cubic_spline)
            {
                error = "animation " + clip.name + " uses unsupported CUBICSPLINE interpolation";
                return false;
            }
            if (sourceChannel.sampler->interpolation != cgltf_interpolation_type_linear &&
                sourceChannel.sampler->interpolation != cgltf_interpolation_type_step)
            {
                error = "animation " + clip.name + " uses an invalid interpolation mode";
                return false;
            }

            ModelAnimationChannel channel;
            channel.nodeIndex = static_cast<uint32_t>(cgltf_node_index(&data, sourceChannel.target_node));
            channel.interpolation = sourceChannel.sampler->interpolation == cgltf_interpolation_type_step
                ? ModelAnimationInterpolation::Step : ModelAnimationInterpolation::Linear;
            size_t valueComponentCount = 0;
            switch (sourceChannel.target_path)
            {
            case cgltf_animation_path_type_translation:
                channel.target = ModelAnimationTarget::Translation;
                valueComponentCount = 3;
                break;
            case cgltf_animation_path_type_rotation:
                channel.target = ModelAnimationTarget::Rotation;
                valueComponentCount = 4;
                break;
            case cgltf_animation_path_type_scale:
                channel.target = ModelAnimationTarget::Scale;
                valueComponentCount = 3;
                break;
            case cgltf_animation_path_type_weights:
                error = "animation " + clip.name + " uses unsupported morph weights";
                return false;
            default:
                error = "animation " + clip.name + " uses an invalid target path";
                return false;
            }
            if (asset.nodes[channel.nodeIndex].usesMatrix)
            {
                error = "animation " + clip.name + " targets matrix node " +
                    std::to_string(channel.nodeIndex) + " with TRS data";
                return false;
            }
            const uint64_t targetKey = static_cast<uint64_t>(channel.nodeIndex) * 4 +
                static_cast<uint64_t>(channel.target);
            if (!animatedTargets.insert(targetKey).second)
            {
                error = "animation " + clip.name + " contains duplicate channels for one node property";
                return false;
            }

            const cgltf_accessor *pTimes = sourceChannel.sampler->input;
            const cgltf_accessor *pValues = sourceChannel.sampler->output;
            if (pTimes == nullptr || pValues == nullptr || pTimes->type != cgltf_type_scalar ||
                pTimes->component_type != cgltf_component_type_r_32f || pTimes->count == 0 ||
                pValues->count != pTimes->count)
            {
                error = "animation " + clip.name + " has invalid sampler accessors";
                return false;
            }
            if ((valueComponentCount == 3 && pValues->type != cgltf_type_vec3) ||
                (valueComponentCount == 4 && pValues->type != cgltf_type_vec4))
            {
                error = "animation " + clip.name + " has an invalid output accessor type";
                return false;
            }

            channel.times.resize(pTimes->count);
            channel.values.resize(pTimes->count);
            for (size_t keyIndex = 0; keyIndex < pTimes->count; ++keyIndex)
            {
                if (!readAccessor(*pTimes, keyIndex, 1, &channel.times[keyIndex]) ||
                    !readAccessor(*pValues, keyIndex, valueComponentCount, channel.values[keyIndex].data()))
                {
                    error = "animation " + clip.name + " contains unreadable keyframe data";
                    return false;
                }
                if (!std::isfinite(channel.times[keyIndex]) || channel.times[keyIndex] < 0.0f ||
                    (keyIndex != 0 && channel.times[keyIndex] <= channel.times[keyIndex - 1]))
                {
                    error = "animation " + clip.name + " keyframe times are not finite and strictly increasing";
                    return false;
                }
                for (size_t component = 0; component < valueComponentCount; ++component)
                {
                    if (!std::isfinite(channel.values[keyIndex][component]))
                    {
                        error = "animation " + clip.name + " contains a non-finite keyframe value";
                        return false;
                    }
                }
            }
            clip.durationSeconds = std::max(clip.durationSeconds, channel.times.back());
            clip.channels.push_back(std::move(channel));
        }
        asset.clips.push_back(std::move(clip));
    }
    return true;
}

void calculateStaticBounds(ModelAsset &asset)
{
    std::vector<ModelMatrix> globalMatrices(asset.nodes.size(), identityModelMatrix());
    for (uint32_t nodeIndex : asset.hierarchyOrder)
    {
        const ModelNode &node = asset.nodes[nodeIndex];
        globalMatrices[nodeIndex] = node.parentIndex >= 0
            ? multiplyModelMatrices(globalMatrices[node.parentIndex], node.matrix) : node.matrix;
        if (node.meshIndex < 0)
        {
            continue;
        }
        for (const ModelPrimitive &primitive : asset.meshes[node.meshIndex].primitives)
        {
            for (const ModelVertex &vertex : primitive.vertices)
            {
                expandBounds(asset.staticBounds, transformPoint(globalMatrices[nodeIndex], vertex.position));
            }
        }
    }
}
}

ModelLoadResult GltfModelLoader::load(
    const AssetFileSystem &assetFileSystem,
    const std::string &virtualPath
) const
{
    ModelLoadResult result;
    const std::optional<std::vector<uint8_t>> sourceBytes = assetFileSystem.readBinaryFile(virtualPath);
    if (!sourceBytes)
    {
        result.error = "model asset was not found: " + virtualPath;
        return result;
    }

    AssetFileContext fileContext = {&assetFileSystem};
    cgltf_options options = {};
    options.file.read = readAssetFile;
    options.file.release = releaseAssetFile;
    options.file.user_data = &fileContext;
    cgltf_data *pParsedData = nullptr;
    cgltf_result parseResult = cgltf_parse(&options, sourceBytes->data(), sourceBytes->size(), &pParsedData);
    if (parseResult != cgltf_result_success || pParsedData == nullptr)
    {
        result.error = "could not parse model " + virtualPath + ": " + cgltfResultName(parseResult);
        return result;
    }
    std::unique_ptr<cgltf_data, CgltfDeleter> data(pParsedData);

    if (data->extensions_required_count != 0)
    {
        result.error = "model " + virtualPath + " requires unsupported glTF extension " +
            std::string(data->extensions_required[0]);
        return result;
    }
    for (size_t bufferIndex = 0; bufferIndex < data->buffers_count; ++bufferIndex)
    {
        if (!isSafeRelativeUri(data->buffers[bufferIndex].uri))
        {
            result.error = "model buffer URI must be package-relative: " +
                std::string(data->buffers[bufferIndex].uri);
            return result;
        }
    }
    for (size_t imageIndex = 0; imageIndex < data->images_count; ++imageIndex)
    {
        if (!isSafeRelativeUri(data->images[imageIndex].uri))
        {
            result.error = "model image URI must be package-relative: " + std::string(data->images[imageIndex].uri);
            return result;
        }
    }

    const cgltf_result bufferResult = cgltf_load_buffers(&options, data.get(), virtualPath.c_str());
    if (bufferResult != cgltf_result_success)
    {
        result.error = "could not load buffers for model " + virtualPath + ": " + cgltfResultName(bufferResult);
        return result;
    }
    if (!rejectUnsupportedFeatures(*data, result.error))
    {
        return result;
    }
    const cgltf_result validationResult = cgltf_validate(data.get());
    if (validationResult != cgltf_result_success)
    {
        result.error = "model " + virtualPath + " failed glTF validation: " + cgltfResultName(validationResult);
        return result;
    }

    std::shared_ptr<ModelAsset> asset = std::make_shared<ModelAsset>();
    asset->sourcePath = virtualPath;
    if (!loadImages(assetFileSystem, virtualPath, *data, *asset, result.error) ||
        !loadMaterials(*data, *asset, result.error) ||
        !loadMeshes(*data, *asset, result.error) ||
        !loadNodes(*data, *asset, result.error) ||
        !loadAnimations(*data, *asset, result.error))
    {
        return result;
    }
    calculateStaticBounds(*asset);
    result.asset = std::move(asset);
    return result;
}

ModelLoadResult ModelAssetCache::load(
    const AssetFileSystem &assetFileSystem,
    const std::string &virtualPath
)
{
    const auto iterator = m_assets.find(virtualPath);
    if (iterator != m_assets.end())
    {
        return {iterator->second, {}, {}};
    }
    ModelLoadResult result = m_loader.load(assetFileSystem, virtualPath);
    if (result)
    {
        m_assets.emplace(virtualPath, result.asset);
    }
    return result;
}

void ModelAssetCache::clear()
{
    m_assets.clear();
}

size_t ModelAssetCache::size() const
{
    return m_assets.size();
}
}
