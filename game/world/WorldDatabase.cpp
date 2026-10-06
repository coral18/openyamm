#include "game/world/WorldDatabase.h"
#include "game/StringUtils.h"
#include "game/maps/MapIdentity.h"

#include <algorithm>
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
        return destination->name;
    if (kind == WorldRecordKind::Trainer)
        return npc->name + " - " + teacher->note;
    if (kind == WorldRecordKind::Npc)
        return npc->name;
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
    if (record.kind == WorldRecordKind::Npc || record.kind == WorldRecordKind::Trainer)
        return discovered(*this, WorldDatabase::npcDiscoveryKey(record.npc->id));
    if (record.kind == WorldRecordKind::House || (record.kind == WorldRecordKind::Travel && record.house != nullptr))
        return discovered(*this, WorldDatabase::houseDiscoveryKey(record.house->id));
    return discovered(*this, WorldDatabase::mapDiscoveryKey(*record.map))
        || visitedMaps.contains(toLowerCopy(record.map->fileName));
}

WorldDatabase::WorldDatabase(const MapStats &maps, const HouseTable &houses, const NpcDialogTable &npcs,
    const MergedTeacherTopicTable &teachers, const std::string &worldId,
    const std::vector<WorldMapPresentation> &presentation)
{
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
            travel.destination = destination;
            m_records.push_back(std::move(travel));
        }
    }
    for (const auto &[id, house] : houses.entries())
    {
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
        for (const auto &route : house.transportRoutes)
        {
            const auto *destination = maps.findByFileName(route.mapFileName);
            // Cross-world travel is not presented as a local route.
            if (destination == nullptr || destination->worldId != worldId)
                continue;
            WorldRecord travel = record;
            travel.id += "/route/" + std::to_string(route.routeIndex);
            travel.parentId = record.id;
            travel.kind = WorldRecordKind::Travel;
            travel.route = &route;
            travel.destination = destination;
            m_records.push_back(std::move(travel));
        }
        for (uint32_t npcId : npcs.getNpcIdsForHouse(id))
        {
            const NpcEntry *npc = npcs.getNpc(npcId);
            if (npc == nullptr)
                continue;
            WorldRecord resident = record;
            resident.id = worldId + ":npc/" + std::to_string(npcId);
            resident.parentId = record.id;
            resident.kind = WorldRecordKind::Npc;
            resident.npc = npc;
            m_records.push_back(resident);
            for (uint32_t topicId : npc->topicIds)
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
const WorldRecord *WorldDatabase::find(const std::string &id) const
{
    const auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : &m_records[it->second];
}

bool WorldDatabase::visible(const WorldRecord &record, const WorldKnowledge &knowledge, bool guide) const
{
    if (guide)
        return true;
    if (!knowledge.knows(record))
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

std::vector<const WorldRecord *> WorldDatabase::query(const WorldKnowledge &knowledge, bool guide,
    const std::string &search, std::optional<WorldRecordKind> kind, const std::string &withinMap) const
{
    std::vector<const WorldRecord *> result;
    const auto needle = toLowerCopy(search);
    for (const auto &record : m_records)
    {
        if (!visible(record, knowledge, guide) || (kind && record.kind != *kind))
            continue;
        if (!withinMap.empty() && mapId(*record.map) != withinMap && record.parentId != withinMap)
            continue;
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
