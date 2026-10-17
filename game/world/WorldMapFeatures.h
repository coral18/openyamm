#pragma once

#include "game/world/WorldInteractionSites.h"

namespace OpenYAMM::Game
{
class ScriptedEventProgram;
struct EventRuntimeState;

enum class WorldMapFeatureKind { Chest, Water, Shrine, Obelisk, Connection };

// A derived view of event metadata at an actual map site, not a second authored content registry.
struct WorldMapFeature
{
    std::string id;
    std::string mapId;
    WorldMapFeatureKind kind = WorldMapFeatureKind::Chest;
    WorldInteractionSite site;
    std::string title;
    std::vector<uint32_t> chestIds;
    std::string targetMap;
};

std::vector<WorldMapFeature> buildWorldMapFeatures(const std::string &mapId,
    const std::vector<WorldInteractionSite> &sites, const ScriptedEventProgram &events);
const char *worldMapFeatureKindName(WorldMapFeatureKind kind);
std::string worldSiteDiscoveryKey(uint16_t eventId, const std::string &siteId);
void recordWorldSiteDiscovery(EventRuntimeState &state, uint16_t eventId, const std::string &siteId);
}
