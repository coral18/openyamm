#include "game/world/WorldMapFeatures.h"
#include "game/world/WorldFloorPlan.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/events/EventRuntime.h"
#include "game/world/WorldDatabase.h"
#include "game/world/WorldGateCatalog.h"
#include "game/tables/JournalAutonoteTable.h"
#include "game/tables/ItemTable.h"
#include "game/tables/SpellTable.h"
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
    if (!feature.components.empty())
        return std::any_of(feature.components.begin(), feature.components.end(), [&](const WorldInteractionSite &site)
            { return worldFloorPlanLayerMatches(layer, site.z); });
    return worldFloorPlanLayerMatches(layer, feature.site.z);
}

bool worldMapFeatureContainsSite(const WorldMapFeature &feature, const std::string &siteId)
{
    const auto matches = [&](const WorldInteractionSite &site)
    {
        return siteId == site.id || std::any_of(site.faceIds.begin(), site.faceIds.end(), [&](uint32_t face)
            { return siteId == site.surfacePrefix + "surface/" + std::to_string(face); });
    };
    return matches(feature.site) || std::any_of(feature.components.begin(), feature.components.end(), matches);
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
    case WorldMapFeatureKind::Decoration: return "Interactive object";
    }
    return "Site";
}

std::optional<WorldMapFeatureKind> worldMapFeatureKind(std::string_view action)
{
    if (action == "open_chest") return WorldMapFeatureKind::Chest;
    if (action == "well" || action == "fountain" || action == "drink") return WorldMapFeatureKind::Water;
    if (action == "shrine") return WorldMapFeatureKind::Shrine;
    if (action == "obelisk") return WorldMapFeatureKind::Obelisk;
    if (action == "rescue" || action == "stone_to_flesh") return WorldMapFeatureKind::Decoration;
    if (action == "dimension_door" || action == "teleport" || action == "passage" || action == "travel"
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
        feature.interactionReference = metadata->interactionReference;
        if (const auto reference = events.siteReferences().find(site.eventId);
            reference != events.siteReferences().end())
            feature.guideReference = reference->second;
        if (const auto notes = events.autonoteReferences().find(site.eventId);
            notes != events.autonoteReferences().end())
            feature.autonoteIds = notes->second;
        feature.targetMap = metadata->targetMap.value_or("");
        feature.targetMaps = metadata->targetMaps;
        features.push_back(std::move(feature));
    }
    std::vector<bool> grouped(features.size(), false);
    for (const auto &[event, groups] : events.siteGroups())
        for (const std::vector<std::string> &group : groups)
        {
            std::vector<size_t> indices;
            for (const std::string &id : group)
            {
                const auto found = std::find_if(features.begin(), features.end(), [&](const WorldMapFeature &feature)
                    { return feature.site.id == id && feature.site.eventId == event; });
                if (found == features.end() || found->kind != WorldMapFeatureKind::Connection)
                    throw std::invalid_argument("Passage group has no matching physical connection: "
                        + mapId + "/" + id);
                indices.push_back(size_t(found - features.begin()));
            }
            WorldMapFeature &anchor = features[indices.front()];
            for (size_t index : indices)
            {
                anchor.components.push_back(features[index].site);
                if (index != indices.front()) grouped[index] = true;
            }
        }
    size_t index = 0;
    std::erase_if(features, [&](const WorldMapFeature &) { return grouped[index++]; });
    return features;
}

