#include "game/world/WorldAtlasPresentation.h"

#include <yaml-cpp/yaml.h>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace OpenYAMM::Game
{
WorldAtlasPresentation parseWorldAtlasPresentation(const std::string &text, const std::string &worldId)
{
    const YAML::Node root = YAML::Load(text);
    if (!root.IsMap() || root["schema"].as<int>() != 1 || root["world"].as<std::string>() != worldId
        || !root["maps"].IsSequence())
        throw std::runtime_error("Unsupported atlas presentation schema for " + worldId);
    WorldAtlasPresentation result;
    result.worldId = worldId;
    result.name = root["name"].as<std::string>();
    result.overviewTexture = root["overview_texture"].as<std::string>("");
    if (result.name.empty())
        throw std::runtime_error("Missing atlas world name");
    std::unordered_set<std::string> files;
    for (const YAML::Node &node : root["maps"])
    {
        WorldMapPresentation map;
        map.fileName = node["file"].as<std::string>();
        if (map.fileName.empty() || !files.insert(map.fileName).second)
            throw std::runtime_error("Empty or duplicate atlas map");
        map.areaId = node["area"].as<int>(0);
        map.parentFileName = node["parent"].as<std::string>("");
        if (node["position"])
        {
            if (!node["position"].IsSequence() || node["position"].size() != 2)
                throw std::runtime_error("Invalid atlas map position");
            const float x = node["position"][0].as<float>(), y = node["position"][1].as<float>();
            if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || x > 1 || y < 0 || y > 1)
                throw std::runtime_error("Atlas map position must be within [0, 1]");
            map.position = {x, y};
        }
        result.maps.push_back(std::move(map));
    }
    return result;
}

WorldAtlasPresentation loadWorldAtlasPresentation(const Engine::AssetFileSystem &assets, const std::string &worldId)
{
    const std::string path = "engine/world/atlas/" + worldId + ".yml";
    const auto text = assets.readTextFile(path);
    if (!text)
        throw std::runtime_error("Missing atlas presentation: " + path);
    return parseWorldAtlasPresentation(*text, worldId);
}
}
