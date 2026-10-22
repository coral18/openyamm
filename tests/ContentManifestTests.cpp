#include "engine/AssetFileSystem.h"
#include "engine/AssetScaleTier.h"
#include "game/content/ContentManifest.h"
#include "game/content/ContentTableComposer.h"
#include "game/data/GameDataLoader.h"
#include "game/maps/MapAssetLoader.h"
#include "game/world/WorldDatabase.h"
#include "engine/TextTable.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace
{
std::filesystem::path makeTemporaryRoot()
{
    const uint64_t tickCount = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() / ("openyamm_manifest_" + std::to_string(tickCount));
}

void writeTextFile(const std::filesystem::path &path, const std::string &contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << contents;
}
}

TEST_CASE("typed metadata overlays precede all map and global callback replacements")
{
    using namespace OpenYAMM::Game;
    const std::filesystem::path temporaryRoot = makeTemporaryRoot();
    const std::filesystem::path assets = temporaryRoot / "assets_dev";
    const std::filesystem::path support = assets / "engine/scripts/common/event_support.lua";
    std::filesystem::create_directories(support.parent_path());
    std::filesystem::copy_file(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua", support);
    const std::filesystem::path mapScripts = assets / "worlds/mm7/events/maps";
    const std::filesystem::path globals = assets / "engine/events";
    writeTextFile(mapScripts / "metadata_fixture.lua", "RegisterEvent(10, '', function() end)");
    writeTextFile(mapScripts / "metadata_fixture_00_item_rewards.lua", "SetMapItemRewards(10, {42})");
    writeTextFile(mapScripts / "metadata_fixture_00_npc_locations.lua", "SetMapNpcLocations(10, {{42, 11}})");
    writeTextFile(mapScripts / "metadata_fixture_00_autonote_refs.lua", "SetMapAutonoteRefs(10, {42})");
    writeTextFile(globals / "Global.lua", "RegisterGlobalEvent(20, '', function() end)");
    writeTextFile(globals / "Global_00_item_rewards.lua", "SetGlobalItemRewards(20, {43})");
    writeTextFile(globals / "Global_00_npc_locations.lua", "SetGlobalNpcLocations(20, {{43, 12}})");
    writeTextFile(globals / "Global_00_autonote_refs.lua", "SetGlobalAutonoteRefs(20, {43})");
    {
        OpenYAMM::Engine::AssetFileSystem fs;
        REQUIRE(fs.initialize(temporaryRoot, assets, OpenYAMM::Engine::AssetScaleTier::X1, "mm7"));
        for (const std::string suffix : {".fixup.lua", "_000_custom.lua", "_zz_custom.lua"})
        {
            for (bool redeclared : {false, true})
            {
                CAPTURE(suffix);
                CAPTURE(redeclared);
                writeTextFile(mapScripts / ("metadata_fixture" + suffix),
                    "ReplaceMapEvent(10, '', function() end)\n"
                    + std::string(redeclared ? "SetMapItemRewards(10, {44})\nSetMapNpcLocations(10, {{44, 13}})\n"
                        "SetMapAutonoteRefs(10, {44})" : ""));
                writeTextFile(globals / ("Global" + suffix),
                    "ReplaceGlobalEvent(20, '', function() end)\n"
                    + std::string(redeclared
                        ? "SetGlobalItemRewards(20, {45})\nSetGlobalNpcLocations(20, {{45, 14}})\n"
                          "SetGlobalAutonoteRefs(20, {45})" : ""));
                fs.refreshLookupCache();
                MapAssetInfo map;
                map.map.fileName = "metadata_fixture.blv";
                map.map.worldId = "mm7";
                std::string error;
                REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fs, map, error), error);
                REQUIRE(map.localEventProgram.has_value());
                REQUIRE(map.globalEventProgram.has_value());
                if (redeclared)
                {
                    REQUIRE(map.localEventProgram->itemRewards().contains(10));
                    REQUIRE(map.globalEventProgram->itemRewards().contains(20));
                    CHECK(map.localEventProgram->itemRewards().at(10) == std::vector<uint32_t>{44});
                    CHECK(map.globalEventProgram->itemRewards().at(20) == std::vector<uint32_t>{45});
                    CHECK(map.localEventProgram->autonoteReferences().at(10) == std::vector<uint32_t>{44});
                    CHECK(map.globalEventProgram->autonoteReferences().at(20) == std::vector<uint32_t>{45});
                    CHECK(map.localEventProgram->npcLocations().at(10)
                        == std::vector<ScriptedEventProgram::NpcLocation>{{44, 13}});
                    CHECK(map.globalEventProgram->npcLocations().at(20)
                        == std::vector<ScriptedEventProgram::NpcLocation>{{45, 14}});
                }
                else
                {
                    CHECK(map.localEventProgram->itemRewards().empty());
                    CHECK(map.globalEventProgram->itemRewards().empty());
                    CHECK(map.localEventProgram->autonoteReferences().empty());
                    CHECK(map.globalEventProgram->autonoteReferences().empty());
                    CHECK(map.localEventProgram->npcLocations().empty());
                    CHECK(map.globalEventProgram->npcLocations().empty());
                }
            }
            std::filesystem::remove(mapScripts / ("metadata_fixture" + suffix));
            std::filesystem::remove(globals / ("Global" + suffix));
        }
    }
    std::filesystem::remove_all(temporaryRoot);
}

