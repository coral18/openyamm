// Existing Atlas regression cases exercised against the target upstream headers and geometry implementation.
#include "doctest/doctest.h"
#include "game/FaceEnums.h"
#include "game/indoor/IndoorMapData.h"
#include "game/ui/GameplayMinimapTransform.h"
#include "game/world/WorldFloorPlan.h"
#include "game/world/WorldMapExploration.h"

#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

using namespace OpenYAMM::Game;

TEST_CASE("Atlas port: world database dungeon plan respects reveal bits and hidden faces")
{
    IndoorMapData map;
    map.vertices = {{0, 0, 0}, {100, 0, 0}, {100, 100, 0}, {0, 100, 0}};
    map.faces.resize(2);
    map.outlines = {{0, 1, 0, 1}, {1, 2, 0, 1}, {2, 3, 0, 1}, {3, 0, 0, 1}, {99, 0, 0, 1}};
    WorldFloorPlanKnowledge knowledge;
    CHECK(buildWorldFloorPlan(map, knowledge, false).pixelsBgra.empty());
    CHECK(buildWorldFloorPlan(map, knowledge, true).outlineCount == 4);
    knowledge.visibleOutlines = {0x80};
    auto explored = buildWorldFloorPlan(map, knowledge, false);
    CHECK(explored.outlineCount == 1);
    CHECK(explored.pixelsBgra.size() == size_t(explored.width) * explored.height * 4);
    knowledge.visibleOutlines = {0x01}; // Only invalid/out-of-range outline bit, not the first outline.
    CHECK(buildWorldFloorPlan(map, knowledge, false).outlineCount == 0);
    // SeenByParty also reveals adjoining outlines.
    knowledge.faceAttributes = {faceAttributeBit(FaceAttribute::SeenByParty), 0};
    CHECK(buildWorldFloorPlan(map, knowledge, false).outlineCount == 4);
    knowledge.invisibleFaces.insert(1);
    CHECK(buildWorldFloorPlan(map, knowledge, false).outlineCount == 0);
    CHECK(buildWorldFloorPlan(map, knowledge, true).outlineCount == 4);
    // Authored invisible geometry is hidden in Guide, too.
    map.faces[1].attributes = faceAttributeBit(FaceAttribute::Invisible);
    CHECK(buildWorldFloorPlan(map, knowledge, true).outlineCount == 0);
    map.vertices[1].x = std::numeric_limits<int>::max();
    map.vertices[0].x = std::numeric_limits<int>::min();
    map.faces[1].attributes = 0;
    CHECK(buildWorldFloorPlan(map, {}, true).outlineCount == 4);
}

TEST_CASE("Atlas port: dungeon height bands handle negative heights and reject nonfinite layer selections")
{
    CHECK(worldFloorPlanLayerElevation(-1) == -512);
    CHECK(worldFloorPlanLayerElevation(0) == 0);
    CHECK_FALSE(worldFloorPlanLayerElevation(INT32_MIN));
    CHECK_FALSE(worldFloorPlanLayerElevation(INT32_MAX));
    for (int32_t height : {INT32_MIN, -768, -256, 0, 256, INT32_MAX})
    {
        const auto layer = worldFloorPlanLayer(float(height));
        REQUIRE(layer.has_value());
        const auto savedHeight = worldFloorPlanLayerElevation(*layer);
        REQUIRE(savedHeight.has_value());
        CHECK(worldFloorPlanLayer(float(*savedHeight)) == layer);
    }
    CHECK(worldFloorPlanLayer(0) == 0);
    CHECK(worldFloorPlanLayer(255.9f) == 0);
    CHECK(worldFloorPlanLayer(256) == 1);
    CHECK(worldFloorPlanLayer(-256) == 0);
    CHECK(worldFloorPlanLayer(-256.1f) == -1);
    CHECK(worldFloorPlanLayer(-768) == -1);
    CHECK(worldFloorPlanLayerMatches(-1, -300));
    CHECK_FALSE(worldFloorPlanLayerMatches(0, -300));
    CHECK_FALSE(worldFloorPlanLayer(std::numeric_limits<float>::infinity()));
    CHECK_FALSE(worldFloorPlanLayer(std::numeric_limits<float>::max()));
    CHECK_FALSE(worldFloorPlanLayerMatches(0, std::numeric_limits<float>::quiet_NaN()));
}

