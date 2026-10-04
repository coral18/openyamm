#pragma once

#include "game/events/EventRuntime.h"
#include "game/gameplay/GameplayProjectileService.h"
#include "game/gameplay/GameplayRuntimeInterfaces.h"
#include "game/party/PartySpellSystem.h"
#include "game/tables/PortraitFxEventTable.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace OpenYAMM::Engine
{
class AssetFileSystem;
}

namespace OpenYAMM::Game
{
class GameSession;
class GameplayScreenRuntime;
class WorldFxSystem;

class GameplayFxService
{
public:
    struct GameplayScreenOverlayState
    {
        float remainingSeconds = 0.0f;
        float durationSeconds = 0.0f;
        float peakAlpha = 0.0f;
        uint32_t colorAbgr = 0x00000000u;
    };

    explicit GameplayFxService(GameSession &session);

    void clear();
    void syncProjectilePresentation();
    GameplayProjectileService::ProjectileImpactSpawnResult spawnProjectileImpactVisual(
        const GameplayProjectileService::ProjectileState &projectile,
        const GameplayProjectileService::ProjectileImpactVisualDefinition &definition,
        float x,
        float y,
        float z,
        bool centerVertically,
        size_t targetActorIndex = static_cast<size_t>(-1));
    GameplayProjectileService::ProjectileImpactSpawnResult spawnWaterSplashImpactVisual(
        const GameplayProjectileService::ProjectileImpactVisualDefinition &definition,
        float x,
        float y,
        float z);
    GameplayProjectileService::ProjectileImpactSpawnResult spawnImmediateSpellImpactVisual(
        const GameplayProjectileService::ProjectileImpactVisualDefinition &definition,
        int sourceSpellId,
        const std::string &sourceObjectName,
        const std::string &sourceObjectSpriteName,
        float x,
        float y,
        float z,
        bool centerVertically,
        bool freezeAnimation);
    void triggerPortraitEventFxWithoutSpeech(
        GameplayScreenRuntime &runtime,
        size_t memberIndex,
        PortraitFxEventKind kind) const;
    void triggerPortraitSpellFx(const PartySpellCastResult &result) const;
    void triggerGameplayScreenOverlay(const PartySpellCastResult::ScreenOverlayRequest &request);
    void advanceGameplayScreenOverlay(float deltaSeconds);
    void renderGameplayScreenOverlay(GameplayScreenRuntime &runtime, int width, int height) const;
    void queueMeleeHitBloodEffect(
        const std::array<float, 3> &contactPosition,
        const std::array<float, 3> &outwardNormal);
    void consumePendingEventFxRequests(GameplayScreenRuntime &runtime) const;
    void consumePendingWorldFxRequests(
        EventRuntimeState *pEventRuntimeState,
        WorldFxSystem &worldFxSystem,
        Engine::AssetFileSystem &assetFileSystem);
    const std::vector<GameplayProjectilePresentationState> &activeProjectilePresentationStates() const;
    const std::vector<GameplayProjectileImpactPresentationState> &activeProjectileImpactPresentationStates() const;

private:
    struct PendingWorldEffect
    {
        std::string id;
        std::array<float, 3> position = {};
        std::array<float, 4> rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        float scale = 1.0f;
        uint32_t seed = 1;
    };

    void consumePendingPortraitEventFxRequest(
        GameplayScreenRuntime &runtime,
        const EventRuntimeState::PortraitFxRequest &request) const;
    void consumePendingSpellFxRequest(
        GameplayScreenRuntime &runtime,
        const EventRuntimeState::SpellFxRequest &request) const;

    GameSession &m_session;
    GameplayScreenOverlayState m_gameplayScreenOverlayState;
    std::vector<GameplayProjectilePresentationState> m_activeProjectilePresentationStates;
    std::vector<GameplayProjectileImpactPresentationState> m_activeProjectileImpactPresentationStates;
    std::vector<PendingWorldEffect> m_pendingWorldEffects;
    uint32_t m_nextWorldEffectSeed = 1;
};
}
