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

void sampleChannel(const ModelAnimationChannel &channel, float timeSeconds, ModelTransform &transform)
{
    size_t rightIndex =
        std::upper_bound(channel.times.begin(), channel.times.end(), timeSeconds) - channel.times.begin();
    if (rightIndex == 0)
    {
        rightIndex = 1;
    }
    if (rightIndex >= channel.times.size())
    {
        rightIndex = channel.times.size() - 1;
    }
    const size_t leftIndex = rightIndex - 1;
    float amount = 0.0f;
    if (channel.interpolation == ModelAnimationInterpolation::Linear && rightIndex != leftIndex)
    {
        const float interval = channel.times[rightIndex] - channel.times[leftIndex];
        amount = interval > 0.0f ? (timeSeconds - channel.times[leftIndex]) / interval : 0.0f;
        amount = std::clamp(amount, 0.0f, 1.0f);
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
    for (size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex)
    {
        const ModelNode &node = asset.nodes[nodeIndex];
        pose.localTransforms[nodeIndex] = node.transform;
        pose.localMatrices[nodeIndex] = node.matrix;
        pose.globalMatrices[nodeIndex] = identityModelMatrix();
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
        if (channel.times.size() == 1)
        {
            ModelTransform &transform = pose.localTransforms[channel.nodeIndex];
            if (channel.target == ModelAnimationTarget::Translation)
            {
                std::copy_n(channel.values[0].begin(), 3, transform.translation.begin());
            }
            else if (channel.target == ModelAnimationTarget::Scale)
            {
                std::copy_n(channel.values[0].begin(), 3, transform.scale.begin());
            }
            else
            {
                transform.rotation = normalizedQuaternion(channel.values[0]);
            }
        }
        else
        {
            sampleChannel(channel, timeSeconds, pose.localTransforms[channel.nodeIndex]);
        }
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
}
