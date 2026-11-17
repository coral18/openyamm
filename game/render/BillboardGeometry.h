#pragma once

#include "game/render/BillboardOpacityMask.h"

#include <bx/math.h>

#include <optional>

namespace OpenYAMM::Game
{
struct BillboardQuad
{
    bx::Vec3 center = {0.0f, 0.0f, 0.0f};
    bx::Vec3 right = {0.0f, 0.0f, 0.0f};
    bx::Vec3 up = {0.0f, 0.0f, 0.0f};
};

inline BillboardQuad billboardQuad(
    const bx::Vec3 &center, const bx::Vec3 &cameraRight, const bx::Vec3 &cameraUp,
    float worldWidth, float worldHeight)
{
    return {center, bx::mul(cameraRight, worldWidth * 0.5f), bx::mul(cameraUp, worldHeight * 0.5f)};
}

inline bx::Vec3 bottomAnchoredBillboardCenter(
    float x,
    float y,
    float z,
    const bx::Vec3 &cameraUp,
    float worldHeight)
{
    const float halfHeight = worldHeight * 0.5f;

    return {
        x + cameraUp.x * halfHeight,
        y + cameraUp.y * halfHeight,
        z + cameraUp.z * halfHeight,
    };
}

inline bx::Vec3 billboardTexturePoint(const BillboardQuad &quad, bool mirrored,
    const std::array<float, 2> &texturePoint)
{
    const float displayU = mirrored ? 1.0f - texturePoint[0] : texturePoint[0];
    const float horizontal = displayU * 2.0f - 1.0f;
    const float vertical = 1.0f - texturePoint[1] * 2.0f;
    return {quad.center.x + quad.right.x * horizontal + quad.up.x * vertical,
        quad.center.y + quad.right.y * horizontal + quad.up.y * vertical,
        quad.center.z + quad.right.z * horizontal + quad.up.z * vertical};
}

inline std::optional<bx::Vec3> nearestOpaqueBillboardPoint(
    const BillboardQuad &quad,
    const BillboardOpacityMask &opacityMask,
    bool mirrored,
    float displayU,
    float displayV)
{
    const float textureU = mirrored ? 1.0f - displayU : displayU;
    const std::optional<std::array<float, 2>> nearestTexturePoint =
        opacityMask.nearestOpaqueNormalized(textureU, displayV);

    if (!nearestTexturePoint)
    {
        return std::nullopt;
    }

    return billboardTexturePoint(quad, mirrored, *nearestTexturePoint);
}
}