TEST_CASE("content manifests default when package files are absent")
{
    const std::filesystem::path temporaryRoot = makeTemporaryRoot();
    const std::filesystem::path assetRoot = temporaryRoot / "assets_dev";
    std::filesystem::create_directories(assetRoot);

    {
        OpenYAMM::Engine::AssetFileSystem assetFileSystem;
        REQUIRE(assetFileSystem.initialize(
            temporaryRoot,
            assetRoot,
            OpenYAMM::Engine::AssetScaleTier::X1,
            "mm8"));

        std::string error;
        const OpenYAMM::Game::WorldManifest world =
            OpenYAMM::Game::loadActiveWorldManifestOrDefault(assetFileSystem, "mm8", error);
        CHECK(error.empty());
        CHECK_FALSE(world.loadedFromFile);
        CHECK_EQ(world.id, "mm8");
        CHECK_EQ(world.start.mapFileName, "out01.odm");
        CHECK_FALSE(world.partyMovement.declared);
        CHECK_FALSE(world.actorAwareness.declared);
    }

    std::filesystem::remove_all(temporaryRoot);
}

TEST_CASE("content manifests parse mounted world definition")
{
    const std::filesystem::path temporaryRoot = makeTemporaryRoot();
    const std::filesystem::path assetRoot = temporaryRoot / "assets_dev";

    writeTextFile(
        assetRoot / "worlds" / "custom" / "world.yml",
        "id: custom\n"
        "name: Custom World\n"
        "sourceGame: custom\n"
        "qbits:\n"
        "  begin: 90000\n"
        "  end: 99999\n"
        "  mapping: raw_mm9_key_plus_90000\n"
        "partyMovement:\n"
        "  eyeHeight: 152\n"
        "  collisionRadius: 38\n"
        "  collisionHeight: 169\n"
        "  maxStepHeight: 40.96\n"
        "actorAwareness:\n"
        "  partyEngagementRange: 4096\n"
        "start:\n"
        "  map: custom01.odm\n"
        "  introMovie: custom_intro\n");
    {
        OpenYAMM::Engine::AssetFileSystem assetFileSystem;
        REQUIRE(assetFileSystem.initialize(
            temporaryRoot,
            assetRoot,
            OpenYAMM::Engine::AssetScaleTier::X1,
            "custom"));

        std::string error;
        const OpenYAMM::Game::WorldManifest world =
            OpenYAMM::Game::loadActiveWorldManifestOrDefault(assetFileSystem, "custom", error);
        CHECK(error.empty());
        REQUIRE(world.loadedFromFile);
        CHECK_EQ(world.id, "custom");
        CHECK_EQ(world.name, "Custom World");
        CHECK_EQ(world.start.mapFileName, "custom01.odm");
        CHECK_EQ(world.start.introMovie, "custom_intro");
        REQUIRE(world.qbits.declared);
        CHECK_EQ(world.qbits.begin, 90000u);
        CHECK_EQ(world.qbits.end, 99999u);
        CHECK_EQ(world.qbits.mapping, "raw_mm9_key_plus_90000");
        REQUIRE(world.partyMovement.declared);
        CHECK_EQ(world.partyMovement.eyeHeight, 152.0f);
        CHECK_EQ(world.partyMovement.collisionRadius, 38.0f);
        CHECK_EQ(world.partyMovement.collisionHeight, 169.0f);
        CHECK_EQ(world.partyMovement.maxStepHeight, doctest::Approx(40.96f));
        REQUIRE(world.actorAwareness.declared);
        CHECK_EQ(world.actorAwareness.partyEngagementRange, 4096.0f);
    }

    std::filesystem::remove_all(temporaryRoot);
}

