#pragma once

#include <bx/math.h>
#include <cstddef>
#include <cstdint>

namespace OpenYAMM::Game
{
class GameplayScreenRuntime;

class EnemyHealthBarRenderer
{
public:
    static void preload(const GameplayScreenRuntime &context);
    static void render(const GameplayScreenRuntime &context, size_t actorIndex,
        const bx::Vec3 &actorPosition, const bx::Vec3 &anchor,
        float distanceSquared, bool focused, bool attackingParty, uint16_t viewId,
        const bx::Vec3 &cameraPosition, const float *pViewMatrix, const float *pProjectionMatrix, int height);
};
}
