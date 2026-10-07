#include "game/content/ModLoader.h"
#include "game/maps/SaveGame.h"
#include "game/tables/ItemTable.h"
#include "game/tables/HouseTable.h"

#include <doctest/doctest.h>

#include <limits>

using namespace OpenYAMM::Game;

namespace
{
ModManifest manifest(const std::string &id = "karol.test", const std::string &tail = {})
{
    std::string error;
    const auto parsed = parseModManifest("schema_version: 1\nid: " + id + "\nversion: 0.1.0\n" + tail,
        "test/mod.yaml", error);
    REQUIRE_MESSAGE(parsed.has_value(), error);
    return *parsed;
}

ClassMultiplierTable classes()
{
    ClassMultiplierTable table;
    REQUIRE(table.loadFromRows({{"Class", "BaseHP", "HP/level", "BaseSP", "SP/level", "Mana"},
        {"Knight", "35", "5", "0", "0", "None"}, {"Champion", "35", "8", "0", "0", "None"}}));
    return table;
}

const std::string patch =
    "patches:\n  - target: {type: class, id: engine.knight}\n"
    "    operations:\n      - {op: add_number, path: progression.hp_per_level, value: 1}\n";
}

TEST_CASE("mod loader validates explicit manifests and typed values")
{
    const ModManifest mod = manifest("karol.test", patch + "qbits: {begin: 10000, end: 10031}\n");
    CHECK(mod.version.text() == "0.1.0");
    CHECK(mod.idRanges[0].domain == "qbits");
    REQUIRE(mod.classPatches.size() == 1);
    CHECK(mod.classPatches[0].classId == "knight");
    CHECK(mod.classPatches[0].value == 1);
    std::string error;
    for (const std::string &invalid : {
        "schema_version: 2\nid: karol.test\nversion: 0.1.0\n",
        "schema_version: 1\nid: Knight\nversion: 0.1.0\n",
        "schema_version: 1\nid: karol.test\nversion: 0.1\n",
        "schema_version: 1\nid: karol.test\nversion: 01.2.3\n",
        "schema_version: 1\nid: karol.test\nversion: 1.2.3-beta\n",
        "schema_version: 1\nid: karol.test\nversion: 1.2.3\nversion: 9.9.9\n"})
    {
        CHECK_FALSE(parseModManifest(invalid, "test/mod.yaml", error));
        CHECK_FALSE(error.empty());
    }
    for (const std::string &invalid : {
        "qbits: {begin: 9999, end: 11000}\n", "qbits: {begin: 11000, end: 10000}\n",
        "qbits: {begin: -1, end: 10000}\n", "unknown_setting: true\n",
        "dependencies: [other.mod]\n",
        "patches: [{target: {type: class, id: other.knight}, operations: []}]\n",
        "patches: [{target: {type: spell, id: fire}, operations: []}]\n",
        "patches: [{target: {type: class, id: knight}, operations: [{op: add_number, path: bad, value: 1}]}]\n",
        "patches: [{target: {type: class, id: knight}, operations: [{op: set,"
        " path: progression.base_hp, value: 1.5}]}]\n"})
    {
        CHECK_FALSE(parseModManifest("schema_version: 1\nid: karol.test\nversion: 0.1.0\n" + invalid,
            "test/mod.yaml", error));
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("mod loader resolves stable profile priorities without enabling missing dependencies")
{
    ModManifest dependent = manifest("karol.test", "dependencies: [{id: base.rules, min_version: 0.1.0}]\n");
    const ModManifest base = manifest("base.rules");
    const ModManifest unrelated = manifest("other.content");
    ModLoadPlan plan;
    std::string error;
    REQUIRE(resolveModPlan({dependent, unrelated, base}, {}, {}, plan, error));
    REQUIRE(plan.mods.size() == 3);
    CHECK(plan.mods[0].id == "base.rules");
    CHECK(plan.mods[1].id == "karol.test");
    REQUIRE(resolveModPlan({base, dependent, unrelated},
        "enabled: [other.content, karol.test, base.rules]", {}, plan, error));
    CHECK(plan.mods[0].id == "other.content");
    CHECK(plan.mods[1].id == "base.rules");
    CHECK(plan.mods[2].id == "karol.test");
    REQUIRE(resolveModPlan({base, dependent}, "enabled: []", {}, plan, error));
    CHECK(plan.mods.empty());
    CHECK_FALSE(resolveModPlan({dependent}, {}, {}, plan, error));
    CHECK(error.find("requires missing or disabled") != std::string::npos);
    CHECK_FALSE(resolveModPlan({dependent, base}, "enabled: [karol.test]", {}, plan, error));
    CHECK_FALSE(resolveModPlan({base}, "enabled: [unknown.mod]", {}, plan, error));
    CHECK(error.find("Profile enables missing mod") != std::string::npos);
    dependent.dependencies[0].minimumVersion = ModVersion{0, 2, 0};
    CHECK_FALSE(resolveModPlan({dependent, base}, {}, {}, plan, error));
    CHECK(error.find("newer version") != std::string::npos);
    CHECK_FALSE(resolveModPlan({base, base}, {}, {}, plan, error));
    CHECK(error.find("Duplicate mod ID") != std::string::npos);
    CHECK_FALSE(resolveModPlan({base}, "enabled: [base.rules, base.rules]", {}, plan, error));
    CHECK_FALSE(resolveModPlan({base}, "enable: [base.rules]", {}, plan, error));
}

TEST_CASE("mod loader rejects dependency cycles conflicts and QBit collisions with worlds")
{
    ModManifest a = manifest("first.mod", "dependencies: [{id: second.mod}]\n");
    ModManifest b = manifest("second.mod", "dependencies: [{id: first.mod}]\n");
    ModLoadPlan plan;
    std::string error;
    CHECK_FALSE(resolveModPlan({a, b}, {}, {}, plan, error));
    CHECK(error.find("cycle") != std::string::npos);
    a.dependencies.clear(); b.dependencies.clear();
    a.loadAfter = {b.id}; b.loadAfter = {a.id};
    CHECK_FALSE(resolveModPlan({a, b}, {}, {}, plan, error));
    a.loadAfter.clear(); b.loadAfter.clear();
    a.conflicts = {b.id};
    CHECK_FALSE(resolveModPlan({a, b}, {}, {}, plan, error));
    CHECK(error.find("conflict") != std::string::npos);
    REQUIRE(resolveModPlan({a, b}, "enabled: [first.mod]", {}, plan, error));
    a.conflicts.clear();
    a.idRanges = {{"qbits", 10000, 10031}};
    b.idRanges = {{"qbits", 10031, 10050}};
    CHECK_FALSE(resolveModPlan({a, b}, {}, {}, plan, error));
    CHECK(error.find("QBit collision") != std::string::npos);
    b.idRanges[0].begin = 10032;
    REQUIRE(resolveModPlan({a, b}, {}, {}, plan, error));
    WorldManifest world;
    world.id = "mm9";
    world.qbits = {10020, 10040, {}, true};
    CHECK_FALSE(resolveModPlan({a}, {}, {world}, plan, error));
    CHECK(error.find("mm9") != std::string::npos);
    world.qbits.declared = false;
    world.idRanges = {{"qbits", 10031, 10040}};
    CHECK_FALSE(resolveModPlan({a}, {}, {world}, plan, error));
}

TEST_CASE("mod loader composes typed class patches with provenance and atomic failure")
{
    ClassMultiplierTable table = classes();
    ModLoadPlan plan;
    std::string error;
    REQUIRE(resolveModPlan({manifest("karol.test", patch), manifest("hardcore.rules", patch)}, {}, {}, plan, error));
    REQUIRE(applyModClassPatches(plan, table, error));
    CHECK(table.get("Knight")->healthPerLevel == 7);
    CHECK(table.get("Champion")->healthPerLevel == 8);
    CHECK(table.get("Knight")->baseHealth == 35);
    REQUIRE(plan.provenance.size() == 2);
    CHECK(plan.provenance[0].before == 5);
    CHECK(plan.provenance[1].after == 7);
    ModManifest broken = manifest("broken.mod", patch);
    broken.classPatches.push_back({"doesnotexist", "progression.hp_per_level"});
    plan.mods = {broken};
    CHECK_FALSE(applyModClassPatches(plan, table, error));
    CHECK(table.get("Knight")->healthPerLevel == 7);
    CHECK(plan.provenance.size() == 2);
    broken.classPatches.resize(1);
    broken.classPatches[0].value = std::numeric_limits<int>::max();
    plan.mods = {broken};
    CHECK_FALSE(applyModClassPatches(plan, table, error));
    CHECK(table.get("Knight")->healthPerLevel == 7);
    broken.classPatches[0].value = -8;
    plan.mods = {broken};
    CHECK_FALSE(applyModClassPatches(plan, table, error));
    CHECK(table.get("Knight")->healthPerLevel == 7);
}

TEST_CASE("mod loader reports competing assignments instead of silently overwriting mods")
{
    ClassMultiplierTable table = classes();
    ModManifest a = manifest("first.mod", patch);
    ModManifest b = manifest("second.mod", patch);
    a.classPatches[0].operation = b.classPatches[0].operation = ClassPatchOperation::Set;
    a.classPatches[0].value = 6;
    b.classPatches[0].value = 9;
    ModLoadPlan plan{{a, b}, {}};
    std::string error;
    CHECK_FALSE(applyModClassPatches(plan, table, error));
    CHECK(error.find("Conflicting set patches") != std::string::npos);
    CHECK(table.get("Knight")->healthPerLevel == 5);
    b.classPatches[0].value = 6;
    plan.mods = {a, b};
    REQUIRE(applyModClassPatches(plan, table, error));
    CHECK(table.get("Knight")->healthPerLevel == 6);
}

TEST_CASE("mod loader saves active identities even without mod items and rejects absent versions")
{
    GameSaveData save;
    ItemTable items;
    HouseTable houses;
    const ModManifest mod = manifest();
    const std::string id = modSavePackageId(mod);
    CHECK(id == "mods/karol.test@0.1.0");
    save.requiredContentPackages = collectRequiredContentPackages(save, items, houses, {{"engine", 1}, {id, 1}});
    REQUIRE(save.requiredContentPackages.size() == 1);
    CHECK(save.requiredContentPackages.at(id) == 1);
    std::string error;
    CHECK(validateRequiredContentPackages(save, items, houses, {{id, 1}}, error));
    CHECK_FALSE(validateRequiredContentPackages(save, items, houses, {}, error));
    CHECK(error.find("Save requires mod karol.test@0.1.0") != std::string::npos);
    CHECK_FALSE(validateRequiredContentPackages(save, items, houses, {{"mods/karol.test@0.2.0", 1}}, error));
    CHECK_FALSE(validateRequiredContentPackages(save, items, houses, {{id, 2}}, error));
    save.requiredContentPackages.clear(); // Legacy saves retain their existing compatibility.
    CHECK(validateRequiredContentPackages(save, items, houses, {{id, 1}}, error));
}
