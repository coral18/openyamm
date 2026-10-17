#include "game/world/WorldDatabase.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/outdoor/OutdoorMapData.h"
#include "game/indoor/IndoorMapData.h"
#include "game/FaceEnums.h"
#include "game/StringUtils.h"

#include <algorithm>

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
            if (metadata->kind == "enter_dungeon" && metadata->targetMap
                && record.kind == WorldRecordKind::Dungeon
                && toLowerCopy(record.map->fileName) == toLowerCopy(*metadata->targetMap))
                return &record;
        }
        return nullptr;
    };
    const auto add = [&](const std::string &site, uint16_t eventId, double x, double y)
    {
        const WorldRecord *pTarget = target(eventId);
        if (pTarget == nullptr || x < -32768 || x > 32768 || y < -32768 || y > 32768)
            return;
        m_locations.push_back({pSource->id + "/site/" + site + "/event/" + std::to_string(eventId),
                               pSource->id, pTarget->id, eventId, float(x), float(y)});
    };
    const auto sites = worldInteractionSites(map);
    std::erase_if(m_features, [pSource](const WorldMapFeature &feature)
    { return feature.mapId == pSource->id && feature.kind != WorldMapFeatureKind::Monster
        && feature.kind != WorldMapFeatureKind::Note; });
    auto features = buildWorldMapFeatures(pSource->id, sites, events);
    m_features.insert(m_features.end(), features.begin(), features.end());
    for (const WorldInteractionSite &site : sites)
        add(site.id, site.eventId, site.x, site.y);
}

const std::vector<WorldMapLocation> &WorldDatabase::locations() const { return m_locations; }

void WorldDatabase::indexMapLocations(const MapStatsEntry &map, const IndoorMapData &geometry,
                                     const ScriptedEventProgram &events)
{
    const std::string id = mapId(map);
    const WorldRecord *pSource = find(id);
    if (pSource == nullptr || pSource->kind != WorldRecordKind::Dungeon)
        return;
    std::erase_if(m_features, [&id](const WorldMapFeature &feature)
    { return feature.mapId == id && feature.kind != WorldMapFeatureKind::Monster
        && feature.kind != WorldMapFeatureKind::Note; });
    auto features = buildWorldMapFeatures(id, worldInteractionSites(geometry), events);
    m_features.insert(m_features.end(), features.begin(), features.end());
}

const std::vector<WorldMapFeature> &WorldDatabase::features() const { return m_features; }

void WorldDatabase::replaceMapMonsterFeatures(const MapStatsEntry &map, std::vector<WorldMapFeature> features,
    const MonsterTable &monsters, const std::vector<WorldMonsterObservation> &observations)
{
    const std::string id = mapId(map);
    if (find(id) == nullptr)
        return;
    std::erase_if(m_features, [&id](const WorldMapFeature &feature)
    { return feature.mapId == id && feature.kind == WorldMapFeatureKind::Monster; });
    auto observed = buildWorldObservedMonsterFeatures(id, observations, monsters);
    features.insert(features.end(), observed.begin(), observed.end());
    m_features.insert(m_features.end(), features.begin(), features.end());
}

void WorldDatabase::indexMapMonsters(const MapStatsEntry &map, const OutdoorMapData &geometry,
    const MonsterTable &monsters, const std::vector<WorldMonsterObservation> &observations)
{
    replaceMapMonsterFeatures(map, buildWorldEncounterFeatures(map, geometry, monsters), monsters, observations);
}

void WorldDatabase::indexMapMonsters(const MapStatsEntry &map, const IndoorMapData &geometry,
    const MonsterTable &monsters, const std::vector<WorldMonsterObservation> &observations)
{
    replaceMapMonsterFeatures(map, buildWorldEncounterFeatures(map, geometry, monsters), monsters, observations);
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
            m_features.push_back(std::move(feature));
        }
    }
}

