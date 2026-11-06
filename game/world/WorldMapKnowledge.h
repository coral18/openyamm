#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Game
{
struct EventRuntimeState;
struct WorldGateChoice;
struct MapStatsEntry;

std::string worldHouseAddressVersionKey(uint32_t houseId);
std::string worldHouseAddressKey(uint32_t houseId, const std::string &mapId);
bool worldHouseAddressesRecorded(const std::unordered_map<std::string, int32_t> &variables, uint32_t houseId);
bool worldHouseAddressKnown(const std::unordered_map<std::string, int32_t> &variables,
    uint32_t houseId, const std::string &mapId);
// A real visit records its canonical map. Legacy global-only knowledge explicitly retains its former primary address.
void recordWorldHouseVisit(EventRuntimeState &state, uint32_t houseId, const MapStatsEntry &location,
    const MapStatsEntry *pLegacyPrimary);

struct WorldGateOffer
{
    uint16_t eventId = 0;
    std::string networkId, choiceId, mapId;
    int32_t gameMinutes = 0;
};
void recordWorldGateOffers(EventRuntimeState &state, uint16_t eventId, const std::string &networkId,
    const std::vector<WorldGateChoice> &choices, float gameMinutes);
std::vector<WorldGateOffer> worldGateOffers(const EventRuntimeState &state);

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