void WorldDatabase::indexMapInteractionReferences(const MapStatsEntry &map,
    const ItemTable &items, const SpellTable &spells)
{
    const std::string id = mapId(map);
    for (WorldMapFeature &feature : m_features)
    {
        if (feature.mapId != id || !feature.interactionReference) continue;
        const SiteInteractionReference &reference = *feature.interactionReference;
        if (reference.kind == "dimension_door") continue;
        const ItemDefinition *pItem = items.get(reference.itemId);
        if (pItem == nullptr) throw std::invalid_argument("Unknown interaction item: " + feature.id);
        std::string text;
        if (reference.kind == "enter_dungeon")
        {
            const auto destination = std::find_if(m_records.begin(), m_records.end(), [&](const WorldRecord &record)
            { return (record.kind == WorldRecordKind::Region || record.kind == WorldRecordKind::Dungeon)
                && record.map != nullptr && record.map->worldId == map.worldId
                && toLowerCopy(record.map->fileName) == reference.targetMap; });
            if (destination == m_records.end())
                throw std::invalid_argument("Unknown interaction destination: " + feature.id);
            text = "Destination: " + destination->name() + ".\nFirst opening requires carrying: " + pItem->name
                + ".\nThe key is not consumed. Once opened, this entrance can be reused without the key"
                  " while this location retains its open state.";
        }
        else
        {
            const NpcEntry *pNpc = m_pNpcs->getNpc(reference.npcId);
            if (pNpc == nullptr) throw std::invalid_argument("Unknown interaction NPC: " + feature.id);
            if (reference.spellId != 0)
            {
                const SpellEntry *pSpell = spells.findById(int(reference.spellId));
                if (pSpell == nullptr) throw std::invalid_argument("Unknown interaction spell: " + feature.id);
                text += "Use item: " + pItem->name + ".\nAlternative: cast " + pSpell->name
                    + " at this object.\n";
                text += reference.consumesItem ? "The item is consumed when used; casting does not consume it.\n"
                    : "The item is not consumed.\n";
            }
            else
                text += "Requires carrying: " + pItem->name
                    + (reference.consumesItem ? ". The item is consumed.\n" : ". The item is not consumed.\n");
            if (reference.completionQBit != 0) text += "Available only while this person has not been rescued.\n";
            text += "On success: remove this statue and speak with " + pNpc->name + ".";
            if (reference.addsFollower) text += " The rescued person joins the followers.";
        }
        feature.guideReference = std::move(text);
    }
}

void WorldDatabase::indexMapGateReferences(const MapStatsEntry &map, const WorldGateCatalog &gates,
    const Party *pParty)
{
    for (WorldMapFeature &feature : m_features)
    {
        if (feature.mapId != mapId(map) || !feature.interactionReference
            || feature.interactionReference->kind != "dimension_door") continue;
        const WorldGateNetwork *pNetwork = gates.find(feature.interactionReference->network);
        if (pNetwork == nullptr) throw std::invalid_argument("Unknown gate network: " + feature.id);
        feature.title = pNetwork->title;
        feature.guideReference = pNetwork->unlockDescription + "\nDestinations are chosen at this gateway.\n";
        for (const WorldGateChoice &choice : gates.choices(*pNetwork, pParty))
            feature.guideReference += "\n" + choice.pDestination->label + " - " + choice.pLanding->mapLabel;
        feature.guideReference += "\n\nLanding sites follow the party's current campaign progress.";
    }
}

std::vector<WorldGateChoice> WorldDatabase::gateDestinations(const WorldMapFeature &feature,
    const WorldKnowledge &knowledge, const WorldGateCatalog &gates, const Party *pParty, bool guide) const
{
    if (!feature.interactionReference || feature.interactionReference->kind != "dimension_door"
        || !featureVisible(feature, knowledge, guide)) return {};
    const WorldGateNetwork *pNetwork = gates.find(feature.interactionReference->network);
    if (pNetwork == nullptr) return {};
    if (guide) return gates.choices(*pNetwork, pParty);
    std::vector<WorldGateChoice> result;
    const auto found = knowledge.gateOffers.find(feature.mapId);
    if (found == knowledge.gateOffers.end()) return result;
    for (const WorldGateOffer &offer : found->second)
        if (offer.eventId == feature.site.eventId && offer.networkId == pNetwork->id)
            if (const auto choice = gates.match(offer.networkId, offer.choiceId, offer.mapId))
                result.push_back(*choice);
    return result;
}

std::string WorldDatabase::gateOfferText(const WorldMapFeature &feature, const WorldKnowledge &knowledge,
    const WorldGateCatalog &gates) const
{
    if (!feature.interactionReference || feature.interactionReference->kind != "dimension_door"
        || !featureVisible(feature, knowledge, false)) return {};
    const auto found = knowledge.gateOffers.find(feature.mapId);
    if (found == knowledge.gateOffers.end()) return {};
    const WorldGateNetwork *pNetwork = gates.find(feature.interactionReference->network);
    if (pNetwork == nullptr) return "Recorded gateway network is unavailable.";
    std::string text;
    for (const WorldGateOffer &offer : found->second)
    {
        if (offer.eventId != feature.site.eventId || offer.networkId != pNetwork->id) continue;
        if (const auto choice = gates.match(offer.networkId, offer.choiceId, offer.mapId))
        {
            if (text.empty()) text = "Previously offered destinations (availability may change):";
            text += "\n" + choice->pDestination->label + " - " + choice->pLanding->mapLabel + " | Day "
                + std::to_string(offer.gameMinutes / (24 * 60) + 1);
        }
    }
    return text;
}

