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

// A derived read-only index input; definitions remain owned by the loaded NPC registry.
struct WorldNpcState
{
    std::unordered_map<uint32_t, uint32_t> houses;
    std::unordered_map<uint32_t, std::vector<uint32_t>> topics;
    std::unordered_map<uint32_t, std::string> names;
    std::unordered_set<uint32_t> unavailable;
    bool observedOnly = false;
};

WorldNpcState worldCurrentNpcs(const NpcDialogTable &npcs, const EventRuntimeState &state);
WorldNpcState worldObservedNpcs(const NpcDialogTable &npcs, const MergedTeacherTopicTable &teachers,
    const std::unordered_map<std::string, int32_t> &variables);
void recordWorldNpcObservation(EventRuntimeState &state, uint32_t npcId, uint32_t houseId,
    const std::vector<uint32_t> &visibleTeacherTopics);
std::string worldNpcHouseKey(uint32_t npcId);
std::string worldNpcTeacherKey(uint32_t npcId, uint32_t topicId);
std::string worldNpcRosterOfferKey(uint32_t npcId, uint32_t rosterId);
void recordWorldRosterOfferObservation(EventRuntimeState &state, uint32_t npcId, uint32_t rosterId);
bool worldRosterOfferKnown(const std::unordered_map<std::string, int32_t> &variables,
    uint32_t npcId, uint32_t rosterId);
}
