#pragma once

#include "game/events/EventRuntime.h"

#include <cstdint>
#include <string>
#include <span>
#include <vector>

namespace OpenYAMM::Game
{
class MergedNpcProfessionTable;
class NpcDialogTable;
class Party;

struct HiredNpcFollowerView
{
    uint32_t npcId = 0;
    uint32_t professionId = 0;
    uint32_t weeklyCost = 0;
    uint32_t feePercent = 0;
    uint32_t portraitPictureId = 0;
    std::string name;
    std::string profession;
};

std::vector<HiredNpcFollowerView> buildHiredNpcFollowerViews(
    const EventRuntimeState &eventRuntimeState,
    const NpcDialogTable &npcDialogTable,
    const MergedNpcProfessionTable &npcProfessionTable
);

std::vector<HiredNpcFollowerView> buildHiredNpcFollowerViews(
    const EventRuntimeState &eventRuntimeState,
    const Party *pParty,
    const NpcDialogTable &npcDialogTable,
    const MergedNpcProfessionTable &npcProfessionTable
);

uint32_t totalHiredNpcFollowerFeePercent(const EventRuntimeState &eventRuntimeState);
uint32_t hiredNpcFollowerGoldShare(uint32_t goldAmount, const EventRuntimeState &eventRuntimeState);
bool hiredNpcHasProfession(std::span<const HiredNpcFollower> followers, uint32_t professionId);
int hiredNpcTransportDayReduction(const EventRuntimeState &eventRuntimeState, bool stable);
int hiredNpcCrossMapDayReduction(const EventRuntimeState &eventRuntimeState);
int hiredNpcWalkingTravelDays(int baseDays, const EventRuntimeState &eventRuntimeState);
int hiredNpcRestFoodReduction(const EventRuntimeState &eventRuntimeState);
int hiredNpcCampingFoodCost(int baseCost, const EventRuntimeState &eventRuntimeState);
int hiredNpcSkillBonus(std::span<const HiredNpcFollower> followers, const std::string &skillName);
int hiredNpcPrimaryStatBonus(std::span<const HiredNpcFollower> followers, const std::string &statName);
int hiredNpcResistanceBonus(std::span<const HiredNpcFollower> followers, const std::string &resistanceName);
uint32_t hiredNpcGoldFindBonusPercent(const EventRuntimeState &eventRuntimeState);
uint32_t hiredNpcGoldAfterBonusAndFees(uint32_t goldAmount, const EventRuntimeState &eventRuntimeState);
bool hiredNpcCanRepairItemKind(std::span<const HiredNpcFollower> followers, const std::string &equipStat);
}
