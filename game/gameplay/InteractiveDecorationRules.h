#pragma once

#include "game/tables/SpriteTables.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace OpenYAMM::Game
{
struct EventRuntimeState;
struct MapDeltaData;

// Nonzero legacy states adopt the initialization marker unchanged. An initialized all-zero state is depleted.
bool beginInteractiveDecorationInitialization(EventRuntimeState &state, const MapDeltaData *pMapDeltaData);
void markInteractiveDecorationStateInitialized(EventRuntimeState &state);

// Outdoor bindings retain their existing container set and saved decor-var ordering.
enum class InteractiveDecorationRuleSet { Shared, OutdoorBindings };

enum class InteractiveDecorationFamily : uint8_t
{
    None = 0,
    Barrel,
    Cauldron,
    TrashHeap,
    CampFire,
    Cask,
    FlourSack,
    LargeBag,
    Bucket,
    MightAndMagicSixTrashHeap,
    Crystal,
};

struct InteractiveDecorationBindingSpec
{
    uint16_t baseEventId = 0;
    uint8_t eventCount = 0;
    uint8_t initialState = 0;
    bool useSeededInitialState = false;
    bool hideWhenCleared = false;
    InteractiveDecorationFamily family = InteractiveDecorationFamily::None;
};

std::optional<InteractiveDecorationFamily> classifyInteractiveDecorationFamily(
    const DecorationEntry &decoration,
    const std::string &instanceName,
    InteractiveDecorationRuleSet rules = InteractiveDecorationRuleSet::Shared);

std::optional<InteractiveDecorationBindingSpec> resolveInteractiveDecorationBindingSpec(
    const DecorationEntry &decoration,
    const std::string &instanceName,
    InteractiveDecorationRuleSet rules = InteractiveDecorationRuleSet::Shared);

uint32_t makeInteractiveDecorationSeed(
    size_t entityIndex,
    uint16_t decorationListId,
    int x,
    int y,
    int z);

uint8_t initialInteractiveDecorationState(InteractiveDecorationFamily family, uint32_t seed);

bool interactiveDecorationIsCleared(uint8_t state, uint8_t eventCount, bool hideWhenCleared);
}
