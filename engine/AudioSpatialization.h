#pragma once

#include <algorithm>
#include <cmath>

namespace OpenYAMM::Engine
{
inline float positionalAudioPan(float deltaX, float deltaY, float listenerYawRadians)
{
    const float horizontalDistance = std::hypot(deltaX, deltaY);
    if (horizontalDistance <= 0.0001f)
    {
        return 0.0f;
    }

    // World forward is +X at yaw zero; the listener's right is -Y.
    return std::clamp(
        (deltaX * std::sin(listenerYawRadians) - deltaY * std::cos(listenerYawRadians)) / horizontalDistance,
        -1.0f,
        1.0f);
}
}
