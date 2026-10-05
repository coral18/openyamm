#include <doctest/doctest.h>

#include "engine/AssetFileSystem.h"
#include "game/data/GameDataLoader.h"
#include "game/outdoor/OutdoorWorldRuntime.h"
#include "game/ui/GameplayOverlayTypes.h"

#include <filesystem>

using namespace OpenYAMM::Game;

TEST_CASE("ironfist rampart lighting probes cannot cross the walkable floor")
{
    OpenYAMM::Engine::AssetFileSystem assets;
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    REQUIRE(assets.initialize(sourceRoot, sourceRoot / "assets_dev", OpenYAMM::Engine::AssetScaleTier::X1, "mm6"));
    GameDataLoader data;
    REQUIRE(data.loadForHeadlessGameplay(assets));
    REQUIRE(data.loadMapByFileNameForHeadlessGameplay(assets, "outd3.odm"));
    const MapAssetInfo &loaded = *data.getSelectedMap();
    REQUIRE(loaded.outdoorMapData);
    REQUIRE(loaded.outdoorMapData->lightingData);

    OutdoorWorldRuntime world;
    world.initialize(loaded.map, data.getMonsterTable(), data.getMonsterProjectileTable(), data.getObjectTable(),
        data.getSpellTable(), data.getItemTable(), nullptr, nullptr, data.getStandardItemEnchantTable(),
        data.getSpecialItemEnchantTable(), &data.getChestTable(), loaded.outdoorMapData,
        std::nullopt, std::nullopt, std::nullopt);

    // The reported guard's mid-body sample is above the rampart; the nearest dark probe is below it.
    const bx::Vec3 guard = {15519.0f, 11825.0f, 2716.05f};
    const bx::Vec3 belowFloor = {15488.0f, 11776.0f, 2464.0f};
    CHECK(world.hasClearOutdoorLineOfSight(guard, belowFloor));
    CHECK_FALSE(world.hasClearOutdoorLineOfSight(guard, belowFloor, true));
    const std::optional<OutdoorLightingData::Probe> probe = loaded.outdoorMapData->lightingData->sampleProbe(
        {guard.x, guard.y, guard.z}, [&](const std::array<float, 3> &point)
        {
            return world.hasClearOutdoorLineOfSight(guard, {point[0], point[1], point[2]}, true);
        });
    REQUIRE(probe);
    for (size_t channel = 0; channel < 3; ++channel)
    {
        CHECK_GT(probe->sky[channel], 0.1f);
    }
}
