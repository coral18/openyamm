#include "game/world/WorldDatabase.h"
#include "game/StringUtils.h"
#include "game/maps/MapIdentity.h"
#include "game/gameplay/TravelRuntime.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

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
        return npcName.value_or(npc->name);
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
            m_records.push_back(resident);
            const auto topics = npcsState.topics.find(npcId);
            const std::vector<uint32_t> &topicIds = topics != npcsState.topics.end() ? topics->second : npc->topicIds;
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
}

const std::vector<WorldRecord> &WorldDatabase::records() const { return m_records; }
const std::vector<std::string> &WorldDatabase::transportIssues() const { return m_transportIssues; }
const WorldRecord *WorldDatabase::find(const std::string &id) const
{
    const auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : &m_records[it->second];
}

bool WorldDatabase::visible(const WorldRecord &record, const WorldKnowledge &knowledge, bool guide) const
{
    if (guide)
        return true;
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
        const auto *map = find(mapId(*record.map));
        if (map == nullptr || !knowledge.knows(*map))
            return false;
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

std::vector<WorldWalkingLink> WorldDatabase::walkingLinks(const WorldKnowledge &knowledge, bool guide) const
{
    std::vector<WorldWalkingLink> result;
    const auto anchor = [this](const WorldRecord *pMap) -> const WorldRecord *
    {
        // The constructor validates the parent graph; no synthetic map anchors are invented.
        while (pMap != nullptr && !pMap->position && !pMap->parentId.empty())
            pMap = find(pMap->parentId);
        return pMap != nullptr && pMap->position ? pMap : nullptr;
    };
    for (const WorldRecord &record : m_records)
    {
        if (record.walkingRoute == nullptr || !visible(record, knowledge, guide))
            continue;
        const WorldRecord *pSource = anchor(find(mapId(*record.map)));
        const WorldRecord *pDestination = anchor(find(mapId(*record.destination)));
        if (pSource == nullptr || pDestination == nullptr || pSource == pDestination
            || !visible(*pSource, knowledge, guide) || !visible(*pDestination, knowledge, guide))
            continue;
        result.push_back({record.id, pSource->id, pDestination->id, *pSource->position,
            *pDestination->position, !walkingRouteUnlocked(record, knowledge)});
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
        if (!withinMap.empty() && mapId(*record.map) != withinMap && record.parentId != withinMap)
        {
            const bool locatedHere = std::any_of(m_locations.begin(), m_locations.end(),
                [&](const WorldMapLocation &location)
                { return location.recordId == record.id && location.mapId == withinMap; });
            if (!locatedHere)
                continue;
        }
        if (!needle.empty() && toLowerCopy(record.searchText()).find(needle) == std::string::npos)
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
