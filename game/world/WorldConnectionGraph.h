#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class WorldDatabase;
struct WorldKnowledge;
struct MapStatsEntry;

struct WorldConnection
{
    std::string id, sourceId, destinationId;
    uint16_t eventId = 0;
    bool declared = false;
    std::optional<int32_t> observedGameMinutes;
};

struct WorldConnectionGraph
{
    std::vector<const MapStatsEntry *> nodes;
    std::vector<WorldConnection> edges;
};

std::string worldConnectionId(const std::string &source, uint16_t event, const std::string &destination);
// Outdoor endpoints bound a dungeon cluster. Arrows remain directed; no return edge is inferred.
WorldConnectionGraph buildWorldConnectionGraph(const WorldDatabase &world, const WorldKnowledge &knowledge,
    const std::string &rootMapId, bool guide);
}
