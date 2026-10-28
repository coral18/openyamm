#include "game/world/WorldDatabase.h"
#include "game/StringUtils.h"
#include "game/maps/MapIdentity.h"
#include "game/gameplay/TravelRuntime.h"
#include "game/events/ScriptedEventProgram.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <set>

namespace OpenYAMM::Game
{
namespace
{
bool discovered(const WorldKnowledge &knowledge, const std::string &key)
{
    const auto it = knowledge.variables.find(key);
    return it != knowledge.variables.end() && it->second > 0;
}

bool isOutdoor(const MapStatsEntry &map)
{
    return toLowerCopy(map.fileName).ends_with(".odm");
}
} // namespace

const MapStatsEntry *WorldDatabase::mapDefinition(const std::string &canonicalId) const
{
    for (const MapStatsEntry &map : m_pMaps->getEntries())
        if (mapId(map) == canonicalId) return &map;
    return nullptr;
}

std::string WorldDatabase::mapId(const MapStatsEntry &map)
{
    return map.canonicalId.empty() ? buildCanonicalMapId(map.worldId, map.fileName) : map.canonicalId;
}
std::string WorldDatabase::mapDiscoveryKey(const MapStatsEntry &map)
{
    return "openyamm.discovery." + mapId(map);
}
std::string WorldDatabase::houseDiscoveryKey(uint32_t id)
{
    return "openyamm.discovery.house/" + std::to_string(id);
}
std::string WorldDatabase::npcDiscoveryKey(uint32_t id)
{
    return "openyamm.discovery.npc/" + std::to_string(id);
}

std::string WorldRecord::name() const
{
    if (kind == WorldRecordKind::Travel)
    {
        if (walkingEdge)
        {
            const char *direction = "";
            switch (*walkingEdge)
            {
            case MapBoundaryEdge::North: direction = "north"; break;
            case MapBoundaryEdge::South: direction = "south"; break;
            case MapBoundaryEdge::East: direction = "east"; break;
            case MapBoundaryEdge::West: direction = "west"; break;
            }
            return std::string("Walk ") + direction + " to " + destination->name;
        }
        return house->type + " to " + destination->name;
    }
    if (kind == WorldRecordKind::Trainer)
        return npcName.value_or(npc->name) + " - " + teacher->note;
    if (kind == WorldRecordKind::Npc)
        return npcName.value_or(npc->name) + (possibleNpcLocation ? " (possible location)" : "");
    if (kind == WorldRecordKind::House)
        return house->name.empty() ? house->buildingName : house->name;
    return map->name;
}

std::string WorldRecord::searchText() const
{
    std::string text = name() + " " + map->name;
    if (house != nullptr)
        text += " " + house->type;
    if (kind == WorldRecordKind::Travel && house != nullptr)
        text += " " + (house->name.empty() ? house->buildingName : house->name);
    return text;
}

bool WorldKnowledge::knows(const WorldRecord &record) const
{
    if (record.possibleNpcLocation) return false;
    if (informedRecords.contains(record.id))
        return true;
    if (record.kind == WorldRecordKind::Npc || record.kind == WorldRecordKind::Trainer)
    {
        const auto house = variables.find(worldNpcHouseKey(record.npc->id));
        if (record.house == nullptr || house == variables.end()
            || std::bit_cast<uint32_t>(house->second) != record.house->id)
            return false;
        return discovered(*this, WorldDatabase::npcDiscoveryKey(record.npc->id))
            && (record.teacher == nullptr
                || discovered(*this, worldNpcTeacherKey(record.npc->id, record.teacher->topicId)));
    }
    if (record.kind == WorldRecordKind::Travel && record.house != nullptr)
        return record.transport && record.destination != nullptr
            && discovered(*this, WorldDatabase::houseDiscoveryKey(record.house->id))
            && worldTransportObservationMatches(record.house->id, *record.transport,
                WorldDatabase::mapId(*record.destination), variables);
    if (record.kind == WorldRecordKind::House)
        return discovered(*this, WorldDatabase::houseDiscoveryKey(record.house->id));
    return discovered(*this, WorldDatabase::mapDiscoveryKey(*record.map))
        || visitedMaps.contains(toLowerCopy(record.map->fileName));
}

WorldDatabase::WorldDatabase(const MapStats &maps, const HouseTable &houses, const NpcDialogTable &npcs,
    const MergedTeacherTopicTable &teachers, const std::string &worldId,
    const std::vector<WorldMapPresentation> &presentation, const WorldNpcState &npcsState,
    const WorldTransportState &transportState)
    : m_pMaps(&maps), m_pHouses(&houses), m_pNpcs(&npcs), m_observedNpcs(npcsState.observedOnly)
{
    m_transportIssues = transportState.issues;
    std::unordered_map<std::string, const WorldMapPresentation *> layouts;
    std::unordered_map<int, const MapStatsEntry *> areas;
    for (const auto &layout : presentation)
    {
        const auto *map = maps.findByFileName(layout.fileName);
        if (map == nullptr || map->worldId != worldId)
            throw std::invalid_argument("Unknown world map in presentation: " + layout.fileName);
        if (!layouts.emplace(toLowerCopy(layout.fileName), &layout).second)
            throw std::invalid_argument("Duplicate world map presentation: " + layout.fileName);
        if (!layout.parentFileName.empty())
        {
            const auto *parent = maps.findByFileName(layout.parentFileName);
            if (parent == nullptr || parent->worldId != worldId)
                throw std::invalid_argument("Unknown world map parent: " + layout.parentFileName);
        }
        if (layout.areaId != 0 && !areas.emplace(layout.areaId, map).second)
            throw std::invalid_argument("Duplicate world area in presentation");
        if (layout.position && (!std::isfinite(layout.position->first) || !std::isfinite(layout.position->second)
            || layout.position->first < 0 || layout.position->first > 1
            || layout.position->second < 0 || layout.position->second > 1))
            throw std::invalid_argument("Invalid world map position: " + layout.fileName);
    }
    for (const auto &map : maps.getEntries())
    {
        if (map.worldId != worldId)
            continue;
        WorldRecord record;
        record.id = mapId(map);
        record.kind = isOutdoor(map) ? WorldRecordKind::Region : WorldRecordKind::Dungeon;
        record.map = &map;
        const auto layout = layouts.find(toLowerCopy(map.fileName));
        if (layout != layouts.end())
        {
            record.position = layout->second->position;
            if (!layout->second->parentFileName.empty())
                record.parentId = mapId(*maps.findByFileName(layout->second->parentFileName));
        }
        if (record.parentId.empty() && map.areaId != 0 && areas.contains(map.areaId)
            && areas.at(map.areaId) != &map)
            record.parentId = mapId(*areas.at(map.areaId));
        m_records.push_back(std::move(record));
        const MapBoundaryEdge edges[] = {MapBoundaryEdge::North, MapBoundaryEdge::South, MapBoundaryEdge::East, MapBoundaryEdge::West};
        for (int edge = 0; edge < 4; ++edge)
        {
            const auto &transition = *map.edgeTransition(edges[edge]);
            if (!transition)
                continue;
            const auto *destination = maps.findByFileName(transition->destinationMapFileName);
            if (destination == nullptr || destination->worldId != worldId)
                continue;
            WorldRecord travel;
            travel.id = mapId(map) + "/walk/" + std::to_string(edge);
            travel.parentId = mapId(map);
            travel.kind = WorldRecordKind::Travel;
            travel.map = &map;
            travel.walkingRoute = &*transition;
            travel.walkingEdge = edges[edge];
            travel.destination = destination;
            m_records.push_back(std::move(travel));
        }
    }
    for (const auto &[id, house] : houses.entries())
    {
        // Transition cards are presentation for MoveToMap, not enterable houses or shops.
        if (toLowerCopy(house.type) == "dungeon ent")
            continue;
        const auto *map = maps.findById(house.mapId);
        if (map == nullptr || map->worldId != worldId)
            continue;
        WorldRecord record;
        record.id = house.canonicalId.empty() ? worldId + ":house/" + std::to_string(id) : house.canonicalId;
        record.parentId = mapId(*map);
        record.kind = WorldRecordKind::House;
        record.map = map;
        record.house = &house;
        m_records.push_back(record);
        std::vector<WorldTransportEntry> routes;
        if (const auto overrides = transportState.houses.find(id); overrides != transportState.houses.end())
            routes = overrides->second;
        else if (!transportState.observedOnly)
            for (const HouseEntry::TransportRoute &route : house.transportRoutes)
                routes.push_back({route, {}, false, false});
        for (const WorldTransportEntry &transport : routes)
        {
            const HouseEntry::TransportRoute &route = transport.route;
            const auto *destination = maps.findByFileName(route.mapFileName);
            // Cross-world travel is not presented as a local route.
            if (destination == nullptr || destination->worldId != worldId)
                continue;
            WorldRecord travel = record;
            travel.id += "/route/" + std::to_string(route.routeIndex);
            travel.parentId = record.id;
            travel.kind = WorldRecordKind::Travel;
            travel.transport = transport;
            travel.destination = destination;
            m_records.push_back(std::move(travel));
        }
        for (uint32_t npcId : npcs.getNpcIdsForHouse(id, &npcsState.houses))
        {
            const NpcEntry *npc = npcs.getNpc(npcId);
            if (npc == nullptr || npcsState.unavailable.contains(npcId)
                || (npcsState.observedOnly && !npcsState.houses.contains(npcId)))
                continue;
            WorldRecord resident = record;
            resident.id = worldId + ":npc/" + std::to_string(npcId);
            resident.parentId = record.id;
            resident.kind = WorldRecordKind::Npc;
            resident.npc = npc;
            resident.lastObservedNpc = npcsState.observedOnly;
            if (const auto name = npcsState.names.find(npcId); name != npcsState.names.end())
                resident.npcName = name->second;
            const auto topics = npcsState.topics.find(npcId);
            const std::vector<uint32_t> &topicIds = topics != npcsState.topics.end() ? topics->second : npc->topicIds;
            resident.npcTopicIds = topicIds;
            m_records.push_back(resident);
            for (uint32_t topicId : topicIds)
            {
                if (const auto *teacher = teachers.get(topicId))
                {
                    WorldRecord trainer = resident;
                    trainer.id += "/trainer/" + std::to_string(topicId);
                    trainer.parentId = resident.id;
                    trainer.kind = WorldRecordKind::Trainer;
                    trainer.teacher = teacher;
                    m_records.push_back(std::move(trainer));
                }
            }
        }
    }
    std::sort(m_records.begin(), m_records.end(), [](const WorldRecord &a, const WorldRecord &b) { return a.id < b.id; });
    for (size_t i = 0; i < m_records.size(); ++i)
        if (!m_byId.emplace(m_records[i].id, i).second)
            throw std::invalid_argument("Duplicate world record: " + m_records[i].id);
    for (const auto &record : m_records)
    {
        std::unordered_set<std::string> ancestors;
        const WorldRecord *parent = &record;
        while (parent != nullptr && !parent->parentId.empty())
        {
            if (!ancestors.insert(parent->id).second)
                throw std::invalid_argument("Cycle in world presentation: " + record.id);
            parent = find(parent->parentId);
        }
    }
    if (!m_observedNpcs)
        for (const auto &[houseId, house] : houses.entries())
            for (uint32_t npcId : npcs.getNpcIdsForHouse(houseId))
                appendPossibleNpcLocation(npcId, houseId);
}

void WorldDatabase::appendPossibleNpcLocation(uint32_t npcId, uint32_t houseId)
{
    if (m_observedNpcs) return;
    const NpcEntry *pNpc = m_pNpcs->getNpc(npcId);
    const HouseEntry *pHouse = m_pHouses->get(houseId);
    if (pNpc == nullptr || pHouse == nullptr)
        throw std::invalid_argument("Unknown NPC location reference: " + std::to_string(npcId)
            + "/" + std::to_string(houseId));
    const auto house = std::find_if(m_records.begin(), m_records.end(), [houseId](const WorldRecord &record)
    { return record.kind == WorldRecordKind::House && record.house->id == houseId; });
    if (house == m_records.end()) return; // A shared declaration may refer to another mounted world.
    const std::string npcRecordId = house->map->worldId + ":npc/" + std::to_string(npcId);
    const WorldRecord *pCurrent = find(npcRecordId);
    if (pCurrent != nullptr && pCurrent->house->id == houseId) return;
    const std::string id = npcRecordId + "/possible-house/" + std::to_string(houseId);
    if (find(id) != nullptr) return;
    WorldRecord possible = *house;
    possible.id = id;
    possible.parentId = house->id;
    possible.kind = WorldRecordKind::Npc;
    possible.npc = pNpc;
    possible.possibleNpcLocation = true;
    // A destination does not prove future name, topics, training offers or current availability.
    m_byId.emplace(id, m_records.size());
    m_records.push_back(std::move(possible));
}

void WorldDatabase::indexNpcLocations(const ScriptedEventProgram &program)
{
    if (m_observedNpcs) return;
    std::vector<std::pair<uint32_t, uint32_t>> locations;
    for (const auto &[eventId, entries] : program.npcLocations())
        for (const ScriptedEventProgram::NpcLocation &location : entries)
            locations.emplace_back(location.npcId, location.houseId);
    std::sort(locations.begin(), locations.end());
    locations.erase(std::unique(locations.begin(), locations.end()), locations.end());
    for (const auto &[npcId, houseId] : locations) appendPossibleNpcLocation(npcId, houseId);
}

void WorldDatabase::indexMapNpcTopics(const MapStatsEntry &map, const ScriptedEventProgram &program)
{
    const std::string owner = mapId(map);
    // A database belongs to one world. A callback's map is provenance, never the NPC's location.
    if (m_observedNpcs || find(owner) == nullptr) return;
    std::set<std::pair<uint32_t, uint32_t>> references;
    for (const auto &[event, topics] : program.npcTopicReferences())
        for (const ScriptedEventProgram::NpcTopicReference &topic : topics)
            references.emplace(topic.npcId, topic.topicId);
    std::erase_if(m_npcTopicReferences, [&](const WorldNpcTopicReference &reference)
        { return reference.mapId == owner; });
    for (const auto &[npcId, topicId] : references)
        m_npcTopicReferences.push_back({owner, npcId, topicId});
}

const std::vector<WorldNpcTopicReference> &WorldDatabase::npcTopicReferences() const
{
    return m_npcTopicReferences;
}

const std::vector<WorldRecord> &WorldDatabase::records() const { return m_records; }
const std::vector<std::string> &WorldDatabase::transportIssues() const { return m_transportIssues; }
const WorldRecord *WorldDatabase::find(const std::string &id) const
{
    const auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : &m_records[it->second];
}

std::string WorldDatabase::recordDisplayName(const WorldRecord &record, const WorldKnowledge &knowledge,
    bool guide) const
{
    const std::string name = record.name();
    if (record.kind != WorldRecordKind::House) return name;
    const bool shared = std::any_of(m_locations.begin(), m_locations.end(), [&](const WorldMapLocation &location)
        { return location.sharedEntrance && location.recordId == record.id; });
    if (!shared) return name;
    std::vector<std::string> residents;
    for (const WorldRecord &candidate : m_records)
        if (candidate.kind == WorldRecordKind::Npc && candidate.house == record.house
            && visible(candidate, knowledge, guide))
            residents.push_back(candidate.npcName.value_or(candidate.npc->name));
    std::sort(residents.begin(), residents.end());
    residents.erase(std::unique(residents.begin(), residents.end()), residents.end());
    std::string result = name;
    for (size_t i = 0; i < residents.size(); ++i) result += (i == 0 ? " - " : ", ") + residents[i];
    return result;
}

const MapStatsEntry *WorldDatabase::recordMap(const WorldRecord &record, const WorldKnowledge &knowledge,
    bool guide, const std::string &preferredMap) const
{
    if (record.house == nullptr) return record.map;
    if (!guide && !knowledge.knows(record)) return nullptr;
    const WorldRecord *pPrimary = find(mapId(*record.map));
    const bool primaryKnown = pPrimary != nullptr && (guide || knowledge.knows(*pPrimary));
    if (primaryKnown && (preferredMap.empty() || preferredMap == pPrimary->id)) return record.map;
    const MapStatsEntry *pFirst = nullptr;
    for (const WorldMapLocation &location : m_locations)
    {
        const WorldRecord *pTarget = find(location.recordId);
        if (pTarget == nullptr || pTarget->house != record.house
            || (!guide && !houseLocationKnown(location, knowledge))) continue;
        const WorldRecord *pSource = find(location.mapId);
        if (pSource == nullptr) continue;
        if (preferredMap == location.mapId) return pSource->map;
        if (pFirst == nullptr) pFirst = pSource->map;
    }
    return primaryKnown ? record.map : pFirst;
}

std::string WorldDatabase::recordSearchText(const WorldRecord &record, const WorldKnowledge &knowledge,
    bool guide, const std::string &preferredMap) const
{
    const MapStatsEntry *pMap = recordMap(record, knowledge, guide, preferredMap);
    if (pMap == nullptr) return {};
    WorldRecord located = record;
    located.map = pMap;
    std::string text = located.searchText() + " " + recordDisplayName(record, knowledge, guide);
    if (record.house != nullptr && preferredMap.empty())
        for (const WorldMapLocation &location : m_locations)
        {
            const WorldRecord *pTarget = find(location.recordId);
            if (pTarget == nullptr || pTarget->house != record.house
                || (!guide && !houseLocationKnown(location, knowledge))) continue;
            if (const WorldRecord *pSource = find(location.mapId)) text += " " + pSource->name();
        }
    return text;
}

bool WorldDatabase::visible(const WorldRecord &record, const WorldKnowledge &knowledge, bool guide) const
{
    if (record.possibleNpcLocation) return guide;
    if (guide)
    {
        // Once the actual map bindings are indexed, an unused table row is not a physical building.
        if (record.kind == WorldRecordKind::House && m_indexedLocationMaps.contains(mapId(*record.map)))
            return std::any_of(m_locations.begin(), m_locations.end(),
                [&](const WorldMapLocation &location) { return location.recordId == record.id; });
        return true;
    }
    if (!walkingRouteUnlocked(record, knowledge))
        return false;
    bool known = knowledge.knows(record);
    if (!known && record.kind == WorldRecordKind::Dungeon)
        for (const WorldMapLocation &location : m_locations)
        {
            if (location.recordId != record.id)
                continue;
            const WorldRecord *pSource = find(location.mapId);
            const auto exploration = knowledge.regionMaps.find(toLowerCopy(pSource->map->fileName));
            if (knowledge.knows(*pSource) && exploration != knowledge.regionMaps.end()
                && worldRegionPointExplored(exploration->second, location.x, location.y))
                known = true;
        }
    if (!known)
        return false;
    // A visited dungeon can be known without knowledge of its containing region. For located services,
    // require knowledge of the containing map so search cannot disclose an undiscovered location.
    if (record.house != nullptr)
    {
        if (recordMap(record, knowledge, false) == nullptr) return false;
    }
    if (record.destination != nullptr)
    {
        const auto *destination = find(mapId(*record.destination));
        if (destination == nullptr || !knowledge.knows(*destination))
            return false;
    }
    return true;
}

bool WorldDatabase::walkingRouteUnlocked(const WorldRecord &record, const WorldKnowledge &knowledge) const
{
    return record.walkingRoute == nullptr || walkingTravelQuestRequirementsMet(*record.walkingRoute,
        [&knowledge](uint32_t bit) { return knowledge.questBits.contains(bit); });
}

std::vector<WorldTravelLink> WorldDatabase::travelLinks(const WorldKnowledge &knowledge, bool guide,
    const std::string &search) const
{
    std::vector<WorldTravelLink> result;
    const auto anchor = [this](const WorldRecord *pMap) -> const WorldRecord *
    {
        // The constructor validates the parent graph; no synthetic map anchors are invented.
        while (pMap != nullptr && !pMap->position && !pMap->parentId.empty())
            pMap = find(pMap->parentId);
        return pMap != nullptr && pMap->position ? pMap : nullptr;
    };
    for (const WorldRecord *pRecord : query(knowledge, guide, search, WorldRecordKind::Travel))
    {
        const WorldRecord &record = *pRecord;
        const WorldRecord *pSource = anchor(find(mapId(*record.map)));
        const WorldRecord *pDestination = anchor(find(mapId(*record.destination)));
        if (pSource == nullptr || pDestination == nullptr || pSource == pDestination
            || !visible(*pSource, knowledge, guide) || !visible(*pDestination, knowledge, guide))
            continue;
        const WorldTravelKind kind = record.walkingRoute != nullptr ? WorldTravelKind::Walking
            : toLowerCopy(record.house->type) == "boats" ? WorldTravelKind::Boat : WorldTravelKind::Coach;
        // A last-offered connection is retained knowledge, not a live availability prediction.
        const bool locked = !walkingRouteUnlocked(record, knowledge)
            || (record.transport && !record.transport->observed && record.transport->route.requiredQBit != 0
                && !knowledge.questBits.contains(record.transport->route.requiredQBit));
        result.push_back({record.id, pSource->id, pDestination->id, *pSource->position,
            *pDestination->position, locked, kind});
    }
    return result;
}

std::vector<const WorldRecord *> WorldDatabase::query(const WorldKnowledge &knowledge, bool guide,
    const std::string &search, std::optional<WorldRecordKind> kind, const std::string &withinMap, bool includeTravel) const
{
    std::vector<const WorldRecord *> result;
    const auto needle = toLowerCopy(search);
    for (const auto &record : m_records)
    {
        if ((!includeTravel && record.kind == WorldRecordKind::Travel)
            || !visible(record, knowledge, guide) || (kind && record.kind != *kind))
            continue;
        if (!guide && record.house != nullptr && !withinMap.empty() && mapId(*record.map) == withinMap)
        {
            const WorldRecord *pSource = find(withinMap);
            if (pSource == nullptr || !knowledge.knows(*pSource)) continue;
        }
        if (!withinMap.empty() && mapId(*record.map) != withinMap && record.parentId != withinMap)
        {
            const bool locatedHere = std::any_of(m_locations.begin(), m_locations.end(),
                [&](const WorldMapLocation &location)
                {
                    const WorldRecord *pTarget = find(location.recordId);
                    return pTarget != nullptr && location.mapId == withinMap
                        && (location.recordId == record.id
                            || (record.house != nullptr && pTarget->house == record.house))
                        && (guide || pTarget->house == nullptr || houseLocationKnown(location, knowledge));
                });
            if (!locatedHere)
                continue;
        }
        if (!needle.empty()
            && toLowerCopy(recordSearchText(record, knowledge, guide, withinMap)).find(needle) == std::string::npos)
            continue;
        result.push_back(&record);
    }
    std::sort(result.begin(), result.end(), [](const WorldRecord *a, const WorldRecord *b)
    {
        const auto left = toLowerCopy(a->name()), right = toLowerCopy(b->name());
        return left == right ? a->id < b->id : left < right;
    });
    return result;
}

const char *WorldDatabase::kindName(WorldRecordKind kind)
{
    switch (kind)
    {
    case WorldRecordKind::Region: return "Region";
    case WorldRecordKind::Dungeon: return "Dungeon";
    case WorldRecordKind::House: return "Building";
    case WorldRecordKind::Npc: return "NPC";
    case WorldRecordKind::Trainer: return "Trainer";
    case WorldRecordKind::Travel: return "Travel";
    }
    return "";
}
} // namespace OpenYAMM::Game
