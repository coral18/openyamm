#pragma once

#include <cstdint>
#include <functional>

namespace OpenYAMM::Game
{
class IGameplayWorldRuntime;
class Party;
struct MapEdgeTransition;

bool walkingTravelQuestRequirementsMet(const MapEdgeTransition &transition,
    const std::function<bool(uint32_t)> &hasQuestBit);

void applyTravelDaysSideEffects(
    Party &party,
    IGameplayWorldRuntime *pWorldRuntime,
    int travelDays,
    int foodRequired);
}
