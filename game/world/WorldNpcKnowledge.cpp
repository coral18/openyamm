#include "game/world/WorldNpcKnowledge.h"
#include "game/events/EventRuntime.h"
#include "game/tables/NpcDialogTable.h"
#include "game/tables/MergedBaseTables.h"
#include "game/gameplay/GameplayRuntimeInterfaces.h"
#include "game/maps/MapDeltaData.h"
#include "game/maps/MapIdentity.h"
#include "game/tables/MapStats.h"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
const std::string ActorPrefix = "openyamm.discovery.npc-actor.v1/";
const std::string ScriptPrefix = "openyamm.discovery.npc-script.v1/";

bool validMapId(const std::string &id)
{
    const size_t split = id.find(".map.", 6);
    return id.starts_with("world.") && split != std::string::npos && split > 6 && split + 5 < id.size()
        && id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-") == std::string::npos;
}

std::string actorPrefix(uint32_t npcId, const WorldNpcActorLocation &location)
{
    if (npcId == 0 || !validMapId(location.mapId) || location.gameMinutes < 0)
        throw std::invalid_argument("Invalid NPC actor encounter");
    return ActorPrefix + std::to_string(npcId) + "/" + location.mapId + "/";
}

void clearEncounterLocations(EventRuntimeState &state, uint32_t npcId)
{
    const std::string prefix = ActorPrefix + std::to_string(npcId) + "/";
    const std::string script = ScriptPrefix + std::to_string(npcId) + "/";
    for (auto &[key, value] : state.namedGlobalVars)
        if ((key.starts_with(prefix) || key.starts_with(script)) && key.ends_with("/seen")) value = 0;
}

WorldNpcActorLocation readActorLocation(const std::unordered_map<std::string, int32_t> &variables,
    const std::string &prefix, const std::string &mapId)
{
    const auto field = [&](const char *pName)
    {
        const auto found = variables.find(prefix + pName);
        if (found == variables.end()) throw std::invalid_argument("Incomplete saved NPC actor encounter");
        return found->second;
    };
    WorldNpcActorLocation result{mapId, std::bit_cast<uint32_t>(field("actor")),
        field("x"), field("y"), field("z"), field("time")};
    if (!validMapId(result.mapId) || result.gameMinutes < 0)
        throw std::invalid_argument("Invalid saved NPC actor encounter");
    return result;
}

std::string scriptPrefix(uint32_t npcId, const WorldNpcScriptLocation &location)
{
    if (npcId == 0 || !validMapId(location.mapId) || location.eventId == 0 || location.gameMinutes < 0)
        throw std::invalid_argument("Invalid NPC scripted encounter");
    return ScriptPrefix + std::to_string(npcId) + "/" + location.mapId + "/";
}

WorldNpcScriptLocation readScriptLocation(const std::unordered_map<std::string, int32_t> &variables,
    const std::string &prefix, const std::string &mapId)
{
    const auto event = variables.find(prefix + "event"), time = variables.find(prefix + "time");
    if (!validMapId(mapId) || event == variables.end() || time == variables.end()
        || event->second <= 0 || event->second > UINT16_MAX || time->second < 0)
        throw std::invalid_argument("Invalid saved NPC scripted encounter");
    return {mapId, uint16_t(event->second), time->second};
}

std::string teacherPrefix(uint32_t npcId)
{
    return "openyamm.discovery.npc/" + std::to_string(npcId) + "/teacher/";
}
}

void recordWorldNpcActorLocation(EventRuntimeState &state, uint32_t npcId,
    const WorldNpcActorLocation &location)
{
    const std::string prefix = actorPrefix(npcId, location);
    clearEncounterLocations(state, npcId);
    state.namedGlobalVars[prefix + "actor"] = std::bit_cast<int32_t>(location.actorIndex);
    state.namedGlobalVars[prefix + "x"] = location.x;
    state.namedGlobalVars[prefix + "y"] = location.y;
    state.namedGlobalVars[prefix + "z"] = location.z;
    state.namedGlobalVars[prefix + "time"] = location.gameMinutes;
    state.namedGlobalVars[prefix + "seen"] = 1;
}

bool worldNpcActorLocationKnown(const std::unordered_map<std::string, int32_t> &variables,
    uint32_t npcId, const WorldNpcActorLocation &location)
{
    const std::string prefix = actorPrefix(npcId, location);
    const auto seen = variables.find(prefix + "seen");
    if (seen == variables.end() || seen->second == 0) return false;
    if (seen->second != 1) throw std::invalid_argument("Invalid saved NPC actor encounter flag");
    return readActorLocation(variables, prefix, location.mapId) == location;
}

void recordWorldNpcScriptLocation(EventRuntimeState &state, uint32_t npcId,
    const WorldNpcScriptLocation &location)
{
    const std::string prefix = scriptPrefix(npcId, location);
    clearEncounterLocations(state, npcId);
    state.namedGlobalVars[prefix + "event"] = location.eventId;
    state.namedGlobalVars[prefix + "time"] = location.gameMinutes;
    state.namedGlobalVars[prefix + "seen"] = 1;
}