TEST_CASE("Atlas port: dungeon height selection keeps a stable known projection and hides undiscovered layers")
{
    IndoorMapData map;
    map.vertices = {{0, 0, 0}, {100, 0, 0}, {0, 100, 1024}, {100, 100, 1024},
        {100000, 100000, 4096}, {100100, 100000, 4096}};
    map.faces.resize(2);
    map.outlines = {{0, 1, 0, 1, 0}, {2, 3, 0, 1, 1024}, {4, 5, 0, 1, 4096}};
    WorldFloorPlanKnowledge knowledge;
    knowledge.visibleOutlines = {0xc0};
    const auto all = buildWorldFloorPlan(map, knowledge, false);
    const auto lower = buildWorldFloorPlan(map, knowledge, false, {}, 0);
    const auto upper = buildWorldFloorPlan(map, knowledge, false, {}, 2);
    const auto hidden = buildWorldFloorPlan(map, knowledge, false, {}, 8);
    CHECK(all.layers == std::vector<WorldFloorPlanLayer>{0, 2});
    CHECK(lower.layers == all.layers);
    CHECK(upper.layers == all.layers);
    CHECK(all.outlineCount == 2);
    CHECK(lower.outlineCount == 1);
    CHECK(upper.outlineCount == 1);
    CHECK(hidden.outlineCount == 0);
    CHECK(lower.pixelsBgra != upper.pixelsBgra);
    CHECK(lower.worldToUv(50, 50) == upper.worldToUv(50, 50));
    CHECK(lower.worldToUv(50, 50) == hidden.worldToUv(50, 50));
    CHECK(lower.centerX == 50);
    CHECK(lower.centerY == 50);
    CHECK(knowledge.visibleOutlines == std::vector<uint8_t>{0xc0});
    CHECK(buildWorldFloorPlan(map, knowledge, true).layers == std::vector<WorldFloorPlanLayer>{0, 2, 8});
    knowledge.invisibleFaces.insert(1);
    CHECK(buildWorldFloorPlan(map, knowledge, false).layers.empty());
}

