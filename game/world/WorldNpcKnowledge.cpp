#include "game/world/WorldNpcKnowledge.h"
#include "game/events/EventRuntime.h"
#include "game/tables/NpcDialogTable.h"
#include "game/tables/MergedBaseTables.h"

#include <algorithm>
#include <bit>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
std::string teacherPrefix(uint32_t npcId)
{
    return "openyamm.discovery.npc/" + std::to_string(npcId) + "/teacher/";
}
}

std::string worldNpcHouseKey(uint32_t npcId)
{
    return "openyamm.discovery.npc/" + std::to_string(npcId) + "/house";
}

std::string worldNpcTeacherKey(uint32_t npcId, uint32_t topicId)
{
    return teacherPrefix(npcId) + std::to_string(topicId);
}

WorldNpcState worldCurrentNpcs(const NpcDialogTable &npcs, const EventRuntimeState &state)
{
    WorldNpcState result;
    result.houses = state.npcHouseOverrides;
    result.names = state.npcNameOverrides;
    result.unavailable = state.unavailableNpcIds;
    for (const NpcEntry &npc : npcs.entries())
    {
        const auto found = state.npcTopicOverrides.find(npc.id);
        if (found != state.npcTopicOverrides.end())
        {
            result.topics[npc.id] = npcs.getTopicIdsForNpc(npc.id, &found->second);
        }
    }
    return result;
}

void recordWorldNpcObservation(EventRuntimeState &state, uint32_t npcId, uint32_t houseId,
    const std::vector<uint32_t> &visibleTeacherTopics)
{
    if (npcId == 0)
    {
        throw std::invalid_argument("Invalid observed NPC or house ID");
    }
    // Zero is meaningful: an actor/remote dialogue does not prove a residence.
    state.namedGlobalVars[worldNpcHouseKey(npcId)] = std::bit_cast<int32_t>(houseId);
    state.namedGlobalVars["openyamm.discovery.npc/" + std::to_string(npcId)] = 1;
    const std::string prefix = teacherPrefix(npcId);
    for (auto &[key, value] : state.namedGlobalVars)
    {
        if (key.starts_with(prefix))
        {
            // Explicit zero propagates through the session's existing global-variable merge.
            value = 0;
        }
    }
    for (uint32_t topic : visibleTeacherTopics)
    {
        state.namedGlobalVars[prefix + std::to_string(topic)] = 1;
    }
}

WorldNpcState worldObservedNpcs(const NpcDialogTable &npcs, const MergedTeacherTopicTable &teachers,
    const std::unordered_map<std::string, int32_t> &variables)
{
    WorldNpcState result;
    result.observedOnly = true;
    for (const NpcEntry &npc : npcs.entries())
    {
        const auto house = variables.find(worldNpcHouseKey(npc.id));
        if (house == variables.end())
        {
            continue;
        }
        result.houses[npc.id] = std::bit_cast<uint32_t>(house->second);
        std::vector<uint32_t> &topics = result.topics[npc.id];
        const std::string prefix = teacherPrefix(npc.id);
        for (const MergedTeacherTopicEntry &teacher : teachers.entries())
        {
            const auto known = variables.find(prefix + std::to_string(teacher.topicId));
            if (known != variables.end() && known->second > 0)
            {
                topics.push_back(teacher.topicId);
            }
        }
        std::sort(topics.begin(), topics.end());
    }
    return result;
}
}
