#include "game/world/WorldGuide.h"
#include "game/tables/ClassMultiplierTable.h"
#include "game/tables/ClassSkillTable.h"
#include "game/tables/SpellTable.h"
#include "game/tables/ItemTable.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/PotionMixingTable.h"
#include "game/tables/PotionNoteTable.h"
#include "game/tables/HouseTable.h"
#include "game/tables/MapStats.h"
#include "engine/TextTable.h"

#include <doctest/doctest.h>
#include <algorithm>
#include <fstream>
#include <iterator>

using namespace OpenYAMM::Game;

namespace
{
std::vector<std::vector<std::string>> guideTableRows(const std::string &name)
{
    std::ifstream file(std::string(OPENYAMM_SOURCE_DIR) + "/assets_dev/engine/data_tables/" + name);
    REQUIRE(file.is_open());
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const auto table = OpenYAMM::Engine::TextTable::parseTabSeparated(text);
    REQUIRE(table.has_value());
    std::vector<std::vector<std::string>> rows;
    for (size_t i = 0; i < table->getRowCount(); ++i)
        rows.push_back(table->getRow(i));
    return rows;
}
}

TEST_CASE("world guide alchemy orders intermediate steps and chooses the least reagent path")
{
    const std::vector<WorldAlchemyRecipe> recipes = {
        {10, 1, 0}, {11, 2, 0}, {12, 10, 11}, {13, 12, 10}, {13, 12, 12}
    };
    const auto plan = planWorldAlchemy(13, recipes, {1, 2});
    REQUIRE(plan.size() == 4);
    CHECK(plan[0].result == 10);
    CHECK(plan[1].result == 11);
    CHECK(plan[2].result == 12);
    CHECK(plan[3].result == 13);
    CHECK(plan[3].second == 10);
    CHECK(planWorldAlchemy(14, recipes, {1, 2}).empty());
    CHECK(planWorldAlchemy(13, recipes, {1}).empty());
}

TEST_CASE("world guide alchemy rejects unreachable cycles self recipes and excessive expansion")
{
    CHECK(planWorldAlchemy(10, {{10, 11, 1}, {11, 10, 1}}, {1}).empty());
    CHECK(planWorldAlchemy(10, {{10, 10, 1}}, {1}).empty());
    CHECK(planWorldAlchemy(10, {{0, 1, 0}}, {1}).empty());
    std::vector<WorldAlchemyRecipe> recipes;
    for (uint32_t i = 2; i < 20; ++i)
        recipes.push_back({i, i - 1, i - 1});
    CHECK(planWorldAlchemy(11, recipes, {1}).size() == 10);
    CHECK(planWorldAlchemy(12, recipes, {1}).empty());
}

TEST_CASE("world guide spell discovery filters before displaying any reference text")
{
    SpellTable spells;
    REQUIRE(spells.loadFromRows({{"1", "", "Fire Bolt", "Fire", "Bolt", "Visible spell text"},
                                {"2", "", "Secret Spell", "Dark", "Secret", "Hidden spell text"}}));
    WorldGuideSource source;
    source.pSpells = &spells;
    WorldGuideKnowledge knowledge;
    CHECK(buildWorldGuide(source, WorldGuideSection::Spells, knowledge, false).empty());
    knowledge.spells.insert(1);
    const auto visible = buildWorldGuide(source, WorldGuideSection::Spells, knowledge, false);
    REQUIRE(visible.size() == 1);
    CHECK(visible[0].title == "Fire - Fire Bolt");
    CHECK(visible[0].details.find("Visible spell text") != std::string::npos);
    CHECK(visible[0].details.find("Hidden spell text") == std::string::npos);
    CHECK(buildWorldGuide(source, WorldGuideSection::Spells, knowledge, true).size() == 2);
    CHECK(knowledge.spells.size() == 1);
}

TEST_CASE("world guide reflects patched class progression and links exact mastery trainers")
{
    ClassMultiplierTable classes;
    REQUIRE(classes.loadFromRows({{"Class", "HP", "HP/level", "SP", "SP/level", "Mana"},
        {"Knight", "35", "5", "0", "0", "None"}, {"Cleric", "30", "4", "10", "3", "Personality"}}));
    ClassSkillTable skills;
    REQUIRE(skills.loadCapsFromRows({{"Skill", "Knight", "Cleric"}, {"Sword", "G", "-"}}));
    WorldGuideSource source;
    source.pClasses = &classes; source.pSkills = &skills;
    WorldGuideKnowledge knowledge;
    CHECK(buildWorldGuide(source, WorldGuideSection::Classes, knowledge, false).empty());
    knowledge.classes.insert("Knight");
    int before = 0, after = 0;
    std::string error;
    REQUIRE(classes.patchProgression("Knight", ClassProgressionField::HealthPerLevel,
        ClassPatchOperation::AddNumber, 1, before, after, error));
    const auto entries = buildWorldGuide(source, WorldGuideSection::Classes, knowledge, false);
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].details.find("HP/level: 6") != std::string::npos);
    REQUIRE(entries[0].links.size() == 1);
    CHECK(entries[0].links[0].kind == WorldGuideLink::Kind::Trainer);
    CHECK(entries[0].links[0].target == "Sword");
    CHECK(entries[0].links[0].mastery == 3);
    CHECK(buildWorldGuide(source, WorldGuideSection::Classes, knowledge, true).size() == 2);
    CHECK(classes.get("Knight")->healthPerLevel == 6);
}

