#include "game/render/CombatActorHealthBarPolicy.h"
#include "game/ui/GameplayUiSkin.h"
#include <bx/math.h>
#include <doctest/doctest.h>

using namespace OpenYAMM::Game;

TEST_CASE("enemy health bars prioritize the target and combatants under the eight bar cap")
{
    CombatActorHealthBarSelection selection;
    for (size_t index = 0; index < 12; ++index)
    {
        considerCombatActorHealthBarCandidate(selection, {index, float(index * 10000), index}, "nearby");
    }
    considerCombatActorHealthBarCandidate(selection, {90, 2000000, 90, true, false}, "nearby");
    considerCombatActorHealthBarCandidate(selection, {91, 1900000, 91, false, true}, "nearby");
    REQUIRE(selection.count == 8);
    CHECK(selection.candidates[0].actorIndex == 90);
    CHECK(selection.candidates[1].actorIndex == 91);
    for (size_t index = 2; index < selection.count; ++index)
    {
        CHECK(selection.candidates[index].actorIndex == index - 2);
    }
    CombatActorHealthBarSelection tie;
    considerCombatActorHealthBarCandidate(tie, {0, 100, 12}, "nearby");
    considerCombatActorHealthBarCandidate(tie, {1, 100, 4}, "nearby");
    CHECK(tie.candidates[0].actorIndex == 4);
}

TEST_CASE("enemy health bar modes include an untouched target and exclude idle enemies from combat")
{
    for (const char *mode : {"off", "target", "combat", "nearby"})
    {
        CombatActorHealthBarSelection selection;
        considerCombatActorHealthBarCandidate(selection, {0, 100, 0}, mode);
        considerCombatActorHealthBarCandidate(selection, {1, 100, 1, true, false}, mode);
        considerCombatActorHealthBarCandidate(selection, {2, 100, 2, false, true}, mode);
        considerCombatActorHealthBarCandidate(selection,
            {3, MaximumCombatActorHealthBarDistanceSquared + 1000, 3, true, true}, mode);
        const size_t expected = std::string_view(mode) == "off" ? 0
            : std::string_view(mode) == "target" ? 1 : std::string_view(mode) == "combat" ? 2 : 3;
        CHECK(selection.count == expected);
        if (selection.count > 0)
        {
            CHECK(selection.candidates[0].actorIndex == 1);
        }
    }
}

TEST_CASE("enemy health damage trail holds drains and clears on healing or map reset")
{
    CombatActorHealthBarRuntime history;
    history.observe(3, 100, 100, false);
    history.recordCombatEvent(3, 60, 100, 40);
    CHECK(history.inCombat(3));
    CHECK(history.damageRatio(3, 0.6f) == doctest::Approx(1));
    history.advance(0.1f);
    CHECK(history.damageRatio(3, 0.6f) == doctest::Approx(1));
    history.advance(0.15f);
    CHECK(history.damageRatio(3, 0.6f) == doctest::Approx(0.8f));
    history.advance(0.15f);
    CHECK(history.damageRatio(3, 0.6f) == doctest::Approx(0.6f));
    history.observe(3, 80, 100, false);
    CHECK(history.damageRatio(3, 0.8f) == doctest::Approx(0.8f));
    CHECK(history.focus(9) == 9);
    history.advance(0.21f);
    CHECK(history.focus(std::nullopt) == 3);
    history.clear();
    CHECK_FALSE(history.focus(std::nullopt));
    CHECK_FALSE(history.inCombat(3));
    CHECK(history.damageRatio(3, 0.5f) == doctest::Approx(0.5f));
}

