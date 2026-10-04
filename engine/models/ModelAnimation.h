#pragma once

#include "engine/models/ModelAsset.h"

#include <vector>

namespace OpenYAMM::Engine
{
struct ModelPose
{
    std::vector<ModelTransform> localTransforms;
    std::vector<ModelMatrix> localMatrices;
    std::vector<ModelMatrix> globalMatrices;
};

void resetModelPose(const ModelAsset &asset, ModelPose &pose);
void evaluateModelClip(const ModelAsset &asset, uint32_t clipIndex, float timeSeconds, ModelPose &pose);
void evaluateModelHierarchy(const ModelAsset &asset, const ModelMatrix &rootMatrix, ModelPose &pose);
}
