#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class ItemTable;
class ObjectTable;
class ScriptedEventProgram;
struct EventRuntimeState;
struct GameplayChestViewState;
struct GameplayWorldItemInspectState;
struct MapDeltaData;

enum class WorldItemSourceKind { Placed, Chest, Carried, MapReward, SharedReward };

struct WorldItemSource
{
    std::string id, mapId;
    uint32_t itemId = 0, sourceIndex = 0;
    WorldItemSourceKind kind = WorldItemSourceKind::Placed;
    std::optional<std::array<int32_t, 3>> position;
    bool observed = false;
    int32_t gameMinutes = 0;
    std::string providerId;
};

// Reads authored IDs only. Does not materialize random treasure, alter artifacts or advance any RNG.
std::vector<WorldItemSource> buildWorldItemSources(const MapDeltaData &delta,
    const ItemTable &items, const ObjectTable &objects);
std::string worldItemSourceLabel(const WorldItemSource &source);
void recordWorldChestItems(EventRuntimeState &state, const GameplayChestViewState &view, float gameMinutes);
void recordWorldItemPickup(EventRuntimeState &state, const GameplayWorldItemInspectState &item, float gameMinutes);
std::vector<WorldItemSource> worldObservedItemSources(const EventRuntimeState &state);
// Pending origin is saved alongside the existing grant queue; it is not a discovery until delivery succeeds.
void stageWorldItemReward(EventRuntimeState &state, bool detailed, size_t index,
    uint16_t eventId, bool global, float gameMinutes);
void confirmWorldItemReward(EventRuntimeState &state, bool detailed, size_t index);
void clearWorldItemRewardQueue(EventRuntimeState &state);
void recordWorldItemReward(EventRuntimeState &state, uint32_t itemId,
    uint16_t eventId, bool global, float gameMinutes);
}
