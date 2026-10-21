#pragma once

#include "game/world/WorldMapFeatures.h"

#include <span>
#include <unordered_map>

namespace OpenYAMM::Game
{
class IGameplayWorldRuntime;
class MonsterTable;
struct MapStatsEntry;
struct MapDeltaData;

struct WorldMonsterObservation
{
    uint32_t actorId = 0;
    int16_t monsterId = 0;
    int32_t x = 0, y = 0, z = 0;
    bool dead = false;
    int32_t gameMinutes = 0;
    int32_t uniqueNameIndex = 0;
};

// Transient work budget for actual main-view candidates; it never owns saved knowledge.
class WorldMonsterSightTracker
{
public:
    void observeRenderedActors(IGameplayWorldRuntime &world, std::span<const size_t> candidates,
        uint64_t realMilliseconds);
    bool shouldCheckSight(const EventRuntimeState &state, const WorldMonsterObservation &candidate,
        uint64_t realMilliseconds);

private:
    std::string m_map;
    uint64_t m_windowStart = 0;
    uint32_t m_checks = 0;
    size_t m_nextCandidate = 0;
    std::unordered_map<uint32_t, uint64_t> m_lastChecks;
};

// Call only after main-view art/frustum/sector culling, never for previews, reflections or population scans.
void recordWorldVisibleActorObservation(WorldMonsterSightTracker &tracker, IGameplayWorldRuntime &world,
    size_t actorIndex, uint64_t realMilliseconds);
void recordWorldMonsterObservation(EventRuntimeState &state, const WorldMonsterObservation &observation);
std::vector<WorldMonsterObservation> worldMonsterObservations(const EventRuntimeState &state);
// Call only for an actor actually picked/targeted by the player, never for an omniscient actor scan.
void recordWorldActorObservation(IGameplayWorldRuntime &world, size_t actorIndex);
std::vector<WorldMapFeature> buildWorldEncounterFeatures(const MapStatsEntry &map,
    const OutdoorMapData &geometry, const MonsterTable &monsters);
std::vector<WorldMapFeature> buildWorldEncounterFeatures(const MapStatsEntry &map,
    const IndoorMapData &geometry, const MonsterTable &monsters);
// Read only fresh package companion data, never a live or saved population.
std::vector<WorldMapFeature> buildWorldPlacedMonsterFeatures(const MapStatsEntry &map,
    const MapDeltaData &authoredActors, const MonsterTable &monsters);
std::vector<WorldMapFeature> buildWorldObservedMonsterFeatures(const std::string &mapId,
    const std::vector<WorldMonsterObservation> &observations, const MonsterTable &monsters);
}