TEST_CASE("content manifests reject incomplete party movement profiles")
{
    const std::filesystem::path temporaryRoot = makeTemporaryRoot();
    const std::filesystem::path assetRoot = temporaryRoot / "assets_dev";

    writeTextFile(
        assetRoot / "worlds" / "invalid" / "world.yml",
        "id: invalid\n"
        "partyMovement:\n"
        "  eyeHeight: 152\n"
        "  collisionRadius: 38\n");

    {
        OpenYAMM::Engine::AssetFileSystem assetFileSystem;
        REQUIRE(assetFileSystem.initialize(
            temporaryRoot,
            assetRoot,
            OpenYAMM::Engine::AssetScaleTier::X1,
            "mm8"));

        std::string error;
        CHECK_FALSE(OpenYAMM::Game::loadWorldManifest(
            assetFileSystem,
            "worlds/invalid/world.yml",
            "invalid",
            error).has_value());
        CHECK(error.find("partyMovement") != std::string::npos);
    }

    std::filesystem::remove_all(temporaryRoot);
}

TEST_CASE("content manifests reject invalid actor awareness profiles")
{
    const std::filesystem::path temporaryRoot = makeTemporaryRoot();
    const std::filesystem::path assetRoot = temporaryRoot / "assets_dev";

    writeTextFile(
        assetRoot / "worlds" / "invalid" / "world.yml",
        "id: invalid\n"
        "actorAwareness:\n"
        "  partyEngagementRange: 0\n");

    {
        OpenYAMM::Engine::AssetFileSystem assetFileSystem;
        REQUIRE(assetFileSystem.initialize(
            temporaryRoot,
            assetRoot,
            OpenYAMM::Engine::AssetScaleTier::X1,
            "mm8"));

        std::string error;
        CHECK_FALSE(OpenYAMM::Game::loadWorldManifest(
            assetFileSystem,
            "worlds/invalid/world.yml",
            "invalid",
            error).has_value());
        CHECK(error.find("partyEngagementRange") != std::string::npos);
    }

    std::filesystem::remove_all(temporaryRoot);
}