bool worldNpcScriptLocationKnown(const std::unordered_map<std::string, int32_t> &variables,
    uint32_t npcId, const WorldNpcScriptLocation &location)
{
    const std::string prefix = scriptPrefix(npcId, location);
    const auto seen = variables.find(prefix + "seen");
    if (seen == variables.end() || seen->second == 0) return false;
    if (seen->second != 1) throw std::invalid_argument("Invalid saved NPC scripted encounter flag");
    return readScriptLocation(variables, prefix, location.mapId) == location;
}

bool observeWorldNpcActor(EventRuntimeState &state, uint32_t npcId, uint32_t actorIndex,
    const MapStatsEntry &map, const IGameplayWorldRuntime &world)
{
    const MapDeltaData *pDelta = world.mapDeltaData();
    GameplayRuntimeActorState actor;
    if (npcId == 0 || pDelta == nullptr || actorIndex >= pDelta->actors.size()
        || pDelta->actors[actorIndex].npcId <= 0 || uint32_t(pDelta->actors[actorIndex].npcId) != npcId
        || !world.actorRuntimeState(actorIndex, actor) || actor.isDead || actor.isInvisible) return false;
    const auto coordinate = [](double value) -> std::optional<int32_t>
    {
        const double rounded = std::round(value);
        if (!std::isfinite(rounded) || rounded < INT32_MIN || rounded > INT32_MAX) return {};
        return int32_t(rounded);
    };
    const auto x = coordinate(actor.preciseX), y = coordinate(actor.preciseY), z = coordinate(actor.preciseZ);
    const auto time = coordinate(std::floor(double(world.gameMinutes())));
    if (!x || !y || !z || !time || *time < 0) return false;
    recordWorldNpcActorLocation(state, npcId,
        {map.canonicalId.empty() ? buildCanonicalMapId(map.worldId, map.fileName) : map.canonicalId,
            actorIndex, *x, *y, *z, *time});
    return true;
}

std::string worldNpcHouseKey(uint32_t npcId)
{
    return "openyamm.discovery.npc/" + std::to_string(npcId) + "/house";
}

std::string worldNpcTeacherKey(uint32_t npcId, uint32_t topicId)
{
    return teacherPrefix(npcId) + std::to_string(topicId);
}

std::string worldNpcRosterOfferKey(uint32_t npcId, uint32_t rosterId)
{
    return "openyamm.discovery.npc/" + std::to_string(npcId) + "/roster/" + std::to_string(rosterId);
}

void recordWorldRosterOfferObservation(EventRuntimeState &state, uint32_t npcId, uint32_t rosterId)
{
    if (npcId == 0 || rosterId == 0) throw std::invalid_argument("Invalid observed roster invitation");
    state.namedGlobalVars[worldNpcRosterOfferKey(npcId, rosterId)] = 1;
    state.namedGlobalVars["openyamm.discovery.npc/" + std::to_string(npcId)] = 1;
}

bool worldRosterOfferKnown(const std::unordered_map<std::string, int32_t> &variables,
    uint32_t npcId, uint32_t rosterId)
{
    const auto value = variables.find(worldNpcRosterOfferKey(npcId, rosterId));
    if (value == variables.end() || value->second == 0) return false;
    if (value->second != 1) throw std::invalid_argument("Invalid saved roster invitation discovery");
    return true;
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
    if (houseId != 0) clearEncounterLocations(state, npcId);
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
    // One pass over saved variables; never scan every variable again for each indexed NPC or frame.
    for (const auto &[key, value] : variables)
    {
        const bool actor = key.starts_with(ActorPrefix);
        if ((!actor && !key.starts_with(ScriptPrefix)) || !key.ends_with("/seen") || value == 0) continue;
        const std::string &root = actor ? ActorPrefix : ScriptPrefix;
        const size_t separator = key.find('/', root.size());
        uint32_t npcId = 0;
        if (separator == std::string::npos) throw std::invalid_argument("Invalid saved NPC encounter identity");
        const std::string id = key.substr(root.size(), separator - root.size());
        const auto parsed = std::from_chars(id.data(), id.data() + id.size(), npcId);
        if (value != 1 || parsed.ec != std::errc() || parsed.ptr != id.data() + id.size()
            || npcId == 0 || id != std::to_string(npcId))
            throw std::invalid_argument("Invalid saved NPC encounter identity");
        const std::string mapId = key.substr(separator + 1, key.size() - separator - 6);
        if (result.actorLocations.contains(npcId) || result.scriptLocations.contains(npcId))
            throw std::invalid_argument("Duplicate saved NPC encounters");
        const std::string prefix = key.substr(0, key.size() - 4);
        if (actor) result.actorLocations.emplace(npcId, readActorLocation(variables, prefix, mapId));
        else result.scriptLocations.emplace(npcId, readScriptLocation(variables, prefix, mapId));
    }
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