void WorldDatabase::indexInheritedDecorations(const MapStatsEntry &map,
    const std::vector<WorldDecorationSite> &sites, const ScriptedEventProgram &globalEvents)
{
    const std::string id = mapId(map);
    if (find(id) == nullptr) return;
    std::erase_if(m_features, [&](const WorldMapFeature &feature)
        { return feature.mapId == id && feature.site.id.starts_with("inherited-decoration/"); });
    for (const WorldDecorationSite &decoration : sites)
    {
        WorldMapFeature feature;
        feature.id = id + "/site/" + decoration.site.id;
        feature.mapId = id; feature.site = decoration.site;
        feature.kind = WorldMapFeatureKind::Decoration;
        feature.title = decoration.name;
        for (uint32_t offset = 0; offset < decoration.eventCount; ++offset)
        {
            const uint32_t event = uint32_t(decoration.site.eventId) + offset;
            if (event > UINT16_MAX || !globalEvents.hasEvent(uint16_t(event))) continue;
            feature.relatedEventIds.push_back(uint16_t(event));
            const auto notes = globalEvents.autonoteReferences().find(uint16_t(event));
            if (notes != globalEvents.autonoteReferences().end())
                feature.relatedAutonotes.emplace(uint16_t(event), notes->second);
            if (decoration.initialEventId != 0 && event != decoration.initialEventId) continue;
            const auto hint = globalEvents.getHint(uint16_t(event));
            if (hint && !hint->empty()) feature.guideReference += *hint + "\n";
            if (const auto reference = globalEvents.siteReferences().find(uint16_t(event));
                reference != globalEvents.siteReferences().end())
                feature.guideReference += reference->second + "\n";
            if (notes != globalEvents.autonoteReferences().end())
                feature.autonoteIds.insert(feature.autonoteIds.end(), notes->second.begin(), notes->second.end());
        }
        if (feature.relatedEventIds.empty()) continue;
        if (decoration.initiallyDepleted)
            feature.guideReference = "Initially depleted in the authored map; scripts can change remaining contents.";
        else if (!feature.guideReference.empty())
            feature.guideReference = "Initial interaction reference; remaining contents can change after use.\n\n"
                + feature.guideReference;
        std::sort(feature.autonoteIds.begin(), feature.autonoteIds.end());
        feature.autonoteIds.erase(std::unique(feature.autonoteIds.begin(), feature.autonoteIds.end()),
            feature.autonoteIds.end());
        m_features.push_back(std::move(feature));
    }
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
        if (observation.eventId != feature.site.eventId
            && std::find(feature.relatedEventIds.begin(), feature.relatedEventIds.end(), observation.eventId)
                == feature.relatedEventIds.end()) continue;
        if (worldMapFeatureContainsSite(feature, observation.siteId)) result.push_back(&observation);
    }
    return result;
}

std::string WorldDatabase::siteObservationText(const WorldMapFeature &feature, const WorldKnowledge &knowledge,
    const SpellTable *pSpells) const
{
    std::string result;
    for (const WorldSiteObservation *pObservation : siteObservations(feature, knowledge))
    {
        if (!result.empty()) result += "\n\n";
        result += describeWorldSiteObservation(*pObservation, pSpells);
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
    std::vector<uint32_t> noteIds = feature.autonoteIds;
    if (!guide && feature.kind == WorldMapFeatureKind::Decoration)
    {
        noteIds.clear();
        for (const WorldSiteObservation *pObservation : siteObservations(feature, knowledge))
            if (const auto notes = feature.relatedAutonotes.find(pObservation->eventId);
                notes != feature.relatedAutonotes.end())
                noteIds.insert(noteIds.end(), notes->second.begin(), notes->second.end());
        std::sort(noteIds.begin(), noteIds.end());
        noteIds.erase(std::unique(noteIds.begin(), noteIds.end()), noteIds.end());
    }
    for (uint32_t id : noteIds)
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