TEST_CASE("enemy health combat retention uses actual damage and attacking enemies")
{
    CombatActorHealthBarRuntime history;
    history.recordCombatEvent(1, 100, 100, 0);
    CHECK_FALSE(history.inCombat(1));
    history.observe(2, 100, 100, true);
    CHECK(history.inCombat(2));
    history.advance(5.9f);
    history.observe(2, 100, 100, false);
    CHECK(history.inCombat(2));
    history.advance(0.2f);
    CHECK_FALSE(history.inCombat(2));
    CHECK_FALSE(history.focus(std::nullopt));
    history.observe(2, 75, 100, false);
    CHECK(history.inCombat(2));
    CHECK(history.damageRatio(2, 0.75f) == doctest::Approx(1));
    history.observe(2, 100, 200, false);
    CHECK(history.damageRatio(2, 0.5f) == doctest::Approx(0.5f));
}

TEST_CASE("enemy health colors match the accepted party bands and distance scale stays bounded")
{
    CHECK(GameplayUiSkin::healthBand(0.66f) == GameplayUiSkin::HealthBand::High);
    CHECK(GameplayUiSkin::healthBand(0.65f) == GameplayUiSkin::HealthBand::Middle);
    CHECK(GameplayUiSkin::healthBand(0.34f) == GameplayUiSkin::HealthBand::Middle);
    CHECK(GameplayUiSkin::healthBand(0.33f) == GameplayUiSkin::HealthBand::Low);
    CHECK(combatActorHealthBarScale(-1) == doctest::Approx(1));
    CHECK(combatActorHealthBarScale(1000.0f * 1000) == doctest::Approx(1));
    CHECK(combatActorHealthBarScale(2000.0f * 2000) == doctest::Approx(0.5f));
    CHECK(combatActorHealthBarScale(4000.0f * 4000) == doctest::Approx(0.25f));
    CHECK(combatActorHealthBarScale(MaximumCombatActorHealthBarDistanceSquared) == doctest::Approx(0.22f));
    CHECK(combatActorHealthBarScale(MaximumCombatActorHealthBarDistanceSquared * 10) == doctest::Approx(0.22f));
}

TEST_CASE("enemy health bar depth bias stays ahead of its owner without crossing the near plane")
{
    CHECK(combatActorHealthBarDepthScale(100, 100) * 100 == doctest::Approx(98));
    CHECK(combatActorHealthBarDepthScale(120, 100) * 120 == doctest::Approx(98));
    CHECK(combatActorHealthBarDepthScale(80, 100) * 80 == doctest::Approx(78));
    CHECK(combatActorHealthBarDepthScale(1.5f, 100) * 1.5f == doctest::Approx(1.25f));
    CHECK(combatActorHealthBarDepthScale(100, 1.5f) * 100 == doctest::Approx(1.25f));
    CHECK(combatActorHealthBarDepthScale(0, 100) == 0);
    CHECK(combatActorHealthBarDepthScale(100, -1) == 0);
    CHECK(combatActorHealthBarDepthScale(1, 100) == 0);
    CHECK(combatActorHealthBarDepthScale(100, 1) == 0);
}