TEST_CASE("world guide alchemy counts reused reagents and reveals only known formulas")
{
    ItemTable items;
    MergedPotionSettingTable potions;
    MergedReagentSettingTable reagents;
    PotionMixingTable mixes;
    PotionNoteTable notes;
    REQUIRE(potions.loadFromRows({{"Potion", "Item", "Mastery", "Drink", "Use"},
        {"1", "221", "1", "x", ""}, {"2", "222", "1", "x", ""},
        {"3", "223", "2", "x", ""}, {"4", "224", "3", "x", ""}}));
    REQUIRE(reagents.loadFromRows({{"Reagent", "Item", "Result"}, {"1", "100", "221"},
        {"2", "101", "222"}}));
    const std::vector<std::vector<std::string>> matrix = {
        {"220", "", "", "", "", "", "", "221", "222", "223", "224"},
        {"221", "", "", "", "", "", "", "0", "223", "224", "0"},
        {"222", "", "", "", "", "", "", "223", "0", "0", "0"},
        {"223", "", "", "", "", "", "", "224", "0", "0", "0"},
        {"224", "", "", "", "", "", "", "0", "0", "0", "0"}};
    REQUIRE(mixes.loadFromRows(matrix));
    auto noteRows = matrix;
    noteRows[1][8] = noteRows[2][7] = "50";
    noteRows[1][9] = noteRows[3][7] = "51";
    REQUIRE(notes.loadFromRows(noteRows));
    const WorldGuideSource source{&items, nullptr, nullptr, nullptr, &potions, &reagents, &mixes, &notes};
    WorldGuideKnowledge knowledge;
    CHECK(buildWorldGuide(source, WorldGuideSection::Alchemy, knowledge, false).empty());
    knowledge.recipes.insert(51);
    auto discovered = buildWorldGuide(source, WorldGuideSection::Alchemy, knowledge, false);
    REQUIRE(discovered.size() == 1);
    CHECK(discovered[0].id == "224");
    CHECK(discovered[0].details.find("No complete known reagent path") != std::string::npos);
    CHECK(discovered[0].details.find("Item 221 + Item 223") != std::string::npos);
    const auto guide = buildWorldGuide(source, WorldGuideSection::Alchemy, knowledge, true);
    REQUIRE(guide.size() == 4);
    CHECK(guide.back().details.find("2 x Item 100") != std::string::npos);
    CHECK(guide.back().details.find("1 x Item 101") != std::string::npos);
    CHECK(knowledge.items.empty());
    CHECK(knowledge.recipes.size() == 1);
}

TEST_CASE("world guide progress follows canonical tavern rules and preserves campaign state")
{
    MapStats maps;
    HouseTable houses;
    MergedHouseRuleTable rules;
    MergedTransportLocationTable transport;
    REQUIRE(maps.loadFromRows(guideTableRows("map_stats.txt")));
    REQUIRE(houses.loadFromRows(guideTableRows("house_data.txt")));
    REQUIRE(rules.loadFromRows(guideTableRows("house_rules.txt")));
    REQUIRE(transport.loadFromRows(guideTableRows("transport_locations.txt")));
    REQUIRE(houses.applyHouseRules(rules, transport, maps));
    WorldGuideSource source;
    source.pHouses = &houses; source.pMaps = &maps;
    WorldGuideKnowledge knowledge;
    knowledge.hasParty = true;
    knowledge.arcomageWinCount = 5;
    knowledge.arcomageLossCount = 2;
    knowledge.arenaWins = {1, 2, 3, 4};
    for (const char *world : {"mm7", "mm8"})
    {
        source.worldId = world;
        const auto guide = buildWorldGuide(source, WorldGuideSection::Progress, knowledge, true);
        REQUIRE(guide.size() > 2);
        const auto tavern = std::find_if(guide.begin(), guide.end(),
            [](const WorldGuideEntry &entry) { return !entry.links.empty(); });
        REQUIRE(tavern != guide.end());
        CHECK(tavern->details.find("Tower to win:") != std::string::npos);
        for (const auto &entry : guide)
            if (!entry.links.empty())
                CHECK(entry.links.front().target.starts_with(std::string(world) + ":"));
        const auto hidden = buildWorldGuide(source, WorldGuideSection::Progress, knowledge, false);
        REQUIRE(hidden.size() == 2); // Campaign counters only; no unexplored tavern names.
        CHECK(std::none_of(hidden.begin(), hidden.end(),
            [](const WorldGuideEntry &entry) { return !entry.links.empty(); }));
    }
    CHECK(knowledge.houses.empty());
    CHECK(knowledge.arcomageWins.empty());
    CHECK(knowledge.arcomageWinCount == 5);
    CHECK(knowledge.arenaWins[3] == 4);
}
