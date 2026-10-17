#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
struct OutdoorMapData;
struct IndoorMapData;

// A physical interactive surface/component or decoration, before interpreting its event.
struct WorldInteractionSite
{
    std::string id;
    uint16_t eventId = 0;
    float x = 0, y = 0, z = 0;
    std::vector<uint32_t> faceIds;
    std::string surfacePrefix;
};

std::vector<WorldInteractionSite> worldInteractionSites(const OutdoorMapData &map);
std::vector<WorldInteractionSite> worldInteractionSites(const IndoorMapData &map);
}
