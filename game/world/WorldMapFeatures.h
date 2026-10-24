#pragma once

#include "game/world/WorldInteractionSites.h"
#include "game/world/WorldMapKnowledge.h"
#include "game/events/SiteInteractionReference.h"
#include <optional>
#include <string_view>
#include <unordered_map>

namespace OpenYAMM::Game
{
class ScriptedEventProgram;
struct EventRuntimeState;

enum class WorldMapFeatureKind { Chest, Water, Shrine, Obelisk, Connection, Monster, Note, Item, Decoration };

struct WorldMonsterFeature
{
    std::vector<int16_t> definitions;
    int minCount = 0, maxCount = 0;
    uint16_t radius = 0;
    bool observed = false;
    bool dead = false;
    int32_t gameMinutes = 0;
    bool placed = false;
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
    std::vector<uint32_t> autonoteIds;
    std::string guideReference;
    std::string targetMap;
    std::string targetMapId;
    std::optional<WorldMonsterFeature> monster;
    uint32_t noteId = 0;
    bool userNote = false;
    uint32_t itemId = 0;
    bool observedItem = false;
    std::string unidentifiedTitle;
    std::optional<int32_t> noteElevation;
    std::optional<WorldMapArrival> arrival;
    std::vector<uint16_t> relatedEventIds;
    std::unordered_map<uint16_t, std::vector<uint32_t>> relatedAutonotes;
    std::optional<SiteInteractionReference> interactionReference;
    // Explicit components of one authored passage. The first real site anchors its stable ID and marker.
    std::vector<WorldInteractionSite> components;
};

std::vector<WorldMapFeature> buildWorldMapFeatures(const std::string &mapId,
    const std::vector<WorldInteractionSite> &sites, const ScriptedEventProgram &events);
std::optional<WorldMapFeatureKind> worldMapFeatureKind(std::string_view action);
const char *worldMapFeatureKindName(WorldMapFeatureKind kind);
bool worldMapFeatureOnFloorLayer(const WorldMapFeature &feature, std::optional<int32_t> layer);
bool worldMapFeatureContainsSite(const WorldMapFeature &feature, const std::string &siteId);
std::string worldSiteDiscoveryKey(uint16_t eventId, const std::string &siteId);
void recordWorldSiteDiscovery(EventRuntimeState &state, uint16_t eventId, const std::string &siteId);
}