TEST_CASE("content manifests parse stable item contribution contracts")
{
    const std::filesystem::path temporaryRoot = makeTemporaryRoot();
    const std::filesystem::path assetRoot = temporaryRoot / "assets_dev";

    writeTextFile(
        assetRoot / "worlds" / "mm9" / "world.yml",
        "id: mm9\n"
        "name: MM9\n"
        "sourceGame: mm9\n"
        "contentSchemaVersion: 3\n"
        "dependencies:\n"
        "  - engine\n"
        "idRanges:\n"
        "  items:\n"
        "    begin: 10000\n"
        "    end: 10999\n"
        "tableContributions:\n"
        "  - table: items\n"
        "    mode: append\n"
        "    path: data_tables/items.txt\n"
        "    sourceIdColumn: 32\n"
        "    canonicalIdColumn: 33\n");

    {
        OpenYAMM::Engine::AssetFileSystem assetFileSystem;
        REQUIRE(assetFileSystem.initialize(
            temporaryRoot,
            assetRoot,
            OpenYAMM::Engine::AssetScaleTier::X1,
            "mm8"));

        std::string error;
        const std::optional<OpenYAMM::Game::WorldManifest> world = OpenYAMM::Game::loadWorldManifest(
            assetFileSystem,
            "worlds/mm9/world.yml",
            "mm9",
            error);
        REQUIRE(world.has_value());
        CHECK(error.empty());
        CHECK_EQ(world->contentSchemaVersion, 3u);
        REQUIRE_EQ(world->dependencies.size(), 1);
        CHECK_EQ(world->dependencies[0], "engine");

        const OpenYAMM::Game::ContentIdRangeDefinition *pItemRange = world->findIdRange("ITEMS");
        REQUIRE(pItemRange != nullptr);
        CHECK_EQ(pItemRange->begin, 10000u);
        CHECK_EQ(pItemRange->end, 10999u);
        REQUIRE_EQ(world->tableContributions.size(), 1);
        CHECK(world->tableContributions[0].mode == OpenYAMM::Game::TableContributionMode::Append);
        CHECK_EQ(world->tableContributions[0].idDomain, "items");
        CHECK_EQ(world->tableContributions[0].path, "data_tables/items.txt");
        CHECK_EQ(world->tableContributions[0].sourceIdColumn, 32);
        CHECK_EQ(world->tableContributions[0].canonicalIdColumn, 33);
    }

    std::filesystem::remove_all(temporaryRoot);
}

TEST_CASE("content manifests reject escaping contribution paths")
{
    const std::filesystem::path temporaryRoot = makeTemporaryRoot();
    const std::filesystem::path assetRoot = temporaryRoot / "assets_dev";

    writeTextFile(
        assetRoot / "worlds" / "unsafe" / "world.yml",
        "id: unsafe\n"
        "idRanges:\n"
        "  items: {begin: 11000, end: 11999}\n"
        "tableContributions:\n"
        "  - table: items\n"
        "    mode: append\n"
        "    path: ../engine/data_tables/items.txt\n"
        "    sourceIdColumn: 32\n"
        "    canonicalIdColumn: 33\n");

    {
        OpenYAMM::Engine::AssetFileSystem assetFileSystem;
        REQUIRE(assetFileSystem.initialize(
            temporaryRoot,
            assetRoot,
            OpenYAMM::Engine::AssetScaleTier::X1,
            "mm8"));

        std::string error;
        CHECK_FALSE(OpenYAMM::Game::loadWorldManifest(
            assetFileSystem,
            "worlds/unsafe/world.yml",
            "unsafe",
            error).has_value());
        CHECK(error.find("relative path") != std::string::npos);
    }

    std::filesystem::remove_all(temporaryRoot);
}

TEST_CASE("item table contributions compose deterministically with normalized provenance")
{
    std::vector<std::vector<std::string>> baseRows = {{"1", "item001", "Base Sword"}};
    std::vector<std::string> mm9Row(45);
    mm9Row[0] = "10002";
    mm9Row[1] = "mm9_item_10002";
    mm9Row[2] = "Poor Sword";
    mm9Row[32] = "2";
    mm9Row[33] = "mm9:item/poor_sword";

    OpenYAMM::Game::ItemTableContributionRows mm9 = {};
    mm9.packageId = "mm9";
    mm9.idRange = OpenYAMM::Game::ContentIdRangeDefinition{"items", 10000, 10999};
    mm9.sourceIdColumn = 32;
    mm9.canonicalIdColumn = 33;
    mm9.rows.push_back(mm9Row);

    std::vector<std::vector<std::string>> composedRows;
    std::string error;
    REQUIRE(OpenYAMM::Game::composeItemTableRows(baseRows, {mm9}, composedRows, error));
    CHECK(error.empty());
    REQUIRE_EQ(composedRows.size(), 2);
    CHECK_EQ(composedRows[1][17], "mm9");
    CHECK_EQ(composedRows[1][18], "mm9:item/poor_sword");
    CHECK_EQ(composedRows[1][19], "2");
}

