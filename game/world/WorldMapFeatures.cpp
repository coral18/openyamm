#include "game/world/WorldMapFeatures.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/events/EventRuntime.h"

#include <optional>

namespace OpenYAMM::Game
{
const char *worldMapFeatureKindName(WorldMapFeatureKind kind)
{
    switch (kind)
    {
    case WorldMapFeatureKind::Chest: return "Chest";
    case WorldMapFeatureKind::Water: return "Well / fountain";
    case WorldMapFeatureKind::Shrine: return "Shrine";
    case WorldMapFeatureKind::Obelisk: return "Obelisk";
    case WorldMapFeatureKind::Connection: return "Passage / teleport";
    }
    return "Site";
}

std::vector<WorldMapFeature> buildWorldMapFeatures(const std::string &mapId,
    const std::vector<WorldInteractionSite> &sites, const ScriptedEventProgram &events)
{
    std::vector<WorldMapFeature> features;
    for (const WorldInteractionSite &site : sites)
    {
        const auto metadata = events.getContextActionMetadata(site.eventId);
        if (!metadata || metadata->hidden)
            continue;
        std::optional<WorldMapFeatureKind> kind;
        if (metadata->kind == "open_chest" && !metadata->chestIds.empty()) kind = WorldMapFeatureKind::Chest;
        else if (metadata->kind == "well" || metadata->kind == "fountain") kind = WorldMapFeatureKind::Water;
        else if (metadata->kind == "shrine") kind = WorldMapFeatureKind::Shrine;
        else if (metadata->kind == "obelisk") kind = WorldMapFeatureKind::Obelisk;
        else if (metadata->kind == "teleport" || metadata->kind == "passage" || metadata->kind == "travel"
            || metadata->kind == "enter_dungeon" || metadata->kind == "leave_dungeon")
            kind = WorldMapFeatureKind::Connection;
        if (!kind)
            continue;
        WorldMapFeature feature;
        feature.id = mapId + "/site/" + site.id + "/event/" + std::to_string(site.eventId);
        feature.mapId = mapId;
        feature.kind = *kind;
        feature.site = site;
        feature.title = events.getHint(site.eventId).value_or(worldMapFeatureKindName(*kind));
        if (feature.title.empty())
            feature.title = worldMapFeatureKindName(*kind);
        feature.chestIds = metadata->chestIds;
        feature.targetMap = metadata->targetMap.value_or("");
        features.push_back(std::move(feature));
    }
    return features;
}

std::string worldSiteDiscoveryKey(uint16_t eventId, const std::string &siteId)
{
    return "openyamm.discovery.site/" + siteId + "/event/" + std::to_string(eventId);
}

void recordWorldSiteDiscovery(EventRuntimeState &state, uint16_t eventId, const std::string &siteId)
{
    if (eventId != 0 && !siteId.empty())
        state.namedMapVars[worldSiteDiscoveryKey(eventId, siteId)] = 1;
}
}
