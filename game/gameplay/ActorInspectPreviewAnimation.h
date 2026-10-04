#pragma once

#include "game/gameplay/GameplayActorAiTypes.h"
#include "game/tables/MonsterTable.h"
#include "game/tables/SpriteTables.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace OpenYAMM::Game
{
// A silent front-view demonstration, separate from the actor's world animation.
struct ActorInspectPreviewAnimation
{
    int16_t monsterId = 0;
    ActorAiAnimationState animation = ActorAiAnimationState::Bored;
    uint32_t actionTimeTicks = 0;
    uint32_t actionLengthTicks = 0;
    uint32_t displayTimeTicks = 0;
    uint32_t lastUpdateTicks = 0;
    uint32_t randomState = 0x6d2b79f5u;
    bool movingAtFirstInspect = false;

    uint32_t nextRandom()
    {
        randomState = randomState * 1664525u + 1013904223u;
        return randomState >> 16;
    }

    void advance(int16_t typeId, bool moving, const MonsterEntry &entry,
        const std::array<uint16_t, 8> &spriteIndices, uint16_t standingSpriteIndex,
        const SpriteFrameTable &frames, uint32_t visibleTicks)
    {
        const uint32_t deltaTicks = monsterId == typeId
            ? std::min(visibleTicks - lastUpdateTicks, 32u) : 0u;
        lastUpdateTicks = visibleTicks;
        if (monsterId != typeId)
        {
            monsterId = typeId;
            movingAtFirstInspect = moving;
            animation = ActorAiAnimationState::Bored;
            actionTimeTicks = 0;
            actionLengthTicks = 128u + nextRandom() % 256u;
        }
        else if (actionTimeTicks > actionLengthTicks)
        {
            actionTimeTicks = 0;
            if (animation == ActorAiAnimationState::Bored || animation == ActorAiAnimationState::AttackMelee)
            {
                animation = ActorAiAnimationState::Standing;
                actionLengthTicks = 128u + nextRandom() % 128u;
            }
            else
            {
                const bool fidget = (entry.inspectFidgetWhenMoving && movingAtFirstInspect)
                    || nextRandom() % 100u >= static_cast<uint32_t>(entry.inspectAttackChance);
                animation = fidget ? ActorAiAnimationState::Bored : ActorAiAnimationState::AttackMelee;
                uint16_t spriteIndex = spriteIndices[static_cast<size_t>(animation)];
                if (spriteIndex == 0)
                {
                    spriteIndex = standingSpriteIndex;
                }
                const SpriteFrameEntry *pFrame = frames.getFrame(spriteIndex, 0);
                actionLengthTicks = pFrame != nullptr ? std::max(0, pFrame->animationLengthTicks) : 0;
            }
        }
        displayTimeTicks = actionTimeTicks;
        actionTimeTicks += deltaTicks;
    }

    uint32_t frameTimeTicks(const SpriteFrameEntry &firstFrame) const
    {
        // Original SFT lookup uses whole eight-tick units and retains the preceding frame at a boundary.
        if (firstFrame.animationLengthTicks <= 0)
        {
            return 0;
        }
        const uint32_t phase = (displayTimeTicks % firstFrame.animationLengthTicks) / 8u * 8u;
        return phase == 0 ? 0 : phase - 1u;
    }
};
}
