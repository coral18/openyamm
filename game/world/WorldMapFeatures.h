#pragma once

#include "game/world/WorldInteractionSites.h"
#include <optional>

namespace OpenYAMM::Game
{
class ScriptedEventProgram;
struct EventRuntimeState;

enum class WorldMapFeatureKind { Chest, Water, Shrine, Obelisk, Connection, Monster, Note, Item };

struct WorldMonsterFeature
{
    std::vector<int16_t> definitions;
    int minCount = 0, maxCount = 0;
    uint16_t radius = 0;
    bool observed = false;
    bool dead = false;
    int32_t gameMinutes = 0;
};

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
    std::optional<WorldMonsterFeature> monster;
    uint32_t noteId = 0;
    bool userNote = false;
    uint32_t itemId = 0;
    bool observedItem = false;
};

std::vector<WorldMapFeature> buildWorldMapFeatures(const std::string &mapId,
    const std::vector<WorldInteractionSite> &sites, const ScriptedEventProgram &events);
const char *worldMapFeatureKindName(WorldMapFeatureKind kind);
std::string worldSiteDiscoveryKey(uint16_t eventId, const std::string &siteId);
void recordWorldSiteDiscovery(EventRuntimeState &state, uint16_t eventId, const std::string &siteId);
}
