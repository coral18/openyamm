#include "game/world/WorldMonsterKnowledge.h"
#include "game/gameplay/MapEncounterDefinition.h"
#include "game/gameplay/GameplayRuntimeInterfaces.h"
#include "game/events/EventRuntime.h"
#include "game/outdoor/OutdoorMapData.h"
#include "game/indoor/IndoorMapData.h"
#include "game/maps/MapIdentity.h"
#include "game/tables/MonsterTable.h"
#include "game/data/ActorNameResolver.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace OpenYAMM::Game
{
namespace
{
const std::string ObservationPrefix = "openyamm.discovery.monster/";

template<typename Spawn>
std::vector<WorldMapFeature> encounterFeatures(const MapStatsEntry &map, const std::vector<Spawn> &spawns,
                                              const MonsterTable &monsters)
{
    std::vector<WorldMapFeature> result;
    for (size_t i = 0; i < spawns.size(); ++i)
    {
        const Spawn &spawn = spawns[i];
        if (spawn.typeId != 3)
            continue;
        const EncounterSpawnDescriptor descriptor = resolveEncounterSpawnDescriptor(spawn.index);
        const MapEncounterInfo *pEncounter = mapEncounterInfo(map, descriptor.encounterSlot);
        if (pEncounter == nullptr || std::max(pEncounter->minCount, pEncounter->maxCount) <= 0)
            continue;
        WorldMapFeature feature;
        feature.mapId = map.canonicalId.empty() ? buildCanonicalMapId(map.worldId, map.fileName) : map.canonicalId;
        feature.site = {"encounter/" + std::to_string(i), 0, float(spawn.x), float(spawn.y), float(spawn.z)};
        feature.id = feature.mapId + "/" + feature.site.id;
        feature.kind = WorldMapFeatureKind::Monster;
        feature.monster.emplace();
        feature.monster->minCount = std::max(0, std::min(pEncounter->minCount, pEncounter->maxCount));
        feature.monster->maxCount = std::max(0, std::max(pEncounter->minCount, pEncounter->maxCount));
        feature.monster->radius = spawn.radius;
        for (char tier : {'A', 'B', 'C'})
        {
            if (descriptor.fixedTier != '\0' && tier != descriptor.fixedTier)
                continue;
            if (descriptor.fixedTier == '\0' && encounterTierWeights(pEncounter->difficulty)[tier - 'A'] == 0)
                continue;
            const auto *pStats = monsters.findStatsByPictureName(encounterPictureBase(*pEncounter) + " " + tier);
            if (pStats != nullptr && pStats->id > 0 && pStats->id <= INT16_MAX)
                feature.monster->definitions.push_back(int16_t(pStats->id));
        }
        if (feature.monster->definitions.empty())
            continue;
        const auto *pFirst = monsters.findStatsById(feature.monster->definitions.front());
        feature.title = pFirst->name + " - encounter area";
        result.push_back(std::move(feature));
    }
    return result;
}
}

void recordWorldMonsterObservation(EventRuntimeState &state, const WorldMonsterObservation &observation)
{
    if (observation.monsterId <= 0 || observation.gameMinutes < 0 || observation.uniqueNameIndex < 0)
        return;
    const std::string prefix = ObservationPrefix + std::to_string(observation.actorId) + "/";
    state.namedMapVars[prefix + "x"] = observation.x;
    state.namedMapVars[prefix + "y"] = observation.y;
    state.namedMapVars[prefix + "z"] = observation.z;
    state.namedMapVars[prefix + "dead"] = observation.dead ? 1 : 0;
    state.namedMapVars[prefix + "time"] = observation.gameMinutes;
    state.namedMapVars[prefix + "type"] = observation.monsterId;
    state.namedMapVars[prefix + "name"] = observation.uniqueNameIndex;
}

std::vector<WorldMonsterObservation> worldMonsterObservations(const EventRuntimeState &state)
{
    std::vector<WorldMonsterObservation> result;
    for (const auto &[key, value] : state.namedMapVars)
    {
        if (!key.starts_with(ObservationPrefix) || !key.ends_with("/type") || value <= 0 || value > INT16_MAX)
            continue;
        const std::string id = key.substr(ObservationPrefix.size(), key.size() - ObservationPrefix.size() - 5);
        WorldMonsterObservation observation;
        const auto parsed = std::from_chars(id.data(), id.data() + id.size(), observation.actorId);
        if (id.empty() || parsed.ec != std::errc() || parsed.ptr != id.data() + id.size())
            continue;
        if (id != std::to_string(observation.actorId))
            continue;
        const std::string prefix = ObservationPrefix + id + "/";
        if (!state.namedMapVars.contains(prefix + "x") || !state.namedMapVars.contains(prefix + "y")
            || !state.namedMapVars.contains(prefix + "z") || !state.namedMapVars.contains(prefix + "dead")
            || !state.namedMapVars.contains(prefix + "time"))
            continue;
        observation.monsterId = int16_t(value);
        observation.x = state.namedMapVars.at(prefix + "x");
        observation.y = state.namedMapVars.at(prefix + "y");
        observation.z = state.namedMapVars.at(prefix + "z");
        observation.dead = state.namedMapVars.at(prefix + "dead") != 0;
        observation.gameMinutes = state.namedMapVars.at(prefix + "time");
        const auto name = state.namedMapVars.find(prefix + "name");
        if (name != state.namedMapVars.end()) observation.uniqueNameIndex = name->second;
        if (observation.gameMinutes >= 0 && observation.uniqueNameIndex >= 0)
            result.push_back(observation);
    }
    std::sort(result.begin(), result.end(), [](const WorldMonsterObservation &a, const WorldMonsterObservation &b)
    { return a.actorId < b.actorId; });
    return result;
}

void recordWorldActorObservation(IGameplayWorldRuntime &world, size_t actorIndex)
{
    GameplayRuntimeActorState actor;
    EventRuntimeState *pState = world.eventRuntimeState();
    if (pState == nullptr || !world.actorRuntimeState(actorIndex, actor) || actor.isInvisible || actor.monsterId <= 0)
        return;
    const auto valid = [](float value)
    { return std::isfinite(value) && double(value) >= INT32_MIN && double(value) <= INT32_MAX; };
    if (!valid(actor.preciseX) || !valid(actor.preciseY) || !valid(actor.preciseZ) || !valid(world.gameMinutes())
        || world.gameMinutes() < 0)
        return;
    recordWorldMonsterObservation(*pState, {actor.actorId, actor.monsterId,
        int32_t(std::lround(actor.preciseX)), int32_t(std::lround(actor.preciseY)),
        int32_t(std::lround(actor.preciseZ)),
        actor.isDead, int32_t(world.gameMinutes()), actor.uniqueNameIndex});
}

std::vector<WorldMapFeature> buildWorldEncounterFeatures(const MapStatsEntry &map,
    const OutdoorMapData &geometry, const MonsterTable &monsters)
{
    return encounterFeatures(map, geometry.spawns, monsters);
}

std::vector<WorldMapFeature> buildWorldEncounterFeatures(const MapStatsEntry &map,
    const IndoorMapData &geometry, const MonsterTable &monsters)
{
    return encounterFeatures(map, geometry.spawns, monsters);
}

std::vector<WorldMapFeature> buildWorldPlacedMonsterFeatures(const MapStatsEntry &map,
    const MapDeltaData &authoredActors, const MonsterTable &monsters)
{
    std::vector<WorldMapFeature> result;
    const std::string mapId = map.canonicalId.empty()
        ? buildCanonicalMapId(map.worldId, map.fileName) : map.canonicalId;
    for (size_t i = 0; i < authoredActors.actors.size(); ++i)
    {
        const MapDeltaActor &actor = authoredActors.actors[i];
        const int16_t monsterId = resolveMapDeltaActorMonsterId(actor);
        const auto *pStats = monsters.findStatsById(monsterId);
        if (monsterId <= 0 || pStats == nullptr) continue;
        WorldMapFeature feature;
        feature.mapId = mapId;
        feature.site = {"placed/" + std::to_string(i), 0, float(actor.x), float(actor.y), float(actor.z)};
        feature.id = mapId + "/" + feature.site.id;
        feature.kind = WorldMapFeatureKind::Monster;
        const std::string name = resolveMapDeltaActorName(monsters, actor);
        feature.title = (name.empty() ? pStats->name : name) + " - authored placement";
        feature.monster = WorldMonsterFeature{{monsterId}, 1, 1};
        feature.monster->placed = true;
        result.push_back(std::move(feature));
    }
    return result;
}

std::vector<WorldMapFeature> buildWorldObservedMonsterFeatures(const std::string &mapId,
    const std::vector<WorldMonsterObservation> &observations, const MonsterTable &monsters)
{
    std::vector<WorldMapFeature> result;
    for (const WorldMonsterObservation &observation : observations)
    {
        const auto *pStats = monsters.findStatsById(observation.monsterId);
        if (pStats == nullptr)
            continue;
        WorldMapFeature feature;
        feature.mapId = mapId;
        feature.site = {"observed/" + std::to_string(observation.actorId), 0,
            float(observation.x), float(observation.y), float(observation.z)};
        feature.id = mapId + "/" + feature.site.id;
        feature.kind = WorldMapFeatureKind::Monster;
        const std::optional<std::string> name = observation.uniqueNameIndex > 0
            ? monsters.getUniqueName(observation.uniqueNameIndex) : std::nullopt;
        feature.title = (name && !name->empty() ? *name : pStats->name) + " - last observed";
        feature.monster = WorldMonsterFeature{{observation.monsterId}, 1, 1, 0, true,
            observation.dead, observation.gameMinutes};
        result.push_back(std::move(feature));
    }
    return result;
}
}
