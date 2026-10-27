#include "game/gameplay/MapEncounterDefinition.h"
#include <algorithm>

namespace OpenYAMM::Game
{
char summonMonsterTier(uint32_t level)
{
    return char('A' + std::clamp(level, 1u, 3u) - 1u);
}

EncounterSpawnDescriptor resolveEncounterSpawnDescriptor(uint16_t index)
{
    if (index >= 1 && index <= 3)
        return {int(index), '\0'};
    if (index >= 4 && index <= 12)
        return {int((index - 4) % 3) + 1, char('A' + (index - 4) / 3)};
    return {};
}

const MapEncounterInfo *mapEncounterInfo(const MapStatsEntry &map, int slot)
{
    switch (slot)
    {
    case 1: return &map.encounter1;
    case 2: return &map.encounter2;
    case 3: return &map.encounter3;
    default: return nullptr;
    }
}

std::string encounterPictureBase(const MapEncounterInfo &encounter)
{
    return encounter.pictureName.empty() ? encounter.monsterName : encounter.pictureName;
}

const std::array<int, 3> &encounterTierWeights(int difficulty)
{
    static constexpr std::array<std::array<int, 3>, 6> Weights = {{
        {{100, 0, 0}}, {{90, 8, 2}}, {{70, 20, 10}}, {{50, 30, 20}}, {{30, 40, 30}}, {{10, 50, 40}}
    }};
    return Weights[std::clamp(difficulty, 0, 5)];
}
}
