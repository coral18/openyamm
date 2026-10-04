#pragma once

#include "game/ui/RestHourglassAtlas.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace OpenYAMM::Game
{
constexpr float RestHourglassCycleSeconds =
    2.0f * (RestHourglassAtlas::drainSeconds + RestHourglassAtlas::turnSeconds);

struct RestHourglassFrame
{
    int sandFrame = 0;
    float sandRotationRadians = 0;
    float frameRotationRadians = 0;
};

inline RestHourglassFrame restHourglassFrame(float elapsedSeconds)
{
    namespace Atlas = RestHourglassAtlas;
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0)
    {
        elapsedSeconds = 0;
    }
    const float seconds = std::fmod(elapsedSeconds, RestHourglassCycleSeconds);
    const float halfCycle = Atlas::drainSeconds + Atlas::turnSeconds;
    const int orientation = seconds >= halfCycle ? 1 : 0;
    const float phase = seconds - float(orientation) * halfCycle;
    const float transferred = std::min(1.0f, phase / Atlas::drainSeconds);
    const int sandFrame = int(std::lround(transferred * float(Atlas::framesPerOrientation - 1)));
    const float turn = std::clamp((phase - Atlas::drainSeconds) / Atlas::turnSeconds, 0.0f, 1.0f);
    const float rotation = std::numbers::pi_v<float> * turn * turn * (3.0f - 2.0f * turn);
    return {orientation * Atlas::framesPerOrientation + sandFrame, rotation,
        float(orientation) * std::numbers::pi_v<float> + rotation};
}
}
