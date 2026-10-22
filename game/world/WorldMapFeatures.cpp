#include "game/world/WorldMapFeatures.h"
#include "game/world/WorldFloorPlan.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/events/EventRuntime.h"
#include "game/world/WorldDatabase.h"
#include "game/tables/JournalAutonoteTable.h"
#include "game/StringUtils.h"

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

std::optional<WorldMapFeatureKind> worldMapFeatureKind(std::string_view action)
{
    if (action == "open_chest") return WorldMapFeatureKind::Chest;
    if (action == "well" || action == "fountain") return WorldMapFeatureKind::Water;
    if (action == "shrine") return WorldMapFeatureKind::Shrine;
    if (action == "obelisk") return WorldMapFeatureKind::Obelisk;
    if (action == "teleport" || action == "passage" || action == "travel"
        || action == "enter_dungeon" || action == "leave_dungeon") return WorldMapFeatureKind::Connection;
    return std::nullopt;
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
        const std::optional<WorldMapFeatureKind> kind = worldMapFeatureKind(metadata->kind);
        if (!kind || (*kind == WorldMapFeatureKind::Chest && metadata->chestIds.empty())) continue;
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

std::vector<const WorldSiteObservation *> WorldDatabase::siteObservations(const WorldMapFeature &feature,
    const WorldKnowledge &knowledge) const
{
    std::vector<const WorldSiteObservation *> result;
    const WorldRecord *pMap = find(feature.mapId);
    if (pMap == nullptr || !featureVisible(feature, knowledge, false)) return result;
    const auto found = knowledge.siteObservations.find(feature.mapId);
    if (found == knowledge.siteObservations.end()) return result;
    for (const WorldSiteObservation &observation : found->second)
    {
        if (observation.eventId != feature.site.eventId) continue;
        const bool matches = observation.siteId == feature.site.id
            || std::any_of(feature.site.faceIds.begin(), feature.site.faceIds.end(), [&](uint32_t face)
            { return observation.siteId == feature.site.surfacePrefix + "surface/" + std::to_string(face); });
        if (matches) result.push_back(&observation);
    }
    return result;
}

std::string WorldDatabase::siteObservationText(const WorldMapFeature &feature, const WorldKnowledge &knowledge) const
{
    std::string result;
    for (const WorldSiteObservation *pObservation : siteObservations(feature, knowledge))
    {
        if (!result.empty()) result += "\n\n";
        result += describeWorldSiteObservation(*pObservation);
    }
    return result;
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
