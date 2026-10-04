#pragma once

#include "game/fx/EffectDefinition.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Engine
{
class AssetFileSystem;
class ModelAssetCache;
struct ModelAsset;
}

namespace OpenYAMM::Game
{
struct EffectSpriteResource
{
    std::string id;
    uint32_t framesPerSecond = 0;
    uint32_t logicalWidth = 0;
    uint32_t logicalHeight = 0;
    std::vector<std::string> framePaths;
};

class EffectResourceLibrary
{
public:
    bool load(
        const Engine::AssetFileSystem &assetFileSystem,
        const std::string &bindingManifestPath,
        const std::vector<std::shared_ptr<const EffectDefinition>> &definitions,
        std::string &error,
        Engine::ModelAssetCache *pModelAssets = nullptr);
    void clear();

    const EffectSpriteResource *findSprite(const std::string &id) const;
    std::shared_ptr<const Engine::ModelAsset> findModel(const std::string &id) const;
    std::optional<std::string> findAssetPath(const std::string &id) const;
    size_t spriteCount() const;

private:
    std::unordered_map<std::string, std::string> m_assetPaths;
    std::unordered_map<std::string, EffectSpriteResource> m_sprites;
    std::unordered_map<std::string, std::shared_ptr<const Engine::ModelAsset>> m_models;
};
}
