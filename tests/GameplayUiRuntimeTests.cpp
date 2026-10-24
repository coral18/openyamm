#include "engine/AssetFileSystem.h"
#include "engine/ImageAssetLoader.h"
#include "game/data/GameDataLoader.h"
#include "game/ui/GameplayUiRuntime.h"
#include "game/ui/GameplayUiController.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <cstdlib>

TEST_CASE("dimensional travel uses original artwork and matching continent hover regions")
{
    using namespace OpenYAMM;
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    Engine::AssetFileSystem assets;
    const char *pAssetsRoot = std::getenv("OPENYAMM_REGRESSION_ASSETS_ROOT");
    REQUIRE(assets.initialize(sourceRoot, pAssetsRoot != nullptr ? std::filesystem::path(pAssetsRoot)
        : sourceRoot / "assets_dev", Engine::AssetScaleTier::X1));
    Game::GameDataLoader loader;
    REQUIRE(loader.loadForHeadlessGameplay(assets));
    Game::GameDataRepository data;
    data.bind(loader);
    Game::GameplayUiRuntime ui;
    ui.bindDataRepository(&data);
    CHECK_FALSE(ui.ensureDimensionDoorDestinationsLoaded(1, false));
    REQUIRE(ui.ensureDimensionDoorDestinationsLoaded(1, true));
    REQUIRE_EQ(ui.townPortalDestinations().size(), 3);

    Engine::BinaryAssetCache images;
    const std::optional<Engine::ImagePixelsBgra> background =
        Engine::loadImageAssetPixelsBgra(assets, ui.townPortalBackgroundTextureName() + ".bmp", images);
    REQUIRE(background.has_value());
    CHECK_EQ(background->width, 640);
    CHECK_EQ(background->height, 480);
    CHECK(background->assetScaleTier == Engine::AssetScaleTier::X1);

    struct ExpectedDestination
    {
        const char *pLabel;
        const char *pMapName;
        int32_t hitX;
        uint32_t hitWidth;
        int32_t iconX;
        int32_t iconY;
        int iconWidth;
        int iconHeight;
    };
    const std::array<ExpectedDestination, 3> expected = {{
        {"Jadame", "out01.odm", 0, 213, 0, 46, 450, 360},
        {"Enroth", "oute3.odm", 213, 214, 180, 0, 270, 400},
        {"Antagarich", "7out01.odm", 427, 213, 180, 46, 460, 360},
    }};
    for (const ExpectedDestination &entry : expected)
    {
        const auto destination = std::find_if(ui.townPortalDestinations().begin(), ui.townPortalDestinations().end(),
            [&entry](const Game::GameplayTownPortalDestination &candidate) { return candidate.label == entry.pLabel; });
        REQUIRE(destination != ui.townPortalDestinations().end());
        CHECK_EQ(destination->mapName, entry.pMapName);
        CHECK_EQ(destination->hitX, entry.hitX);
        CHECK_EQ(destination->hitWidth, entry.hitWidth);
        CHECK_EQ(destination->hitY, 0);
        CHECK_EQ(destination->hitHeight, 480);
        CHECK_EQ(destination->iconX, entry.iconX);
        CHECK_EQ(destination->iconY, entry.iconY);
        CHECK_EQ(destination->iconWidth, 0);
        CHECK_EQ(destination->iconHeight, 0);
        const std::optional<Engine::ImagePixelsBgra> icon =
            Engine::loadImageAssetPixelsBgra(assets, destination->iconTextureName + ".bmp", images);
        REQUIRE(icon.has_value());
        CHECK_EQ(icon->width, entry.iconWidth);
        CHECK_EQ(icon->height, entry.iconHeight);
        CHECK(icon->assetScaleTier == Engine::AssetScaleTier::X1);
    }
}

TEST_CASE("gate selector follows shared progression choices and refreshes its party-dependent cache")
{
    using namespace OpenYAMM;
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const char *pAssetsRoot = std::getenv("OPENYAMM_REGRESSION_ASSETS_ROOT");
    Engine::AssetFileSystem assets;
    REQUIRE(assets.initialize(sourceRoot, pAssetsRoot != nullptr ? std::filesystem::path(pAssetsRoot)
        : sourceRoot / "assets_dev", Engine::AssetScaleTier::X1));
    Game::GameDataLoader loader;
    REQUIRE(loader.loadForHeadlessGameplay(assets));
    Game::GameDataRepository data;
    data.bind(loader);
    Game::GameplayUiRuntime ui;
    ui.bindDataRepository(&data);
    const auto &gates = data.worldGateCatalog();
    REQUIRE(gates.defaultNetwork());
    Game::Party party;
    for (bool progressed : {false, true, false})
    {
        party.setQuestBit(4, progressed);
        party.setQuestBit(519, progressed);
        const auto choices = gates.choices(*gates.defaultNetwork(), &party);
        REQUIRE(ui.ensureDimensionDoorDestinationsLoaded(1, true, &party));
        const auto &rendered = ui.townPortalDestinations();
        REQUIRE(rendered.size() == choices.size());
        for (size_t index = 0; index < choices.size(); ++index)
        {
            const auto &landing = *choices[index].pLanding;
            CHECK(rendered[index].id == choices[index].pDestination->id);
            CHECK(rendered[index].mapName == landing.mapFileName);
            CHECK(rendered[index].x == landing.x);
            CHECK(rendered[index].y == landing.y);
            CHECK(rendered[index].z == landing.z);
            CHECK(rendered[index].directionDegrees == landing.directionDegrees);
            CHECK(rendered[index].useMapStartPosition == landing.useMapStartPosition);
            CHECK(rendered[index].unlockQBitId == 0);
        }
    }
    CHECK_FALSE(ui.ensureDimensionDoorDestinationsLoaded(1, true, &party, "missing.network"));
    CHECK(ui.townPortalDestinations().empty());
    REQUIRE(ui.ensureDimensionDoorDestinationsLoaded(1, true, &party));
    CHECK_FALSE(ui.ensureDimensionDoorDestinationsLoaded(1, false, &party));
    CHECK(ui.townPortalDestinations().empty());
}

TEST_CASE("gate selector replacement and cancellation cannot reuse a previous physical gateway origin")
{
    using namespace OpenYAMM::Game;
    GameplayUiController ui;
    for (const auto mode : {GameplayUiController::UtilitySpellOverlayMode::DimensionDoor,
        GameplayUiController::UtilitySpellOverlayMode::TownPortal})
    {
        ui.openUtilitySpellOverlay(GameplayUiController::UtilitySpellOverlayMode::DimensionDoor, 31, 0);
        auto &source = ui.utilitySpellOverlay();
        source.gateNetworkId = "merge.dimension_doors";
        source.gateSourceMapId = "world.mm7.map.7d29";
        source.gateSourceMapFileName = "7d29.blv";
        source.gateSourceEventId = 65031;
        ui.openUtilitySpellOverlay(mode, 31, 0);
        CHECK(ui.utilitySpellOverlay().gateNetworkId.empty());
        CHECK(ui.utilitySpellOverlay().gateSourceMapId.empty());
        CHECK(ui.utilitySpellOverlay().gateSourceMapFileName.empty());
        CHECK(ui.utilitySpellOverlay().gateSourceEventId == 0);
        ui.closeUtilitySpellOverlay();
        CHECK_FALSE(ui.utilitySpellOverlay().active);
    }
}