TEST_CASE("Atlas port: overlapping known floors refine their own band without leaking hidden floors")
{
    IndoorMapData map;
    map.vertices = {{0, 0, -240}, {100, 0, -240}, {100, 100, -240}, {0, 100, -240},
        {25, 0, 0}, {125, 0, 0}, {125, 100, 0}, {25, 100, 0}};
    map.faces.resize(2);
    for (uint16_t i = 0; i < 2; ++i)
    {
        IndoorFace &face = map.faces[i];
        face.planeNormal = {{0, 0, 65536}};
        face.vertexIndices = {uint16_t(i * 4), uint16_t(i * 4 + 1), uint16_t(i * 4 + 2), uint16_t(i * 4 + 3)};
        for (uint16_t edge = 0; edge < 4; ++edge)
            map.outlines.push_back({uint16_t(i * 4 + edge), uint16_t(i * 4 + (edge + 1) % 4),
                i, i, int16_t(i ? 0 : -240)});
    }
    WorldFloorPlanKnowledge knowledge;
    knowledge.visibleOutlines = {0xf0};
    const auto lowerOnly = buildWorldFloorPlan(map, knowledge, false);
    CHECK(lowerOnly.refinedBands.empty());
    CHECK(lowerOnly.layers == std::vector<WorldFloorPlanLayer>{0});
    const auto guide = buildWorldFloorPlan(map, knowledge, true);
    CHECK(guide.refinedBands.contains(0));
    CHECK(guide.layers == std::vector<WorldFloorPlanLayer>{{0, 1}, {0, 3}});
    CHECK(buildWorldFloorPlan(map, knowledge, false).pixelsBgra == lowerOnly.pixelsBgra);
    CHECK(buildWorldFloorPlan(map, knowledge, false).refinedBands.empty());
    CHECK(knowledge.visibleOutlines == std::vector<uint8_t>{0xf0});
    CHECK(knowledge.faceAttributes.empty());

    knowledge.visibleOutlines = {0xff};
    const auto all = buildWorldFloorPlan(map, knowledge, false);
    const auto lowerLayer = all.layerAtHeight(-240), upperLayer = all.layerAtHeight(0);
    REQUIRE(lowerLayer.has_value());
    REQUIRE(upperLayer.has_value());
    CHECK(lowerLayer != upperLayer);
    CHECK(worldFloorPlanLayerMatches(lowerLayer, -240));
    CHECK_FALSE(worldFloorPlanLayerMatches(lowerLayer, 0));
    const auto lower = buildWorldFloorPlan(map, knowledge, false, {}, lowerLayer);
    const auto upper = buildWorldFloorPlan(map, knowledge, false, {}, upperLayer);
    CHECK(lower.outlineCount == 4);
    CHECK(upper.outlineCount == 4);
    CHECK(lower.pixelsBgra != upper.pixelsBgra);
    CHECK(lower.worldToUv(50, 50) == upper.worldToUv(50, 50));
    for (WorldFloorPlanLayer layer : all.layers)
    {
        const auto elevation = worldFloorPlanLayerElevation(layer);
        REQUIRE(elevation.has_value());
        CHECK(all.layerAtHeight(float(*elevation)) == layer);
    }
    CHECK(all.layerAtHeight(1024) == WorldFloorPlanLayer{2});
    CHECK(worldFloorPlanLayerElevation(WorldFloorPlanLayer{0, 1}) == -192);
    CHECK(worldFloorPlanLayerElevation(WorldFloorPlanLayer{0, 3}) == 64);
    CHECK(worldFloorPlanLayerForHeight(-768, true) == WorldFloorPlanLayer{-1, 1});
    CHECK(worldFloorPlanLayerForHeight(-384, true) == WorldFloorPlanLayer{-1, 4});
    CHECK_FALSE(worldFloorPlanLayerForHeight(std::numeric_limits<float>::infinity(), true));
    CHECK_FALSE(worldFloorPlanLayerElevation(WorldFloorPlanLayer{0, 5}));
    CHECK_THROWS_AS(buildWorldFloorPlan(map, knowledge, false, {}, WorldFloorPlanLayer{0, 5}), std::invalid_argument);
    map.faces[1].attributes = faceAttributeBit(FaceAttribute::Untouchable);
    CHECK(buildWorldFloorPlan(map, knowledge, false).refinedBands.empty());
    knowledge.faceAttributes = {0, 0}; // A live script can restore a previously untouchable floor.
    CHECK(buildWorldFloorPlan(map, knowledge, false).refinedBands.contains(0));
    knowledge.invisibleFaces.insert(1);
    CHECK(buildWorldFloorPlan(map, knowledge, false).refinedBands.empty());
    map.faces[1].attributes = faceAttributeBit(FaceAttribute::Invisible);
    CHECK(buildWorldFloorPlan(map, knowledge, true).refinedBands.empty());
}

