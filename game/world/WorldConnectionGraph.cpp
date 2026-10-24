#include "game/world/WorldConnectionGraph.h"
#include "game/world/WorldDatabase.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace OpenYAMM::Game
{
std::string worldConnectionId(const std::string &source, uint16_t event, const std::string &destination)
{
    return source + "/event/" + std::to_string(event) + "/to/" + destination;
}

void WorldDatabase::indexMapConnections(const MapStatsEntry &map, const ScriptedEventProgram &events)
{
    const std::string source = mapId(map);
    std::vector<WorldConnection> connections;
    for (const auto &[event, targets] : events.connectionTargets())
        for (const std::string &file : targets)
        {
            const auto destination = std::find_if(m_pMaps->getEntries().begin(), m_pMaps->getEntries().end(),
                [&](const MapStatsEntry &entry)
                {
                    return entry.worldId == map.worldId && toLowerCopy(entry.fileName) == file;
                });
            if (destination == m_pMaps->getEntries().end())
                throw std::invalid_argument("Unknown connection target: " + source + "/" + file);
            const std::string target = mapId(*destination);
            connections.push_back({worldConnectionId(source, event, target), source, target, event, true, {}});
        }
    std::erase_if(m_connections, [&](const WorldConnection &entry)
    {
        return entry.sourceId == source;
    });
    m_connections.insert(m_connections.end(), connections.begin(), connections.end());
}

const std::vector<WorldConnection> &WorldDatabase::connections() const
{
    return m_connections;
}

WorldConnectionGraph buildWorldConnectionGraph(const WorldDatabase &world, const WorldKnowledge &knowledge,
    const std::string &rootMapId, bool guide)
{
    WorldConnectionGraph graph;
    const WorldRecord *pRoot = world.find(rootMapId);
    if (pRoot == nullptr || (pRoot->kind != WorldRecordKind::Region && pRoot->kind != WorldRecordKind::Dungeon)
        || !world.visible(*pRoot, knowledge, guide)) return graph;
    const auto localMap = [&](const std::string &id) -> const MapStatsEntry *
    {
        const WorldRecord *pRecord = world.find(id);
        return pRecord != nullptr
            && (pRecord->kind == WorldRecordKind::Region || pRecord->kind == WorldRecordKind::Dungeon)
            ? pRecord->map : nullptr;
    };
    std::map<std::string, WorldConnection> edges;
    if (guide)
        for (const WorldConnection &connection : world.connections())
            if (localMap(connection.sourceId) && localMap(connection.destinationId))
                edges.emplace(connection.id, connection);
    for (const auto &[owner, arrivals] : knowledge.arrivals)
    {
        const MapStatsEntry *pDestination = localMap(owner);
        // Legacy filename ownership is resolved only inside this world's catalog.
        if (pDestination == nullptr && !owner.starts_with("world."))
            for (const WorldRecord &record : world.records())
                if ((record.kind == WorldRecordKind::Region || record.kind == WorldRecordKind::Dungeon)
                    && toLowerCopy(record.map->fileName) == owner) pDestination = record.map;
        if (pDestination == nullptr || !knowledge.visitedMaps.contains(toLowerCopy(pDestination->fileName))) continue;
        for (const WorldMapArrival &arrival : arrivals)
        {
            const MapStatsEntry *pSource = localMap(arrival.sourceMapId);
            if (pSource == nullptr || arrival.eventId == 0 || arrival.gameMinutes < 0
                || toLowerCopy(arrival.mapFileName) != toLowerCopy(pDestination->fileName)
                || !knowledge.visitedMaps.contains(toLowerCopy(pSource->fileName))) continue;
            const std::string destination = WorldDatabase::mapId(*pDestination);
            const std::string id = worldConnectionId(arrival.sourceMapId, arrival.eventId, destination);
            const auto found = edges.try_emplace(id,
                WorldConnection{id, arrival.sourceMapId, destination, arrival.eventId, false, {}}).first;
            if (!found->second.observedGameMinutes || *found->second.observedGameMinutes < arrival.gameMinutes)
                found->second.observedGameMinutes = arrival.gameMinutes;
        }
    }
    std::set<std::string> included = {rootMapId}, expanded;
    std::vector<std::string> pending = {rootMapId};
    while (!pending.empty())
    {
        const std::string current = pending.back();
        pending.pop_back();
        if (!expanded.insert(current).second) continue;
        for (const auto &[id, edge] : edges)
        {
            if (edge.sourceId != current && edge.destinationId != current) continue;
            const std::string next = edge.sourceId == current ? edge.destinationId : edge.sourceId;
            if (included.insert(next).second && localMap(next)->fileName.ends_with(".blv")) pending.push_back(next);
        }
    }
    for (const std::string &id : included) graph.nodes.push_back(localMap(id));
    std::sort(graph.nodes.begin(), graph.nodes.end(), [&](const MapStatsEntry *a, const MapStatsEntry *b)
    {
        if (WorldDatabase::mapId(*a) == rootMapId) return WorldDatabase::mapId(*b) != rootMapId;
        if (WorldDatabase::mapId(*b) == rootMapId) return false;
        return a->name == b->name ? WorldDatabase::mapId(*a) < WorldDatabase::mapId(*b) : a->name < b->name;
    });
    for (const auto &[id, edge] : edges)
        if (included.contains(edge.sourceId) && included.contains(edge.destinationId)) graph.edges.push_back(edge);
    return graph;
}
}
