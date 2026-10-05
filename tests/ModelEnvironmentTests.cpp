#include "engine/render/ModelEnvironment.h"

#include <bx/uint32_t.h>
#include <doctest/doctest.h>

#include <cmath>
#include <algorithm>
#include <stdexcept>

using namespace OpenYAMM::Engine;

TEST_CASE("model sky environment keeps linear colour, hemisphere orientation and complete filtered cube mips")
{
    const std::array<uint8_t, 4> blue = {255, 0, 0, 255};
    const ModelSkyEnvironment sky = {"blue", 1, 1, 1, 1, blue};
    const std::vector<ModelEnvironmentMip> mips = prepareModelSkyEnvironment(sky);
    REQUIRE(mips.size() == 8);
    uint16_t expectedSize = 128;
    for (const ModelEnvironmentMip &mip : mips)
    {
        CHECK(mip.size == expectedSize);
        expectedSize /= 2;
        for (const std::vector<uint16_t> &face : mip.rgbaHalfFaces)
        {
            CHECK(face.size() == size_t(mip.size) * mip.size * 4);
            for (size_t offset : {size_t(0), (face.size() / 8) * 4, face.size() - 4})
            {
                CHECK(bx::halfToFloat(face[offset]) == 0);
                CHECK(bx::halfToFloat(face[offset + 1]) == 0);
                REQUIRE(std::isfinite(bx::halfToFloat(face[offset + 2])));
                CHECK(bx::halfToFloat(face[offset + 3]) == 1);
            }
        }
    }
    const size_t center = (64 * 128 + 64) * 4 + 2;
    const float upper = bx::halfToFloat(mips[0].rgbaHalfFaces[4][center]);
    const float lower = bx::halfToFloat(mips[0].rgbaHalfFaces[5][center]);
    CHECK(upper == doctest::Approx(1.0f / 0.0722f).epsilon(0.001f));
    CHECK(lower == doctest::Approx(upper * 0.15f).epsilon(0.001f));
    CHECK(bx::halfToFloat(mips.back().rgbaHalfFaces[4][2]) < upper);
    ModelSkyEnvironment invalid = sky;
    invalid.logicalHeight = 0;
    CHECK_THROWS_AS(prepareModelSkyEnvironment(invalid), std::invalid_argument);
    invalid = sky;
    invalid.bgraPixels = {};
    CHECK_THROWS_AS(prepareModelSkyEnvironment(invalid), std::invalid_argument);
}

TEST_CASE("model environment integrated BRDF preserves dielectric and rough material energy")
{
    const std::vector<uint8_t> pixels = prepareModelEnvironmentBrdf();
    REQUIRE(pixels.size() == size_t(ModelEnvironmentBrdfSize) * ModelEnvironmentBrdfSize * 4);
    const auto response = [&](size_t row, size_t column)
    {
        const size_t offset = (row * ModelEnvironmentBrdfSize + column) * 4;
        return (0.04f * pixels[offset] + pixels[offset + 1]) / 255.0f;
    };
    CHECK(response(0, 63) == doctest::Approx(0.04f).epsilon(0.02f));
    CHECK(response(63, 63) > 0);
    CHECK(response(63, 63) < response(0, 63));
    CHECK(response(0, 0) > response(0, 63));
    bool linearChannels = true;
    float maximumEnergy = 0;
    for (size_t offset = 0; offset < pixels.size(); offset += 4)
    {
        linearChannels = linearChannels && pixels[offset + 2] == 0 && pixels[offset + 3] == 255;
        maximumEnergy = std::max(maximumEnergy, (0.04f * pixels[offset] + pixels[offset + 1]) / 255.0f);
    }
    CHECK(linearChannels);
    CHECK(maximumEnergy <= 1.0f);
}
