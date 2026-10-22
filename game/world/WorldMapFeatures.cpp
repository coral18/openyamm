#include "game/world/WorldMapFeatures.h"
#include "game/world/WorldFloorPlan.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/events/EventRuntime.h"
#include "game/world/WorldDatabase.h"
#include "game/tables/JournalAutonoteTable.h"

#include <algorithm>
#include <optional>
#include <stdexcept>

namespace OpenYAMM::Game
{
bool worldMapFeatureOnFloorLayer(const WorldMapFeature &feature, std::optional<int32_t> layer)
{
    if (feature.kind == WorldMapFeatureKind::Note)
        return !feature.noteElevation || worldFloorPlanLayerMatches(layer, float(*feature.noteElevation));
    return worldFloorPlanLayerMatches(layer, feature.site.z);
}

const char *worldMapFeatureKindName(WorldMapFeatureKind kind)
{
    switch (kind)
    {
    case WorldMapFeatureKind::Chest: return "Chest";
    case WorldMapFeatureKind::Water: return "Well / fountain";
    case WorldMapFeatureKind::Shrine: return "Shrine";
    case WorldMapFeatureKind::Obelisk: return "Obelisk";
    case WorldMapFeatureKind::Connection: return "Passage / teleport";
    case WorldMapFeatureKind::Monster: return "Monster";
    case WorldMapFeatureKind::Note: return "Note";
    case WorldMapFeatureKind::Item: return "Item source";
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
        if (const auto reference = events.siteReferences().find(site.eventId);
            reference != events.siteReferences().end())
            feature.guideReference = reference->second;
        if (const auto notes = events.autonoteReferences().find(site.eventId);
            notes != events.autonoteReferences().end())
            feature.autonoteIds = notes->second;
        feature.targetMap = metadata->targetMap.value_or("");
        features.push_back(std::move(feature));
    }
    return features;
}

std::string_view WorldDatabase::siteReference(const WorldMapFeature &feature,
    const WorldKnowledge &knowledge, bool guide) const
{
    return guide && featureVisible(feature, knowledge, true) ? feature.guideReference : std::string_view{};
}

std::vector<const JournalAutonoteEntry *> WorldDatabase::siteJournalEntries(const WorldMapFeature &feature,
    const JournalAutonoteTable &notes, const WorldKnowledge &knowledge, bool guide) const
{
    std::vector<const JournalAutonoteEntry *> result;
    if (!featureVisible(feature, knowledge, guide)) return result;
    for (uint32_t id : feature.autonoteIds)
    {
        if (!guide && !knowledge.autonoteIds.contains(id)) continue;
        const auto note = std::lower_bound(notes.entries().begin(), notes.entries().end(), id,
            [](const JournalAutonoteEntry &entry, uint32_t value) { return (entry.noteBit >> 16) < value; });
        if (note == notes.entries().end() || (note->noteBit >> 16) != id || note->text.empty() || note->text == "0")
            throw std::invalid_argument("Unknown site journal reference: " + std::to_string(id));
        result.push_back(&*note);
    }
    return result;
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
