#include "game/fx/EffectSystem.h"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>

namespace
{
std::shared_ptr<OpenYAMM::Game::EffectDefinition> makeDefinition()
{
    using namespace OpenYAMM::Game;
    std::shared_ptr<EffectDefinition> definition = std::make_shared<EffectDefinition>();
    definition->id = "test:fire";
    definition->durationSeconds = 0.4f;
    definition->timingProfile = EffectTimingProfile::Mm9Native;

    EffectSpriteEmitterDefinition emitter;
    emitter.componentId = 6;
    emitter.startSeconds = 0.0f;
    emitter.endSeconds = 0.08f;
    emitter.spriteResource = "test:sprite/fire";
    emitter.spritesPerEmission = 1;
    emitter.emissionIntervalSeconds = 0.001f;
    emitter.particleLifetimeSeconds = 0.2f;
    emitter.emissionShape = EffectEmissionShape::PlaneIn;
    emitter.radius = 2.0f;
    emitter.velocity = 0.0f;
    emitter.colorKeys = {
        {.time = 0.0f, .rgb = {255, 128, 0}, .transparency = 100},
        {.time = 1.0f, .rgb = {128, 0, 0}, .transparency = 213},
    };
    emitter.scaleKeys = {{.time = 0.0f, .value = 0.2f}, {.time = 1.0f, .value = 0.4f}};
    definition->spriteEmitters.push_back(std::move(emitter));

    EffectSoundDefinition sound;
    sound.componentId = 10;
    sound.startSeconds = 0.0f;
    sound.endSeconds = 0.1f;
    sound.soundResource = "test:sound/fire";
    sound.pitch = 1.25f;
    sound.innerRadius = 10.0f;
    sound.outerRadius = 50.0f;
    definition->sounds.push_back(std::move(sound));
    return definition;
}

OpenYAMM::Game::EffectLibrary makeLibrary()
{
    OpenYAMM::Game::EffectLibrary library;
    std::string error;
    REQUIRE(library.add(makeDefinition(), error));
    return library;
}
}

TEST_CASE("EffectSystem loading dependencies include delayed sprites only for active effects")
{
    using namespace OpenYAMM::Game;
    std::shared_ptr<EffectDefinition> definition = makeDefinition();
    definition->spriteEmitters.front().startSeconds = 0.2f;
    definition->spriteEmitters.front().endSeconds = 0.3f;
    EffectSpriteDefinition sprite;
    sprite.componentId = 20;
    sprite.spriteResource = "test:sprite/later";
    sprite.startSeconds = 0.2f;
    sprite.endSeconds = 0.3f;
    definition->sprites.push_back(sprite);
    EffectLibrary library;
    std::string error;
    REQUIRE(library.add(definition, error));
    EffectSystem effects(&library);
    CHECK(effects.activeSpriteResources().empty());
    const EffectHandle first = effects.spawn("test:fire");
    const EffectHandle second = effects.spawn("test:fire");
    REQUIRE(effects.contains(first));
    REQUIRE(effects.contains(second));
    CHECK(effects.particles().empty());
    CHECK(effects.fixedSprites().empty());
    CHECK(effects.activeSpriteResources() == std::vector<std::string>{"test:sprite/fire", "test:sprite/later"});
    effects.stop(first, EffectStopMode::Immediate);
    CHECK_EQ(effects.activeSpriteResources().size(), 2);
    effects.stop(second, EffectStopMode::Immediate);
    CHECK(effects.activeSpriteResources().empty());
}

