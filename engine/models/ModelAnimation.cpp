#include "engine/models/ModelAnimation.h"

#include <algorithm>
#include <cmath>

namespace OpenYAMM::Engine
{
namespace
{
std::array<float, 4> normalizedQuaternion(const std::array<float, 4> &value)
{
    const float length = std::sqrt(
        value[0] * value[0] + value[1] * value[1] + value[2] * value[2] + value[3] * value[3]);
    if (length <= 0.0f)
    {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }
    return {value[0] / length, value[1] / length, value[2] / length, value[3] / length};
}

std::array<float, 4> interpolateQuaternion(
    const std::array<float, 4> &leftValue,
    const std::array<float, 4> &rightValue,
    float amount
)
{
    std::array<float, 4> left = normalizedQuaternion(leftValue);
    std::array<float, 4> right = normalizedQuaternion(rightValue);
    float dot = left[0] * right[0] + left[1] * right[1] + left[2] * right[2] + left[3] * right[3];
    if (dot < 0.0f)
    {
        dot = -dot;
        for (float &component : right)
        {
            component = -component;
        }
    }
    dot = std::clamp(dot, -1.0f, 1.0f);
    if (dot > 0.9995f)
    {
        std::array<float, 4> result = {};
        for (size_t component = 0; component < result.size(); ++component)
        {
            result[component] = left[component] + (right[component] - left[component]) * amount;
        }
        return normalizedQuaternion(result);
    }

    const float angle = std::acos(dot);
    const float denominator = std::sin(angle);
    const float leftWeight = std::sin((1.0f - amount) * angle) / denominator;
    const float rightWeight = std::sin(amount * angle) / denominator;
    std::array<float, 4> result = {};
    for (size_t component = 0; component < result.size(); ++component)
    {
        result[component] = left[component] * leftWeight + right[component] * rightWeight;
    }
    return normalizedQuaternion(result);
}

void sampleChannel(const ModelAnimationChannel &channel, float timeSeconds, ModelTransform &transform,
    std::vector<float> &weights)
{
    size_t rightIndex =
        std::upper_bound(channel.times.begin(), channel.times.end(), timeSeconds) - channel.times.begin();
    const size_t leftIndex = rightIndex == 0 ? 0 : rightIndex - 1;
    rightIndex = std::min(rightIndex, channel.times.size() - 1);
    float amount = 0.0f;
    if (channel.interpolation == ModelAnimationInterpolation::Linear && rightIndex != leftIndex)
    {
        const float interval = channel.times[rightIndex] - channel.times[leftIndex];
        amount = interval > 0.0f ? (timeSeconds - channel.times[leftIndex]) / interval : 0.0f;
        amount = std::clamp(amount, 0.0f, 1.0f);
    }

    if (channel.target == ModelAnimationTarget::Weights)
    {
        for (size_t i = 0; i < weights.size(); ++i)
        {
            weights[i] = channel.weightValues[leftIndex][i]
                + (channel.weightValues[rightIndex][i] - channel.weightValues[leftIndex][i]) * amount;
        }
        return;
    }
    const std::array<float, 4> &left = channel.values[leftIndex];
    const std::array<float, 4> &right = channel.values[rightIndex];
    if (channel.target == ModelAnimationTarget::Rotation)
    {
        transform.rotation = channel.interpolation == ModelAnimationInterpolation::Step
            ? normalizedQuaternion(left) : interpolateQuaternion(left, right, amount);
        return;
    }

    std::array<float, 3> *pOutput = channel.target == ModelAnimationTarget::Translation
        ? &transform.translation : &transform.scale;
    for (size_t component = 0; component < 3; ++component)
    {
        (*pOutput)[component] = left[component] + (right[component] - left[component]) * amount;
    }
}
}

void resetModelPose(const ModelAsset &asset, ModelPose &pose)
{
    pose.localTransforms.resize(asset.nodes.size());
    pose.localMatrices.resize(asset.nodes.size());
    pose.globalMatrices.resize(asset.nodes.size());
    pose.morphWeights.resize(asset.nodes.size());
    for (size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex)
    {
        const ModelNode &node = asset.nodes[nodeIndex];
        pose.localTransforms[nodeIndex] = node.transform;
        pose.localMatrices[nodeIndex] = node.matrix;
        pose.globalMatrices[nodeIndex] = identityModelMatrix();
        pose.morphWeights[nodeIndex] = node.weights;
    }
}

void evaluateModelClip(const ModelAsset &asset, uint32_t clipIndex, float timeSeconds, ModelPose &pose)
{
    resetModelPose(asset, pose);
    if (clipIndex >= asset.clips.size())
    {
        return;
    }
    const ModelAnimationClip &clip = asset.clips[clipIndex];
    for (const ModelAnimationChannel &channel : clip.channels)
    {
        sampleChannel(channel, timeSeconds, pose.localTransforms[channel.nodeIndex],
            pose.morphWeights[channel.nodeIndex]);
    }
    for (size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex)
    {
        if (!asset.nodes[nodeIndex].usesMatrix)
        {
            pose.localMatrices[nodeIndex] = composeModelTransform(pose.localTransforms[nodeIndex]);
        }
    }
}

void evaluateModelHierarchy(const ModelAsset &asset, const ModelMatrix &rootMatrix, ModelPose &pose)
{
    for (uint32_t nodeIndex : asset.hierarchyOrder)
    {
        const ModelNode &node = asset.nodes[nodeIndex];
        pose.globalMatrices[nodeIndex] = node.parentIndex >= 0
            ? multiplyModelMatrices(pose.globalMatrices[node.parentIndex], pose.localMatrices[nodeIndex])
            : multiplyModelMatrices(rootMatrix, pose.localMatrices[nodeIndex]);
    }
}

void deformModelPose(const ModelAsset &asset, ModelPose &pose)
{
    // ponytail: CPU deformation suits this three-actor trial; use GPU skinning for large crowds.
    pose.deformedVertices.resize(asset.nodes.size());
    for (size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex)
    {
        const ModelNode &node = asset.nodes[nodeIndex];
        if (node.meshIndex < 0)
        {
            continue;
        }
        const ModelMesh &mesh = asset.meshes[node.meshIndex];
        std::vector<ModelMatrix> joints, normals;
        if (node.skinIndex >= 0)
        {
            const ModelSkin &skin = asset.skins[node.skinIndex];
            for (size_t i = 0; i < skin.joints.size(); ++i)
            {
                joints.push_back(multiplyModelMatrices(
                    pose.globalMatrices[skin.joints[i]], skin.inverseBindMatrices[i]));
                normals.push_back(modelNormalMatrix(joints.back()));
            }
        }
        pose.deformedVertices[nodeIndex].resize(mesh.primitives.size());
        for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex)
        {
            const ModelPrimitive &primitive = mesh.primitives[primitiveIndex];
            if (node.skinIndex < 0 && primitive.morphTargets.empty())
            {
                continue;
            }
            std::vector<ModelVertex> &vertices = pose.deformedVertices[nodeIndex][primitiveIndex];
            if (!modelMatrixVisible(pose.globalMatrices[nodeIndex]))
            {
                vertices.clear();
                continue;
            }
            vertices = primitive.vertices;
            for (size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex)
            {
                ModelVertex &vertex = vertices[vertexIndex];
                for (size_t targetIndex = 0; targetIndex < primitive.morphTargets.size(); ++targetIndex)
                {
                    const float weight = pose.morphWeights[nodeIndex][targetIndex];
                    if (weight == 0.0f)
                    {
                        continue;
                    }
                    const ModelMorphTarget &target = primitive.morphTargets[targetIndex];
                    for (size_t axis = 0; axis < 3; ++axis)
                    {
                        if (!target.positions.empty())
                        {
                            vertex.position[axis] += weight * target.positions[vertexIndex][axis];
                        }
                        if (!target.normals.empty())
                        {
                            vertex.normal[axis] += weight * target.normals[vertexIndex][axis];
                        }
                    }
                }
                if (node.skinIndex >= 0)
                {
                    const ModelVertexInfluences &influences = primitive.influences[vertexIndex];
                    const ModelVertex original = vertex;
                    vertex.position = {};
                    vertex.normal = {};
                    for (size_t i = 0; i < influences.weights.size(); ++i)
                    {
                        const float weight = influences.weights[i];
                        if (weight == 0.0f)
                        {
                            continue;
                        }
                        const ModelMatrix &joint = joints[influences.joints[i]];
                        const ModelMatrix &normal = normals[influences.joints[i]];
                        for (size_t axis = 0; axis < 3; ++axis)
                        {
                            vertex.position[axis] += weight * (joint[axis] * original.position[0]
                                + joint[4 + axis] * original.position[1] + joint[8 + axis] * original.position[2]
                                + joint[12 + axis]);
                            vertex.normal[axis] += weight * (normal[axis] * original.normal[0]
                                + normal[4 + axis] * original.normal[1] + normal[8 + axis] * original.normal[2]);
                        }
                    }
                }
                const float length = std::sqrt(vertex.normal[0] * vertex.normal[0]
                    + vertex.normal[1] * vertex.normal[1] + vertex.normal[2] * vertex.normal[2]);
                if (length > 0.0f)
                {
                    for (float &component : vertex.normal)
                    {
                        component /= length;
                    }
                }
            }
        }
    }
}
}
