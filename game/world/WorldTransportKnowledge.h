#pragma once

#include "game/gameplay/TransportOffer.h"
#include <optional>
#include <unordered_map>

namespace OpenYAMM::Game
{
struct EventRuntimeState;
class MapStats;
struct MapStatsEntry;

struct WorldTransportEntry
{
    HouseEntry::TransportRoute route;
    std::optional<TransportQuote> quote;
    bool observed = false;
    bool current = false;
};

struct WorldTransportState
{
    std::unordered_map<uint32_t, std::vector<WorldTransportEntry>> houses;
    std::vector<std::string> issues;
    bool observedOnly = false;
};

std::string worldTransportKey(uint32_t houseId, uint32_t routeIndex);
void recordWorldTransportObservation(EventRuntimeState &state, uint32_t houseId,
    const TransportOffer &offer, const std::vector<MapStatsEntry> &maps);
WorldTransportState worldObservedTransport(const HouseTable &houses, const MapStats &maps,
    const std::unordered_map<std::string, int32_t> &variables);
WorldTransportState worldCurrentTransport(const HouseTable &houses, uint32_t mapId, const EventRuntimeState &state);
bool worldTransportObservationMatches(uint32_t houseId, const WorldTransportEntry &entry,
    const std::string &canonicalDestination,
    const std::unordered_map<std::string, int32_t> &variables);
}
