#pragma once

#include "game/tables/MapStats.h"
#include <array>

namespace OpenYAMM::Game
{
struct EncounterSpawnDescriptor
{
    int encounterSlot = 0;
    char fixedTier = '\0';
};

EncounterSpawnDescriptor resolveEncounterSpawnDescriptor(uint16_t index);
const MapEncounterInfo *mapEncounterInfo(const MapStatsEntry &map, int slot);
std::string encounterPictureBase(const MapEncounterInfo &encounter);
const std::array<int, 3> &encounterTierWeights(int difficulty);
}