TEST_CASE("EffectSystem preserves world particles when a moving emitter changes transform")
{
    using namespace OpenYAMM::Game;
    EffectLibrary library = makeLibrary();
    EffectSystem effects(&library);
    const EffectHandle handle = effects.spawn("test:fire");
    REQUIRE(effects.contains(handle));

    std::vector<EffectSoundEvent> soundEvents = effects.consumeSoundEvents();
    REQUIRE_EQ(soundEvents.size(), 1);
    CHECK_EQ(soundEvents[0].kind, EffectSoundEventKind::Start);
    CHECK(soundEvents[0].pitch == doctest::Approx(1.25f));

    effects.update(1.0f / 60.0f);
    REQUIRE_EQ(effects.particles().size(), 1);
    const float firstX = effects.particles()[0].position[0];
    CHECK(effects.particles()[0].color[3] == doctest::Approx(1.0f - 100.0f / 255.0f));

    REQUIRE(effects.setTransform(handle, {10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 1.0f));
    effects.update(1.0f / 60.0f);
    REQUIRE_EQ(effects.particles().size(), 2);
    CHECK(effects.particles()[0].position[0] == doctest::Approx(firstX));
    CHECK(effects.particles()[1].position[0] > 7.0f);
}

TEST_CASE("EffectSystem MM9 timing emits at most once per compatibility tick and expires early")
{
    using namespace OpenYAMM::Game;
    EffectLibrary library = makeLibrary();
    EffectSystem effects(&library);
    const EffectHandle handle = effects.spawn("test:fire");

    effects.update(3.0f / 60.0f);
    CHECK_EQ(effects.particles().size(), 3);
    effects.update(4.0f / 60.0f);
    CHECK_EQ(effects.particles().size(), 3);
    CHECK(effects.elapsedSeconds(handle) == doctest::Approx(7.0f / 60.0f));

    const std::vector<EffectSoundEvent> soundEvents = effects.consumeSoundEvents();
    REQUIRE_EQ(soundEvents.size(), 2);
    CHECK_EQ(soundEvents[0].kind, EffectSoundEventKind::Start);
    CHECK_EQ(soundEvents[1].kind, EffectSoundEventKind::Stop);
}

TEST_CASE("EffectSystem exposes 30 60 and 120 Hz MM9 diagnostic policies")
{
    using namespace OpenYAMM::Game;
    EffectLibrary library = makeLibrary();
    for (const std::pair<uint32_t, size_t> expectation : {
             std::pair<uint32_t, size_t>{30, 1},
             std::pair<uint32_t, size_t>{60, 3},
             std::pair<uint32_t, size_t>{120, 6}})
    {
        EffectSystem effects(&library);
        REQUIRE(effects.setReferenceUpdateRate(expectation.first));
        CHECK_EQ(effects.referenceUpdateRate(), expectation.first);
        REQUIRE(effects.contains(effects.spawn("test:fire")));
        effects.update(0.05f);
        CHECK_EQ(effects.particles().size(), expectation.second);
    }

    EffectSystem effects(&library);
    CHECK_FALSE(effects.setReferenceUpdateRate(59));
    CHECK_EQ(effects.referenceUpdateRate(), 60u);
}

TEST_CASE("EffectSystem immediate and drain stops have distinct lifecycle behavior")
{
    using namespace OpenYAMM::Game;
    EffectLibrary library = makeLibrary();
    EffectSystem effects(&library);
    const EffectHandle immediate = effects.spawn("test:fire");
    effects.update(1.0f / 60.0f);
    REQUIRE_FALSE(effects.particles().empty());
    REQUIRE(effects.stop(immediate, EffectStopMode::Immediate));
    CHECK_FALSE(effects.contains(immediate));
    CHECK(effects.particles().empty());

    const EffectHandle draining = effects.spawn("test:fire");
    effects.update(1.0f / 60.0f);
    REQUIRE_EQ(effects.particles().size(), 1);
    REQUIRE(effects.stop(draining, EffectStopMode::Drain));
    effects.update(1.0f / 60.0f);
    CHECK_EQ(effects.particles().size(), 1);
    effects.update(10.0f / 60.0f);
    CHECK(effects.particles().empty());
    CHECK_FALSE(effects.contains(draining));
}

TEST_CASE("EffectSystem rejects duplicate definitions and stale handles")
{
    using namespace OpenYAMM::Game;
    EffectLibrary library;
    std::string error;
    REQUIRE(library.add(makeDefinition(), error));
    CHECK_FALSE(library.add(makeDefinition(), error));
    CHECK(error.find("duplicate") != std::string::npos);

    EffectSystem effects(&library);
    const EffectHandle oldHandle = effects.spawn("test:fire");
    REQUIRE(effects.stop(oldHandle, EffectStopMode::Immediate));
    const EffectHandle replacement = effects.spawn("test:fire");
    CHECK_EQ(oldHandle.index, replacement.index);
    CHECK_NE(oldHandle.generation, replacement.generation);
    CHECK_FALSE(effects.pause(oldHandle, true));
}

TEST_CASE("EffectSystem samples predictable emitters and applies per-tick rotation")
{
    using namespace OpenYAMM::Game;
    std::shared_ptr<EffectDefinition> definition = std::make_shared<EffectDefinition>();
    definition->id = "test:rotating";
    definition->durationSeconds = 0.2f;
    definition->timingProfile = EffectTimingProfile::Predictable;
    EffectSpriteEmitterDefinition emitter;
    emitter.componentId = 1;
    emitter.spriteResource = "test:sprite/particle";
    emitter.endSeconds = 0.1f;
    emitter.emissionIntervalSeconds = 0.001f;
    emitter.particleLifetimeSeconds = 1.0f;
    emitter.emissionShape = EffectEmissionShape::Plane;
    emitter.planeDirection = {1.0f, 0.0f, 0.0f};
    emitter.velocity = 4.0f;
    emitter.rotationPerTick = {0.0f, 0.0f, 1.57079632679f};
    definition->spriteEmitters.push_back(emitter);

    EffectLibrary library;
    std::string error;
    REQUIRE(library.add(definition, error));
    EffectSystem effects(&library);
    REQUIRE(effects.contains(effects.spawn(definition->id)));
    effects.update(2.0f / 60.0f);

    REQUIRE_EQ(effects.particles().size(), 2);
    CHECK(effects.particles()[0].velocity[0] == doctest::Approx(4.0f));
    CHECK(effects.particles()[0].velocity[1] == doctest::Approx(0.0f));
    CHECK(effects.particles()[1].velocity[0] == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(effects.particles()[1].velocity[1] == doctest::Approx(4.0f));
}

TEST_CASE("EffectSystem local particles follow later root transforms")
{
    using namespace OpenYAMM::Game;
    std::shared_ptr<EffectDefinition> definition = std::make_shared<EffectDefinition>();
    definition->id = "test:local";
    definition->durationSeconds = 0.2f;
    EffectSpriteEmitterDefinition emitter;
    emitter.componentId = 2;
    emitter.spriteResource = "test:sprite/particle";
    emitter.endSeconds = 0.02f;
    emitter.emissionIntervalSeconds = 1.0f;
    emitter.particleLifetimeSeconds = 1.0f;
    emitter.offset = {2.0f, 0.0f, 0.0f};
    emitter.particleSpace = EffectParticleSpace::Local;
    definition->spriteEmitters.push_back(emitter);

    EffectLibrary library;
    std::string error;
    REQUIRE(library.add(definition, error));
    EffectSystem effects(&library);
    const EffectHandle handle = effects.spawn(definition->id);
    effects.update(1.0f / 60.0f);
    REQUIRE_EQ(effects.particles().size(), 1);
    CHECK(effects.particles()[0].position[0] == doctest::Approx(2.0f));

    REQUIRE(effects.setTransform(handle, {10.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, 2.0f));
    effects.update(1.0f / 60.0f);
    CHECK(effects.particles()[0].position[0] == doctest::Approx(14.0f));
}

TEST_CASE("EffectSystem applies effect rotation to world-plane sprite bases")
{
    using namespace OpenYAMM::Game;
    std::shared_ptr<EffectDefinition> definition = std::make_shared<EffectDefinition>();
    definition->id = "test:world_plane";
    definition->durationSeconds = 1.0f;

    EffectSpriteDefinition sprite;
    sprite.componentId = 1;
    sprite.endSeconds = 1.0f;
    sprite.spriteResource = "test:sprite/fixed";
    sprite.alignment = EffectSpriteAlignment::WorldPlane;
    sprite.planeRight = {1.0f, 0.0f, 0.0f};
    sprite.planeUp = {0.0f, 0.0f, 1.0f};
    definition->sprites.push_back(sprite);

    EffectSpriteEmitterDefinition emitter;
    emitter.componentId = 2;
    emitter.endSeconds = 0.02f;
    emitter.spriteResource = "test:sprite/particle";
    emitter.emissionIntervalSeconds = 1.0f;
    emitter.particleLifetimeSeconds = 1.0f;
    emitter.particleSpace = EffectParticleSpace::Local;
    emitter.alignment = EffectSpriteAlignment::WorldPlane;
    emitter.planeRight = {1.0f, 0.0f, 0.0f};
    emitter.planeUp = {0.0f, 0.0f, 1.0f};
    definition->spriteEmitters.push_back(emitter);

    EffectLibrary library;
    std::string error;
    REQUIRE(library.add(definition, error));
    EffectSystem effects(&library);
    const EffectHandle handle = effects.spawn(definition->id);
    effects.update(1.0f / 60.0f);
    REQUIRE_EQ(effects.particles().size(), 1);
    REQUIRE_EQ(effects.fixedSprites().size(), 1);
    CHECK_EQ(effects.particles()[0].planeRight, std::array<float, 3>{1.0f, 0.0f, 0.0f});
    CHECK_EQ(effects.fixedSprites()[0].planeUp, std::array<float, 3>{0.0f, 0.0f, 1.0f});

    const float halfRoot = std::sqrt(0.5f);
    REQUIRE(effects.setTransform(handle, {}, {0.0f, 0.0f, halfRoot, halfRoot}, 1.0f));
    effects.update(1.0f / 60.0f);
    REQUIRE_EQ(effects.particles().size(), 1);
    REQUIRE_EQ(effects.fixedSprites().size(), 1);
    CHECK(effects.particles()[0].planeRight[0] == doctest::Approx(0.0f).epsilon(0.0001));
    CHECK(effects.particles()[0].planeRight[1] == doctest::Approx(1.0f).epsilon(0.0001));
    CHECK(effects.fixedSprites()[0].planeRight[0] == doctest::Approx(0.0f).epsilon(0.0001));
    CHECK(effects.fixedSprites()[0].planeRight[1] == doctest::Approx(1.0f).epsilon(0.0001));
}

TEST_CASE("EffectSystem restart and seek rebuild deterministic instance state")
{
    using namespace OpenYAMM::Game;
    EffectLibrary library = makeLibrary();
    EffectSystem effects(&library);
    const EffectHandle handle = effects.spawn("test:fire", {.seed = 123});
    effects.consumeSoundEvents();
    effects.update(3.0f / 60.0f);
    REQUIRE_EQ(effects.particles().size(), 3);
    const std::array<float, 3> expectedPosition = effects.particles()[0].position;

    REQUIRE(effects.seek(handle, 3.0f / 60.0f));
    REQUIRE_EQ(effects.particles().size(), 3);
    CHECK(effects.elapsedSeconds(handle) == doctest::Approx(3.0f / 60.0f));
    CHECK_EQ(effects.particles()[0].position, expectedPosition);

    REQUIRE(effects.restart(handle));
    CHECK(effects.particles().empty());
    CHECK(effects.elapsedSeconds(handle) == doctest::Approx(0.0f));
}

TEST_CASE("EffectLibrary rejects non-finite fixed-sprite track values")
{
    using namespace OpenYAMM::Game;
    std::shared_ptr<EffectDefinition> definition = std::make_shared<EffectDefinition>();
    definition->id = "test:invalid_fixed_sprite";
    definition->durationSeconds = 1.0f;
    EffectSpriteDefinition sprite;
    sprite.componentId = 1;
    sprite.endSeconds = 1.0f;
    sprite.spriteResource = "test:sprite";
    sprite.scaleKeys.push_back({.time = 0.0f, .value = std::numeric_limits<float>::infinity()});
    definition->sprites.push_back(std::move(sprite));

    EffectLibrary library;
    std::string error;
    CHECK_FALSE(library.add(definition, error));
    CHECK(error.find("finite") != std::string::npos);

    std::shared_ptr<EffectDefinition> invalidPlane = std::make_shared<EffectDefinition>();
    invalidPlane->id = "test:invalid_sprite_plane";
    invalidPlane->durationSeconds = 1.0f;
    EffectSpriteDefinition planeSprite;
    planeSprite.componentId = 1;
    planeSprite.endSeconds = 1.0f;
    planeSprite.spriteResource = "test:sprite";
    planeSprite.alignment = EffectSpriteAlignment::WorldPlane;
    planeSprite.planeRight = {1.0f, 0.0f, 0.0f};
    planeSprite.planeUp = {2.0f, 0.0f, 0.0f};
    invalidPlane->sprites.push_back(planeSprite);
    error.clear();
    CHECK_FALSE(library.add(invalidPlane, error));
    CHECK(error.find("invalid sprite component") != std::string::npos);
}

TEST_CASE("EffectSystem clear tears down models particles sprites and sound ownership")
{
    using namespace OpenYAMM;
    std::shared_ptr<Game::EffectDefinition> definition = std::make_shared<Game::EffectDefinition>();
    definition->id = "test:teardown";
    definition->durationSeconds = 10.0f;
    definition->models.push_back({
        .componentId = 1,
        .startSeconds = 0.0f,
        .endSeconds = 10.0f,
        .modelResource = "test:model",
        .visible = true,
    });
    definition->sprites.push_back({
        .componentId = 2,
        .startSeconds = 0.0f,
        .endSeconds = 10.0f,
        .spriteResource = "test:sprite",
    });
    definition->spriteEmitters.push_back({
        .componentId = 3,
        .startSeconds = 0.0f,
        .endSeconds = 10.0f,
        .spriteResource = "test:particle",
        .emissionIntervalSeconds = 1.0f,
        .particleLifetimeSeconds = 10.0f,
    });
    definition->sounds.push_back({
        .componentId = 4,
        .startSeconds = 0.0f,
        .endSeconds = 10.0f,
        .soundResource = "test:sound",
        .loop = true,
    });

    Game::EffectLibrary library;
    std::string error;
    REQUIRE(library.add(definition, error));
    Engine::ModelInstanceSystem models;
    const std::shared_ptr<Engine::ModelAsset> modelAsset = std::make_shared<Engine::ModelAsset>();
    modelAsset->sourcePath = "test:model";
    Game::EffectSystem effects(&library);
    effects.setModelRuntime(
        &models,
        [&](const std::string &id)
        {
            return id == "test:model" ? modelAsset : nullptr;
        });

    const Game::EffectHandle handle = effects.spawn(definition->id);
    REQUIRE(effects.contains(handle));
    REQUIRE_EQ(models.size(), 1u);
    REQUIRE_EQ(effects.fixedSprites().size(), 1u);
    REQUIRE_EQ(effects.consumeSoundEvents().size(), 1u);
    effects.update(1.0f / 60.0f);
    REQUIRE_EQ(effects.particles().size(), 1u);

    effects.clear();
    CHECK_EQ(effects.size(), 0u);
    CHECK_EQ(models.size(), 0u);
    CHECK(effects.particles().empty());
    CHECK(effects.fixedSprites().empty());
    const std::vector<Game::EffectSoundEvent> teardownEvents = effects.consumeSoundEvents();
    REQUIRE_EQ(teardownEvents.size(), 1u);
    CHECK_EQ(teardownEvents.front().kind, Game::EffectSoundEventKind::Stop);
    CHECK_FALSE(effects.contains(handle));
}
