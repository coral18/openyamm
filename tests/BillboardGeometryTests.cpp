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


TEST_CASE("visual sight anchors use opaque pixel centers and preserve mirrors and mask replacement")
{
    using namespace OpenYAMM::Game;
    BillboardOpacityMask mask;
    CHECK_FALSE(mask.firstOpaqueNormalized());
    std::vector<uint8_t> pixels(4 * 4 * 4, 0);
    pixels[(3 * 4 + 3) * 4 + 3] = 255; // Low corpse pixel; the image center is transparent.
    mask.assignFromBgra(pixels, 4, 4);
    REQUIRE(mask.firstOpaqueNormalized());
    CHECK(mask.isOpaqueNormalized((*mask.firstOpaqueNormalized())[0], (*mask.firstOpaqueNormalized())[1]));
    const BillboardQuad quad{{10, 20, 30}, {4, 0, 0}, {0, 0, 8}};
    const bx::Vec3 point = billboardTexturePoint(quad, false, *mask.firstOpaqueNormalized());
    CHECK(point.x == doctest::Approx(13));
    CHECK(point.z == doctest::Approx(24));
    const bx::Vec3 mirror = billboardTexturePoint(quad, true, *mask.firstOpaqueNormalized());
    CHECK(mirror.x == doctest::Approx(7));
    CHECK(mirror.z == doctest::Approx(24));
    REQUIRE(mask.assignBits(4, 4, {0x01, 0x00}));
    REQUIRE(mask.firstOpaqueNormalized());
    CHECK((*mask.firstOpaqueNormalized())[0] == doctest::Approx(0.125));
    CHECK((*mask.firstOpaqueNormalized())[1] == doctest::Approx(0.125));
    REQUIRE(mask.assignBits(4, 4, {0x00, 0x00}));
    CHECK_FALSE(mask.firstOpaqueNormalized());
    mask.assignFromBgra({}, 0, 0);
    CHECK_FALSE(mask.firstOpaqueNormalized());
}
