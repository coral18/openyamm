#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace OpenYAMM::Game
{
inline float actorVoicePitch(float shrinkRemainingSeconds, float shrinkDamageMultiplier)
{
    if (shrinkRemainingSeconds <= 0.0f || shrinkDamageMultiplier <= 0.0f)
    {
        return 1.0f;
    }

    // Native actor WAVs are 22050 Hz. Preserve these ratios after mixer resampling.
    const int power = int(std::lround(1.0f / shrinkDamageMultiplier));
    switch (power)
    {
    case 2: return 1.5f;
    case 3: return 2.0f;
    case 4: return 2.5f;
    default: return 1.0f;
    }
}

inline int actorVoiceVolume(float deltaX, float deltaY, float deltaZ)
{
    // MM8's ordinary sample mixer uses integer feet-to-feet distance, not Euclidean distance.
    std::array<float, 3> components = {std::abs(deltaX), std::abs(deltaY), std::abs(deltaZ)};
    std::sort(components.begin(), components.end());
    if (components[2] >= 8193.0f)
    {
        return 0;
    }

    const int distance = int(components[2]) + 11 * int(components[1]) / 32 + int(components[0]) / 4;
    return distance > 8192 ? 0 : 114 - 100 * distance / 8192;
}
}
