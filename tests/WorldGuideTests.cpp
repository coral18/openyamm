#include "game/world/WorldGuide.h"
#include "game/world/WorldDatabase.h"
#include "game/tables/ClassMultiplierTable.h"
#include "game/tables/ClassSkillTable.h"
#include "game/tables/SpellTable.h"
#include "game/tables/ItemTable.h"
#include "game/tables/MergedBaseTables.h"
#include "game/tables/PotionMixingTable.h"
#include "game/tables/PotionNoteTable.h"
#include "game/tables/HouseTable.h"
#include "game/tables/MapStats.h"
#include "game/tables/MonsterTable.h"
#include "engine/TextTable.h"
#include "engine/AssetFileSystem.h"
#include "game/tables/JournalQuestTable.h"
#include "game/tables/NpcDialogTable.h"

#include <doctest/doctest.h>
#include <algorithm>
#include <fstream>
#include <iterator>

using namespace OpenYAMM::Game;

TEST_CASE("world guide bestiary filters unknown definitions before exposing their stats")
{
    std::vector<std::string> known(38, "0"), unknown(38, "0");
    known[0] = "1"; known[1] = "Goblin"; known[2] = "Goblin A"; known[4] = "40";
    unknown[0] = "2"; unknown[1] = "Secret Dragon"; unknown[2] = "Dragon A"; unknown[4] = "999";
    MonsterTable monsters;
    REQUIRE(monsters.loadStatsFromRows({known, unknown}));
    WorldGuideSource source;
    source.pMonsters = &monsters;
    WorldGuideKnowledge knowledge;
    CHECK(buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false).empty());
    knowledge.monsters.insert(1);
    const auto discovered = buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false);
    REQUIRE(discovered.size() == 1);
    CHECK(discovered[0].title == "Goblin");
    CHECK(discovered[0].details.find("HP: 40") != std::string::npos);
    CHECK(discovered[0].details.find("Secret Dragon") == std::string::npos);
    CHECK(discovered[0].details.find("Special attack: 0") == std::string::npos);
    CHECK(buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, true).size() == 2);
    CHECK(knowledge.monsters.size() == 1);
}

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

TEST_CASE("world guide quests preserve journal conditions and record both sides of a bit change")
{
    JournalQuestTable quests;
    REQUIRE(quests.loadFromRows({{"3", "Deliver the letter", "", ""},
        {"10000", "Find the witness", "", "", "10;11", "12"},
        {"10001", "Unrelated", "", "", "", "99"}}));
    std::unordered_set<uint32_t> bits{10};
    const auto hasBit = [&bits](uint32_t id) { return bits.contains(id); };
    CHECK(quests.questsKnownAroundChange(11, false, true, hasBit).empty());
    bits.insert(11);
    CHECK(quests.questsKnownAroundChange(11, false, true, hasBit) == std::vector<uint32_t>{10000});
    bits.insert(12);
    CHECK(quests.questsKnownAroundChange(12, false, true, hasBit) == std::vector<uint32_t>{10000});
    CHECK_FALSE(quests.isVisible(quests.entries()[1], hasBit));
    bits.erase(12);
    CHECK(quests.questsKnownAroundChange(12, true, false, hasBit) == std::vector<uint32_t>{10000});
    CHECK(quests.questsKnownAroundChange(12, false, false, hasBit).empty());
    bits.insert(3);
    CHECK(quests.questsKnownAroundChange(3, false, true, hasBit) == std::vector<uint32_t>{3});
    bits.erase(3);
    CHECK(quests.questsKnownAroundChange(3, true, false, hasBit) == std::vector<uint32_t>{3});
    CHECK(quests.discoveryKey(3) == "openyamm.discovery.quest/3");
}

TEST_CASE("world guide quest metadata rejects broken or conflicting relationships transactionally")
{
    JournalQuestTable quests;
    REQUIRE(quests.loadFromRows({{"3", "Letter", "", ""}}));
    const std::vector<std::string> valid{"3", "mm8", "objective", "map", "world.mm8.map.out02", "journal"};
    REQUIRE(quests.loadLocationRows({valid}));
    CHECK_FALSE(quests.loadLocationRows({valid, valid}));
    REQUIRE(quests.entries()[0].locations.size() == 1);
    const std::vector<std::pair<size_t, std::string>> invalid = {
        {0, "900"}, {0, "-3"}, {1, ""}, {2, "finish"}, {3, "rumour"}, {4, ""}, {5, "anything"}
    };
    for (const auto &[column, value] : invalid)
    {
        auto row = valid;
        row[column] = value;
        CHECK_FALSE(quests.loadLocationRows({row}));
        CHECK(quests.entries()[0].locations[0].target == "world.mm8.map.out02");
    }
    for (const std::string &id : {"-1", "0", "1x", "4294967296", "01"})
    {
        auto row = valid;
        row[3] = "npc"; row[4] = id;
        CHECK_FALSE(quests.loadLocationRows({row}));
    }
    auto otherWorld = valid; otherWorld[1] = "mm7";
    CHECK_FALSE(quests.loadLocationRows({valid, otherWorld}));
    CHECK(quests.loadLocationRows({}));
    CHECK(quests.entries()[0].locations.empty());
}