TEST_CASE("enemy health bar depth bias preserves projected corners and solid scenery occlusion")
{
    const bx::Vec3 eye = {-12051.7f, 752.02f, 771.0f};
    for (bool homogeneousDepth : {false, true})
    {
        for (float pitch : {-1.0f, -0.2f, 0.0f, 0.3f, 1.0f})
        {
            for (float yaw : {0.0f, 1.663f})
            {
                const bx::Vec3 forward = {std::cos(yaw) * std::cos(pitch),
                    std::sin(yaw) * std::cos(pitch), std::sin(pitch)};
                float view[16];
                float projection[16];
                float viewProjection[16];
                bx::mtxLookAt(view, eye, bx::add(eye, forward), {0, 0, 1}, bx::Handedness::Right);
                bx::mtxProj(projection, 65.0f, 16.0f / 9.0f, 1.0f, 18000.0f,
                    homogeneousDepth, bx::Handedness::Right);
                bx::mtxMul(viewProjection, view, projection);
                const bx::Vec3 viewForward = {-view[2], -view[6], -view[10]};
                const bx::Vec3 cameraRight = {view[0], view[4], view[8]};
                const bx::Vec3 cameraUp = {view[1], view[5], view[9]};
                const auto project = [&](const bx::Vec3 &position)
                {
                    const float point[] = {position.x, position.y, position.z, 1};
                    float clip[4];
                    bx::vec4MulMtx(clip, point, viewProjection);
                    return bx::Vec3{clip[0] / clip[3], clip[1] / clip[3], clip[2] / clip[3]};
                };
                for (float distance : {500.0f, 5000.0f})
                {
                    const bx::Vec3 actor = bx::add(eye, bx::mul(forward, distance));
                    const bx::Vec3 anchor = bx::add(actor, {0, 0, 240});
                    const float anchorDepth = bx::dot(bx::sub(anchor, eye), viewForward);
                    const float actorDepth = bx::dot(bx::sub(actor, eye), viewForward);
                    const float depthScale = combatActorHealthBarDepthScale(anchorDepth, actorDepth);
                    REQUIRE(depthScale > 0);
                    const bx::Vec3 renderAnchor = bx::add(eye, bx::mul(bx::sub(anchor, eye), depthScale));
                    const float renderDepth = bx::dot(bx::sub(renderAnchor, eye), viewForward);
                    CHECK(std::abs(renderDepth - (std::min(anchorDepth, actorDepth) - 2)) < 0.002f);
                    CHECK(renderDepth < actorDepth);
                    const float worldPerPixel = 2 * anchorDepth / (900 * projection[5]);
                    for (float x : {-100.0f, 100.0f})
                    {
                        for (float y : {-35.0f, 12.0f})
                        {
                            const bx::Vec3 offset = bx::add(bx::mul(cameraRight, x * worldPerPixel),
                                bx::mul(cameraUp, y * worldPerPixel));
                            const bx::Vec3 original = project(bx::add(anchor, offset));
                            const bx::Vec3 biased = project(bx::add(renderAnchor, bx::mul(offset, depthScale)));
                            CHECK(std::abs(biased.x - original.x) < 0.00002f);
                            CHECK(std::abs(biased.y - original.y) < 0.00002f);
                            CHECK(biased.z <= original.z);
                        }
                    }
                    // Geometry and unrelated creatures closer than the small bias still win the depth test.
                    const bx::Vec3 blocker = bx::add(eye, bx::mul(viewForward,
                        std::min(anchorDepth, actorDepth) - 8));
                    CHECK(project(blocker).z < project(renderAnchor).z);
                }
            }
        }
    }
}

TEST_CASE("enemy health focus bridges alpha gaps but switches targets immediately")
{
    CombatActorHealthBarRuntime history;
    CHECK(history.focus(1) == 1);
    history.advance(0.1f);
    CHECK(history.focus(std::nullopt) == 1);
    CHECK(history.focus(2) == 2);
    history.advance(0.19f);
    CHECK(history.focus(std::nullopt) == 2);
    history.advance(0.02f);
    CHECK_FALSE(history.focus(std::nullopt));
    history.recordCombatEvent(3, 90, 100, 10);
    CHECK(history.focus(4) == 4);
    history.advance(0.21f);
    CHECK(history.focus(std::nullopt) == 3);
    history.clear();
    CHECK_FALSE(history.focus(std::nullopt));
}

TEST_CASE("enemy health consecutive hits do not restore an already drained damage trail")
{
    CombatActorHealthBarRuntime history;
    history.observe(1, 100, 100, false);
    history.recordCombatEvent(1, 60, 100, 40);
    history.advance(0.5f);
    history.recordCombatEvent(1, 50, 100, 10);
    CHECK(history.damageRatio(1, 0.5f) == doctest::Approx(0.6f));
    history.advance(0.25f);
    history.observe(1, 40, 100, false);
    CHECK(history.damageRatio(1, 0.4f) == doctest::Approx(0.55f));
    history.observeActivity(8, true);
    CHECK(history.inCombat(8));
    history.advance(6.01f);
    CHECK_FALSE(history.inCombat(8));
}
