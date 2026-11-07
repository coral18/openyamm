#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace OpenYAMM::Game
{
class NpcDialogTable;
class MergedTeacherTopicTable;
struct EventRuntimeState;
struct MapStatsEntry;
class IGameplayWorldRuntime;

// A remembered encounter, not a live tracker or an authored spawn guarantee.
struct WorldNpcActorLocation
{
    std::string mapId;
    uint32_t actorIndex = 0;
    int32_t x = 0, y = 0, z = 0;
    int32_t gameMinutes = 0;
    bool operator==(const WorldNpcActorLocation &) const = default;
};

struct WorldNpcScriptLocation
{
    std::string mapId;
    uint16_t eventId = 0;
    int32_t gameMinutes = 0;
    bool operator==(const WorldNpcScriptLocation &) const = default;
};

// A derived read-only index input; definitions remain owned by the loaded NPC registry.
struct WorldNpcState
{
    std::unordered_map<uint32_t, uint32_t> houses;
    std::unordered_map<uint32_t, std::vector<uint32_t>> topics;
    std::unordered_map<uint32_t, std::string> names;
    std::unordered_set<uint32_t> unavailable;
    std::unordered_map<uint32_t, WorldNpcActorLocation> actorLocations;
    std::unordered_map<uint32_t, WorldNpcScriptLocation> scriptLocations;
    bool observedOnly = false;
};

WorldNpcState worldCurrentNpcs(const NpcDialogTable &npcs, const EventRuntimeState &state);
WorldNpcState worldObservedNpcs(const NpcDialogTable &npcs, const MergedTeacherTopicTable &teachers,
    const std::unordered_map<std::string, int32_t> &variables);
void recordWorldNpcObservation(EventRuntimeState &state, uint32_t npcId, uint32_t houseId,
    const std::vector<uint32_t> &visibleTeacherTopics);
void recordWorldNpcActorLocation(EventRuntimeState &state, uint32_t npcId,
    const WorldNpcActorLocation &location);
bool observeWorldNpcActor(EventRuntimeState &state, uint32_t npcId, uint32_t actorIndex,
    const MapStatsEntry &map, const IGameplayWorldRuntime &world);
bool worldNpcActorLocationKnown(const std::unordered_map<std::string, int32_t> &variables,
    uint32_t npcId, const WorldNpcActorLocation &location);
void recordWorldNpcScriptLocation(EventRuntimeState &state, uint32_t npcId,
    const WorldNpcScriptLocation &location);
bool worldNpcScriptLocationKnown(const std::unordered_map<std::string, int32_t> &variables,
    uint32_t npcId, const WorldNpcScriptLocation &location);
std::string worldNpcHouseKey(uint32_t npcId);
std::string worldNpcTeacherKey(uint32_t npcId, uint32_t topicId);
std::string worldNpcRosterOfferKey(uint32_t npcId, uint32_t rosterId);
void recordWorldRosterOfferObservation(EventRuntimeState &state, uint32_t npcId, uint32_t rosterId);
bool worldRosterOfferKnown(const std::unordered_map<std::string, int32_t> &variables,
    uint32_t npcId, uint32_t rosterId);
}