TEST_CASE("keyed table contributions can use a non-leading item id column")
{
    const std::vector<std::vector<std::string>> baseRows = {
        {"Id", "Item id", "Notes"},
        {"1", "84", "Leather Jerkin"},
        {"1", "109", "Helm"},
    };

    OpenYAMM::Game::KeyedTableContributionRows mm9 = {};
    mm9.packageId = "mm9";
    mm9.idRange = OpenYAMM::Game::ContentIdRangeDefinition{"items", 10000, 10999};
    mm9.idColumn = 1;
    mm9.rows = {
        {"Id", "Item id", "Notes"},
        {"1", "10086", "Arming Doublet"},
    };

    std::vector<std::vector<std::string>> composedRows;
    std::string error;
    REQUIRE(OpenYAMM::Game::composeKeyedAppendTableRows(baseRows, {mm9}, composedRows, error, 1));
    CHECK(error.empty());
    REQUIRE_EQ(composedRows.size(), 4);
    CHECK_EQ(composedRows.back()[1], "10086");
}

TEST_CASE("item table contributions reject id and range collisions")
{
    std::vector<std::string> firstRow(34);
    firstRow[0] = "10002";
    firstRow[32] = "2";
    firstRow[33] = "mm9:item/poor_sword";

    OpenYAMM::Game::ItemTableContributionRows first = {};
    first.packageId = "mm9";
    first.idRange = OpenYAMM::Game::ContentIdRangeDefinition{"items", 10000, 10999};
    first.sourceIdColumn = 32;
    first.canonicalIdColumn = 33;
    first.rows.push_back(firstRow);

    OpenYAMM::Game::ItemTableContributionRows overlapping = first;
    overlapping.packageId = "custom";
    overlapping.idRange = OpenYAMM::Game::ContentIdRangeDefinition{"items", 10900, 11900};
    overlapping.rows[0][33] = "custom:item/poor_sword";

    std::vector<std::vector<std::string>> composedRows;
    std::string error;
    CHECK_FALSE(OpenYAMM::Game::composeItemTableRows({}, {first, overlapping}, composedRows, error));
    CHECK(error.find("overlapping") != std::string::npos);

    first.idRange = OpenYAMM::Game::ContentIdRangeDefinition{"items", 10001, 10001};
    CHECK_FALSE(OpenYAMM::Game::composeItemTableRows({}, {first}, composedRows, error));
    CHECK(error.find("outside") != std::string::npos);
}

