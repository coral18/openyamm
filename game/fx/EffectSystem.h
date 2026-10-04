#pragma once

#include "engine/models/ModelInstance.h"
#include "game/fx/EffectDefinition.h"

#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Game
{
struct EffectHandle
{
    uint32_t index = std::numeric_limits<uint32_t>::max();
    uint32_t generation = 0;

    bool operator==(const EffectHandle &) const = default;
};

struct EffectSpawnParams
{
    std::array<float, 3> position = {};
    std::array<float, 4> rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    float scale = 1.0f;
    uint32_t seed = 9;
};

enum class EffectStopMode
{
    Immediate,
    Drain
};

struct EffectParticle
{
    EffectHandle owner;
    uint32_t componentId = 0;
    std::string spriteResource;
    std::array<float, 3> position = {};
    std::array<float, 3> velocity = {};
    std::array<float, 3> localPosition = {};
    std::array<float, 3> localVelocity = {};
    std::array<float, 4> color = {1.0f, 1.0f, 1.0f, 1.0f};
    float scale = 1.0f;
    float stretchU = 1.0f;
    float stretchV = 1.0f;
    float ageSeconds = 0.0f;
    float spriteAnimationSeconds = 0.0f;
    float lifetimeSeconds = 1.0f;
    EffectBlendMode blendMode = EffectBlendMode::Alpha;
    EffectParticleSpace particleSpace = EffectParticleSpace::World;
    EffectSpriteAlignment alignment = EffectSpriteAlignment::CameraFacing;
    std::array<float, 3> planeRight = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> planeUp = {0.0f, 0.0f, 1.0f};
};

struct EffectFixedSprite
{
    EffectHandle owner;
    uint32_t componentId = 0;
    std::string spriteResource;
    std::array<float, 3> position = {};
    std::array<float, 4> color = {1.0f, 1.0f, 1.0f, 1.0f};
    float scale = 1.0f;
    float stretchU = 1.0f;
    float stretchV = 1.0f;
    float spriteAnimationSeconds = 0.0f;
    EffectBlendMode blendMode = EffectBlendMode::Additive;
    EffectSpriteAlignment alignment = EffectSpriteAlignment::CameraFacing;
    std::array<float, 3> planeRight = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> planeUp = {0.0f, 0.0f, 1.0f};
};

enum class EffectSoundEventKind
{
    Start,
    Stop,
    Move
};

struct EffectSoundEvent
{
    EffectSoundEventKind kind = EffectSoundEventKind::Start;
    EffectHandle owner;
    uint32_t componentId = 0;
    std::string soundResource;
    std::array<float, 3> position = {};
    bool spatial = true;
    bool loop = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    float innerRadius = 100.0f;
    float outerRadius = 500.0f;
};

class EffectLibrary
{
public:
    bool add(std::shared_ptr<const EffectDefinition> definition, std::string &error);
    bool replace(
        const std::vector<std::shared_ptr<const EffectDefinition>> &definitions,
        std::string &error);
    const EffectDefinition *find(const std::string &id) const;
    std::vector<std::string> ids() const;
    void clear();
    size_t size() const;

private:
    friend class EffectSystem;
    std::unordered_map<std::string, std::shared_ptr<const EffectDefinition>> m_definitions;
};

class EffectSystem
{
public:
    explicit EffectSystem(const EffectLibrary *pLibrary = nullptr);

    void setLibrary(const EffectLibrary *pLibrary);
    void setModelRuntime(
        Engine::ModelInstanceSystem *pInstances,
        std::function<std::shared_ptr<const Engine::ModelAsset>(const std::string &)> resolver);
    EffectHandle spawn(const std::string &id, const EffectSpawnParams &params = {});
    bool setTransform(
        EffectHandle handle,
        const std::array<float, 3> &position,
        const std::array<float, 4> &rotation,
        float scale);
    bool stop(EffectHandle handle, EffectStopMode mode);
    bool pause(EffectHandle handle, bool paused);
    bool restart(EffectHandle handle);
    bool seek(EffectHandle handle, float timeSeconds);
    bool setReferenceUpdateRate(uint32_t updatesPerSecond);
    uint32_t referenceUpdateRate() const;
    void update(float deltaSeconds, bool worldPaused = false);
    void clear();

    bool contains(EffectHandle handle) const;
    float elapsedSeconds(EffectHandle handle) const;
    size_t size() const;
    size_t estimatedTransientBytes() const;
    const std::vector<EffectParticle> &particles() const;
    const std::vector<EffectFixedSprite> &fixedSprites() const;
    // Includes delayed components, before their first particle or animation frame exists.
    std::vector<std::string> activeSpriteResources() const;
    std::vector<EffectSoundEvent> consumeSoundEvents();

private:
    struct Random
    {
        uint32_t state = 9;

        uint32_t next();
        float signedValue();
    };

    struct EmissionSample
    {
        std::array<float, 3> offset = {};
        std::array<float, 3> velocity = {};
    };

    struct EmitterState
    {
        bool started = false;
        bool finished = false;
        float lastEmissionSeconds = -1000000000.0f;
        std::array<float, 3> previousPosition = {};
        std::array<float, 3> rotation = {};
        std::vector<EmissionSample> cachedSamples;
    };

    struct SoundState
    {
        bool started = false;
        bool stopped = false;
    };

    struct ModelState
    {
        Engine::ModelInstanceHandle handle;
    };

    struct Slot
    {
        uint32_t generation = 1;
        bool active = false;
        bool paused = false;
        bool draining = false;
        float elapsedSeconds = 0.0f;
        EffectSpawnParams transform;
        std::shared_ptr<const EffectDefinition> definition;
        Random random;
        std::vector<EmitterState> emitters;
        std::vector<SoundState> sounds;
        std::vector<ModelState> models;
    };

    Slot *find(EffectHandle handle);
    const Slot *find(EffectHandle handle) const;
    void tick(float stepSeconds);
    void tickInstance(uint32_t slotIndex, Slot &slot, float stepSeconds);
    void startEmitter(Slot &slot, size_t emitterIndex);
    EmissionSample createEmissionSample(Slot &slot, const EffectSpriteEmitterDefinition &definition);
    void emitParticles(uint32_t slotIndex, Slot &slot, size_t emitterIndex);
    void updateParticles(float stepSeconds, const EffectHandle *pOwnerFilter = nullptr);
    void updateSounds(uint32_t slotIndex, Slot &slot);
    bool createModels(Slot &slot);
    void updateModels(Slot &slot);
    void updateFixedSprites(uint32_t slotIndex, Slot &slot);
    std::optional<std::array<float, 3>> linkedPosition(
        const Slot &slot,
        const EffectComponentLink &link) const;
    void finishSlot(uint32_t slotIndex, Slot &slot, bool immediate);

    const EffectLibrary *m_pLibrary = nullptr;
    uint32_t m_referenceUpdatesPerSecond = 60;
    float m_updateAccumulatorSeconds = 0.0f;
    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_freeIndices;
    size_t m_activeCount = 0;
    std::vector<EffectParticle> m_particles;
    std::vector<EffectFixedSprite> m_fixedSprites;
    std::vector<EffectSoundEvent> m_soundEvents;
    Engine::ModelInstanceSystem *m_pModelInstances = nullptr;
    std::function<std::shared_ptr<const Engine::ModelAsset>(const std::string &)> m_modelResolver;
};
}