TEST_CASE("world guide quests filter world and unknown hints before exposing text or links")
{
    JournalQuestTable quests;
    REQUIRE(quests.loadFromRows({{"3", "Deliver the letter", "", ""},
        {"4", "Secret quest", "", ""}, {"587", "Clean the castle", "", ""}}));
    REQUIRE(quests.loadLocationRows({
        {"3", "mm8", "objective", "map", "world.mm8.map.out02", "journal"},
        {"3", "mm8", "objective", "map", "world.mm8.map.d09", "guide"},
        {"4", "mm8", "objective", "map", "world.mm8.map.out03", "journal"},
        {"587", "mm7", "objective", "map", "world.mm7.map.7d29", "journal"}}));
    MapStats maps;
    REQUIRE(maps.loadFromRows(guideTableRows("map_stats.txt")));
    WorldGuideSource source;
    source.pQuests = &quests; source.pMaps = &maps; source.worldId = "mm8";
    WorldGuideKnowledge knowledge;
    CHECK(buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false).empty());
    knowledge.activeQuests.insert(3);
    const auto active = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(active.size() == 1);
    CHECK(active[0].details.find("Active quest") != std::string::npos);
    REQUIRE(active[0].links.size() == 1);
    CHECK(active[0].links[0].target == "world.mm8.map.out02");
    CHECK(active[0].links[0].informed);
    CHECK(active[0].details.find("Merchant House") == std::string::npos);
    knowledge.activeQuests.clear(); knowledge.quests.insert(3);
    const auto history = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(history.size() == 1);
    CHECK(history[0].details.find("Recorded earlier") != std::string::npos);
    CHECK(history[0].details.find("Completed") == std::string::npos);
    const auto guide = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, true);
    REQUIRE(guide.size() == 2);
    const auto letter = std::find_if(guide.begin(), guide.end(),
        [](const WorldGuideEntry &entry) { return entry.id == "3"; });
    REQUIRE(letter != guide.end());
    CHECK(letter->links.size() == 2);
    source.worldId = "mm7";
    CHECK(buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false).empty());
    CHECK(buildWorldGuide(source, WorldGuideSection::Quests, knowledge, true).size() == 1);
}

TEST_CASE("world guide quest relationships resolve against the current shared content")
{
    JournalQuestTable quests;
    REQUIRE(quests.loadFromRows(guideTableRows("english/quests.txt")));
    REQUIRE(quests.loadLocationRows(guideTableRows("quest_locations.txt")));
    MapStats maps; HouseTable houses; NpcDialogTable npcs; ItemTable items;
    REQUIRE(maps.loadFromRows(guideTableRows("map_stats.txt")));
    REQUIRE(houses.loadFromRows(guideTableRows("house_data.txt")));
    REQUIRE(npcs.loadNpcRows(guideTableRows("npc.txt")));
    OpenYAMM::Engine::AssetFileSystem assets;
    REQUIRE(items.load(assets, guideTableRows("items.txt"), {}));
    REQUIRE(quests.validateLocations(maps, npcs, houses, items));
    WorldGuideSource source;
    source.pQuests = &quests; source.pMaps = &maps; source.pHouses = &houses;
    source.pNpcs = &npcs; source.pItems = &items;
    WorldGuideKnowledge knowledge;
    for (const std::string &world : {"mm7", "mm8"})
    {
        source.worldId = world;
        const auto guide = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, true);
        REQUIRE(guide.size() >= 8);
        for (const WorldGuideEntry &entry : guide)
        {
            CHECK((entry.details.find("Objective:") != std::string::npos
                || entry.details.find("Return to:") != std::string::npos));
            CHECK_FALSE(entry.details.empty());
        }
    }
    // Invalid map, item, NPC and house references are fatal instead of producing dead Atlas links.
    for (const std::string &kind : {"map", "npc", "house", "item"})
    {
        REQUIRE(quests.loadLocationRows({{"3", "mm8", "objective", kind,
            kind == "map" ? "world.mm7.map.7d29" : "4294967295", "journal"}}));
        CHECK_FALSE(quests.validateLocations(maps, npcs, houses, items));
    }
}

