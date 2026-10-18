#pragma once

#include <cstdint>
#include <functional>
#include "game/tables/HouseTable.h"

namespace OpenYAMM::Game
{
class IGameplayWorldRuntime;
class Party;
struct MapEdgeTransition;
struct EventRuntimeState;

HouseEntry::TransportRoute effectiveTransportRoute(const HouseEntry &houseEntry,
    const HouseEntry::TransportRoute &baseRoute, const EventRuntimeState *pEventRuntimeState);

bool walkingTravelQuestRequirementsMet(const MapEdgeTransition &transition,
    const std::function<bool(uint32_t)> &hasQuestBit);

void applyTravelDaysSideEffects(
    Party &party,
    IGameplayWorldRuntime *pWorldRuntime,
    int travelDays,
    int foodRequired);
}