bool WorldDatabase::featureVisible(const WorldMapFeature &feature, const WorldKnowledge &knowledge, bool guide) const
{
    const WorldRecord *pMap = find(feature.mapId);
    if (pMap == nullptr)
        return false;
    if (guide)
        return true;
    if (feature.kind == WorldMapFeatureKind::Note)
        return knowledge.visitedMaps.contains(toLowerCopy(pMap->map->fileName));
    if (feature.kind == WorldMapFeatureKind::Monster)
        return feature.monster && feature.monster->observed;
    const std::string file = toLowerCopy(pMap->map->fileName);
    if (const auto known = knowledge.interactedSites.find(file); known != knowledge.interactedSites.end())
    {
        if (known->second.contains(worldSiteDiscoveryKey(feature.site.eventId, feature.site.id)))
            return true;
        for (uint32_t face : feature.site.faceIds)
            if (known->second.contains(worldSiteDiscoveryKey(feature.site.eventId,
                feature.site.surfacePrefix + "surface/" + std::to_string(face))))
                return true;
    }
    if (pMap->kind == WorldRecordKind::Region)
    {
        const auto exploration = knowledge.regionMaps.find(file);
        return exploration != knowledge.regionMaps.end()
            && worldRegionPointExplored(exploration->second, feature.site.x, feature.site.y);
    }
    const auto exploration = knowledge.floorPlans.find(file);
    if (exploration != knowledge.floorPlans.end())
        for (uint32_t face : feature.site.faceIds)
            if (face < exploration->second.faceAttributes.size()
                && !exploration->second.invisibleFaces.contains(face)
                && !hasFaceAttribute(exploration->second.faceAttributes[face], FaceAttribute::Invisible)
                && hasFaceAttribute(exploration->second.faceAttributes[face], FaceAttribute::SeenByParty))
                return true;
    return false;
}

std::vector<const WorldMapFeature *> WorldDatabase::queryFeatures(const WorldKnowledge &knowledge, bool guide,
    const std::string &search, const std::string &withinMap) const
{
    const std::string query = toLowerCopy(search);
    std::vector<const WorldMapFeature *> result;
    for (const WorldMapFeature &feature : m_features)
    {
        const WorldRecord *pMap = find(feature.mapId);
        if (pMap == nullptr || (!withinMap.empty() && feature.mapId != withinMap)
            || !featureVisible(feature, knowledge, guide))
            continue;
        const std::string text = feature.title + " " + worldMapFeatureKindName(feature.kind) + " " + pMap->name();
        if (query.empty() || toLowerCopy(text).find(query) != std::string::npos)
            result.push_back(&feature);
    }
    std::sort(result.begin(), result.end(), [](const WorldMapFeature *a, const WorldMapFeature *b)
    { return a->title < b->title || (a->title == b->title && a->id < b->id); });
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
        const auto exploration = knowledge.regionMaps.find(toLowerCopy(pSource->map->fileName));
        return exploration != knowledge.regionMaps.end()
            && worldRegionPointExplored(exploration->second, location.x, location.y);
    }
    return !recordsAtLocation(location, knowledge, false).empty();
}

std::vector<const WorldRecord *> WorldDatabase::recordsAtLocation(const WorldMapLocation &location,
    const WorldKnowledge &knowledge, bool guide) const
{
    std::vector<const WorldRecord *> result;
    const WorldRecord *pTarget = find(location.recordId);
    if (pTarget == nullptr)
        return result;
    for (const WorldRecord &record : m_records)
        if (record.kind != WorldRecordKind::Travel
            && (&record == pTarget || (pTarget->house != nullptr && record.house == pTarget->house))
            && visible(record, knowledge, guide))
            result.push_back(&record);
    std::sort(result.begin(), result.end(), [](const WorldRecord *a, const WorldRecord *b)
    { return a->name() < b->name(); });
    return result;
}
} // namespace OpenYAMM::Game
