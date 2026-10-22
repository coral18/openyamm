#include "doctest/doctest.h"
#include "tests/PartySpellTestHarness.h"
#include "game/gameplay/GameplayInteractionController.h"
#include "game/world/WorldSiteObservations.h"

using namespace OpenYAMM::Game;

namespace
{
const std::string Site = "bmodel/38/surface/10";
}

TEST_CASE("shared site activation observes actual results and ignores failed hidden or map-changing actions")
{
    class SiteWorld : public OpenYAMM::Tests::PartySpellTestWorldRuntime
    {
    public:
        Party heroes;
        EventRuntimeState state;
        bool allowed = true, succeeds = true, indoor = false, switchMap = false;
        int calls = 0;
        Party *party() override
        {
            return &heroes;
        }
        const Party *party() const override
        {
            return &heroes;
        }
        EventRuntimeState *eventRuntimeState() override
        {
            return &state;
        }
        const EventRuntimeState *eventRuntimeState() const override
        {
            return &state;
        }
        bool isIndoorMap() const override
        {
            return indoor;
        }
        float gameMinutes() const override
        {
            return 42;
        }
        bool canActivateWorldHit(const GameplayWorldHit &, GameplayInteractionMethod) const override
        {
            return allowed;
        }
        bool activateWorldHit(const GameplayWorldHit &) override
        {
            ++calls;
            if (!succeeds) return false;
            heroes.addGold(200);
            if (switchMap) state.mapFileName = "out01.odm";
            return true;
        }
    };
    const auto hit = []
    {
        GameplayWorldHit result;
        result.hasHit = true;
        result.kind = GameplayWorldHitKind::EventTarget;
        GameplayEventTargetHit target;
        target.targetKind = GameplayWorldEventTargetKind::Surface;
        target.targetIndex = 38;
        target.secondaryIndex = 10;
        target.triggeredEventId = 223;
        GameplayEventTargetContextActionMetadata metadata;
        metadata.kind = "well";
        target.contextActionMetadata = metadata;
        result.eventTarget = target;
        return result;
    }();
    const auto activate = [](SiteWorld &world, const GameplayWorldHit &target)
    {
        GameplayScreenState::WorldInteractionInputState state;
        GameplayInteractionController::KeyboardActivationInteractionInput input;
        input.activationPressed = true;
        input.allowInteraction = true;
        input.currentTickNanoseconds = 1;
        input.currentHit = target;
        input.pWorldRuntime = &world;
        return GameplayInteractionController::updateKeyboardActivationInteraction(state, input).activated;
    };
    for (int variant = 0; variant < 5; ++variant)
    {
        CAPTURE(variant);
        SiteWorld world;
        world.heroes.seed(Party::createDefaultSeed());
        world.state.mapFileName = "7out02.odm";
        world.setMapName(world.state.mapFileName);
        world.allowed = variant != 1;
        world.succeeds = variant != 2;
        world.indoor = variant == 3;
        world.switchMap = variant == 4;
        const bool activated = activate(world, hit);
        CHECK(activated == (variant != 1 && variant != 2));
        const auto observations = worldSiteObservations(world.state);
        if (variant == 0 || variant == 3)
        {
            REQUIRE(observations.size() == 1);
            CHECK(observations[0].siteId == (world.indoor ? "surface/38" : Site));
            REQUIRE(observations[0].effects.size() == 1);
            CHECK(observations[0].effects[0].minDelta() == 200);
        }
        else
            CHECK(observations.empty());
        if (variant == 1) CHECK(world.calls == 0);
    }
    SiteWorld hidden;
    hidden.heroes.seed(Party::createDefaultSeed());
    auto hiddenHit = hit;
    hiddenHit.eventTarget->contextActionMetadata->hidden = true;
    CHECK(activate(hidden, hiddenHit));
    CHECK(worldSiteObservations(hidden.state).empty());
}
