#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
// Declared interaction possibilities belonging to an active callback, never current party knowledge.
struct SiteInteractionReference
{
    std::string kind;
    std::string network;
    uint32_t itemId = 0;
    uint32_t spellId = 0;
    uint32_t npcId = 0;
    uint32_t spriteId = 0;
    uint32_t completionQBit = 0;
    uint32_t requiredQBit = 0;
    uint32_t objectiveGroupQBit = 0;
    std::vector<uint32_t> otherObjectiveQBits;
    bool consumesItem = false;
    bool addsFollower = false;
    // Empty means the legacy party-wide item requirement.
    std::string itemScope;
    uint32_t openedQBit = 0;
    std::string openedMapFlag;
    std::string targetMap;
};
}