TEST_CASE("world guide campaign clues do not turn undisclosed quest destinations into discoveries")
{
    JournalQuestTable quests;
    REQUIRE(quests.loadFromRows(guideTableRows("english/quests.txt")));
    REQUIRE(quests.loadLocationRows(guideTableRows("quest_locations.txt")));
    MapStats maps;
    REQUIRE(maps.loadFromRows(guideTableRows("map_stats.txt")));
    ItemTable items;
    OpenYAMM::Engine::AssetFileSystem assets;
    REQUIRE(items.load(assets, guideTableRows("items.txt"), {}));
    WorldGuideSource source;
    source.pQuests = &quests;
    source.pMaps = &maps;
    source.pItems = &items;
    source.worldId = "mm7";
    WorldGuideKnowledge knowledge;
    knowledge.activeQuests.insert(541);
    const auto discovery = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(discovery.size() == 1);
    const auto hasTarget = [](const WorldGuideEntry &entry, const std::string &target)
    {
        return std::any_of(entry.links.begin(), entry.links.end(), [&](const WorldGuideLink &link)
        { return link.target == target; });
    };
    // The request identifies the school and Erathia. The cipher still hides the precise tomb.
    CHECK(hasTarget(discovery.front(), "world.mm7.map.7d14"));
    CHECK(hasTarget(discovery.front(), "world.mm7.map.7out03"));
    CHECK_FALSE(hasTarget(discovery.front(), "world.mm7.map.mdt11"));
    CHECK_FALSE(hasTarget(discovery.front(), "1503"));
    knowledge.items.insert(1503);
    const auto withCipher = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(withCipher.size() == 1);
    CHECK(hasTarget(withCipher.front(), "1503"));
    const auto guide = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, true);
    const auto ninja = std::find_if(guide.begin(), guide.end(),
        [](const WorldGuideEntry &entry) { return entry.id == "541"; });
    REQUIRE(ninja != guide.end());
    CHECK(hasTarget(*ninja, "world.mm7.map.mdt11"));
    const auto tomb = std::find_if(ninja->links.begin(), ninja->links.end(),
        [](const WorldGuideLink &link) { return link.target == "world.mm7.map.mdt11"; });
    REQUIRE(tomb != ninja->links.end());
    CHECK_FALSE(tomb->informed);

    source.worldId = "mm8";
    knowledge.activeQuests = {68};
    const auto troll = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(troll.size() == 1);
    CHECK_FALSE(hasTarget(troll.front(), "world.mm8.map.d43"));
    const auto all = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, true);
    const auto homeland = std::find_if(all.begin(), all.end(),
        [](const WorldGuideEntry &entry) { return entry.id == "68"; });
    REQUIRE(homeland != all.end());
    CHECK(hasTarget(*homeland, "world.mm8.map.d43"));
    CHECK(knowledge.activeQuests == std::unordered_set<uint32_t>{68});
    CHECK(knowledge.quests.empty());
}

TEST_CASE("world guide quest contacts link only to the selected current or observed NPC index")
{
    MapStats maps;
    REQUIRE(maps.loadFromRows(guideTableRows("map_stats.txt")));
    HouseTable houses;
    REQUIRE(houses.loadFromRows(guideTableRows("house_data.txt")));
    NpcDialogTable npcs;
    REQUIRE(npcs.loadNpcRows(guideTableRows("npc.txt")));
    JournalQuestTable quests;
    REQUIRE(quests.loadFromRows({{"10000", "Ask the Butler", "", ""}}));
    REQUIRE(quests.loadLocationRows({{"10000", "mm7", "objective", "npc", "397", "journal"}}));
    MergedTeacherTopicTable teachers;
    WorldNpcState current;
    current.houses[397] = 1169;
    current.names[397] = "Current name";
    WorldDatabase guideWorld(maps, houses, npcs, teachers, "mm7", {}, current);
    WorldGuideSource source;
    WorldKnowledge locationKnowledge;
    source.worldId = "mm7"; source.pQuests = &quests; source.pNpcs = &npcs;
    source.pWorld = &guideWorld; source.pWorldKnowledge = &locationKnowledge;
    WorldGuideKnowledge knowledge;
    knowledge.quests.insert(10000);
    const auto guide = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, true);
    REQUIRE(guide.size() == 1);
    REQUIRE(guide[0].links.size() == 1);
    CHECK(guide[0].links[0].target == "mm7:npc/397");
    CHECK(guide[0].details.find("Current name") != std::string::npos);
    CHECK_FALSE(guide[0].links[0].informed);
    // Quest knowledge alone never tells the party the NPC's secret current residence.
    locationKnowledge.variables[WorldDatabase::npcDiscoveryKey(397)] = 1;
    locationKnowledge.variables[worldNpcHouseKey(397)] = 240;
    locationKnowledge.variables["openyamm.discovery.world.mm7.map.7d29"] = 1;
    const auto hidden = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(hidden.size() == 1);
    CHECK(hidden[0].links.empty());
    CHECK(hidden[0].details.find("Current name") == std::string::npos);
    WorldNpcState observed;
    observed.observedOnly = true;
    observed.houses[397] = 240;
    observed.topics[397] = {};
    WorldDatabase knownWorld(maps, houses, npcs, teachers, "mm7", {}, observed);
    source.pWorld = &knownWorld;
    locationKnowledge.variables[WorldDatabase::npcDiscoveryKey(397)] = 1;
    locationKnowledge.variables["openyamm.discovery.world.mm7.map.7out02"] = 1;
    const auto met = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(met.size() == 1);
    REQUIRE(met[0].links.size() == 1);
    CHECK(met[0].links[0].target == "mm7:npc/397");
    CHECK_FALSE(met[0].links[0].informed);
    CHECK(met[0].details.find("Butler") != std::string::npos);
    CHECK(knownWorld.find(met[0].links[0].target)->house->id == 240);
}
