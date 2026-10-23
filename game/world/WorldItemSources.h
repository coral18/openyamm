#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class Party;
class ItemTable;
class ObjectTable;
class ScriptedEventProgram;
class IGameplayWorldRuntime;
struct EventRuntimeState;
struct GameplayChestViewState;
struct GameplayCorpseViewState;
struct GameplayChestItemState;
struct GameplayWorldItemInspectState;
struct MapDeltaData;

enum class WorldItemSourceKind { Placed, Chest, Carried, MapReward, SharedReward, DeathDrop };

struct WorldItemSource
{
    std::string id, mapId;
    uint32_t itemId = 0, sourceIndex = 0;
    WorldItemSourceKind kind = WorldItemSourceKind::Placed;
    std::optional<std::array<int32_t, 3>> position;
    bool observed = false;
    int32_t gameMinutes = 0;
    std::string providerId;
    int16_t monsterId = 0;
};

struct WorldChestObservation
{
    uint32_t chestId = 0;
    uint32_t visibleSlots = 0;
    bool empty = false;
    int32_t gameMinutes = 0;
};

// Reads authored IDs only. Does not materialize random treasure, alter artifacts or advance any RNG.
std::vector<WorldItemSource> buildWorldItemSources(const MapDeltaData &delta,
    const ItemTable &items, const ObjectTable &objects);
std::string worldItemSourceLabel(const WorldItemSource &source);
// Record only a view actually inspected by the party, including an empty view.
void recordWorldChestView(EventRuntimeState &state, const GameplayChestViewState &view, float gameMinutes,
    Party *pParty = nullptr);
std::vector<WorldChestObservation> worldChestObservations(const EventRuntimeState &state);
void recordWorldItemPickup(EventRuntimeState &state, const GameplayWorldItemInspectState &item, float gameMinutes);
// Call only after taking an item successfully. Does not inspect or materialize other creature loot.
void recordWorldActorItem(IGameplayWorldRuntime &runtime, const GameplayCorpseViewState &source,
    const GameplayChestItemState &item);
std::vector<WorldItemSource> worldObservedItemSources(const EventRuntimeState &state);
// Pending origin is saved alongside the existing grant queue; it is not a discovery until delivery succeeds.
void stageWorldItemReward(EventRuntimeState &state, bool detailed, size_t index,
    uint16_t eventId, bool global, float gameMinutes);
void confirmWorldItemReward(EventRuntimeState &state, bool detailed, size_t index);
void clearWorldItemRewardQueue(EventRuntimeState &state);
void recordWorldItemReward(EventRuntimeState &state, uint32_t itemId,
    uint16_t eventId, bool global, float gameMinutes);
}