TEST_CASE("mounted MM6 MM7 MM8 NPC and journal declarations resolve against shared registries")
{
    using namespace OpenYAMM::Game;
    const std::filesystem::path root(OPENYAMM_SOURCE_DIR);
    const auto rows = [&](const std::string &name)
    {
        std::ifstream file(root / "assets_dev/engine/data_tables" / name, std::ios::binary);
        REQUIRE(file.is_open());
        const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        const auto table = OpenYAMM::Engine::TextTable::parseTabSeparated(text);
        REQUIRE(table);
        std::vector<std::vector<std::string>> result;
        for (size_t i = 0; i < table->getRowCount(); ++i) result.push_back(table->getRow(i));
        return result;
    };
    MapStats maps;
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    JournalAutonoteTable notes;
    REQUIRE(notes.loadFromRows(rows("english/autonote.txt")));
    REQUIRE(maps.loadFromRows(rows("map_stats.txt")));
    REQUIRE(houses.loadFromRows(rows("house_data.txt")));
    REQUIRE(npcs.loadNpcRows(rows("npc.txt")));
    for (const std::string world : {"mm6", "mm7", "mm8"})
    {
        CAPTURE(world);
        OpenYAMM::Engine::AssetFileSystem fs;
        REQUIRE(fs.initialize(root, root / "assets_dev", OpenYAMM::Engine::AssetScaleTier::X1, world));
        WorldDatabase database(maps, houses, npcs, teachers, world);
        size_t scripts = 0, declarations = 0, journalReferences = 0;
        for (const MapStatsEntry &map : maps.getEntries())
        {
            if (map.worldId != world) continue;
            const std::string stem = std::filesystem::path(map.fileName).stem().string();
            if (!fs.exists("worlds/" + world + "/events/maps/" + stem + ".lua")) continue;
            MapAssetInfo asset;
            asset.map = map;
            std::string error;
            REQUIRE_MESSAGE(GameDataLoader::loadEventPrograms(fs, asset, error), (map.fileName + ": " + error));
            ++scripts;
            if (world == "mm7" && map.fileName == "7out02.odm")
            {
                REQUIRE(asset.localEventProgram);
                CHECK(asset.localEventProgram->autonoteReferences().at(218) == std::vector<uint32_t>{262});
                CHECK(asset.localEventProgram->autonoteReferences().at(220) == std::vector<uint32_t>{309});
            }
            if (world == "mm8" && map.fileName == "out01.odm")
            {
                REQUIRE(asset.localEventProgram);
                CHECK(asset.localEventProgram->autonoteReferences().at(101) == std::vector<uint32_t>{206});
                CHECK(asset.localEventProgram->autonoteReferences().at(150) == std::vector<uint32_t>{16});
            }
            if (world == "mm8" && map.fileName == "d38.blv")
            {
                REQUIRE(asset.localEventProgram);
                CHECK(asset.localEventProgram->npcLocations().at(901)
                    == std::vector<ScriptedEventProgram::NpcLocation>{{24, 662}});
            }
            for (const auto *pProgram : {&asset.localEventProgram, &asset.globalEventProgram})
            {
                REQUIRE(pProgram->has_value());
                for (const auto &[eventId, locations] : (*pProgram)->npcLocations())
                    for (const ScriptedEventProgram::NpcLocation &location : locations)
                    {
                        CAPTURE(map.fileName);
                        CAPTURE(eventId);
                        CAPTURE(location.npcId);
                        CAPTURE(location.houseId);
                        REQUIRE(npcs.getNpc(location.npcId) != nullptr);
                        REQUIRE(houses.get(location.houseId) != nullptr);
                        ++declarations;
                    }
                database.indexNpcLocations(**pProgram);
                for (const auto &[eventId, ids] : (*pProgram)->autonoteReferences())
                    for (uint32_t id : ids)
                    {
                        CAPTURE(map.fileName);
                        CAPTURE(eventId);
                        CAPTURE(id);
                        REQUIRE(std::any_of(notes.entries().begin(), notes.entries().end(),
                            [id](const JournalAutonoteEntry &note)
                            { return (note.noteBit >> 16) == id && !note.text.empty() && note.text != "0"; }));
                        ++journalReferences;
                    }
            }
        }
        CHECK(scripts > 50);
        CHECK(declarations > 0);
        CHECK(journalReferences > 0);
        CHECK(database.query({}, false, "possible location").empty());
        if (world == "mm7")
        {
            CHECK(database.find("mm7:npc/393/possible-house/1158") != nullptr);
            CHECK(database.find("mm7:npc/416/possible-house/1164") != nullptr);
        }
        else if (world == "mm8")
        {
            CHECK(database.find("mm8:npc/52/possible-house/751") != nullptr);
            const WorldRecord *pWaterLord = database.find("mm8:npc/24");
            REQUIRE(pWaterLord != nullptr);
            CHECK(pWaterLord->house->id == 662);
            CHECK(database.find("mm8:npc/24/possible-house/662") == nullptr);
        }
    }
}
