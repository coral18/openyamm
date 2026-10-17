#pragma once

#include "game/world/WorldDatabase.h"
#include "engine/AssetFileSystem.h"

namespace OpenYAMM::Game
{
struct WorldAtlasPresentation
{
    std::string worldId;
    std::string name;
    // Empty means a native overview assembled from discovered regional maps.
    std::string overviewTexture;
    std::vector<WorldMapPresentation> maps;
};

WorldAtlasPresentation parseWorldAtlasPresentation(const std::string &text, const std::string &worldId);
WorldAtlasPresentation loadWorldAtlasPresentation(const Engine::AssetFileSystem &assets, const std::string &worldId);
}
