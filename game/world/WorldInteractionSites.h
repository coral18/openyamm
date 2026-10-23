#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
struct OutdoorMapData;
struct IndoorMapData;
class DecorationTable;
struct MapDeltaData;

// A physical interactive surface/component or decoration, before interpreting its event.
struct WorldInteractionSite
{
    std::string id;
    uint16_t eventId = 0;
    float x = 0, y = 0, z = 0;
    std::vector<uint32_t> faceIds;
    std::string surfacePrefix;
};

// Possible shared callbacks for one inherited decoration. No state is seeded or loot rolled.
struct WorldDecorationSite
{
    WorldInteractionSite site;
    uint8_t eventCount = 0;
    uint16_t initialEventId = 0;
    bool initiallyDepleted = false;
    std::string name;
};

std::vector<WorldDecorationSite> worldInheritedDecorationSites(const OutdoorMapData &map,
    const DecorationTable &decorations, const MapDeltaData *pInitialState = nullptr);
std::vector<WorldDecorationSite> worldInheritedDecorationSites(const IndoorMapData &map,
    const DecorationTable &decorations, const MapDeltaData *pInitialState = nullptr);
std::vector<WorldInteractionSite> worldInteractionSites(const OutdoorMapData &map);
std::vector<WorldInteractionSite> worldInteractionSites(const IndoorMapData &map);
}
