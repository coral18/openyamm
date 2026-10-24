#pragma once

#include <cstdint>
#include <string>

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
    bool consumesItem = false;
    bool addsFollower = false;
    std::string openedMapFlag;
    std::string targetMap;
};
}
