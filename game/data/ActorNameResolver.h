#pragma once

#include "game/maps/MapDeltaData.h"
#include "game/tables/MonsterTable.h"

#include <string>

namespace OpenYAMM::Game
{
int16_t resolveMapDeltaActorMonsterId(const MapDeltaActor &actor);
std::string resolveMapDeltaActorName(const MonsterTable &monsterTable, const MapDeltaActor &actor);
}
