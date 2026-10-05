#include "engine/render/ModelSunShadows.h"

#include <doctest/doctest.h>

using namespace OpenYAMM::Engine;

TEST_CASE("model sunlight shadow projection matches backend origins and depth conventions")
{
    const std::array<float, 3> camera = {-9728.0f, -11919.0f, 257.0f};
    const std::array<float, 3> direction = {-0.5f, -0.5f, std::sqrt(0.5f)};
    for (bool homogeneousDepth : {false, true})
    {
        for (bool bottomLeft : {false, true})
        {
            const ModelSunShadowCascade cascade = modelSunShadowCascade(
                camera, direction, ModelSunShadowRadii[0], homogeneousDepth, bottomLeft);
            const float receiver[4] = {camera[0], camera[1], camera[2], 1.0f};
            const float caster[4] = {camera[0] + direction[0] * 100.0f,
                camera[1] + direction[1] * 100.0f, camera[2] + direction[2] * 100.0f, 1.0f};
            float receiverUv[4];
            float casterUv[4];
            bx::vec4MulMtx(receiverUv, receiver, cascade.textureMatrix.data());
            bx::vec4MulMtx(casterUv, caster, cascade.textureMatrix.data());
            CHECK(receiverUv[0] == doctest::Approx(0.5f).epsilon(0.001f));
            CHECK(receiverUv[1] == doctest::Approx(0.5f).epsilon(0.001f));
            CHECK(receiverUv[2] == doctest::Approx(0.5f).epsilon(0.001f));
            CHECK(casterUv[0] == doctest::Approx(receiverUv[0]).epsilon(0.001f));
            CHECK(casterUv[1] == doctest::Approx(receiverUv[1]).epsilon(0.001f));
            CHECK(casterUv[2] < receiverUv[2]);
            const bx::Vec3 right = bx::normalize(bx::cross({0, 0, 1},
                {direction[0], direction[1], direction[2]}));
            const bx::Vec3 up = bx::cross({direction[0], direction[1], direction[2]}, right);
            const float above[4] = {camera[0] + up.x * 100.0f, camera[1] + up.y * 100.0f,
                camera[2] + up.z * 100.0f, 1.0f};
            float aboveUv[4];
            bx::vec4MulMtx(aboveUv, above, cascade.textureMatrix.data());
            CHECK((aboveUv[1] > receiverUv[1]) == bottomLeft);
        }
    }
}

TEST_CASE("model sunlight cascades retain crossing casters and reject distant and invalid bounds")
{
    for (const std::array<float, 3> light : {std::array<float, 3>{0, 0, 1},
        std::array<float, 3>{-0.5f, -0.5f, std::sqrt(0.5f)}})
    {
        const ModelSunShadowCascade cascade = modelSunShadowCascade({0, 0, 0}, light, 1024.0f, true, true);
        CHECK(modelSunShadowIntersects(cascade, {{-50, -50, -50}, {50, 50, 180}, true}));
        CHECK(modelSunShadowIntersects(cascade, {{-2048, -2048, -100}, {2048, 2048, 180}, true}));
        CHECK_FALSE(modelSunShadowIntersects(cascade, {{10000, 10000, 0}, {10100, 10100, 180}, true}));
        CHECK_FALSE(modelSunShadowIntersects(cascade, {{-10100, -10100, 0}, {-10000, -10000, 180}, true}));
        CHECK_FALSE(modelSunShadowIntersects(cascade, {}));
        for (float component : cascade.textureMatrix)
        {
            CHECK(std::isfinite(component));
        }
    }
}

TEST_CASE("model sunlight cascade translation stays stable within a shadow texel")
{
    const std::array<float, 3> light = {-0.5f, -0.5f, std::sqrt(0.5f)};
    const ModelSunShadowCascade first = modelSunShadowCascade({0, 0, 0}, light, 1024.0f, true, true);
    const bx::Vec3 right = bx::normalize(bx::cross({0, 0, 1}, {light[0], light[1], light[2]}));
    const ModelSunShadowCascade shifted = modelSunShadowCascade(
        {right.x * 0.1f, right.y * 0.1f, 0}, light, 1024.0f, true, true);
    for (size_t index = 0; index < first.textureMatrix.size(); ++index)
    {
        CHECK(first.textureMatrix[index] == doctest::Approx(shifted.textureMatrix[index]).epsilon(0.00001f));
    }
}
