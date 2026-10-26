#include "game/world/WorldDatabase.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/outdoor/OutdoorMapData.h"
#include "game/indoor/IndoorMapData.h"
#include "game/FaceEnums.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace OpenYAMM::Game
{
void WorldDatabase::indexMapLocations(const OutdoorMapData &map, const ScriptedEventProgram &events)
{
    const WorldRecord *pSource = nullptr;
    for (const WorldRecord &record : m_records)
        if (record.kind == WorldRecordKind::Region && record.map->worldId == map.worldId
            && toLowerCopy(record.map->fileName) == toLowerCopy(map.fileName))
            pSource = &record;
    if (pSource == nullptr)
        return;

    indexMapSites(*pSource, worldInteractionSites(map), events);
}

void WorldDatabase::indexMapSites(const WorldRecord &source, const std::vector<WorldInteractionSite> &sites,
    const ScriptedEventProgram &events)
{
    const WorldRecord *pSource = &source;
    std::erase_if(m_locations, [pSource](const WorldMapLocation &location) { return location.mapId == pSource->id; });
    const auto target = [&](uint16_t eventId) -> const WorldRecord *
    {
        const auto metadata = events.getContextActionMetadata(eventId);
        if (!metadata || metadata->hidden || (metadata->source != "opcode" && metadata->source != "script"))
            return nullptr;
        for (const WorldRecord &record : m_records)
        {
            if (metadata->kind == "enter_house" && metadata->houseId
                && record.kind == WorldRecordKind::House && record.house->id == *metadata->houseId
                && record.map == pSource->map)
                return &record;
            if (pSource->kind == WorldRecordKind::Region
                && metadata->kind == "enter_dungeon" && metadata->targetMap
                && record.kind == WorldRecordKind::Dungeon
                && toLowerCopy(record.map->fileName) == toLowerCopy(*metadata->targetMap))
                return &record;
        }
        return nullptr;
    };
    const auto add = [&](const std::string &site, uint16_t eventId, double x, double y, double z)
    {
        const WorldRecord *pTarget = target(eventId);
        if (pTarget == nullptr || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return;
        if (pSource->kind == WorldRecordKind::Region
            && (x < -32768 || x > 32768 || y < -32768 || y > 32768))
            return;
        m_locations.push_back({pSource->id + "/site/" + site + "/event/" + std::to_string(eventId),
                               pSource->id, pTarget->id, eventId, float(x), float(y), float(z)});
    };
    std::erase_if(m_features, [pSource](const WorldMapFeature &feature)
    { return feature.mapId == pSource->id && feature.kind != WorldMapFeatureKind::Monster
        && feature.kind != WorldMapFeatureKind::Note && feature.kind != WorldMapFeatureKind::Item
        && !feature.arrival && !feature.site.id.starts_with("inherited-decoration/"); });
    auto features = buildWorldMapFeatures(pSource->id, sites, events);
    for (const WorldMapFeature &feature : features)
        for (const std::string &file : feature.targetMaps)
            if (std::none_of(m_pMaps->getEntries().begin(), m_pMaps->getEntries().end(),
                [&](const MapStatsEntry &map)
                { return map.worldId == pSource->map->worldId && toLowerCopy(map.fileName) == file; }))
                throw std::invalid_argument("Unknown connection destination: " + feature.id + "/" + file);

    m_features.insert(m_features.end(), features.begin(), features.end());
    for (const WorldInteractionSite &site : sites)
    {
        const bool groupedComponent = std::any_of(features.begin(), features.end(), [&](const WorldMapFeature &feature)
        {
            return !feature.components.empty() && feature.site.eventId == site.eventId
                && feature.site.id != site.id && worldMapFeatureContainsSite(feature, site.id);
        });
        if (groupedComponent) continue;
        add(site.id, site.eventId, site.x, site.y, site.z);
    }
}

const std::vector<WorldMapLocation> &WorldDatabase::locations() const { return m_locations; }

void WorldDatabase::indexMapLocations(const MapStatsEntry &map, const IndoorMapData &geometry,
                                     const ScriptedEventProgram &events)
{
    const std::string id = mapId(map);
    const WorldRecord *pSource = find(id);
    if (pSource == nullptr || pSource->kind != WorldRecordKind::Dungeon)
        return;
    indexMapSites(*pSource, worldInteractionSites(geometry), events);
}

const std::vector<WorldMapFeature> &WorldDatabase::features() const { return m_features; }

void WorldDatabase::indexMapArrivals(const WorldKnowledge &knowledge)
{
    std::erase_if(m_features, [](const WorldMapFeature &feature) { return feature.arrival.has_value(); });
    for (const auto &[file, arrivals] : knowledge.arrivals)
        for (const WorldMapArrival &arrival : arrivals)
        {
            const MapStatsEntry *pDestination = mapDefinition(file);
            if (pDestination == nullptr && !file.starts_with("world."))
                for (const WorldRecord &record : m_records)
                    if ((record.kind == WorldRecordKind::Region || record.kind == WorldRecordKind::Dungeon)
                        && toLowerCopy(record.map->fileName) == file) pDestination = record.map;
            const MapStatsEntry *pSource = mapDefinition(arrival.sourceMapId);
            if (pDestination == nullptr || pSource == nullptr || arrival.eventId == 0
                || toLowerCopy(pDestination->fileName) != toLowerCopy(arrival.mapFileName)) continue;
            WorldMapFeature feature;
            feature.id = mapId(*pDestination) + "/arrival/" + arrival.sourceMapId
                + "/event/" + std::to_string(arrival.eventId);
            feature.mapId = mapId(*pDestination);
            feature.kind = WorldMapFeatureKind::Connection;
            feature.site = {"arrival/" + arrival.sourceMapId + "/event/" + std::to_string(arrival.eventId),
                0, float(arrival.x), float(arrival.y), float(arrival.z)};
            feature.title = "Arrival from " + pSource->name;
            feature.targetMap = pSource->fileName;
            feature.targetMapId = mapId(*pSource);
            feature.arrival = arrival;
            m_features.push_back(std::move(feature));
        }
}

std::vector<const WorldMapFeature *> WorldDatabase::connectionArrivals(const WorldMapFeature &source,
    const WorldKnowledge &knowledge) const
{
    std::vector<const WorldMapFeature *> result;
    if (source.kind != WorldMapFeatureKind::Connection || source.arrival) return result;
    for (const WorldMapFeature &feature : m_features)
        if (feature.arrival && feature.arrival->sourceMapId == source.mapId
            && feature.arrival->eventId == source.site.eventId
            && (find(feature.mapId) != nullptr ? featureVisible(feature, knowledge, false)
                : knowledge.visitedMaps.contains(toLowerCopy(feature.arrival->mapFileName))))
            result.push_back(&feature);
    std::sort(result.begin(), result.end(), [](const WorldMapFeature *a, const WorldMapFeature *b)
    {
        if (a->arrival->gameMinutes != b->arrival->gameMinutes)
            return a->arrival->gameMinutes > b->arrival->gameMinutes;
        return a->id < b->id;
    });
    return result;
}

std::vector<const MapStatsEntry *> WorldDatabase::connectionTargets(const WorldMapFeature &source,
    const WorldKnowledge &knowledge, bool guide) const
{
    std::vector<const MapStatsEntry *> targets;
    if (!guide || source.kind != WorldMapFeatureKind::Connection || source.arrival
        || !featureVisible(source, knowledge, guide)) return targets;
    const MapStatsEntry *pSource = mapDefinition(source.mapId);
    if (pSource == nullptr) return targets;
    for (const std::string &file : source.targetMaps)
        for (const MapStatsEntry &map : m_pMaps->getEntries())
            if (map.worldId == pSource->worldId && toLowerCopy(map.fileName) == file) targets.push_back(&map);
    return targets;
}

void WorldDatabase::replaceMapMonsterFeatures(const MapStatsEntry &map, std::vector<WorldMapFeature> features,
    const MonsterTable &monsters, const std::vector<WorldMonsterObservation> &observations,
    const MapDeltaData *pAuthoredActors)
{
    const std::string id = mapId(map);
    if (find(id) == nullptr)
        return;
    std::erase_if(m_features, [&id](const WorldMapFeature &feature)
    { return feature.mapId == id && feature.kind == WorldMapFeatureKind::Monster; });
    if (pAuthoredActors != nullptr)
    {
        auto placed = buildWorldPlacedMonsterFeatures(map, *pAuthoredActors, monsters);
        features.insert(features.end(), placed.begin(), placed.end());
    }
    auto observed = buildWorldObservedMonsterFeatures(id, observations, monsters);
    features.insert(features.end(), observed.begin(), observed.end());
    m_features.insert(m_features.end(), features.begin(), features.end());
}

void WorldDatabase::indexMapMonsters(const MapStatsEntry &map, const OutdoorMapData &geometry,
    const MonsterTable &monsters, const std::vector<WorldMonsterObservation> &observations,
    const MapDeltaData *pAuthoredActors)
{
    replaceMapMonsterFeatures(map, buildWorldEncounterFeatures(map, geometry, monsters),
        monsters, observations, pAuthoredActors);
}

void WorldDatabase::indexMapMonsters(const MapStatsEntry &map, const IndoorMapData &geometry,
    const MonsterTable &monsters, const std::vector<WorldMonsterObservation> &observations,
    const MapDeltaData *pAuthoredActors)
{
    replaceMapMonsterFeatures(map, buildWorldEncounterFeatures(map, geometry, monsters),
        monsters, observations, pAuthoredActors);
}

void WorldDatabase::indexObservedMonsters(const WorldKnowledge &knowledge, const MonsterTable &monsters)
{
    std::erase_if(m_features, [](const WorldMapFeature &feature)
    { return feature.monster && feature.monster->observed; });
    for (const WorldRecord &map : m_records)
    {
        if (map.kind != WorldRecordKind::Region && map.kind != WorldRecordKind::Dungeon) continue;
        const auto found = knowledge.monsterObservations.find(toLowerCopy(map.map->fileName));
        if (found == knowledge.monsterObservations.end()) continue;
        auto observed = buildWorldObservedMonsterFeatures(map.id, found->second, monsters);
        m_features.insert(m_features.end(), observed.begin(), observed.end());
    }
}

void WorldDatabase::indexMapNotes(const WorldKnowledge &knowledge)
{
    std::erase_if(m_features, [](const WorldMapFeature &feature)
    { return feature.kind == WorldMapFeatureKind::Note; });
    for (const WorldRecord &map : m_records)
    {
        if (map.kind != WorldRecordKind::Region && map.kind != WorldRecordKind::Dungeon)
            continue;
        const auto found = knowledge.notes.find(toLowerCopy(map.map->fileName));
        if (found == knowledge.notes.end())
            continue;
        for (const WorldMapNote &note : found->second)
        {
            if (!note.value.active || note.value.text.empty()
                || toLowerCopy(note.value.mapFileName) != toLowerCopy(map.map->fileName))
                continue;
            WorldMapFeature feature;
            feature.id = map.id + "/note/" + std::to_string(note.value.id);
            feature.mapId = map.id;
            feature.site = {"note/" + std::to_string(note.value.id), 0,
                float(note.value.x), float(note.value.y), 0};
            feature.kind = WorldMapFeatureKind::Note;
            feature.title = note.value.text;
            feature.noteId = note.value.id;
            feature.userNote = note.userOwned;
            feature.noteElevation = note.elevation;
            if (note.elevation) feature.site.z = float(*note.elevation);
            m_features.push_back(std::move(feature));
        }
    }
}

std::vector<WorldChestObservation> WorldDatabase::observedChests(const WorldMapFeature &feature,
    const WorldKnowledge &knowledge) const
{
    std::vector<WorldChestObservation> result;
    const WorldRecord *pMap = find(feature.mapId);
    if (pMap == nullptr || feature.kind != WorldMapFeatureKind::Chest) return result;
    const auto found = knowledge.chestObservations.find(toLowerCopy(pMap->map->fileName));
    if (found == knowledge.chestObservations.end()) return result;
    for (const WorldChestObservation &chest : found->second)
        if (std::find(feature.chestIds.begin(), feature.chestIds.end(), chest.chestId) != feature.chestIds.end())
            result.push_back(chest);
    return result;
}

bool WorldDatabase::featureSiteVisible(const WorldMapFeature &feature, const WorldInteractionSite &site,
    const WorldKnowledge &knowledge, bool guide) const
{
    const WorldRecord *pMap = find(feature.mapId);
    if (pMap == nullptr) return false;
    if (guide) return true;
    if (feature.arrival || feature.kind == WorldMapFeatureKind::Note)
        return knowledge.visitedMaps.contains(toLowerCopy(pMap->map->fileName));
    if (feature.kind == WorldMapFeatureKind::Monster)
        return feature.monster && feature.monster->observed;
    if (feature.kind == WorldMapFeatureKind::Item)
        return feature.observedItem && visible(*pMap, knowledge, false);
    const std::string file = toLowerCopy(pMap->map->fileName);
    if (const auto known = knowledge.interactedSites.find(file); known != knowledge.interactedSites.end())
    {
        if (known->second.contains(worldSiteDiscoveryKey(site.eventId, site.id))) return true;
        for (uint16_t event : feature.relatedEventIds)
            if (known->second.contains(worldSiteDiscoveryKey(event, site.id))) return true;
        for (uint32_t face : site.faceIds)
            if (known->second.contains(worldSiteDiscoveryKey(site.eventId,
                site.surfacePrefix + "surface/" + std::to_string(face)))) return true;
    }
    if (pMap->kind == WorldRecordKind::Region)
    {
        const auto exploration = knowledge.regionMaps.find(file);
        return exploration != knowledge.regionMaps.end()
            && worldRegionPointExplored(exploration->second, site.x, site.y);
    }
    const auto exploration = knowledge.floorPlans.find(file);
    if (exploration != knowledge.floorPlans.end())
        for (uint32_t face : site.faceIds)
            if (face < exploration->second.faceAttributes.size()
                && !exploration->second.invisibleFaces.contains(face)
                && !hasFaceAttribute(exploration->second.faceAttributes[face], FaceAttribute::Invisible)
                && hasFaceAttribute(exploration->second.faceAttributes[face], FaceAttribute::SeenByParty))
                return true;
    return false;
}

std::vector<const WorldInteractionSite *> WorldDatabase::featureSites(const WorldMapFeature &feature,
    const WorldKnowledge &knowledge, bool guide, std::optional<int32_t> layer) const
{
    std::vector<const WorldInteractionSite *> sites;
    if (feature.kind == WorldMapFeatureKind::Note && !worldMapFeatureOnFloorLayer(feature, layer))
        return sites;
    const auto add = [&](const WorldInteractionSite &site)
    {
        if ((feature.kind == WorldMapFeatureKind::Note || worldFloorPlanLayerMatches(layer, site.z))
            && featureSiteVisible(feature, site, knowledge, guide)) sites.push_back(&site);
    };
    if (feature.components.empty()) add(feature.site);
    else for (const WorldInteractionSite &site : feature.components) add(site);
    return sites;
}

bool WorldDatabase::featureVisible(const WorldMapFeature &feature, const WorldKnowledge &knowledge, bool guide) const
{
    if (feature.components.empty()) return featureSiteVisible(feature, feature.site, knowledge, guide);
    return std::any_of(feature.components.begin(), feature.components.end(), [&](const WorldInteractionSite &site)
        { return featureSiteVisible(feature, site, knowledge, guide); });
}

const std::string &WorldDatabase::featureTitle(const WorldMapFeature &feature,
    const WorldKnowledge &knowledge, bool guide) const
{
    return guide || feature.unidentifiedTitle.empty() || knowledge.identifiedItems.contains(feature.itemId)
        ? feature.title : feature.unidentifiedTitle;
}

std::vector<const WorldMapFeature *> WorldDatabase::queryFeatures(const WorldKnowledge &knowledge, bool guide,
    const std::string &search, const std::string &withinMap, const SpellTable *pSpells, const ItemTable *pItems) const
{
    const std::string query = toLowerCopy(search);
    std::vector<const WorldMapFeature *> result;
    for (const WorldMapFeature &feature : m_features)
    {
        const WorldRecord *pMap = find(feature.mapId);
        if (pMap == nullptr || (!withinMap.empty() && feature.mapId != withinMap)
            || !featureVisible(feature, knowledge, guide))
            continue;
        const std::string text = featureTitle(feature, knowledge, guide) + " "
            + worldMapFeatureKindName(feature.kind) + " " + pMap->name()
            + " " + std::string(siteReference(feature, knowledge, guide))
            + " " + siteObservationText(feature, knowledge, pSpells, pItems);
        if (query.empty() || toLowerCopy(text).find(query) != std::string::npos)
            result.push_back(&feature);
    }
    std::sort(result.begin(), result.end(), [&](const WorldMapFeature *a, const WorldMapFeature *b)
    {
        const std::string &aTitle = featureTitle(*a, knowledge, guide);
        const std::string &bTitle = featureTitle(*b, knowledge, guide);
        return aTitle < bTitle || (aTitle == bTitle && a->id < b->id);
    });
    return result;
}

bool WorldDatabase::locationVisible(const WorldMapLocation &location, const WorldKnowledge &knowledge, bool guide) const
{
    const WorldRecord *pSource = find(location.mapId);
    const WorldRecord *pTarget = find(location.recordId);
    if (pSource == nullptr || pTarget == nullptr)
        return false;
    if (guide)
        return true;
    if (!knowledge.knows(*pSource))
        return false;
    if (pTarget->kind == WorldRecordKind::Dungeon)
    {
        const auto group = std::find_if(m_features.begin(), m_features.end(), [&](const WorldMapFeature &feature)
            { return feature.id == location.id && !feature.components.empty(); });
        if (group != m_features.end()) return featureVisible(*group, knowledge, false);
        const auto exploration = knowledge.regionMaps.find(toLowerCopy(pSource->map->fileName));
        return exploration != knowledge.regionMaps.end()
            && worldRegionPointExplored(exploration->second, location.x, location.y);
    }
    return !recordsAtLocation(location, knowledge, false).empty();
}

std::vector<WorldMapLocation> WorldDatabase::queryLocations(const WorldKnowledge &knowledge, bool guide,
    const std::string &withinMap, std::optional<int32_t> layer) const
{
    std::vector<WorldMapLocation> locations;
    for (const WorldMapLocation &location : m_locations)
    {
        if ((!withinMap.empty() && location.mapId != withinMap) || !locationVisible(location, knowledge, guide))
            continue;
        const auto group = std::find_if(m_features.begin(), m_features.end(), [&](const WorldMapFeature &feature)
            { return feature.id == location.id && !feature.components.empty(); });
        if (group != m_features.end())
        {
            const auto sites = featureSites(*group, knowledge, guide, layer);
            if (sites.empty()) continue;
            WorldMapLocation projected = location;
            projected.x = sites.front()->x;
            projected.y = sites.front()->y;
            projected.z = sites.front()->z;
            locations.push_back(std::move(projected));
        }
        else if (worldFloorPlanLayerMatches(layer, location.z)) locations.push_back(location);
    }
    return locations;
}

std::vector<const WorldRecord *> WorldDatabase::recordsAtLocation(const WorldMapLocation &location,
    const WorldKnowledge &knowledge, bool guide, bool includeTravel) const
{
    std::vector<const WorldRecord *> result;
    const WorldRecord *pTarget = find(location.recordId);
    if (pTarget == nullptr)
        return result;
    for (const WorldRecord &record : m_records)
        if ((record.kind != WorldRecordKind::Travel || includeTravel)
            && (&record == pTarget || (pTarget->house != nullptr && record.house == pTarget->house))
            && visible(record, knowledge, guide))
            result.push_back(&record);
    std::sort(result.begin(), result.end(), [](const WorldRecord *a, const WorldRecord *b)
    { return a->name() < b->name(); });
    return result;
}
} // namespace OpenYAMM::Game
