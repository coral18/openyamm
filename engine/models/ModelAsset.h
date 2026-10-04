#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Engine
{
using ModelMatrix = std::array<float, 16>;

struct ModelTransform
{
    std::array<float, 3> translation = {0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 3> scale = {1.0f, 1.0f, 1.0f};
};

struct ModelVertex
{
    std::array<float, 3> position = {};
    std::array<float, 3> normal = {};
    std::array<float, 2> texCoord = {};
};

enum class ModelAlphaMode
{
    Opaque,
    Mask,
    Blend
};

struct ModelImage
{
    std::string name;
    std::string sourcePath;
    std::vector<uint8_t> pngBytes;
};

struct ModelMaterial
{
    std::string name;
    std::array<float, 4> baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    int imageIndex = -1;
    ModelAlphaMode alphaMode = ModelAlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    bool unlit = false;
};

struct ModelPrimitive
{
    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;
    int materialIndex = -1;
};

struct ModelMesh
{
    std::string name;
    std::vector<ModelPrimitive> primitives;
};

struct ModelNode
{
    std::string name;
    int parentIndex = -1;
    std::vector<uint32_t> childIndices;
    int meshIndex = -1;
    bool usesMatrix = false;
    ModelTransform transform;
    ModelMatrix matrix = {};
};

enum class ModelAnimationInterpolation
{
    Step,
    Linear
};

enum class ModelAnimationTarget
{
    Translation,
    Rotation,
    Scale
};

struct ModelAnimationChannel
{
    uint32_t nodeIndex = 0;
    ModelAnimationTarget target = ModelAnimationTarget::Translation;
    ModelAnimationInterpolation interpolation = ModelAnimationInterpolation::Linear;
    std::vector<float> times;
    std::vector<std::array<float, 4>> values;
};

struct ModelAnimationClip
{
    std::string name;
    float durationSeconds = 0.0f;
    std::vector<ModelAnimationChannel> channels;
};

struct ModelBounds
{
    std::array<float, 3> min = {};
    std::array<float, 3> max = {};
    bool valid = false;
};

struct ModelAsset
{
    std::string sourcePath;
    std::vector<ModelImage> images;
    std::vector<ModelMaterial> materials;
    std::vector<ModelMesh> meshes;
    std::vector<ModelNode> nodes;
    std::vector<uint32_t> hierarchyOrder;
    std::vector<ModelAnimationClip> clips;
    std::unordered_map<std::string, uint32_t> nodeIndicesByName;
    std::unordered_map<std::string, uint32_t> clipIndicesByName;
    ModelBounds staticBounds;

    std::optional<uint32_t> findNode(const std::string &name) const;
    std::optional<uint32_t> findClip(const std::string &name) const;
};

ModelMatrix identityModelMatrix();
ModelMatrix composeModelTransform(const ModelTransform &transform);
ModelMatrix multiplyModelMatrices(const ModelMatrix &left, const ModelMatrix &right);
ModelTransform gltfModelPlacement(
    const std::array<float, 3> &openYammPosition,
    float yawRadians = 0.0f,
    float uniformScale = 1.0f);
ModelTransform gltfModelPlacement(
    const std::array<float, 3> &openYammPosition,
    const std::array<float, 4> &openYammRotation,
    float uniformScale = 1.0f);
}
