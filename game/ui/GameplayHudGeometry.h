#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace OpenYAMM::Game
{
struct HudSpritePreviewQuad
{
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
    float u0 = 0;
    float v0 = 0;
    float u1 = 1;
    float v1 = 1;
};

// Sprite dimensions and crop coordinates are native logical pixels, independent of texture resolution.
inline HudSpritePreviewQuad actorInspectSpriteQuad(float x, float y, float size, float scale,
    float width, float height, float offsetX, float offsetY, float canvasHeight, int yOffset)
{
    if (size <= 0 || scale <= 0 || width <= 0 || height <= 0)
    {
        return {};
    }
    const float spriteX = x + size * 0.5f + (offsetX - width * 0.5f) * scale;
    const float spriteY = y + (canvasHeight - offsetY - height + yOffset) * scale;
    const float left = std::max(x, spriteX);
    const float top = std::max(y, spriteY);
    const float right = std::min(x + size, spriteX + width * scale);
    const float bottom = std::min(y + size, spriteY + height * scale);
    if (left >= right || top >= bottom)
    {
        return {};
    }
    return {left, top, right - left, bottom - top,
        (left - spriteX) / (width * scale), (top - spriteY) / (height * scale),
        (right - spriteX) / (width * scale), (bottom - spriteY) / (height * scale)};
}

constexpr int HudArcTextureHeight = 32;
constexpr float HudArcEdgeTexels = 2.0f;

struct HudArcRange
{
    float begin;
    float end;
    uint16_t segments;
};

inline HudArcRange hudArcRange(float sweepDegrees, float begin, float end)
{
    if (!std::isfinite(sweepDegrees) || !std::isfinite(begin) || !std::isfinite(end))
    {
        return {};
    }
    begin = std::clamp(begin, 0.0f, 1.0f);
    end = std::clamp(end, begin, 1.0f);
    const float degrees = std::min(360.0f, std::abs(sweepDegrees)) * (end - begin);
    return {begin, end, static_cast<uint16_t>(std::ceil(degrees * 3.0f / 10.0f))};
}

struct HudArcCrossSection
{
    float innerX;
    float innerY;
    float outerX;
    float outerY;
};

struct HudArcEdgeProfile
{
    float outerWidth;
    float coreWidth;
    float coreOpacity;
};

// A one-screen-pixel transition centred on each nominal edge. Thin strokes retain their coverage.
inline HudArcEdgeProfile hudArcEdgeProfile(float strokeWidth)
{
    const float width = std::max(0.0f, strokeWidth);
    return {std::max(1.0f, width) + 1.0f, std::max(0.0f, width - 1.0f), std::min(1.0f, width)};
}

// Offset along the ellipse normal, preserving stroke width even on a tall portrait.
inline HudArcCrossSection hudArcCrossSection(float radiusX, float radiusY, float angleDegrees, float strokeWidth)
{
    const float angle = angleDegrees * 0.01745329252f;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    const float nx = cosine / radiusX;
    const float ny = sine / radiusY;
    const float halfWidth = strokeWidth * 0.5f / std::sqrt(nx * nx + ny * ny);
    const float x = radiusX * cosine;
    const float y = radiusY * sine;
    return {x - nx * halfWidth, y - ny * halfWidth, x + nx * halfWidth, y + ny * halfWidth};
}

// Clip a segment in screen coordinates; crossing segments remain visible even when both endpoints are outside.
inline bool clipHudLineToCircle(float centerX, float centerY, float radius,
    float &x0, float &y0, float &x1, float &y1)
{
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float px = x0 - centerX;
    const float py = y0 - centerY;
    const float a = dx * dx + dy * dy;
    const float c = px * px + py * py - radius * radius;
    if (a < 0.00001f)
    {
        return c <= 0;
    }
    const float b = 2 * (px * dx + py * dy);
    const float discriminant = b * b - 4 * a * c;
    if (discriminant < 0)
    {
        return false;
    }
    const float root = std::sqrt(discriminant);
    const float begin = std::max(0.0f, (-b - root) / (2 * a));
    const float end = std::min(1.0f, (-b + root) / (2 * a));
    if (begin >= end)
    {
        return false;
    }
    x1 = x0 + end * dx;
    y1 = y0 + end * dy;
    x0 += begin * dx;
    y0 += begin * dy;
    return true;
}
}
