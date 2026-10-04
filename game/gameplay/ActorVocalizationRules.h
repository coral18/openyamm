#pragma once

#include <cmath>
#include <cstdint>
#include <numbers>

namespace OpenYAMM::Game
{
inline uint32_t actorVocalizationRoll(uint32_t actorId, uint32_t decisionCount, uint32_t salt)
{
    uint32_t value = actorId * 0x9e3779b9u + decisionCount * 0x85ebca6bu + salt;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return (value ^ (value >> 16)) % 100u;
}

inline bool actorChoosesBored(uint32_t actorId, uint32_t decisionCount)
{
    return actorVocalizationRoll(actorId, decisionCount, 0x7134ab21u) < 50u;
}

inline bool actorFidgetFacesListener(float requestedYaw, float deltaListenerX, float deltaListenerY)
{
    constexpr float pi = std::numbers::pi_v<float>;
    const float difference = std::remainder(requestedYaw - std::atan2(deltaListenerY, deltaListenerX), 2.0f * pi);
    return difference >= -pi / 8.0f && difference < pi / 8.0f;
}

inline bool actorBoredSoundRoll(uint32_t actorId, uint32_t decisionCount)
{
    return actorVocalizationRoll(actorId, decisionCount, 0x04b1d0f5u) < 5u;
}

inline bool actorWanderSoundRoll(uint32_t actorId, uint32_t decisionCount)
{
    return actorVocalizationRoll(actorId, decisionCount, 0xb0839de2u) < 2u;
}
}
