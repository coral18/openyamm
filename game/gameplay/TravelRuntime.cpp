#include "game/gameplay/TravelRuntime.h"

#include "game/gameplay/GameplayRuntimeInterfaces.h"
#include "game/party/Party.h"
#include "game/events/EventRuntime.h"
#include "game/tables/MapStats.h"

#include <algorithm>

namespace OpenYAMM::Game
{
bool walkingTravelQuestRequirementsMet(const MapEdgeTransition &transition,
    const std::function<bool(uint32_t)> &hasQuestBit)
{
    return transition.requiredQuestBitsAny.empty()
        || std::any_of(transition.requiredQuestBitsAny.begin(), transition.requiredQuestBitsAny.end(), hasQuestBit);
}

HouseEntry::TransportRoute effectiveTransportRoute(
    const HouseEntry &houseEntry,
    const HouseEntry::TransportRoute &baseRoute,
    const EventRuntimeState *pEventRuntimeState)
{
    if (pEventRuntimeState == nullptr)
    {
        return baseRoute;
    }

    const uint64_t key = EventRuntime::transportRouteOverrideKey(houseEntry.id, baseRoute.routeIndex);
    const auto iterator = pEventRuntimeState->transportRouteOverrides.find(key);

    if (iterator == pEventRuntimeState->transportRouteOverrides.end())
    {
        return baseRoute;
    }

    const EventRuntimeState::TransportRouteOverride &overrideRoute = iterator->second;
    HouseEntry::TransportRoute route = baseRoute;
    route.destinationName = overrideRoute.destinationName.empty()
        ? baseRoute.destinationName
        : overrideRoute.destinationName;
    route.mapFileName = overrideRoute.mapFileName.empty() ? baseRoute.mapFileName : overrideRoute.mapFileName;
    route.daysAvailable = overrideRoute.daysAvailable;
    route.travelDays = overrideRoute.travelDays == 0 ? baseRoute.travelDays : overrideRoute.travelDays;
    route.x = overrideRoute.x;
    route.y = overrideRoute.y;
    route.z = overrideRoute.z;
    route.directionDegrees = overrideRoute.directionDegrees;
    route.requiredQBit = overrideRoute.requiredQBit;
    route.useMapStartPosition = overrideRoute.useMapStartPosition;
    return route;
}


void applyTravelDaysSideEffects(
    Party &party,
    IGameplayWorldRuntime *pWorldRuntime,
    int travelDays,
    int foodRequired)
{
    travelDays = std::max(0, travelDays);
    foodRequired = std::max(0, foodRequired);
    constexpr float MinutesPerDay = 24.0f * 60.0f;
    if (travelDays > 0 && pWorldRuntime != nullptr)
    {
        const float travelMinutes = static_cast<float>(travelDays) * MinutesPerDay;
        pWorldRuntime->advanceGameMinutes(travelMinutes);
        party.advanceTimedStates(travelMinutes * 60.0f);
    }
    if (travelDays == 0 && foodRequired == 0)
    {
        return;
    }

    party.restAndHealAll();
    const int availableFood = party.food();
    if (availableFood > 0)
    {
        party.addFood(-foodRequired);
    }
    if (availableFood >= foodRequired)
    {
        return;
    }

    const float gameMinutes = pWorldRuntime != nullptr ? pWorldRuntime->gameMinutes() : 0.0f;
    for (size_t memberIndex = 0; memberIndex < party.members().size(); ++memberIndex)
    {
        const Character *pMember = party.member(memberIndex);
        if (pMember != nullptr && pMember->health > 0)
        {
            party.applyMemberCondition(memberIndex, CharacterCondition::Weak, gameMinutes);
        }
    }
}
}