TEST_CASE("Atlas port: known walkable ramps remain continuous across dungeon height slices")
{
    IndoorMapData map;
    map.vertices = {{0, 0, -240}, {100, 0, -240}, {100, 100, -240}, {0, 100, -240},
        {0, 0, 0}, {100, 0, 0}, {100, 100, 0}, {0, 100, 0},
        {200, 0, -240}, {300, 0, -240}, {300, 400, 0}, {200, 400, 0}};
    map.faces.resize(3);
    for (uint16_t i = 0; i < 2; ++i)
    {
        map.faces[i].facetType = 3;
        map.faces[i].planeNormal = {{0, 0, 65536}};
        map.faces[i].vertexIndices = {uint16_t(i * 4), uint16_t(i * 4 + 1),
            uint16_t(i * 4 + 2), uint16_t(i * 4 + 3)};
        for (uint16_t edge = 0; edge < 4; ++edge)
            map.outlines.push_back({uint16_t(i * 4 + edge), uint16_t(i * 4 + (edge + 1) % 4),
                i, i, int16_t(i ? 0 : -240)});
    }
    map.faces[2].facetType = 4;
    map.faces[2].planeNormal = {{0, -39322, 65536}};
    map.faces[2].vertexIndices = {8, 9, 10, 11};
    map.outlines.push_back({8, 11, 2, 2, -120});
    WorldFloorPlanKnowledge known;
    known.visibleOutlines = {0xff, 0x80};
    const auto all = buildWorldFloorPlan(map, known, false);
    REQUIRE(all.refinedBands.contains(0));
    const auto lower = buildWorldFloorPlan(map, known, false, {}, WorldFloorPlanLayer{0, 1});
    const auto middle = buildWorldFloorPlan(map, known, false, {}, WorldFloorPlanLayer{0, 2});
    CHECK(lower.outlineCount == 5); // Four lower-room edges plus the lower portion of the known ramp.
    CHECK(middle.outlineCount == 1);
    CHECK(lower.worldToUv(200, 80) == middle.worldToUv(200, 80));
    const auto drawn = [](const WorldFloorPlanRaster &plan, float x, float y)
    {
        const auto uv = plan.worldToUv(x, y);
        const int px = int(std::lround(uv.first * plan.width));
        const int py = int(std::lround(uv.second * plan.height));
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (px + dx >= 0 && px + dx < plan.width && py + dy >= 0 && py + dy < plan.height
                    && plan.pixelsBgra[(size_t(py + dy) * plan.width + px + dx) * 4] == 190) return true;
        return false;
    };
    CHECK(drawn(lower, 200, 80));
    CHECK_FALSE(drawn(lower, 200, 320));
    CHECK_FALSE(drawn(middle, 200, 80));
    CHECK(drawn(middle, 200, 320));
    known.visibleOutlines[1] = 0;
    const auto undiscovered = buildWorldFloorPlan(map, known, false);
    CHECK(undiscovered.outlineCount == 8);
    CHECK(buildWorldFloorPlan(map, known, true).outlineCount == 9);
    CHECK(buildWorldFloorPlan(map, known, false).pixelsBgra == undiscovered.pixelsBgra);
    CHECK(known.visibleOutlines == std::vector<uint8_t>{0xff, 0});
    known.visibleOutlines[1] = 0x80;
    map.faces[2].facetType = 1; // A sloped wall edge retains its authored height, not a fictitious ramp.
    CHECK(buildWorldFloorPlan(map, known, false, {}, WorldFloorPlanLayer{0, 1}).outlineCount == 4);
    CHECK(drawn(buildWorldFloorPlan(map, known, false, {}, WorldFloorPlanLayer{0, 2}), 200, 80));
    map.faces[2].facetType = 4;
    map.faces[2].attributes = faceAttributeBit(FaceAttribute::Invisible);
    CHECK(buildWorldFloorPlan(map, known, true).outlineCount == 8);
}

TEST_CASE("Atlas port: dungeon height refinement detects crossing floors and ignores separate rooms and touching edges")
{
    IndoorMapData map;
    map.vertices = {{0, 0, -240}, {1000, 0, -240}, {1000, 80, -240}, {0, 80, -240},
        {750, -1500, 0}, {830, -1500, 0}, {830, 500, 0}, {750, 500, 0}};
    map.faces.resize(2);
    for (uint16_t i = 0; i < 2; ++i)
    {
        map.faces[i].planeNormal = {{0, 0, 65536}};
        map.faces[i].vertexIndices = {uint16_t(i * 4), uint16_t(i * 4 + 1),
            uint16_t(i * 4 + 2), uint16_t(i * 4 + 3)};
        map.outlines.push_back({uint16_t(i * 4), uint16_t(i * 4 + 1), i, i, int16_t(i ? 0 : -240)});
    }
    CHECK(buildWorldFloorPlan(map, {}, true).refinedBands.contains(0));
    for (size_t i = 4; i < 8; ++i) map.vertices[i].x += 250;
    CHECK(buildWorldFloorPlan(map, {}, true).refinedBands.empty()); // Only a shared edge at x=1000.
    for (size_t i = 4; i < 8; ++i) map.vertices[i].x += 1000;
    CHECK(buildWorldFloorPlan(map, {}, true).refinedBands.empty());
}

