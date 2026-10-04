#pragma once

#include "engine/models/ModelAsset.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Engine
{
class AssetFileSystem;

struct ModelLoadResult
{
    std::shared_ptr<const ModelAsset> asset;
    std::string error;
    std::vector<std::string> warnings;

    explicit operator bool() const
    {
        return asset != nullptr;
    }
};

class GltfModelLoader
{
public:
    ModelLoadResult load(const AssetFileSystem &assetFileSystem, const std::string &virtualPath) const;
};

class ModelAssetCache
{
public:
    ModelLoadResult load(const AssetFileSystem &assetFileSystem, const std::string &virtualPath);
    void clear();
    size_t size() const;

private:
    GltfModelLoader m_loader;
    std::unordered_map<std::string, std::shared_ptr<const ModelAsset>> m_assets;
};
}
