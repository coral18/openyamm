#pragma once

#include "game/maps/MapDeltaData.h"
#include "game/tables/MonsterTable.h"

#include <string>

namespace OpenYAMM::Game
{
int16_t resolveMapDeltaActorMonsterId(const MapDeltaActor &actor);
std::string resolveMapDeltaActorName(const MonsterTable &monsterTable, const MapDeltaActor &actor);
std::string resolveSpawnedMapActorName(const MonsterTable &monsterTable,
    const MonsterTable::MonsterStatsEntry &stats, uint32_t uniqueNameId);
}
