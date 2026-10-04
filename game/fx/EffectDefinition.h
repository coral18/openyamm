#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace OpenYAMM::Game
{
struct EffectVectorKey
{
    float time = 0.0f;
    std::array<float, 3> value = {};
};

struct EffectScalarKey
{
    float time = 0.0f;
    float value = 0.0f;
};

struct EffectColorKey
{
    float time = 0.0f;
    std::array<uint8_t, 3> rgb = {255, 255, 255};
    uint8_t transparency = 0;
};

enum class EffectEmissionShape
{
    Point,
    Plane,
    Sphere,
    PlaneIn,
    PlaneOut,
    Cone,
    Circle
};

enum class EffectBlendMode
{
    Alpha,
    Additive,
    Multiply
};

enum class EffectParticleSpace
{
    World,
    Local
};

enum class EffectSpriteAlignment
{
    CameraFacing,
    WorldPlane
};

struct EffectComponentLink
{
    std::optional<uint32_t> componentId;
    std::string nodeName;
};

struct EffectSpriteEmitterDefinition
{
    uint32_t componentId = 0;
    float startSeconds = 0.0f;
    float endSeconds = 0.0f;
    uint32_t repeatCount = 1;
    std::string spriteResource;
    uint32_t spritesPerEmission = 1;
    float emissionIntervalSeconds = 0.0f;
    float particleLifetimeSeconds = 1.0f;
    EffectEmissionShape emissionShape = EffectEmissionShape::Point;
    float radius = 0.0f;
    std::array<float, 3> planeDirection = {0.0f, 1.0f, 0.0f};
    float velocity = 0.0f;
    std::array<float, 3> windDirection = {};
    float windAmount = 0.0f;
    std::array<float, 3> offset = {};
    std::array<float, 3> rotationPerTick = {};
    float stretchU = 1.0f;
    float stretchV = 1.0f;
    EffectBlendMode blendMode = EffectBlendMode::Alpha;
    EffectParticleSpace particleSpace = EffectParticleSpace::World;
    EffectSpriteAlignment alignment = EffectSpriteAlignment::CameraFacing;
    std::array<float, 3> planeRight = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> planeUp = {0.0f, 0.0f, 1.0f};
    EffectComponentLink link;
    std::vector<EffectColorKey> colorKeys;
    std::vector<EffectScalarKey> scaleKeys;
    std::vector<EffectVectorKey> motionKeys;
};

struct EffectModelDefinition
{
    uint32_t componentId = 0;
    float startSeconds = 0.0f;
    float endSeconds = 0.0f;
    uint32_t repeatCount = 1;
    std::string modelResource;
    std::string animationClip;
    std::array<float, 3> offset = {};
    bool visible = false;
    EffectComponentLink link;
};

struct EffectNullDefinition
{
    uint32_t componentId = 0;
    float startSeconds = 0.0f;
    float endSeconds = 0.0f;
    uint32_t repeatCount = 1;
    std::array<float, 3> offset = {};
    std::array<float, 3> rotationPerTick = {};
    EffectComponentLink link;
    std::vector<EffectScalarKey> scaleKeys;
    std::vector<EffectVectorKey> motionKeys;
};

struct EffectSpriteDefinition
{
    uint32_t componentId = 0;
    float startSeconds = 0.0f;
    float endSeconds = 0.0f;
    uint32_t repeatCount = 1;
    std::string spriteResource;
    std::array<float, 3> offset = {};
    float stretchU = 1.0f;
    float stretchV = 1.0f;
    EffectBlendMode blendMode = EffectBlendMode::Additive;
    EffectSpriteAlignment alignment = EffectSpriteAlignment::CameraFacing;
    std::array<float, 3> planeRight = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> planeUp = {0.0f, 0.0f, 1.0f};
    EffectComponentLink link;
    std::vector<EffectColorKey> colorKeys;
    std::vector<EffectScalarKey> scaleKeys;
    std::vector<EffectVectorKey> motionKeys;
};

struct EffectSoundDefinition
{
    uint32_t componentId = 0;
    float startSeconds = 0.0f;
    float endSeconds = 0.0f;
    std::string soundResource;
    bool spatial = true;
    bool loop = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    float innerRadius = 100.0f;
    float outerRadius = 500.0f;
    bool followsEmitter = false;
};

enum class EffectTimingProfile
{
    Predictable,
    Mm9Native
};

struct EffectDefinition
{
    std::string id;
    float durationSeconds = 0.0f;
    EffectTimingProfile timingProfile = EffectTimingProfile::Predictable;
    std::vector<EffectModelDefinition> models;
    std::vector<EffectNullDefinition> nulls;
    std::vector<EffectSpriteDefinition> sprites;
    std::vector<EffectSpriteEmitterDefinition> spriteEmitters;
    std::vector<EffectSoundDefinition> sounds;
};
}
