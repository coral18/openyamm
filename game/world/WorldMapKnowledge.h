#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
struct EventRuntimeState;

struct WorldMapArrival
{
    std::string sourceMapId;
    uint16_t eventId = 0;
    std::string mapFileName;
    int32_t x = 0, y = 0, z = 0;
    int32_t gameMinutes = 0;
};

// Call only after a successful event-driven move, using the actual destination runtime position.
bool recordWorldMapArrival(EventRuntimeState &state, const std::string &sourceMapId, uint32_t eventId,
    float x, float y, float z, float gameMinutes);
std::vector<WorldMapArrival> worldMapArrivals(const EventRuntimeState &state, const std::string &mapFileName);

// Map refills replace gameplay state, not the party's observations and annotations.
void restoreWorldMapKnowledgeAfterRefill(const EventRuntimeState &previous, EventRuntimeState &refilled);
}
