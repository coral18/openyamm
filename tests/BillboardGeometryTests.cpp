#include "doctest/doctest.h"

#include "game/render/BillboardGeometry.h"

TEST_CASE("bottom-anchored billboard keeps its lower edge at the world position")
{
    const bx::Vec3 cameraUp = {0.0f, -0.6f, 0.8f};
    const bx::Vec3 center = OpenYAMM::Game::bottomAnchoredBillboardCenter(
        10.0f,
        20.0f,
        30.0f,
        cameraUp,
        200.0f);
    const bx::Vec3 lowerEdgeCenter = {
        center.x - cameraUp.x * 100.0f,
        center.y - cameraUp.y * 100.0f,
        center.z - cameraUp.z * 100.0f,
    };

    CHECK_EQ(lowerEdgeCenter.x, doctest::Approx(10.0f));
    CHECK_EQ(lowerEdgeCenter.y, doctest::Approx(20.0f));
    CHECK_EQ(lowerEdgeCenter.z, doctest::Approx(30.0f));
}

TEST_CASE("nearest opaque billboard points preserve displayed mirroring")
{
    std::vector<uint8_t> pixels(3 * 1 * 4, 0);
    pixels[3] = 255;
    OpenYAMM::Game::BillboardOpacityMask mask;
    mask.assignFromBgra(pixels, 3, 1);
    const OpenYAMM::Game::BillboardQuad quad = {
        .center = {10.0f, 20.0f, 30.0f},
        .right = {3.0f, 0.0f, 0.0f},
        .up = {0.0f, 0.0f, 1.0f},
    };

    const std::optional<bx::Vec3> normal = OpenYAMM::Game::nearestOpaqueBillboardPoint(
        quad, mask, false, 1.5f, 0.5f);
    const std::optional<bx::Vec3> mirrored = OpenYAMM::Game::nearestOpaqueBillboardPoint(
        quad, mask, true, -0.5f, 0.5f);

    REQUIRE(normal.has_value());
    CHECK(normal->x == doctest::Approx(9.0f));
    CHECK(normal->y == doctest::Approx(20.0f));
    CHECK(normal->z == doctest::Approx(30.0f));
    REQUIRE(mirrored.has_value());
    CHECK(mirrored->x == doctest::Approx(11.0f));
    CHECK(mirrored->y == doctest::Approx(20.0f));
    CHECK(mirrored->z == doctest::Approx(30.0f));
}
