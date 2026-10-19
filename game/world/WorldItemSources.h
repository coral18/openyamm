#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
class ItemTable;
class ObjectTable;
struct EventRuntimeState;
struct GameplayChestViewState;
struct GameplayWorldItemInspectState;
struct MapDeltaData;

enum class WorldItemSourceKind { Placed, Chest, Carried };

struct WorldItemSource
{
    std::string id, mapId;
    uint32_t itemId = 0, sourceIndex = 0;
    WorldItemSourceKind kind = WorldItemSourceKind::Placed;
    std::optional<std::array<int32_t, 3>> position;
    bool observed = false;
    int32_t gameMinutes = 0;
};

// Reads authored IDs only. Does not materialize random treasure, alter artifacts or advance any RNG.
std::vector<WorldItemSource> buildWorldItemSources(const MapDeltaData &delta,
    const ItemTable &items, const ObjectTable &objects);
std::string worldItemSourceLabel(const WorldItemSource &source);
void recordWorldChestItems(EventRuntimeState &state, const GameplayChestViewState &view, float gameMinutes);
void recordWorldItemPickup(EventRuntimeState &state, const GameplayWorldItemInspectState &item, float gameMinutes);
std::vector<WorldItemSource> worldObservedItemSources(const EventRuntimeState &state);
}
