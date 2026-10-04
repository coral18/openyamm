#pragma once

#include "game/fx/EffectDefinition.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Engine
{
class AssetFileSystem;
}

namespace OpenYAMM::Game
{
class EffectDefinitionLoader
{
public:
    explicit EffectDefinitionLoader(const Engine::AssetFileSystem *pAssetFileSystem);

    std::optional<std::vector<std::shared_ptr<const EffectDefinition>>> load(
        const std::string &virtualPath,
        std::string &error) const;

    static std::optional<std::vector<std::shared_ptr<const EffectDefinition>>> parse(
        const std::string &text,
        std::string &error);

private:
    const Engine::AssetFileSystem *m_pAssetFileSystem = nullptr;
};
}