TEST_CASE("Atlas port: world database outdoor fog shares saved exploration without revealing empty maps")
{
    GameplayMinimapState projection;
    WorldRegionMapKnowledge knowledge;
    const auto alpha = [](const WorldRegionFog &fog, int x, int y)
    { return fog.pixelsBgra[size_t(y * WorldRegionFog::Size + x) * 4 + 3]; };
    auto hidden = buildWorldRegionFog(knowledge, projection);
    REQUIRE(hidden.pixelsBgra.size() == WorldRegionFog::Size * WorldRegionFog::Size * 4);
    CHECK(alpha(hidden, 0, 0) == 255);
    CHECK(alpha(hidden, 87, 87) == 255);
    knowledge.fullyRevealedCells = {0x80};
    knowledge.partiallyRevealedCells = {0xc0};
    auto fog = buildWorldRegionFog(knowledge, projection);
    CHECK(alpha(fog, 0, 0) == 0); // Fully revealed wins over partially revealed.
    CHECK(alpha(fog, 1, 0) == 144);
    CHECK(alpha(fog, 2, 0) == 255);
    CHECK(knowledge.fullyRevealedCells == std::vector<uint8_t>{0x80});
    projection.flipU = true;
    auto flipped = buildWorldRegionFog(knowledge, projection);
    CHECK(alpha(flipped, 87, 0) == 0);
    CHECK(alpha(flipped, 86, 0) == 144);
    CHECK(alpha(flipped, 0, 0) == 255);
    projection.flipU = false;
    projection.flipV = false;
    CHECK(alpha(buildWorldRegionFog(knowledge, projection), 0, 87) == 0);
    projection.revealEntireMap = true; // Atlas still requires actual discoveries even if art allows a full map.
    CHECK(alpha(buildWorldRegionFog({}, projection), 0, 0) == 255);
}

TEST_CASE("Atlas port: world database party marker follows minimap direction and dungeon projection")
{
    constexpr float pi = std::numbers::pi_v<float>;
    GameplayMinimapState heading;
    CHECK(gameplayMinimapDisplayYaw(heading, 0) == doctest::Approx(0));
    CHECK(gameplayMinimapDisplayYaw(heading, pi / 2) == doctest::Approx(pi / 2));
    CHECK(std::abs(gameplayMinimapDisplayYaw(heading, pi)) == doctest::Approx(pi));
    CHECK(gameplayMinimapDisplayYaw(heading, -pi / 2) == doctest::Approx(-pi / 2));
    heading.flipU = true;
    CHECK(std::abs(gameplayMinimapDisplayYaw(heading, 0)) == doctest::Approx(pi));
    heading.flipU = false;
    heading.flipV = false;
    CHECK(gameplayMinimapDisplayYaw(heading, pi / 2) == doctest::Approx(-pi / 2));
    const auto northWest = gameplayMinimapWorldToUv({}, -32768, 32768);
    CHECK(northWest.x == 0);
    CHECK(northWest.y == 0);
    IndoorMapData map;
    map.vertices = {{-100, -100, 0}, {100, 100, 0}};
    map.faces.resize(2);
    map.outlines = {{0, 1, 0, 1}};
    const auto plan = buildWorldFloorPlan(map, {}, true);
    const auto center = plan.worldToUv(0, 0);
    CHECK(center.first == doctest::Approx(0.5));
    CHECK(center.second == doctest::Approx(0.5));
    const auto northEast = plan.worldToUv(100, 100);
    CHECK(northEast.first > center.first);
    CHECK(northEast.second < center.second);
}
