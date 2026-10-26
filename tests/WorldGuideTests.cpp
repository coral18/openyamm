#include "game/world/WorldGuide.h"
#include "game/world/WorldDatabase.h"
#include "game/tables/ClassMultiplierTable.h"
#include "game/tables/ClassSkillTable.h"
#include "game/tables/SpellTable.h"
#include "game/tables/ItemTable.h"
#include "game/tables/JournalAutonoteTable.h"
#include "game/tables/SpriteTables.h"
#include "game/gameplay/InteractiveDecorationRules.h"
#include "game/StringUtils.h"
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
#include "game/gameplay/GuildSpellbookRules.h"
#include "game/gameplay/GameplayRuntimeInterfaces.h"
#include "game/world/WorldItemSources.h"
#include "game/world/WorldMapKnowledge.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/tables/ObjectTable.h"
#include "game/tables/RosterTable.h"
#include "game/party/Party.h"
#include "game/events/EventRuntime.h"
#include "game/maps/MapDeltaData.h"
#include "game/outdoor/OutdoorMapData.h"
#include "game/indoor/IndoorMapData.h"

#include <doctest/doctest.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>

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

TEST_CASE("world guide bestiary links only visible map sources to their precise monster features")
{
    std::vector<std::string> monster(38, "0");
    monster[0] = "11"; monster[1] = "Goblin"; monster[2] = "Goblin A";
    MonsterTable monsters;
    REQUIRE(monsters.loadStatsFromRows({monster}));
    REQUIRE(monsters.loadUniqueNamesFromRows({{"1", "Gruk the Red"}}));
    std::vector<std::string> row(35);
    row[0] = "63"; row[1] = "Harmondale"; row[2] = "7out02.odm"; row[34] = "mm7";
    MapStats maps;
    REQUIRE(maps.loadFromRows({row}));
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    WorldDatabase database(maps, houses, npcs, teachers, "mm7");
    MapStatsEntry map = *maps.findById(63);
    map.encounter1.monsterName = map.encounter1.pictureName = "Goblin";
    map.encounter1.minCount = map.encounter1.maxCount = 2;
    OutdoorMapData geometry;
    geometry.spawns = {{100, 200, 0, 64, 3, 1}};
    MapDeltaData delta;
    delta.actors.resize(1);
    delta.actors[0].monsterInfoId = 11;
    delta.actors[0].uniqueNameIndex = 1;
    delta.actors[0].x = 300;
    database.indexMapMonsters(map, geometry, monsters, {}, &delta);
    WorldGuideSource source;
    WorldKnowledge worldKnowledge;
    WorldGuideKnowledge knowledge;
    source.pMonsters = &monsters;
    source.pWorld = &database;
    source.pWorldKnowledge = &worldKnowledge;
    knowledge.monsters.insert(11); // Knowing a species does not disclose its other locations.
    const auto undisclosed = buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false);
    REQUIRE(undisclosed.size() == 1);
    CHECK(undisclosed[0].links.empty());
    const auto guide = buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, true);
    REQUIRE(guide.size() == 1);
    REQUIRE(guide[0].links.size() == 2);
    CHECK(guide[0].links[0].kind == WorldGuideLink::Kind::Feature);
    CHECK(guide[0].links[0].target == WorldDatabase::mapId(map) + "/encounter/0");
    CHECK(guide[0].links[1].target == WorldDatabase::mapId(map) + "/placed/0");
    CHECK(guide[0].links[1].label.find("Gruk the Red") != std::string::npos);
    CHECK(guide[0].links[1].label.find("Harmondale (300, 0)") != std::string::npos);
    worldKnowledge.monsterObservations[map.fileName] = {{8, 11, 42, 43, 44, false, 100, 1}};
    database.indexObservedMonsters(worldKnowledge, monsters); // Past maps need no geometry load.
    database.indexObservedMonsters(worldKnowledge, monsters); // Refresh must not duplicate links.
    const auto discovery = buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false);
    REQUIRE(discovery[0].links.size() == 1);
    CHECK(discovery[0].links[0].target == WorldDatabase::mapId(map) + "/observed/8");
    CHECK(discovery[0].links[0].label.find("Gruk the Red - last observed") != std::string::npos);
    CHECK(discovery[0].links[0].label.find("(42, 43)") != std::string::npos);
    CHECK(worldKnowledge.visitedMaps.empty());
    CHECK(buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, true)[0].links.size() == 3);
}

TEST_CASE("world guide monster variants follow runtime picture tiers and never disclose unknown definitions")
{
    const auto monsterRow = [](int id, const std::string &name, const std::string &picture)
    {
        std::vector<std::string> row(38, "0");
        row[0] = std::to_string(id); row[1] = name; row[2] = picture;
        return row;
    };
    MonsterTable monsters;
    REQUIRE(monsters.loadStatsFromRows({monsterRow(6, "Disguised peasant", "Goblin A"),
        monsterRow(11, "Goblin warrior", "GOBLIN A"), monsterRow(501, "Goblin shaman", "Goblin b"),
        monsterRow(24000, "Goblin king", "Goblin C"), monsterRow(4, "Goblin statue", "Goblin")}));
    WorldGuideSource source;
    source.pMonsters = &monsters;
    WorldGuideKnowledge knowledge;
    knowledge.monsters.insert(11);
    const auto first = buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false);
    REQUIRE(first.size() == 1);
    CHECK(first[0].links.empty());
    knowledge.monsters.insert(501);
    const auto learned = buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false);
    REQUIRE(learned.size() == 2);
    for (const WorldGuideEntry &entry : learned)
    {
        REQUIRE(entry.links.size() == 1);
        CHECK(entry.links[0].kind == WorldGuideLink::Kind::Monster);
        CHECK(entry.links[0].target == (entry.id == "11" ? "501" : "11"));
        CHECK(entry.links[0].label.find("Goblin king") == std::string::npos);
        CHECK(entry.links[0].target != "6");
    }
    const auto guide = buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, true);
    REQUIRE(guide.size() == 5);
    for (const WorldGuideEntry &entry : guide)
        if (entry.id == "6" || entry.id == "4")
            CHECK(entry.links.empty()); // A reused picture or a missing explicit tier cannot invent a family.
        else
        {
            REQUIRE(entry.links.size() == 2);
            CHECK(entry.links[0].kind == WorldGuideLink::Kind::Monster);
            CHECK(entry.links[1].kind == WorldGuideLink::Kind::Monster);
            CHECK(entry.links[0].target != entry.id);
            CHECK(entry.links[1].target != entry.id);
        }
    CHECK(knowledge.monsters == std::unordered_set<int16_t>{11, 501});
    CHECK(buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false).size() == 2);
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
    CHECK(visible[0].title == "Fire Magic - Fire Bolt");
    CHECK(visible[0].details.find("Visible spell text") != std::string::npos);
    CHECK(visible[0].details.find("Hidden spell text") == std::string::npos);
    CHECK(buildWorldGuide(source, WorldGuideSection::Spells, knowledge, true).size() == 2);
    CHECK(knowledge.spells.size() == 1);
}

TEST_CASE("guild spellbook pool respects school tier weights and explicit exclusions")
{
    HouseEntry guild;
    guild.type = "Fire Guild";
    guild.spellbookStockRule.quality = 1;
    ItemDefinition book;
    book.itemId = 400; book.name = "Torch Light";
    book.equipStat = "Book"; book.mod1 = "S1"; book.value = 400;
    REQUIRE(spellbookSpellId(book) == 1);
    CHECK(guildSpellbookStockWeight(guild, book) == 1);
    book.value = 401;
    CHECK(guildSpellbookStockWeight(guild, book) == 0);
    guild.spellbookStockRule.quality = 2;
    CHECK(guildSpellbookStockWeight(guild, book) == 1);
    book.value = 9000;
    CHECK(guildSpellbookStockWeight(guild, book) == 0);
    book.randomTreasureWeights[0] = 3;
    CHECK(guildSpellbookStockWeight(guild, book) == 3); // Actual treasure weight precedes value-based fallback.
    book.mod1 = "S12";
    CHECK(guildSpellbookStockWeight(guild, book) == 0);
    guild.type = "Elemental Guild";
    CHECK(guildSpellbookStockWeight(guild, book) == 3);
    book.mod1 = "S45";
    CHECK(guildSpellbookStockWeight(guild, book) == 0);
    guild.type = "Self Guild";
    CHECK(guildSpellbookStockWeight(guild, book) == 3);
    book.contentFlags = {"NoShop"};
    CHECK(guildSpellbookStockWeight(guild, book) == 0);
    book.contentFlags.clear();
    guild.vendorStockProfile = VendorStockProfile::Spellbook;
    CHECK(guildSpellbookStockWeight(guild, book) == 0); // Deterministic page stock bypasses this random pool.
    guild.vendorStockProfile = VendorStockProfile::None;
    guild.spellbookStockRule.quality = 0;
    CHECK(guildSpellbookStockWeight(guild, book) == 0);
    guild.spellbookStockRule.quality = 11;
    for (const char *pEncoding : {"S", "S0", "S-1", "S1junk", "S4294967296"})
    {
        book.mod1 = pEncoding;
        CHECK_FALSE(spellbookSpellId(book));
        CHECK(guildSpellbookStockWeight(guild, book) == 0);
    }
    book.mod1 = "S45"; book.equipStat = "Scroll";
    CHECK_FALSE(spellbookSpellId(book));
    CHECK(guildSpellbookStockWeight(guild, book) == 0);
}

TEST_CASE("world guide links spellbooks and possible local guilds without disclosing unknown vendors")
{
    OpenYAMM::Engine::AssetFileSystem assets;
    ItemTable items;
    SpellTable spells;
    HouseTable houses;
    MapStats maps;
    MergedHouseRuleTable rules;
    MergedTransportLocationTable transport;
    REQUIRE(items.load(assets, guideTableRows("items.txt"), guideTableRows("random_items.txt")));
    REQUIRE(spells.loadFromRows(guideTableRows("spells.txt")));
    REQUIRE(maps.loadFromRows(guideTableRows("map_stats.txt")));
    REQUIRE(houses.loadFromRows(guideTableRows("house_data.txt")));
    REQUIRE(rules.loadFromRows(guideTableRows("house_rules.txt")));
    REQUIRE(transport.loadFromRows(guideTableRows("transport_locations.txt")));
    REQUIRE(houses.applyHouseRules(rules, transport, maps));
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    for (const auto &[worldId, houseId] : std::vector<std::pair<std::string, uint32_t>>{{"mm7", 129}, {"mm8", 182}})
    {
        INFO(worldId);
        WorldDatabase world(maps, houses, npcs, teachers, worldId);
        WorldKnowledge discoveries;
        WorldGuideKnowledge knowledge;
        WorldGuideSource source;
        source.pItems = &items; source.pSpells = &spells;
        source.pWorld = &world; source.pWorldKnowledge = &discoveries; source.worldId = worldId;
        CHECK(buildWorldGuide(source, WorldGuideSection::Spells, knowledge, false).empty());
        knowledge.spells.insert(1);
        const auto hidden = buildWorldGuide(source, WorldGuideSection::Spells, knowledge, false);
        REQUIRE(hidden.size() == 1);
        CHECK(hidden.front().links.empty());
        const std::string target = worldId + ":house/" + std::to_string(houseId);
        const auto containsGuild = [&](const WorldGuideEntry &entry)
        {
            return std::any_of(entry.links.begin(), entry.links.end(), [&](const WorldGuideLink &link)
            { return link.kind == WorldGuideLink::Kind::Location && link.target == target; });
        };
        const auto guide = buildWorldGuide(source, WorldGuideSection::Spells, knowledge, true);
        const auto torch = std::find_if(guide.begin(), guide.end(),
            [](const WorldGuideEntry &entry) { return entry.id == "1"; });
        REQUIRE(torch != guide.end());
        CHECK(torch->title == "Fire Magic - Torch Light"); // The TXT Res column is not the magic school.
        CHECK(containsGuild(*torch));
        CHECK(torch->details.find("not current shelves") != std::string::npos);
        for (const WorldGuideLink &link : torch->links)
            if (link.kind == WorldGuideLink::Kind::Location)
            {
                REQUIRE(world.find(link.target) != nullptr);
                CHECK(world.find(link.target)->map->worldId == worldId);
                CHECK(world.find(link.target)->kind == WorldRecordKind::House);
            }
        CHECK(discoveries.variables.empty());
        const HouseEntry *pGuild = houses.get(houseId);
        REQUIRE(pGuild != nullptr);
        const MapStatsEntry *pMap = maps.findById(pGuild->mapId);
        REQUIRE(pMap != nullptr);
        discoveries.visitedMaps.insert(pMap->fileName);
        CHECK(buildWorldGuide(source, WorldGuideSection::Spells, knowledge, false).front().links.empty());
        discoveries.variables[WorldDatabase::houseDiscoveryKey(houseId)] = 1;
        const auto local = buildWorldGuide(source, WorldGuideSection::Spells, knowledge, false);
        REQUIRE(local.size() == 1);
        REQUIRE(local.front().links.size() == 1);
        CHECK(containsGuild(local.front()));
        CHECK_FALSE(local.front().links.front().informed);
        CHECK(discoveries.regionMaps.empty()); // A source link is not a terrain discovery.
        knowledge.spells.clear(); knowledge.items.insert(400);
        CHECK(buildWorldGuide(source, WorldGuideSection::Spells, knowledge, false).empty());
        knowledge.identifiedItems.insert(400);
        const auto ownedBook = buildWorldGuide(source, WorldGuideSection::Spells, knowledge, false);
        REQUIRE(ownedBook.size() == 1);
        CHECK(ownedBook.front().id == "1");
        CHECK(ownedBook.front().details.starts_with("Spellbook encountered"));
        REQUIRE(ownedBook.front().links.size() == 2);
        CHECK(ownedBook.front().links.front().kind == WorldGuideLink::Kind::Item);
        CHECK(ownedBook.front().links.front().target == "400");
        const auto itemPage = buildWorldGuide(source, WorldGuideSection::Items, knowledge, false);
        REQUIRE(itemPage.size() == 1);
        REQUIRE(itemPage.front().links.size() == 1);
        CHECK(itemPage.front().links.front().kind == WorldGuideLink::Kind::Spell);
        CHECK(itemPage.front().links.front().target == "1");
        CHECK(knowledge.spells.empty()); // Inspecting the reference never learns the spell.
    }
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

TEST_CASE("world guide class comparisons use sparse mod races and registry promotions without leaking unknown paths")
{
    ClassMultiplierTable classes;
    REQUIRE(classes.loadFromRows({{"Class", "HP", "HP/level", "SP", "SP/level", "Mana"},
        {"CinderNovice", "20", "3", "5", "1", "Intellect"},
        {"CinderAdept", "25", "5", "8", "2", "Intellect"},
        {"Unrelated", "80", "9", "0", "0", "None"}}));
    ClassSkillTable skills;
    REQUIRE(skills.loadCapsFromRows({{"Skill", "CinderNovice", "CinderAdept", "Unrelated"},
        {"Learning", "B", "E", "-"}, {"Sword", "M", "G", "G"}}));
    REQUIRE(skills.loadClassMetadataFromRows({{"#", "Kind", "Step", "Note"},
        {"0", "77", "0", "CinderNovice"}, {"1", "77", "1", "CinderAdept"},
        {"2", "88", "1", "Unrelated"}}));
    MergedRaceSkillTable racial;
    MergedCharacterSelectionTable selection;
    std::string error;
    REQUIRE(racial.loadFromYaml("rules:\n- {race: Windborn, race_id: 97, skill: Learning, add: 1}\n", error));
    REQUIRE(skills.applyRaceSkillOverrides(racial));
    REQUIRE(selection.loadFromYaml("race_class_availability:\n"
        "- {race: Human, race_id: 0, classes: [CinderNovice]}\n"
        "- {race: Windborn, race_id: 97, classes: [CinderNovice]}\n"
        "new_game_continents:\n- {name: Testland, available_class_ids: [0], available_race_ids: [0,97]}\n", error));
    CHECK(selection.raceNames().size() == 2);
    CHECK(selection.raceNames().at(97) == "Windborn");
    WorldGuideSource source;
    source.pClasses = &classes; source.pSkills = &skills; source.pCharacterSelection = &selection;
    WorldGuideKnowledge knowledge;
    knowledge.classes.insert("CinderNovice"); knowledge.races.insert(0);
    const auto discovered = buildWorldGuide(source, WorldGuideSection::Classes, knowledge, false);
    REQUIRE(discovered.size() == 1);
    CHECK(discovered.front().details.find("Windborn") == std::string::npos);
    CHECK(discovered.front().details.find("CinderAdept") == std::string::npos);
    CHECK(std::none_of(discovered.front().links.begin(), discovered.front().links.end(),
        [](const WorldGuideLink &link) { return link.kind == WorldGuideLink::Kind::Class; }));
    const auto guide = buildWorldGuide(source, WorldGuideSection::Classes, knowledge, true);
    const auto novice = std::find_if(guide.begin(), guide.end(),
        [](const WorldGuideEntry &entry) { return entry.id == "CinderNovice"; });
    REQUIRE(novice != guide.end());
    CHECK(novice->details.find("Windborn mastery differences") != std::string::npos);
    CHECK(novice->details.find("Learning: Normal -> Expert") != std::string::npos);
    CHECK(novice->details.find("CinderAdept: base HP +5, HP/level +2, base SP +3, SP/level +1") != std::string::npos);
    REQUIRE(novice->links.back().kind == WorldGuideLink::Kind::Class);
    CHECK(novice->links.back().target == "CinderAdept");
    CHECK(std::none_of(novice->links.begin(), novice->links.end(),
        [](const WorldGuideLink &link) { return link.target == "Unrelated"; }));
    const auto adept = std::find_if(guide.begin(), guide.end(),
        [](const WorldGuideEntry &entry) { return entry.id == "CinderAdept"; });
    REQUIRE(adept != guide.end());
    CHECK(adept->links.back().label == "Earlier tier: CinderNovice");
    CHECK(knowledge.classes == std::unordered_set<std::string>{"CinderNovice"});
    CHECK(knowledge.races == std::unordered_set<uint32_t>{0});
    knowledge.races.insert(97);
    const auto knownRace = buildWorldGuide(source, WorldGuideSection::Classes, knowledge, false);
    CHECK(knownRace.front().details.find("Windborn mastery differences") != std::string::npos);
    const auto raceTrainer = std::find_if(knownRace.front().links.begin(), knownRace.front().links.end(),
        [](const WorldGuideLink &link) { return link.label.starts_with("Windborn:"); });
    REQUIRE(raceTrainer != knownRace.front().links.end());
    CHECK(raceTrainer->kind == WorldGuideLink::Kind::Trainer);
    CHECK(raceTrainer->target == "Learning");
    CHECK(raceTrainer->mastery == 1); // Expert teacher-topic rank, not SkillMastery::Expert's numeric value.
    knowledge.classes.insert("CinderAdept");
    int before = 0, after = 0;
    REQUIRE(classes.patchProgression("CinderAdept", ClassProgressionField::HealthPerLevel,
        ClassPatchOperation::AddNumber, 1, before, after, error));
    const auto patched = buildWorldGuide(source, WorldGuideSection::Classes, knowledge, false);
    const auto patchedNovice = std::find_if(patched.begin(), patched.end(),
        [](const WorldGuideEntry &entry) { return entry.id == "CinderNovice"; });
    REQUIRE(patchedNovice != patched.end());
    CHECK(patchedNovice->details.find("HP/level +3") != std::string::npos);
}

TEST_CASE("world class promotion references do not invent legacy paths or wrap overflowing steps")
{
    ClassSkillTable skills;
    CHECK(skills.promotionClassNamesFromMetadata("Knight").empty());
    REQUIRE(skills.loadClassMetadataFromRows({{"#", "Kind", "Step", "Note"},
        {"0", "77", "4294967295", "FinalTier"}, {"1", "77", "0", "FirstTier"}}));
    CHECK(skills.promotionClassNamesFromMetadata("FinalTier").empty());
    CHECK(skills.promotionClassNamesFromMetadata("AbsentClass").empty());
}

TEST_CASE("world class comparison grid preserves full families and filters undiscovered classes and races")
{
    ClassMultiplierTable classes;
    REQUIRE(classes.loadFromRows({{"Class", "HP", "HP/level", "SP", "SP/level", "Mana"},
        {"CinderNovice", "20", "3", "5", "1", "Intellect"},
        {"CinderAdept", "25", "5", "8", "2", "Intellect"},
        {"CinderSage", "30", "6", "10", "3", "Intellect"},
        {"CinderScholar", "30", "5", "12", "4", "Intellect"},
        {"CinderSeer", "32", "4", "15", "5", "Intellect"},
        {"CinderWarden", "40", "8", "5", "1", "Intellect"},
        {"Unrelated", "80", "9", "0", "0", "None"}}));
    const std::vector<std::vector<std::string>> metadata = {{"#", "Kind", "Step", "Note"},
        {"0", "77", "0", "CinderNovice"}, {"1", "77", "1", "CinderAdept"},
        {"2", "77", "2", "CinderSage"}, {"3", "77", "2", "CinderScholar"},
        {"4", "77", "2", "CinderSeer"}, {"5", "77", "2", "CinderWarden"},
        {"6", "88", "0", "Unrelated"}};
    REQUIRE(classes.applyClassExtraRows(metadata));
    ClassSkillTable skills;
    REQUIRE(skills.loadCapsFromRows({{"Skill", "CinderNovice", "CinderAdept", "CinderSage",
        "CinderScholar", "CinderSeer", "CinderWarden", "Unrelated"},
        {"Learning", "B", "E", "M", "G", "M", "B", "G"},
        {"Sword", "-", "-", "-", "-", "-", "M", "G"}}));
    REQUIRE(skills.loadClassMetadataFromRows(metadata));
    MergedCharacterSelectionTable selection;
    MergedRaceSkillTable racial;
    std::string error;
    REQUIRE(selection.loadFromYaml("race_class_availability:\n"
        "- {race: Human, race_id: 0, classes: [CinderNovice]}\n"
        "- {race: Windborn, race_id: 97, classes: [CinderNovice]}\n"
        "new_game_continents:\n- {name: Testland, available_class_ids: [0], available_race_ids: [0,97]}\n", error));
    REQUIRE(racial.loadFromYaml("rules:\n- {race: Windborn, race_id: 97, skill: Learning, add: 1}\n", error));
    REQUIRE(skills.applyRaceSkillOverrides(racial));
    WorldGuideSource source;
    source.pClasses = &classes; source.pSkills = &skills; source.pCharacterSelection = &selection;
    WorldGuideKnowledge knowledge;
    knowledge.classes = {"CinderNovice", "CinderSage"}; knowledge.races = {0};
    CHECK_FALSE(buildWorldClassComparison(source, knowledge, false, "CinderAdept"));
    CHECK_FALSE(buildWorldClassComparison(source, knowledge, true, "AbsentClass"));
    CHECK_FALSE(buildWorldClassComparison(source, knowledge, false, "CinderNovice", 97));
    CHECK_FALSE(buildWorldClassComparison(source, knowledge, true, "CinderNovice", 98));
    const auto known = buildWorldClassComparison(source, knowledge, false, "CinderNovice");
    REQUIRE(known);
    CHECK_FALSE(known->raceId);
    REQUIRE(known->classes.size() == 2);
    CHECK(known->classes[0].id == "CinderNovice");
    CHECK(known->classes[1].id == "CinderSage"); // Hidden intermediate tier must not sever known relationships.
    REQUIRE(known->races.size() == 1);
    CHECK(known->races[0].first == 0);
    REQUIRE(known->skills.size() == 1);
    CHECK(known->skills[0].skill == "Learning");
    CHECK(known->skills[0].caps == std::vector<SkillMastery>{SkillMastery::Normal, SkillMastery::Master});
    const auto human = buildWorldClassComparison(source, knowledge, false, "CinderNovice", 0);
    REQUIRE(human);
    REQUIRE(human->raceId);
    CHECK(*human->raceId == 0); // Human zero is distinct from the base-class option.
    CHECK(human->skills[0].caps == known->skills[0].caps);
    const auto guide = buildWorldClassComparison(source, knowledge, true, "CinderSage", 97);
    REQUIRE(guide);
    REQUIRE(guide->classes.size() == 6); // All branches survive UI paging; unrelated classes are absent.
    CHECK(guide->classes.front().id == "CinderSage");
    CHECK(guide->classes[1].id == "CinderNovice");
    CHECK(guide->classes[2].id == "CinderAdept");
    CHECK(guide->classes.back().id == "CinderWarden");
    REQUIRE(guide->skills.size() == 2);
    for (const WorldClassComparisonRow &row : guide->skills)
        for (size_t i = 0; i < guide->classes.size(); ++i)
            CHECK(row.caps[i] == skills.getEffectiveCap(guide->classes[i].id, 97, row.skill));
    CHECK(knowledge.classes == std::unordered_set<std::string>{"CinderNovice", "CinderSage"});
    CHECK(knowledge.races == std::unordered_set<uint32_t>{0});
    int before = 0, after = 0;
    REQUIRE(classes.patchProgression("CinderSage", ClassProgressionField::HealthPerLevel,
        ClassPatchOperation::AddNumber, 1, before, after, error));
    CHECK(guide->classes.front().pDefinition == classes.get("CinderSage"));
    CHECK(guide->classes.front().pDefinition->healthPerLevel == 7);
    ClassSkillTable withoutMetadata;
    source.pSkills = &withoutMetadata;
    const auto isolated = buildWorldClassComparison(source, knowledge, true, "CinderNovice");
    REQUIRE(isolated);
    REQUIRE(isolated->classes.size() == 1);
    CHECK(isolated->classes.front().id == "CinderNovice");
    CHECK(isolated->skills.empty());
    source.pClasses = nullptr;
    CHECK_FALSE(buildWorldClassComparison(source, knowledge, true, "CinderNovice"));
}

TEST_CASE("world guide canonical class comparisons use the gameplay race evaluator and promotion registry")
{
    const auto text = [](const std::string &fileName)
    {
        std::ifstream file(std::string(OPENYAMM_SOURCE_DIR) + "/assets_dev/engine/data_tables/" + fileName);
        REQUIRE(file.is_open());
        return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    };
    ClassMultiplierTable classes;
    ClassSkillTable skills;
    REQUIRE(classes.loadFromRows(guideTableRows("class_multipliers.txt")));
    REQUIRE(classes.applyClassExtraRows(guideTableRows("class_extra.txt")));
    REQUIRE(skills.loadCapsFromRows(guideTableRows("class_skills.txt")));
    REQUIRE(skills.loadStartingSkillsFromRows(guideTableRows("class_starting_skills.txt")));
    REQUIRE(skills.loadClassMetadataFromRows(guideTableRows("class_extra.txt")));
    REQUIRE(skills.loadClassSpellPointMetadataFromRows(guideTableRows("class_multipliers.txt")));
    MergedRaceSkillTable racial;
    MergedCharacterSelectionTable selection;
    std::string error;
    REQUIRE(racial.loadFromYaml(text("race_skills.yml"), error));
    REQUIRE(selection.loadFromYaml(text("character_selection.yml"), error));
    REQUIRE(skills.applyRaceSkillOverrides(racial));
    CHECK(skills.promotionClassNamesFromMetadata("Knight") == std::vector<std::string>{"Cavalier"});
    CHECK(skills.promotionClassNamesFromMetadata("Cavalier")
        == std::vector<std::string>{"BlackKnight", "Champion"});
    WorldGuideSource source;
    source.pClasses = &classes; source.pSkills = &skills; source.pCharacterSelection = &selection;
    WorldGuideKnowledge knowledge;
    knowledge.classes.insert("Knight"); knowledge.races.insert(0);
    const auto entries = buildWorldGuide(source, WorldGuideSection::Classes, knowledge, false);
    REQUIRE(entries.size() == 1);
    const size_t starting = entries.front().details.find("Starting skills by race:");
    REQUIRE(starting != std::string::npos);
    const std::string startingChoices = entries.front().details.substr(starting,
        entries.front().details.find("Current class mastery limits:", starting) - starting);
    CHECK(startingChoices.find("Human\nGranted: Sword, Leather Armor") != std::string::npos);
    CHECK(startingChoices.find("Learning") == std::string::npos);
    CHECK(startingChoices.find("Vampire") == std::string::npos);

    const SkillMastery base = skills.getClassCap("Knight", "Learning");
    const SkillMastery human = skills.getEffectiveCap("Knight", 0, "Learning");
    REQUIRE(base != human);
    CHECK(entries.front().details.find("Learning: " + masteryDisplayName(base) + " -> " + masteryDisplayName(human))
        != std::string::npos);
    const auto trainer = std::find_if(entries.front().links.begin(), entries.front().links.end(),
        [](const WorldGuideLink &link) { return link.label.starts_with("Human: Learning"); });
    REQUIRE(trainer != entries.front().links.end());
    CHECK(trainer->mastery == uint32_t(human) - 1);
    CHECK(entries.front().details.find("Vampire mastery differences") == std::string::npos);
    CHECK(knowledge.classes.size() == 1);
    const auto baseGrid = buildWorldClassComparison(source, knowledge, false, "Knight");
    const auto humanGrid = buildWorldClassComparison(source, knowledge, false, "Knight", 0);
    REQUIRE(baseGrid);
    REQUIRE(humanGrid);
    const auto learningCap = [](const WorldClassComparison &grid)
    {
        const auto row = std::find_if(grid.skills.begin(), grid.skills.end(),
            [](const WorldClassComparisonRow &entry) { return entry.skill == "Learning"; });
        REQUIRE(row != grid.skills.end());
        return row->caps.front();
    };
    CHECK(learningCap(*baseGrid) == base);
    CHECK(learningCap(*humanGrid) == human);
}

TEST_CASE("world guide alchemy counts reused reagents and reveals only known formulas")
{
    ItemTable items;
    OpenYAMM::Engine::AssetFileSystem assets;
    std::vector<std::vector<std::string>> itemRows;
    for (uint32_t id : {100, 101, 102, 221, 222, 223, 224})
    {
        std::vector<std::string> cells(17);
        cells[0] = std::to_string(id); cells[2] = "Item " + std::to_string(id);
        itemRows.push_back(std::move(cells));
    }
    REQUIRE(items.load(assets, itemRows, {}));
    MergedPotionSettingTable potions;
    MergedReagentSettingTable reagents;
    PotionMixingTable mixes;
    PotionNoteTable notes;
    REQUIRE(potions.loadFromRows({{"Potion", "Item", "Mastery", "Drink", "Use"},
        {"1", "221", "1", "x", ""}, {"2", "222", "1", "x", ""},
        {"3", "223", "2", "x", ""}, {"4", "224", "3", "x", ""}}));
    REQUIRE(reagents.loadFromRows({{"Reagent", "Item", "Result"}, {"1", "100", "221"},
        {"2", "101", "222"}, {"3", "102", "221"}}));
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
    const auto base = std::find_if(guide.begin(), guide.end(),
        [](const WorldGuideEntry &entry) { return entry.id == "221"; });
    REQUIRE(base != guide.end());
    CHECK(std::count_if(base->links.begin(), base->links.end(), [](const WorldGuideLink &link)
        { return link.kind == WorldGuideLink::Kind::Item && link.target == "102"; }) == 1);
    knowledge.items = {100, 221};
    const auto partial = buildWorldGuide(source, WorldGuideSection::Alchemy, knowledge, false);
    const auto knownBase = std::find_if(partial.begin(), partial.end(),
        [](const WorldGuideEntry &entry) { return entry.id == "221"; });
    REQUIRE(knownBase != partial.end());
    CHECK(std::none_of(knownBase->links.begin(), knownBase->links.end(),
        [](const WorldGuideLink &link) { return link.target == "102"; }));
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
    CHECK(quests.knowledgeAroundChange(11, false, true, hasBit).quests.empty());
    bits.insert(11);
    CHECK(quests.knowledgeAroundChange(11, false, true, hasBit).quests == std::vector<uint32_t>{10000});
    bits.insert(12);
    CHECK(quests.knowledgeAroundChange(12, false, true, hasBit).quests == std::vector<uint32_t>{10000});
    CHECK_FALSE(quests.isVisible(quests.entries()[1], hasBit));
    bits.erase(12);
    CHECK(quests.knowledgeAroundChange(12, true, false, hasBit).quests == std::vector<uint32_t>{10000});
    CHECK(quests.knowledgeAroundChange(12, false, false, hasBit).quests.empty());
    bits.insert(3);
    CHECK(quests.knowledgeAroundChange(3, false, true, hasBit).quests == std::vector<uint32_t>{3});
    bits.erase(3);
    CHECK(quests.knowledgeAroundChange(3, true, false, hasBit).quests == std::vector<uint32_t>{3});
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

TEST_CASE("world guide conditional clues retain facts learned before quest or stage bits disappear")
{
    JournalQuestTable quests;
    REQUIRE(quests.loadFromRows({{"3", "Find the witness", "", ""}}));
    REQUIRE(quests.loadLocationRows({
        {"3", "mm8", "objective", "map", "world.mm8.map.out02", "journal", "10;11", "12"},
        {"3", "mm8", "objective", "map", "world.mm8.map.out03", "guide", "10", ""}}));
    const JournalQuestLocation &clue = quests.entries().front().locations.front();
    const std::string key = quests.locationDiscoveryKey(3, clue);
    CHECK(key == "openyamm.discovery.quest/3/location/objective/map/world.mm8.map.out02");
    std::unordered_set<uint32_t> bits{3, 10};
    const auto hasBit = [&bits](uint32_t bit) { return bits.contains(bit); };
    CHECK_FALSE(quests.locationAvailable(clue, hasBit));
    CHECK(quests.knowledgeAroundChange(3, false, true, hasBit).locations.empty());
    bits.insert(11);
    CHECK(quests.locationAvailable(clue, hasBit));
    const JournalQuestKnowledgeChange learned = quests.knowledgeAroundChange(11, false, true, hasBit);
    CHECK(learned.quests.empty());
    CHECK(learned.locations == std::vector<std::string>{key});
    // The old side matters when a condition becomes false or the journal entry is removed.
    bits.insert(12);
    CHECK_FALSE(quests.locationAvailable(clue, hasBit));
    CHECK(quests.knowledgeAroundChange(12, false, true, hasBit).locations == learned.locations);
    bits.erase(12);
    bits.erase(3);
    CHECK(quests.knowledgeAroundChange(3, true, false, hasBit).locations == learned.locations);
    bits.erase(11);
    CHECK(quests.knowledgeAroundChange(11, true, false, hasBit).locations.empty());
    CHECK(quests.knowledgeAroundChange(11, false, false, hasBit).locations.empty());

    MapStats maps;
    REQUIRE(maps.loadFromRows(guideTableRows("map_stats.txt")));
    WorldGuideSource source;
    source.pQuests = &quests;
    source.pMaps = &maps;
    source.worldId = "mm8";
    WorldGuideKnowledge knowledge;
    knowledge.quests.insert(3);
    // An old quest history flag cannot invent a later clue after its condition disappeared.
    knowledge.questBits = {10, 11};
    const auto legacy = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(legacy.size() == 1);
    CHECK(legacy.front().links.empty());
    knowledge.questBits.clear();
    knowledge.questLocations.insert(learned.locations.begin(), learned.locations.end());
    const auto history = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(history.size() == 1);
    REQUIRE(history.front().links.size() == 1);
    CHECK(history.front().links.front().target == "world.mm8.map.out02");
    CHECK(history.front().links.front().informed);
    CHECK(history.front().details.find("recorded earlier") != std::string::npos);
    CHECK(history.front().details.find("Alvar") == std::string::npos);
    const auto guide = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, true);
    REQUIRE(guide.size() == 1);
    REQUIRE(guide.front().links.size() == 2);
    CHECK_FALSE(guide.front().links.back().informed);
    CHECK(guide.front().details.find("conditional clue") != std::string::npos);
}

TEST_CASE("world guide conditional metadata rejects partial contradictory and malformed conditions")
{
    JournalQuestTable quests;
    REQUIRE(quests.loadFromRows({{"3", "A quest", "", ""}}));
    const std::vector<std::string> valid{
        "3", "mm8", "objective", "map", "world.mm8.map.out02", "journal", "10;11", "12"};
    REQUIRE(quests.loadLocationRows({valid}));
    for (const std::string &bad : {"-1", "+1", "0", "10x", "4294967296", "-4294967295"})
    {
        auto row = valid;
        row[6] = bad;
        CHECK_FALSE(quests.loadLocationRows({row}));
        CHECK(quests.entries().front().locations.front().requiredQBitIds == std::vector<uint32_t>{10, 11});
    }
    auto contradictory = valid;
    contradictory[7] = "11";
    CHECK_FALSE(quests.loadLocationRows({contradictory}));
    auto partial = valid;
    partial.pop_back();
    CHECK_FALSE(quests.loadLocationRows({partial}));
    CHECK(quests.entries().front().locations.front().forbiddenQBitIds == std::vector<uint32_t>{12});
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
    knowledge.questBits.insert(569);
    const auto deciphered = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(deciphered.size() == 1);
    CHECK(hasTarget(deciphered.front(), "world.mm7.map.mdt11"));

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
    knowledge.questBits.insert(69);
    const auto foundHome = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(foundHome.size() == 1);
    CHECK(hasTarget(foundHome.front(), "world.mm8.map.d43"));
    CHECK(knowledge.activeQuests == std::unordered_set<uint32_t>{68});
    CHECK(knowledge.quests.empty());
}

TEST_CASE("MM8 campaign relationships distinguish accepted clues from later discoveries")
{
    JournalQuestTable quests;
    REQUIRE(quests.loadFromRows(guideTableRows("english/quests.txt")));
    REQUIRE(quests.loadLocationRows(guideTableRows("quest_locations.txt")));
    MapStats maps;
    REQUIRE(maps.loadFromRows(guideTableRows("map_stats.txt")));
    NpcDialogTable npcs;
    REQUIRE(npcs.loadNpcRows(guideTableRows("npc.txt")));
    WorldGuideSource source;
    source.pQuests = &quests; source.pMaps = &maps; source.pNpcs = &npcs; source.worldId = "mm8";
    WorldGuideKnowledge knowledge;
    const auto hasMap = [&](uint32_t qbit, const std::string &target, bool guide)
    {
        knowledge.activeQuests = {qbit};
        const auto entries = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, guide);
        const auto quest = std::find_if(entries.begin(), entries.end(),
            [&](const WorldGuideEntry &entry) { return entry.id == std::to_string(qbit); });
        REQUIRE(quest != entries.end());
        return std::any_of(quest->links.begin(), quest->links.end(),
            [&](const WorldGuideLink &link) { return link.target == target; });
    };
    // Bastian requests a witness, not a named Troll at a secretly known address.
    CHECK(hasMap(25, "world.mm8.map.out04", false));
    CHECK(buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false)[0].details.find(
        "Overdune Snapfinger") == std::string::npos);
    CHECK(hasMap(17, "world.mm8.map.out05", false));
    CHECK_FALSE(hasMap(17, "world.mm8.map.d17", false));
    CHECK(hasMap(17, "world.mm8.map.d17", true));
    CHECK(hasMap(18, "world.mm8.map.out08", false));
    CHECK_FALSE(hasMap(18, "world.mm8.map.d24", false));
    CHECK(hasMap(18, "world.mm8.map.d24", true));
    // Quixote discloses Zog's fortress; Deftclaw only knows that Quixote stole his egg.
    CHECK(hasMap(31, "world.mm8.map.d25", false));
    CHECK_FALSE(hasMap(33, "world.mm8.map.d25", false));
    CHECK(hasMap(33, "world.mm8.map.d25", true));
    CHECK(hasMap(70, "world.mm8.map.out06", false));
    CHECK_FALSE(hasMap(70, "world.mm8.map.d20", false));
    CHECK(hasMap(70, "world.mm8.map.d20", true));
    // Vetrinus explicitly names both Khel's library and the Mad Necromancer's jar supply.
    CHECK(hasMap(82, "world.mm8.map.d40", false));
    CHECK(hasMap(82, "world.mm8.map.d20", false));
    CHECK(hasMap(46, "world.mm8.map.d10", false));
    CHECK_FALSE(hasMap(46, "world.mm8.map.d35", false));
    CHECK(hasMap(46, "world.mm8.map.d35", true));
    for (uint32_t qbit : {48, 50, 52, 54})
    {
        for (const std::string &prison : {"d36", "d37", "d38", "d39"})
            CHECK_FALSE(hasMap(qbit, "world.mm8.map." + prison, false));
    }
    CHECK(hasMap(48, "world.mm8.map.d37", true));
    CHECK(hasMap(50, "world.mm8.map.d39", true));
    CHECK(hasMap(52, "world.mm8.map.d38", true));
    CHECK(hasMap(54, "world.mm8.map.d36", true));
    CHECK(knowledge.quests.empty());
    CHECK(knowledge.questLocations.empty());
    CHECK(knowledge.questBits.empty());
}

TEST_CASE("MM8 delivery quests reveal individual recipients only after actual handoffs")
{
    JournalQuestTable quests;
    REQUIRE(quests.loadFromRows(guideTableRows("english/quests.txt")));
    REQUIRE(quests.loadLocationRows(guideTableRows("quest_locations.txt")));
    NpcDialogTable npcs;
    REQUIRE(npcs.loadNpcRows(guideTableRows("npc.txt")));
    WorldGuideSource source;
    source.pQuests = &quests;
    source.pNpcs = &npcs;
    source.worldId = "mm8";
    struct DeliveryQuest
    {
        uint32_t id;
        uint32_t firstDeliveryBit;
        std::vector<uint32_t> recipients;
    };
    for (const DeliveryQuest &delivery : std::vector<DeliveryQuest>{
        {101, 102, {163, 187, 139, 209, 80, 81}}, {142, 143, {119, 132, 135, 149, 82, 198}}})
    {
        CAPTURE(delivery.id);
        const auto quest = std::find_if(quests.entries().begin(), quests.entries().end(),
            [&](const JournalQuestEntry &entry) { return entry.qbitId == delivery.id; });
        REQUIRE(quest != quests.entries().end());
        if (delivery.id == 142)
        {
            CHECK(quest->text.find("Return to Pole in Rust.") != std::string::npos);
            CHECK(quest->text.find("Hobert") == std::string::npos);
        }
        WorldGuideKnowledge knowledge;
        knowledge.activeQuests.insert(delivery.id);
        knowledge.questBits.insert(delivery.id);
        const auto hasBit = [&](uint32_t id) { return knowledge.questBits.contains(id); };
        const auto accepted = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
        REQUIRE(accepted.size() == 1);
        const auto all = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, true);
        const auto guide = std::find_if(all.begin(), all.end(),
            [&](const WorldGuideEntry &entry) { return entry.id == std::to_string(delivery.id); });
        REQUIRE(guide != all.end());
        const JournalQuestKnowledgeChange acceptance =
            quests.knowledgeAroundChange(delivery.id, false, true, hasBit);
        CHECK(std::none_of(acceptance.locations.begin(), acceptance.locations.end(),
            [](const std::string &key) { return key.find("/location/objective/npc/") != std::string::npos; }));
        for (size_t i = 0; i < delivery.recipients.size(); ++i)
        {
            const NpcEntry *pNpc = npcs.getNpc(delivery.recipients[i]);
            REQUIRE(pNpc != nullptr);
            CHECK(accepted.front().details.find("Objective: " + pNpc->name) == std::string::npos);
            CHECK(guide->details.find("Objective: " + pNpc->name + " (conditional clue)") != std::string::npos);
        }
        CHECK(knowledge.questLocations.empty()); // Guide browsing grants no discoveries.
        // Reverse order checks that the delivery flags describe independent contacts, not a linear quest stage.
        for (size_t remaining = delivery.recipients.size(); remaining > 0; --remaining)
        {
            const size_t i = remaining - 1;
            const uint32_t bit = delivery.firstDeliveryBit + uint32_t(i);
            knowledge.questBits.insert(bit);
            const JournalQuestKnowledgeChange change = quests.knowledgeAroundChange(bit, false, true, hasBit);
            REQUIRE(change.locations.size() == 1);
            CHECK(change.locations.front() == "openyamm.discovery.quest/" + std::to_string(delivery.id)
                + "/location/objective/npc/" + std::to_string(delivery.recipients[i]));
            knowledge.questLocations.insert(change.locations.front());
            const auto partial = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
            REQUIRE(partial.size() == 1);
            for (size_t j = 0; j < delivery.recipients.size(); ++j)
            {
                const std::string label = "Objective: " + npcs.getNpc(delivery.recipients[j])->name;
                CHECK((partial.front().details.find(label) != std::string::npos) == (j >= i));
            }
        }
        knowledge.quests.insert(delivery.id);
        knowledge.activeQuests.clear();
        knowledge.questBits.clear();
        const auto history = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
        REQUIRE(history.size() == 1);
        for (uint32_t npc : delivery.recipients)
            CHECK(history.front().details.find("Objective: " + npcs.getNpc(npc)->name + " (recorded earlier)")
                != std::string::npos);
        // Old completion flags without recorded clue history cannot retroactively disclose recipients.
        knowledge.questLocations.clear();
        for (size_t i = 0; i < delivery.recipients.size(); ++i)
            knowledge.questBits.insert(delivery.firstDeliveryBit + uint32_t(i));
        const auto oldHistory = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
        REQUIRE(oldHistory.size() == 1);
        for (uint32_t npc : delivery.recipients)
            CHECK(oldHistory.front().details.find("Objective: " + npcs.getNpc(npc)->name) == std::string::npos);
    }
}

TEST_CASE("MM7 campaign relationships preserve the disclosed castle branch after a quest leaves the journal")
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
    source.pQuests = &quests; source.pMaps = &maps; source.pItems = &items; source.worldId = "mm7";
    WorldGuideKnowledge knowledge;
    const auto hasTarget = [](const WorldGuideEntry &entry, const std::string &target)
    {
        return std::any_of(entry.links.begin(), entry.links.end(),
            [&](const WorldGuideLink &link) { return link.target == target; });
    };
    knowledge.activeQuests = {694};
    knowledge.questBits = {611, 694};
    const auto accepted = quests.knowledgeAroundChange(694, false, true,
        [&](uint32_t bit) { return knowledge.questBits.contains(bit); });
    knowledge.quests.insert(accepted.quests.begin(), accepted.quests.end());
    knowledge.questLocations.insert(accepted.locations.begin(), accepted.locations.end());
    const auto light = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(light.size() == 1);
    CHECK(hasTarget(light[0], "world.mm7.map.7d30"));
    CHECK_FALSE(hasTarget(light[0], "world.mm7.map.d03"));
    knowledge.activeQuests.clear();
    knowledge.questBits = {612};
    const auto remembered = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(remembered.size() == 1);
    CHECK(hasTarget(remembered[0], "world.mm7.map.7d30"));
    CHECK_FALSE(hasTarget(remembered[0], "world.mm7.map.d03"));
    // A fresh Dark-path request discloses Castle Gloaming instead; Guide offers both alternatives.
    knowledge = {};
    knowledge.activeQuests = {694}; knowledge.questBits = {612, 694};
    const auto dark = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(dark.size() == 1);
    CHECK(hasTarget(dark[0], "world.mm7.map.d03"));
    CHECK_FALSE(hasTarget(dark[0], "world.mm7.map.7d30"));
    const auto guide = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, true);
    const auto tapestry = std::find_if(guide.begin(), guide.end(),
        [](const WorldGuideEntry &entry) { return entry.id == "694"; });
    REQUIRE(tapestry != guide.end());
    CHECK(hasTarget(*tapestry, "world.mm7.map.d03"));
    CHECK(hasTarget(*tapestry, "world.mm7.map.7d30"));
    CHECK(knowledge.questLocations.empty());
    CHECK(knowledge.quests.empty());

    knowledge.activeQuests = {528, 539, 533, 706};
    const auto clues = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, false);
    REQUIRE(clues.size() == 4);
    for (const WorldGuideEntry &entry : clues)
    {
        CHECK_FALSE(hasTarget(entry, "world.mm7.map.7d28"));
        CHECK_FALSE(hasTarget(entry, "world.mm7.map.7d13"));
        CHECK(entry.details.find("Contestant's Shield") == std::string::npos);
        CHECK(entry.details.find("Lady Carmine's Dagger") == std::string::npos);
        CHECK(entry.details.find("Arcomage Deck") == std::string::npos);
    }
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

TEST_CASE("chest inspections distinguish empty visible contents and unconfirmed overflow without disclosing loot")
{
    EventRuntimeState state;
    GameplayChestViewState view;
    view.chestId = 0;
    CHECK(worldChestObservations(state).empty());
    recordWorldChestView(state, view, 100);
    auto observations = worldChestObservations(state);
    REQUIRE(observations.size() == 1);
    CHECK(observations[0].chestId == 0);
    CHECK(observations[0].visibleSlots == 0);
    CHECK(observations[0].empty);
    CHECK(worldObservedItemSources(state).empty());
    GameplayChestItemState visible, hidden;
    visible.itemId = 42; hidden.itemId = 43;
    view.items = {visible}; view.hiddenItems = {hidden};
    recordWorldChestView(state, view, 200);
    observations = worldChestObservations(state);
    REQUIRE(observations.size() == 1);
    CHECK(observations[0].visibleSlots == 1);
    CHECK_FALSE(observations[0].empty);
    REQUIRE(worldObservedItemSources(state).size() == 1);
    CHECK(worldObservedItemSources(state)[0].itemId == 42);
    view.items.clear();
    recordWorldChestView(state, view, 300);
    observations = worldChestObservations(state);
    CHECK(observations[0].visibleSlots == 0);
    CHECK_FALSE(observations[0].empty); // Hidden overflow is never mislabeled as an empty chest.
    CHECK(worldObservedItemSources(state).size() == 1);
    view.hiddenItems.clear();
    recordWorldChestView(state, view, 400);
    observations = worldChestObservations(state);
    CHECK(observations[0].empty);
    CHECK(observations[0].gameMinutes == 400);
    CHECK(worldObservedItemSources(state).size() == 1); // Historical item sources are not erased by looting.
    const auto before = state.namedMapVars;
    recordWorldChestView(state, view, std::numeric_limits<float>::infinity());
    recordWorldChestView(state, view, -1);
    CHECK(state.namedMapVars == before);
    EventRuntimeState refilled;
    restoreWorldMapKnowledgeAfterRefill(state, refilled);
    REQUIRE(worldChestObservations(refilled).size() == 1);
    CHECK(worldChestObservations(refilled)[0].empty);
}

TEST_CASE("chest inspection records reject incomplete contradictory and noncanonical saved keys")
{
    EventRuntimeState state;
    state.namedMapVars["openyamm.discovery.item/chest/0/42/time"] = 100;
    CHECK(worldChestObservations(state).empty()); // Old item history cannot invent inspection status.
    const std::string key = "openyamm.discovery.chest/0/";
    state.namedMapVars[key + "time"] = 100;
    CHECK(worldChestObservations(state).empty());
    state.namedMapVars[key + "visible"] = 0;
    CHECK(worldChestObservations(state).empty());
    state.namedMapVars[key + "empty"] = 1;
    REQUIRE(worldChestObservations(state).size() == 1);
    for (const char *suffix : {"time", "visible", "empty"})
        state.namedMapVars[std::string("openyamm.discovery.chest/00/") + suffix] = state.namedMapVars.at(key + suffix);
    CHECK(worldChestObservations(state).size() == 1);
    state.namedMapVars[key + "visible"] = 1;
    CHECK(worldChestObservations(state).empty());
    state.namedMapVars[key + "empty"] = 2;
    CHECK(worldChestObservations(state).empty());
    state.namedMapVars[key + "empty"] = 0;
    REQUIRE(worldChestObservations(state).size() == 1);
    state.namedMapVars[key + "visible"] = -1;
    CHECK(worldChestObservations(state).empty());
}

TEST_CASE("creature loot identities are optional for old sources and reject invalid saved definitions")
{
    EventRuntimeState state;
    const std::string key = "openyamm.discovery.item/carried/3/540/";
    state.namedMapVars[key + "time"] = 100;
    state.namedMapVars[key + "x"] = 1;
    state.namedMapVars[key + "y"] = 2;
    state.namedMapVars[key + "z"] = 3;
    REQUIRE(worldObservedItemSources(state).size() == 1);
    CHECK(worldObservedItemSources(state)[0].monsterId == 0);
    state.namedMapVars[key + "monster"] = 45;
    CHECK(worldObservedItemSources(state)[0].monsterId == 45);
    state.namedMapVars[key + "monster"] = -1;
    CHECK(worldObservedItemSources(state).empty());
    state.namedMapVars[key + "monster"] = 32768;
    CHECK(worldObservedItemSources(state).empty());
    state.namedMapVars[key + "monster"] = 0;
    REQUIRE(worldObservedItemSources(state).size() == 1);
    CHECK(worldObservedItemSources(state)[0].monsterId == 0);
}

TEST_CASE("creature loot reference links require a recovered relationship in Discovery and preserve rule chances")
{
    OpenYAMM::Engine::AssetFileSystem assets;
    ItemTable items;
    REQUIRE(items.load(assets, guideTableRows("items.txt"), {}));
    REQUIRE(items.get(201) != nullptr);
    REQUIRE(items.get(540) != nullptr);
    const auto row = [](int id, const std::string &name)
    {
        std::vector<std::string> result(38, "0");
        result[0] = std::to_string(id); result[1] = result[2] = name;
        return result;
    };
    MonsterTable monsters;
    REQUIRE(monsters.loadStatsFromRows({row(45, "Dragonslayer"), row(85, "Dire Wolf")}));
    REQUIRE(monsters.loadDeathDropsFromRows({{"45", "Old monster name", "540", "Old item name", "100", "Rule"},
        {"85", "Old wolf", "201", "Old reagent", "20", "Rule"},
        {"45", "Invalid item", "999999", "Missing", "100", "Rule"},
        {"99", "Missing creature", "540", "Sword", "100", "Rule"}}));
    MapStats maps;
    REQUIRE(maps.loadFromRows(guideTableRows("map_stats.txt")));
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    WorldDatabase world(maps, houses, npcs, teachers, "mm7");
    WorldKnowledge known;
    known.visitedMaps.insert("7out02.odm");
    WorldGuideSource source;
    source.pItems = &items; source.pMonsters = &monsters;
    source.pWorld = &world; source.pWorldKnowledge = &known;
    WorldGuideKnowledge knowledge;
    knowledge.items = {201, 540}; knowledge.monsters = {45, 85};
    // Knowing both ends independently does not establish the relationship.
    for (const WorldGuideEntry &entry : buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false))
        CHECK(entry.links.empty());
    for (const WorldGuideEntry &entry : buildWorldGuide(source, WorldGuideSection::Items, knowledge, false))
        CHECK(entry.links.empty());
    const auto find = [](const std::vector<WorldGuideEntry> &entries, const std::string &id) -> const WorldGuideEntry &
    {
        const auto match = std::find_if(entries.begin(), entries.end(),
            [&](const WorldGuideEntry &entry) { return entry.id == id; });
        REQUIRE(match != entries.end());
        return *match;
    };
    const auto guideMonsters = buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, true);
    const auto &wolf = find(guideMonsters, "85");
    REQUIRE(wolf.links.size() == 1);
    CHECK(wolf.links[0].kind == WorldGuideLink::Kind::Item);
    CHECK(wolf.links[0].target == "201");
    CHECK(wolf.links[0].label == "Death drop (20%): " + items.get(201)->name);
    const auto &slayer = find(guideMonsters, "45");
    REQUIRE(slayer.links.size() == 1);
    CHECK(slayer.links[0].label == "Death drop (100%): " + items.get(540)->name);
    const auto guideItems = buildWorldGuide(source, WorldGuideSection::Items, knowledge, true);
    const auto &reagent = find(guideItems, "201");
    REQUIRE(reagent.links.size() == 1);
    CHECK(reagent.links[0].kind == WorldGuideLink::Kind::Monster);
    CHECK(reagent.links[0].label == "Death drop (20%): Dire Wolf");
    CHECK(world.features().empty()); // A drop rule is not a physical map position.
    WorldItemSource observation;
    observation.id = "observed/carried/3/540"; observation.kind = WorldItemSourceKind::Carried;
    observation.itemId = 540; observation.monsterId = 45;
    observation.position = {{1, 2, 3}}; observation.sourceIndex = 3; observation.observed = true;
    known.itemObservations["7out02.odm"] = {observation, observation}; // Repeated sightings yield one relation.
    world.indexObservedItemSources(known, items);
    const auto discovered = buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false);
    REQUIRE(find(discovered, "45").links.size() == 1);
    CHECK(find(discovered, "45").links[0].label == "Recovered loot: " + items.get(540)->name);
    CHECK(find(discovered, "85").links.empty());
    const auto discoveredItems = buildWorldGuide(source, WorldGuideSection::Items, knowledge, false);
    const auto &sword = find(discoveredItems, "540");
    CHECK(std::count_if(sword.links.begin(), sword.links.end(), [](const WorldGuideLink &link)
        { return link.kind == WorldGuideLink::Kind::Monster && link.target == "45"; }) == 1);
    CHECK(find(discoveredItems, "201").links.empty());
    WorldItemSource ground = observation;
    ground.id = "observed/death-drop/85/201"; ground.kind = WorldItemSourceKind::DeathDrop;
    ground.itemId = 201; ground.monsterId = 85; ground.sourceIndex = 85;
    known.itemObservations["7out02.odm"].push_back(ground);
    world.indexObservedItemSources(known, items);
    const auto groundMonsters = buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false);
    REQUIRE(find(groundMonsters, "85").links.size() == 1);
    CHECK(find(groundMonsters, "85").links[0].label == "Recovered loot: " + items.get(201)->name);
    known.itemObservations["7out02.odm"].pop_back();
    known.itemObservations["7out02.odm"][0].monsterId = 0;
    known.itemObservations["7out02.odm"].resize(1);
    world.indexObservedItemSources(known, items);
    CHECK(find(buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false), "45").links.empty());
}

TEST_CASE("ground creature loot is discovered only on receipt and never inherited by ordinary drops")
{
    EventRuntimeState state;
    GameplayWorldItemInspectState pickup;
    pickup.item.objectDescriptionId = 201;
    pickup.position = {{100, -200, 300}};
    pickup.lootMonsterId = 85;
    CHECK(worldObservedItemSources(state).empty());
    recordWorldItemPickup(state, pickup, 100);
    auto observed = worldObservedItemSources(state);
    REQUIRE(observed.size() == 1);
    CHECK(observed[0].kind == WorldItemSourceKind::DeathDrop);
    CHECK(observed[0].monsterId == 85);
    CHECK(observed[0].sourceIndex == 85);
    CHECK(observed[0].itemId == 201);
    CHECK(worldItemSourceLabel(observed[0]) == "Recovered ground drop");
    pickup.lootMonsterId = 86;
    recordWorldItemPickup(state, pickup, 101);
    pickup.lootMonsterId = 0;
    recordWorldItemPickup(state, pickup, 102);
    observed = worldObservedItemSources(state);
    REQUIRE(observed.size() == 3);
    CHECK(std::count_if(observed.begin(), observed.end(), [](const WorldItemSource &source)
        { return source.monsterId > 0; }) == 2);
    const auto ordinary = std::find_if(observed.begin(), observed.end(), [](const WorldItemSource &source)
        { return source.kind == WorldItemSourceKind::Placed; });
    REQUIRE(ordinary != observed.end());
    CHECK(ordinary->monsterId == 0);
    CHECK(ordinary->gameMinutes == 102);
    const auto before = state.namedMapVars;
    pickup.lootMonsterId = -1;
    recordWorldItemPickup(state, pickup, 103);
    CHECK(state.namedMapVars == before);
    EventRuntimeState refilled;
    restoreWorldMapKnowledgeAfterRefill(state, refilled);
    CHECK(worldObservedItemSources(refilled).size() == 3);
    for (const char *index : {"0", "085", "32768", "-1", "4294967296"})
        for (const char *suffix : {"time", "x", "y", "z"})
            state.namedMapVars[std::string("openyamm.discovery.item/death-drop/") + index + "/201/" + suffix] = 1;
    state.namedMapVars["openyamm.discovery.item/death-drop/87/201/time"] = 100; // Missing coordinates.
    CHECK(worldObservedItemSources(state).size() == 3);
}

TEST_CASE("world item source observations record visible chest slots and successful pickups with strict save keys")
{
    EventRuntimeState state;
    GameplayChestViewState chest;
    chest.chestId = 7;
    GameplayChestItemState visible, hidden, gold;
    visible.itemId = 42; hidden.itemId = 43; gold.itemId = 44; gold.isGold = true;
    chest.items = {visible, gold}; chest.hiddenItems = {hidden};
    recordWorldChestView(state, chest, 123);
    auto observed = worldObservedItemSources(state);
    REQUIRE(observed.size() == 1);
    CHECK(observed.front().itemId == 42);
    CHECK(observed.front().kind == WorldItemSourceKind::Chest);
    CHECK(observed.front().sourceIndex == 7);
    CHECK(observed.front().gameMinutes == 123);
    CHECK_FALSE(observed.front().position);
    const auto unchanged = state.namedMapVars;
    CHECK(worldObservedItemSources(state).size() == 1);
    CHECK(state.namedMapVars == unchanged);
    EventRuntimeState refilled;
    restoreWorldMapKnowledgeAfterRefill(state, refilled);
    REQUIRE(worldObservedItemSources(refilled).size() == 1);
    CHECK(worldObservedItemSources(refilled).front().itemId == 42);
    recordWorldChestView(state, chest, std::numeric_limits<float>::infinity());
    CHECK(state.namedMapVars == unchanged);
    GameplayWorldItemInspectState pickup;
    pickup.item.objectDescriptionId = 51;
    recordWorldItemPickup(state, pickup, 124); // Position absent: never invent the party's position as a source.
    CHECK(state.namedMapVars == unchanged);
    pickup.position = {{100.4f, -200.7f, 300}};
    recordWorldItemPickup(state, pickup, 124);
    observed = worldObservedItemSources(state);
    REQUIRE(observed.size() == 2);
    const auto found = std::find_if(observed.begin(), observed.end(),
        [](const WorldItemSource &source) { return source.itemId == 51; });
    REQUIRE(found != observed.end());
    CHECK(found->observed);
    REQUIRE(found->position);
    CHECK(*found->position == std::array<int32_t, 3>{100, -201, 300});
    CHECK(found->gameMinutes == 124);
    const auto afterPickup = state.namedMapVars;
    pickup.isGold = true;
    recordWorldItemPickup(state, pickup, 125);
    pickup.isGold = false; pickup.position = {{std::numeric_limits<float>::quiet_NaN(), 0, 0}};
    recordWorldItemPickup(state, pickup, 125);
    pickup.position = {{0, 0, 0}};
    recordWorldItemPickup(state, pickup, -1);
    CHECK(state.namedMapVars == afterPickup);
    for (const char *key : {"placed/0/52/time", "placed/01/51/time", "chest/7/043/time",
        "chest/-1/43/time", "chest/7/4294967296/time", "chest/7/0/time", "carried/0/43/time"})
        state.namedMapVars[std::string("openyamm.discovery.item/") + key] = 123;
    CHECK(worldObservedItemSources(state).size() == 2);
}

TEST_CASE("world item source catalog excludes random chest placeholders "
    "and keeps discovery separate from authored loot")
{
    OpenYAMM::Engine::AssetFileSystem assets;
    ItemTable items;
    ObjectTable objects;
    REQUIRE(items.load(assets, guideTableRows("items.txt"), {}));
    REQUIRE(objects.loadRows(guideTableRows("object_list.txt")));
    const auto item = std::find_if(items.entries().begin(), items.entries().end(), [](const ItemDefinition &entry)
    { return entry.itemId > 0 && !entry.name.empty() && entry.equipStat != "Gold"; });
    REQUIRE(item != items.entries().end());
    const uint32_t itemId = item->itemId;
    const auto object = std::find_if(objects.entries().begin(), objects.entries().end(), [](const ObjectEntry &entry)
    { return (entry.flags & 0x10) == 0; });
    REQUIRE(object != objects.entries().end());
    MapDeltaData delta;
    MapDeltaSpriteObject placed;
    placed.objectDescriptionId = uint16_t(std::distance(objects.entries().begin(), object));
    placed.x = 101; placed.y = 202; placed.z = 303; placed.rawContainingItem.resize(36);
    const auto putId = [](std::vector<uint8_t> &bytes, size_t offset, uint32_t id)
    { for (size_t i = 0; i < 4; ++i) bytes[offset + i] = uint8_t(id >> (8 * i)); };
    putId(placed.rawContainingItem, 0, itemId);
    delta.spriteObjects.push_back(placed);
    placed.semanticPolicyHidden = true; delta.spriteObjects.push_back(placed);
    MapDeltaChest chest;
    chest.rawItems.resize(36 * 3);
    putId(chest.rawItems, 0, itemId); putId(chest.rawItems, 36, itemId);
    putId(chest.rawItems, 72, uint32_t(-6));
    delta.chests.push_back(chest);
    MapDeltaActor actor; actor.carriedItemId = itemId; actor.x = -123;
    actor.monsterInfoId = 45; actor.monsterId = 85;
    delta.actors.push_back(actor);
    delta.actors.emplace_back(); // Zero means no carried item, even if table row zero has a placeholder name.
    const auto catalog = buildWorldItemSources(delta, items, objects);
    REQUIRE(catalog.size() == 3);
    CHECK(catalog[0].kind == WorldItemSourceKind::Placed);
    CHECK(catalog[1].kind == WorldItemSourceKind::Chest);
    CHECK(catalog[2].kind == WorldItemSourceKind::Carried);
    CHECK(catalog[2].monsterId == 45);
    CHECK(delta.chests[0].rawItems == chest.rawItems);
    MapStats maps;
    REQUIRE(maps.loadFromRows(guideTableRows("map_stats.txt")));
    HouseTable houses; NpcDialogTable npcs; MergedTeacherTopicTable teachers;
    WorldDatabase world(maps, houses, npcs, teachers, "mm7");
    const MapStatsEntry *pMap = maps.findByFileName("7out02.odm");
    REQUIRE(pMap != nullptr);
    world.indexMapItemSources(*pMap, catalog, items);
    world.indexMapItemSources(*pMap, catalog, items); // Re-index replaces, never accumulates duplicate sources.
    WorldKnowledge known;
    known.visitedMaps.insert("7out02.odm");
    CHECK(world.itemSources(known, false).empty());
    CHECK(world.itemSources(known, true).size() == 3);
    CHECK(world.queryFeatures(known, false).empty());
    CHECK(world.queryFeatures(known, true).size() == 2);
    EventRuntimeState state;
    GameplayWorldItemInspectState pickup; pickup.item.objectDescriptionId = itemId;
    pickup.position = {{101, 202, 303}};
    recordWorldItemPickup(state, pickup, 400);
    known.itemObservations["7out02.odm"] = worldObservedItemSources(state);
    world.indexObservedItemSources(known, items);
    REQUIRE(world.itemSources(known, false).size() == 1);
    CHECK(world.itemSources(known, false).front()->observed);
    REQUIRE(world.queryFeatures(known, false).size() == 1);
    CHECK(world.queryFeatures(known, false).front()->itemId == itemId);
    WorldGuideSource source;
    source.pItems = &items; source.pWorld = &world; source.pWorldKnowledge = &known;
    WorldGuideKnowledge knowledge; knowledge.items.insert(itemId);
    const auto entries = buildWorldGuide(source, WorldGuideSection::Items, knowledge, false);
    REQUIRE(entries.size() == 1);
    REQUIRE(entries.front().links.size() == 1);
    CHECK(entries.front().links.front().kind == WorldGuideLink::Kind::Feature);
    CHECK(entries.front().links.front().label.find("Last picked up: Harmondale") != std::string::npos);
    const auto allItems = buildWorldGuide(source, WorldGuideSection::Items, knowledge, true);
    CHECK(std::none_of(allItems.begin(), allItems.end(), [](const WorldGuideEntry &entry) { return entry.id == "0"; }));
    const auto before = known.itemObservations;
    CHECK(buildWorldGuide(source, WorldGuideSection::Items, knowledge, true).size() > 1);
    CHECK(known.itemObservations.size() == before.size());
    // Taking an item from a creature adds a historical feature, not its entire possible loot table.
    const std::string carried = "openyamm.discovery.item/carried/5/" + std::to_string(itemId) + "/";
    state.namedMapVars[carried + "x"] = -321;
    state.namedMapVars[carried + "y"] = 123;
    state.namedMapVars[carried + "z"] = 55;
    state.namedMapVars[carried + "time"] = 410;
    known.itemObservations["7out02.odm"] = worldObservedItemSources(state);
    world.indexObservedItemSources(known, items);
    const auto withCreature = buildWorldGuide(source, WorldGuideSection::Items, knowledge, false);
    REQUIRE(withCreature.size() == 1);
    REQUIRE(withCreature.front().links.size() == 2);
    const auto creature = std::find_if(withCreature.front().links.begin(), withCreature.front().links.end(),
        [](const WorldGuideLink &link) { return link.label.starts_with("Taken from creature: Harmondale"); });
    REQUIRE(creature != withCreature.front().links.end());
    CHECK(creature->kind == WorldGuideLink::Kind::Feature);
    CHECK(creature->label.find("(-321, 123)") != std::string::npos);
    known.visitedMaps.clear();
    CHECK(world.itemSources(known, false).empty());
    CHECK(world.queryFeatures(known, false).empty());
}

TEST_CASE("world scripted rewards follow active handlers and NPC topics without becoming discoveries")
{
    OpenYAMM::Engine::AssetFileSystem assets;
    ItemTable items;
    MapStats maps;
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    REQUIRE(items.load(assets, guideTableRows("items.txt"), {}));
    REQUIRE(maps.loadFromRows(guideTableRows("map_stats.txt")));
    REQUIRE(houses.loadFromRows(guideTableRows("house_data.txt")));
    REQUIRE(npcs.loadNpcRows(guideTableRows("npc.txt")));
    const auto item = std::find_if(items.entries().begin(), items.entries().end(), [](const ItemDefinition &entry)
    { return entry.itemId > 0 && !entry.name.empty() && entry.equipStat != "Gold"; });
    REQUIRE(item != items.entries().end());
    std::ifstream supportFile(std::string(OPENYAMM_SOURCE_DIR)
        + "/assets_dev/engine/scripts/common/event_support.lua");
    REQUIRE(supportFile.is_open());
    const std::string support{std::istreambuf_iterator<char>(supportFile), std::istreambuf_iterator<char>()};
    const std::string code = support + "\nRegisterEvent(10, '', function() end)\nSetMapItemRewards(10, {"
        + std::to_string(item->itemId) + "})\nRegisterGlobalEvent(11, '', function() end)\n"
        + "SetGlobalItemRewards(11, {" + std::to_string(item->itemId) + "})\n";
    std::string error;
    const auto local = ScriptedEventProgram::loadFromLuaText(code, "reward-map", ScriptedEventScope::Map, error);
    const auto global = ScriptedEventProgram::loadFromLuaText(code, "reward-npc", ScriptedEventScope::Global, error);
    REQUIRE(local.has_value());
    REQUIRE(global.has_value());
    for (const std::string worldId : {"mm7", "mm8"})
    {
        CAPTURE(worldId);
        const WorldDatabase original(maps, houses, npcs, teachers, worldId);
        const auto npc = std::find_if(original.records().begin(), original.records().end(), [](const WorldRecord &r)
        { return r.kind == WorldRecordKind::Npc && r.npc && r.map; });
        REQUIRE(npc != original.records().end());
        WorldNpcState current;
        current.topics[npc->npc->id] = {11, 11};
        WorldDatabase world(maps, houses, npcs, teachers, worldId, {}, current);
        WorldKnowledge known;
        known.visitedMaps.insert(npc->map->fileName);
        const std::vector<WorldInteractionSite> sites = {
            {"reward-site", 10, 101.4f, -202.7f, 303},
            {"invalid-site", 10, std::numeric_limits<float>::infinity(), 0, 0}};
        world.indexMapItemRewards(*npc->map, *local, sites, items);
        world.indexSharedItemRewards(*global, items);
        const auto all = world.itemSources(known, true);
        const auto rewardCount = [&](WorldItemSourceKind kind)
        { return std::count_if(all.begin(), all.end(), [kind](const auto *p) { return p->kind == kind; }); };
        CHECK(rewardCount(WorldItemSourceKind::MapReward) == 1);
        CHECK(rewardCount(WorldItemSourceKind::SharedReward) >= 1);
        const auto personal = std::find_if(all.begin(), all.end(), [&](const auto *p)
        { return p->providerId == npc->id; });
        REQUIRE(personal != all.end());
        CHECK((*personal)->sourceIndex == 11);
        CHECK(std::count_if(all.begin(), all.end(), [&](const auto *p) { return p->providerId == npc->id; }) == 1);
        REQUIRE(world.queryFeatures(known, true).size() == 1);
        CHECK(world.queryFeatures(known, true).front()->site.x == 101);
        CHECK(world.queryFeatures(known, true).front()->site.y == -203);
        CHECK(world.itemSources(known, false).empty());
        CHECK(world.queryFeatures(known, false).empty());
        WorldGuideSource source;
        source.pItems = &items; source.pWorld = &world; source.pWorldKnowledge = &known;
        WorldGuideKnowledge reference;
        reference.items.insert(item->itemId);
        const auto guide = buildWorldGuide(source, WorldGuideSection::Items, reference, true);
        const auto entry = std::find_if(guide.begin(), guide.end(), [&](const WorldGuideEntry &e)
        { return e.id == std::to_string(item->itemId); });
        REQUIRE(entry != guide.end());
        CHECK(std::any_of(entry->links.begin(), entry->links.end(), [&](const WorldGuideLink &link)
        { return link.target == npc->id && link.label.find("Possible NPC reward:") == 0; }));
        const auto discovery = buildWorldGuide(source, WorldGuideSection::Items, reference, false);
        REQUIRE(discovery.size() == 1);
        CHECK(discovery.front().links.empty());
        // Reindexing physical sources does not erase the independent scripted source layer.
        world.indexMapItemSources(*npc->map, {}, items);
        world.indexMapItemRewards(*npc->map, *local, sites, items);
        world.indexSharedItemRewards(*global, items);
        CHECK(world.itemSources(known, true).size() == all.size());
        // Replaced handlers invalidate old metadata even when the event number remains the same.
        const auto replaced = ScriptedEventProgram::loadFromLuaText(code
            + "\nevt.map[10] = function() end", "replacement", ScriptedEventScope::Map, error);
        REQUIRE(replaced.has_value());
        world.indexMapItemRewards(*npc->map, *replaced, sites, items);
        CHECK(world.queryFeatures(known, true).empty());
        current.topics[npc->npc->id] = {};
        WorldDatabase changed(maps, houses, npcs, teachers, worldId, {}, current);
        changed.indexSharedItemRewards(*global, items);
        const auto changedSources = changed.itemSources(known, true);
        CHECK(std::none_of(changedSources.begin(), changedSources.end(), [&](const auto *p)
        { return p->providerId == npc->id; }));
    }
}

TEST_CASE("world reward receipts reject stale pending origins and malformed discovery records")
{
    EventRuntimeState state;
    state.grantedItemIds = {42};
    stageWorldItemReward(state, false, 0, 10, true, 123);
    CHECK(worldObservedItemSources(state).empty());
    EventRuntimeState refilled;
    restoreWorldMapKnowledgeAfterRefill(state, refilled);
    CHECK(refilled.namedMapVars.empty()); // Pending offers do not survive a world refill as discoveries.
    state.grantedItemIds[0] = 43;
    confirmWorldItemReward(state, false, 0);
    CHECK(worldObservedItemSources(state).empty());
    stageWorldItemReward(state, false, 0, 10, true, 123);
    clearWorldItemRewardQueue(state);
    confirmWorldItemReward(state, false, 0);
    CHECK(worldObservedItemSources(state).empty());
    recordWorldItemReward(state, 43, 10, true, 124);
    REQUIRE(worldObservedItemSources(state).size() == 1);
    const auto valid = state.namedMapVars;
    recordWorldItemReward(state, 0, 10, true, 124);
    recordWorldItemReward(state, 43, 0, true, 124);
    recordWorldItemReward(state, 43, 10, true, -1);
    recordWorldItemReward(state, 43, 10, true, std::numeric_limits<float>::infinity());
    CHECK(state.namedMapVars == valid);
    for (const char *key : {"map-reward/0/42/time", "shared-reward/65536/42/time",
        "map-reward/01/42/time", "map-reward/1/042/time", "shared-reward/1/0/time"})
        state.namedMapVars[std::string("openyamm.discovery.item/") + key] = 123;
    CHECK(worldObservedItemSources(state).size() == 1);
    EventRuntimeState wideId;
    wideId.grantedItemIds = {UINT32_MAX};
    stageWorldItemReward(wideId, false, 0, 12, true, 125);
    confirmWorldItemReward(wideId, false, 0);
    REQUIRE(worldObservedItemSources(wideId).size() == 1);
    CHECK(worldObservedItemSources(wideId).front().itemId == UINT32_MAX);
}

TEST_CASE("world guide starting choices follow race creation rules and never reveal unknown race profiles")
{
    ClassMultiplierTable classes;
    REQUIRE(classes.loadFromRows({{"Class", "HP", "HP/level", "SP", "SP/level", "Mana"},
        {"CinderNovice", "20", "3", "5", "1", "Intellect"},
        {"CinderAdept", "25", "5", "8", "2", "Intellect"}}));
    ClassSkillTable skills;
    REQUIRE(skills.loadCapsFromRows({{"Skill", "CinderNovice", "CinderAdept"},
        {"Sword", "M", "G"}, {"Axe", "M", "G"}, {"Learning", "B", "E"}, {"Perception", "-", "-"}}));
    REQUIRE(skills.loadStartingSkillsFromRows({{"Class", "Sword", "Axe", "Learning", "Perception"},
        {"CinderNovice", "F", "C", "-", "-"}, {"CinderAdept", "F", "C", "-", "-"}}));
    MergedCharacterSelectionTable selection;
    MergedRaceSkillTable racial;
    std::string error;
    REQUIRE(selection.loadFromYaml("race_class_availability:\n"
        "- {race: Human, race_id: 0, classes: [CinderNovice]}\n"
        "- {race: Twin, race_id: 12, classes: [CinderNovice]}\n"
        "- {race: Windborn, race_id: 97, classes: [CinderNovice]}\n"
        "- {race: Unlisted, race_id: 98, classes: [CinderAdept]}\n"
        "new_game_continents:\n- {name: Testland, available_class_ids: [0], available_race_ids: [0,12,97,98]}\n",
        error));
    REQUIRE(racial.loadFromYaml("rules:\n"
        "- {race: Windborn, race_id: 97, skill: Perception, min_mastery: basic}\n"
        "- {race: Windborn, race_id: 97, skill: Learning, add: 1}\n", error));
    REQUIRE(skills.applyRaceSkillOverrides(racial));
    CHECK(skills.getEffectiveStartingSkillAvailability("CinderNovice", 97, "Perception")
        == StartingSkillAvailability::CanLearn);
    CHECK(skills.getEffectiveStartingSkillAvailability("CinderNovice", 97, "Learning")
        == StartingSkillAvailability::None);
    WorldGuideSource source;
    source.pClasses = &classes;
    source.pSkills = &skills;
    source.pCharacterSelection = &selection;
    WorldGuideKnowledge knowledge;
    knowledge.classes.insert("CinderNovice");
    knowledge.races.insert(0);
    const auto startingText = [](const WorldGuideEntry &entry)
    {
        const size_t start = entry.details.find("Starting skills by race:");
        return start == std::string::npos ? std::string{}
            : entry.details.substr(start, entry.details.find("Current class mastery limits:", start) - start);
    };
    auto discovered = buildWorldGuide(source, WorldGuideSection::Classes, knowledge, false);
    REQUIRE(discovered.size() == 1);
    const std::string known = startingText(discovered[0]);
    CHECK(known.find("Human\nGranted: Sword\nOptional choices: Axe") != std::string::npos);
    CHECK(known.find("Windborn") == std::string::npos);
    CHECK(known.find("Twin") == std::string::npos);
    CHECK(known.find("Perception") == std::string::npos);
    const auto guide = buildWorldGuide(source, WorldGuideSection::Classes, knowledge, true);
    const auto novice = std::find_if(guide.begin(), guide.end(),
        [](const WorldGuideEntry &entry) { return entry.id == "CinderNovice"; });
    REQUIRE(novice != guide.end());
    const std::string reference = startingText(*novice);
    CHECK(reference.find("Human, Twin\nGranted: Sword\nOptional choices: Axe") != std::string::npos);
    CHECK(reference.find("Windborn\nGranted: Sword\nOptional choices: Axe, Perception") != std::string::npos);
    CHECK(reference.find("Unlisted") == std::string::npos);
    CHECK(reference.find("Learning") == std::string::npos);
    CHECK(knowledge.races == std::unordered_set<uint32_t>{0});
    CHECK(knowledge.classes == std::unordered_set<std::string>{"CinderNovice"});
    knowledge.races.insert(97);
    discovered = buildWorldGuide(source, WorldGuideSection::Classes, knowledge, false);
    CHECK(startingText(discovered[0]).find("Windborn") != std::string::npos);
    // A data mod can replace the starting defaults without changing the mastery caps.
    REQUIRE(skills.loadStartingSkillsFromRows({{"Class", "Sword", "Axe"}, {"CinderNovice", "C", "F"}}));
    discovered = buildWorldGuide(source, WorldGuideSection::Classes, knowledge, false);
    CHECK(startingText(discovered[0]).find("Granted: Axe\nOptional choices: Sword") != std::string::npos);
    CHECK(skills.getClassCap("CinderNovice", "Sword") == SkillMastery::Master);
    source.pCharacterSelection = nullptr;
    discovered = buildWorldGuide(source, WorldGuideSection::Classes, knowledge, false);
    CHECK(startingText(discovered[0]).empty());
}

TEST_CASE("world item comparison validates both known items and uses active base definitions without mutations")
{
    const auto row = [](uint32_t id, const std::string &name, const std::string &dice, int bonus)
    {
        std::vector<std::string> cells(31);
        cells[0] = std::to_string(id); cells[2] = name; cells[3] = "500";
        cells[4] = "Weapon"; cells[5] = "Sword"; cells[6] = dice; cells[7] = std::to_string(bonus);
        cells[16] = "Authored item notes"; cells[26] = "Knight"; cells[27] = "15"; cells[28] = "10";
        return cells;
    };
    OpenYAMM::Engine::AssetFileSystem assets;
    ItemTable items;
    REQUIRE(items.load(assets, {row(3, "Known blade", "2d3", 1), row(97, "Secret blade", "3d4", 5)}, {}));
    WorldGuideSource source;
    source.pItems = &items;
    WorldGuideKnowledge knowledge;
    knowledge.items.insert(3);
    CHECK_FALSE(buildWorldItemComparison(source, knowledge, false, 3, 97));
    CHECK_FALSE(buildWorldItemComparison(source, knowledge, false, 97, 3));
    CHECK_FALSE(buildWorldItemComparison(source, knowledge, true, 3, 3));
    CHECK_FALSE(buildWorldItemComparison(source, knowledge, true, 0, 97));
    CHECK_FALSE(buildWorldItemComparison(source, knowledge, true, 3, UINT32_MAX));
    const auto guide = buildWorldItemComparison(source, knowledge, true, 3, 97);
    REQUIRE(guide);
    CHECK(guide->items[0] == items.get(3));
    CHECK(guide->items[1] == items.get(97));
    CHECK(guide->details[0].find("Attack: +1   Damage: 2d3+1") != std::string::npos);
    CHECK(guide->details[1].find("Attack: +5   Damage: 3d4+5") != std::string::npos);
    CHECK(guide->details[1].find("Required Might: 15") != std::string::npos);
    CHECK(guide->details[1].find("Required Accuracy: 10") != std::string::npos);
    CHECK(guide->details[1].find("Class families: Knight") != std::string::npos);
    CHECK(guide->details[1].find("Authored item notes") != std::string::npos);
    CHECK(knowledge.items == std::unordered_set<uint32_t>{3});
    CHECK_FALSE(buildWorldItemComparison(source, knowledge, false, 3, 97));
    knowledge.items.insert(97);
    REQUIRE(buildWorldItemComparison(source, knowledge, false, 3, 97));
    const auto catalog = buildWorldGuide(source, WorldGuideSection::Items, knowledge, false);
    const auto selected = std::find_if(catalog.begin(), catalog.end(),
        [](const WorldGuideEntry &entry) { return entry.id == "97"; });
    REQUIRE(selected != catalog.end());
    CHECK(selected->details == guide->details[1]);
    REQUIRE(items.load(assets, {row(3, "Known blade", "2d3", 1), row(97, "Modded blade", "4d5", -2)}, {}));
    const auto modded = buildWorldItemComparison(source, knowledge, false, 3, 97);
    REQUIRE(modded);
    CHECK(modded->items[1]->name == "Modded blade");
    CHECK(modded->details[1].find("Attack: -2   Damage: 4d5-2") != std::string::npos);
    // These historical restrictions are supplied by ItemRuntime even without authored class roots.
    auto classRow = row(515, "Restricted blade", "2d3", 1);
    auto raceRow = row(532, "Elven artifact", "3d4", 5);
    classRow.resize(17); raceRow.resize(17); raceRow[8] = "artifact";
    REQUIRE(items.load(assets, {classRow, raceRow}, {}));
    const auto legacy = buildWorldItemComparison(source, knowledge, true, 515, 532);
    REQUIRE(legacy);
    CHECK(legacy->details[0].find("Class family: Knight") != std::string::npos);
    CHECK(legacy->details[1].find("Restriction: Dark Elf or Elf") != std::string::npos);
    CHECK(legacy->details[1].find("Rarity: Artifact") != std::string::npos);
    source.pItems = nullptr;
    CHECK_FALSE(buildWorldItemComparison(source, knowledge, true, 3, 97));
}


TEST_CASE("world item identification hides true names statistics and comparisons until actually learned")
{
    const auto row = [](uint32_t id, const std::string &name)
    {
        std::vector<std::string> cells(31);
        cells[0] = std::to_string(id); cells[2] = name; cells[3] = "999";
        cells[4] = "Weapon"; cells[5] = "Sword"; cells[6] = "7d9"; cells[7] = "5";
        cells[9] = "20"; cells[10] = "Sword"; cells[16] = "Secret item powers";
        return cells;
    };
    OpenYAMM::Engine::AssetFileSystem assets;
    ItemTable items;
    REQUIRE(items.load(assets, {row(3, "Secret blade"), row(97, "Hidden blade")}, {}));
    WorldGuideSource source;
    source.pItems = &items;
    WorldGuideKnowledge knowledge;
    knowledge.items = {3, 97};
    const auto unknown = buildWorldGuide(source, WorldGuideSection::Items, knowledge, false);
    REQUIRE(unknown.size() == 2);
    for (const WorldGuideEntry &entry : unknown)
    {
        CHECK(entry.title == "Sword");
        CHECK(entry.details.find("Not identified") != std::string::npos);
        for (const char *pSecret : {"Secret", "Hidden", "999", "7d9", "Attack", "Value"})
            CHECK(entry.details.find(pSecret) == std::string::npos);
    }
    CHECK_FALSE(buildWorldItemComparison(source, knowledge, false, 3, 97));
    const auto guide = buildWorldGuide(source, WorldGuideSection::Items, knowledge, true);
    REQUIRE(guide.size() == 2);
    CHECK(guide[0].details.find("Secret item powers") != std::string::npos);
    CHECK(buildWorldItemComparison(source, knowledge, true, 3, 97));
    CHECK(knowledge.identifiedItems.empty());
    CHECK_FALSE(buildWorldItemComparison(source, knowledge, false, 3, 97));
    knowledge.identifiedItems.insert(3);
    CHECK_FALSE(buildWorldItemComparison(source, knowledge, false, 3, 97));
    knowledge.identifiedItems.insert(97);
    const auto known = buildWorldGuide(source, WorldGuideSection::Items, knowledge, false);
    REQUIRE(known.size() == 2);
    CHECK(known[0].title == "Hidden blade");
    CHECK(known[1].title == "Secret blade");
    CHECK(buildWorldItemComparison(source, knowledge, false, 3, 97));
    // An inspected chest can teach a type without granting ownership.
    knowledge.items.clear();
    CHECK(buildWorldGuide(source, WorldGuideSection::Items, knowledge, false).size() == 2);
    CHECK(buildWorldItemComparison(source, knowledge, false, 3, 97));
}

TEST_CASE("world item map titles and search honor identification history in Discovery and remain pure in Guide")
{
    std::vector<std::string> row(31);
    row[0] = "97"; row[2] = "Secret blade"; row[4] = "Weapon"; row[5] = "Sword";
    row[9] = "20"; row[10] = "Sword";
    OpenYAMM::Engine::AssetFileSystem assets;
    ItemTable items;
    REQUIRE(items.load(assets, {row}, {}));
    std::vector<std::string> mapRow(35);
    mapRow[0] = "63"; mapRow[1] = "Harmondale"; mapRow[2] = "7out02.odm"; mapRow[34] = "mm7";
    MapStats maps;
    REQUIRE(maps.loadFromRows({mapRow}));
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    WorldDatabase database(maps, houses, npcs, teachers, "mm7");
    WorldItemSource item;
    item.id = "test"; item.itemId = 97; item.observed = true;
    item.position = {{100, 200, 0}};
    WorldKnowledge knowledge;
    knowledge.visitedMaps.insert("7out02.odm");
    knowledge.itemObservations["7out02.odm"] = {item};
    database.indexObservedItemSources(knowledge, items);
    REQUIRE(database.features().size() == 1);
    const WorldMapFeature &feature = database.features()[0];
    CHECK(database.featureTitle(feature, knowledge, false).find("Secret") == std::string::npos);
    CHECK(database.featureTitle(feature, knowledge, false).find("Sword") != std::string::npos);
    CHECK(database.queryFeatures(knowledge, false, "Secret").empty());
    CHECK(database.queryFeatures(knowledge, false, "Sword").size() == 1);
    CHECK(database.queryFeatures(knowledge, true, "Secret").size() == 1);
    CHECK(knowledge.identifiedItems.empty());
    CHECK(database.queryFeatures(knowledge, false, "Secret").empty());
    knowledge.identifiedItems.insert(97);
    CHECK(database.queryFeatures(knowledge, false, "Secret").size() == 1);
    CHECK(database.featureTitle(feature, knowledge, false).find("Secret") != std::string::npos);
    // A recovered unknown item still has a real creature source; conceal its name rather than dropping the link.
    std::vector<std::string> monsterRow(38, "0");
    monsterRow[0] = "1"; monsterRow[1] = "Test beast"; monsterRow[2] = "Beast A";
    MonsterTable monsters;
    REQUIRE(monsters.loadStatsFromRows({monsterRow}));
    knowledge.itemObservations["7out02.odm"][0].kind = WorldItemSourceKind::Carried;
    knowledge.itemObservations["7out02.odm"][0].monsterId = 1;
    database.indexObservedItemSources(knowledge, items);
    WorldGuideSource source;
    source.pItems = &items; source.pMonsters = &monsters;
    source.pWorld = &database; source.pWorldKnowledge = &knowledge;
    WorldGuideKnowledge references;
    references.monsters.insert(1); references.items.insert(97);
    const auto bestiary = buildWorldGuide(source, WorldGuideSection::Monsters, references, false);
    REQUIRE(bestiary.size() == 1);
    REQUIRE(bestiary[0].links.size() == 1);
    CHECK(bestiary[0].links[0].label == "Recovered loot: Sword");
    CHECK(bestiary[0].links[0].target == "97");
    const auto catalog = buildWorldGuide(source, WorldGuideSection::Items, references, false);
    REQUIRE(catalog.size() == 1);
    CHECK(catalog[0].title == "Sword");
    CHECK(std::any_of(catalog[0].links.begin(), catalog[0].links.end(), [](const WorldGuideLink &link)
        { return link.kind == WorldGuideLink::Kind::Monster && link.target == "1"; }));
}


TEST_CASE("inherited decoration sites share binding slots and keep script-owned entities separate")
{
    DecorationTable decorations;
    const auto row = [](int id, const char *pName, const char *pHint)
    {
        std::vector<std::string> cells(15, "0");
        cells[0] = std::to_string(id); cells[1] = pName; cells[2] = pHint; cells[14].clear();
        return cells;
    };
    REQUIRE(decorations.loadRows({row(1, "tree", "Tree"), row(2, "dec32", "Barrel"),
        row(3, "crystl0", "Crystal")}));
    OutdoorMapData map;
    map.entities.resize(5);
    map.entities[0].name = "tree";
    map.entities[1].name = "dec32"; map.entities[1].eventIdSecondary = 42;
    map.entities[2].name = "dec32"; map.entities[2].eventIdPrimary = 55; map.entities[2].x = 100;
    map.entities[3].name = "dec32"; map.entities[3].x = 200;
    map.entities[4].name = "crystl0"; map.entities[4].x = 300;
    const auto sites = worldInheritedDecorationSites(map, decorations);
    REQUIRE(sites.size() == 3);
    CHECK(sites[0].site.id == "inherited-decoration/0");
    CHECK(sites[0].site.eventId == 268);
    CHECK(sites[0].name == "Barrel");
    CHECK(sites[0].eventCount == 8);
    CHECK(sites[0].site.x == 100);
    CHECK(sites[0].initialEventId >= 269);
    CHECK(sites[0].initialEventId <= 275);
    MapDeltaData authored;
    authored.eventVariables.decorVars[0] = 4;
    const auto original = authored.eventVariables.decorVars;
    const auto authoredSites = worldInheritedDecorationSites(map, decorations, &authored);
    REQUIRE(authoredSites.size() == 3);
    CHECK(authoredSites[0].initialEventId == 272);
    CHECK(authoredSites[1].initialEventId == 268);
    CHECK(authored.eventVariables.decorVars == original);
    CHECK(sites[1].site.id == "inherited-decoration/1");
    CHECK(sites[1].site.x == 200);
    CHECK(sites[2].site.eventId == 65310);
    CHECK(sites[2].name == "Crystal");
    CHECK(sites[2].eventCount == 1);
    CHECK(sites[2].site.x == 300);
    const auto direct = worldInteractionSites(map);
    REQUIRE(direct.size() == 1);
    CHECK(direct[0].eventId == 42);
    IndoorMapData indoor;
    for (const auto &entity : map.entities)
    {
        IndoorEntity copy;
        copy.name = entity.name; copy.eventIdPrimary = entity.eventIdPrimary;
        copy.eventIdSecondary = entity.eventIdSecondary; copy.x = entity.x;
        indoor.entities.push_back(copy);
    }
    const auto indoorSites = worldInheritedDecorationSites(indoor, decorations);
    REQUIRE(indoorSites.size() == sites.size());
    for (size_t i = 0; i < sites.size(); ++i)
    {
        CHECK(indoorSites[i].site.id == sites[i].site.id);
        CHECK(indoorSites[i].site.eventId == sites[i].site.eventId);
        CHECK(indoorSites[i].site.x == sites[i].site.x);
    }
    map.entities.assign(130, map.entities[3]);
    const auto bounded = worldInheritedDecorationSites(map, decorations);
    REQUIRE(bounded.size() == 125);
    CHECK(bounded.back().site.id == "inherited-decoration/124");
}

TEST_CASE("shared decoration variants discover only the interacted object and preserve local indexing")
{
    std::ifstream support(std::string(OPENYAMM_SOURCE_DIR) + "/assets_dev/engine/scripts/common/event_support.lua");
    REQUIRE(support);
    const std::string prefix{std::istreambuf_iterator<char>(support), std::istreambuf_iterator<char>()};
    std::string error;
    const auto global = ScriptedEventProgram::loadFromLuaText(prefix + R"(
        RegisterGlobalEvent(268, 'Empty barrel', function() error('must not execute') end)
        RegisterGlobalEvent(269, 'Red barrel', function() error('must not execute') end)
        RegisterGlobalEvent(270, 'Yellow barrel', function() error('must not execute') end)
        SetGlobalAutonoteRefs(270, {21})
        SetGlobalSiteReference(270, 'Accuracy +2')
        SetGlobalSiteReference(269, 'Might +2; contents change after use.')
        SetGlobalAutonoteRefs(269, {17})
    )", "shared-decoration-catalog", ScriptedEventScope::Global, error);
    REQUIRE_MESSAGE(global, error);
    const auto local = ScriptedEventProgram::loadFromLuaText(prefix + R"(
        RegisterEvent(269, 'Unrelated map event', function() error('must not execute') end)
    )", "local-decoration-catalog", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(local, error);
    MapStats maps;
    std::vector<std::string> row(35);
    row[0] = "63"; row[1] = "Harmondale"; row[2] = "7out02.odm"; row[34] = "mm7";
    REQUIRE(maps.loadFromRows({row}));
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    WorldDatabase world(maps, houses, npcs, teachers, "mm7");
    WorldDecorationSite first, second;
    first.site = {"inherited-decoration/0", 268, 100, 200, 300};
    first.eventCount = 8; first.initialEventId = 269; first.name = "Barrel";
    second = first; second.site.id = "inherited-decoration/1"; second.site.x = 400;
    world.indexInheritedDecorations(*maps.findById(63), {first, second}, *global);
    WorldKnowledge knowledge;
    knowledge.visitedMaps.insert("7out02.odm");
    CHECK(world.queryFeatures(knowledge, false).empty());
    REQUIRE(world.queryFeatures(knowledge, true).size() == 2);
    CHECK(knowledge.interactedSites.empty());
    knowledge.interactedSites["7out02.odm"].insert(worldSiteDiscoveryKey(270, second.site.id));
    const auto known = world.queryFeatures(knowledge, false);
    REQUIRE(known.size() == 1);
    CHECK(known[0]->site.x == 400);
    CHECK(world.siteReference(*known[0], knowledge, false).empty());
    CHECK(world.siteReference(*known[0], knowledge, true).find("Might +2") != std::string::npos);
    CHECK(known[0]->autonoteIds == std::vector<uint32_t>{17});
    CHECK(world.siteReference(*known[0], knowledge, true).find("Accuracy") == std::string::npos);
    knowledge.siteObservations[known[0]->mapId].push_back({second.site.id, 270, 100});
    CHECK(world.siteObservations(*known[0], knowledge).size() == 1);
    JournalAutonoteTable notes;
    REQUIRE(notes.loadFromRows(guideTableRows("english/autonote.txt")));
    knowledge.autonoteIds = {17, 21};
    const auto learned = world.siteJournalEntries(*known[0], notes, knowledge, false);
    REQUIRE(learned.size() == 1);
    CHECK((learned[0]->noteBit >> 16) == 21);
    const auto initial = world.siteJournalEntries(*known[0], notes, knowledge, true);
    REQUIRE(initial.size() == 1);
    CHECK((initial[0]->noteBit >> 16) == 17);
    OutdoorMapData geometry;
    geometry.worldId = "mm7"; geometry.fileName = "7out02.odm";
    world.indexMapLocations(geometry, *local);
    CHECK(world.queryFeatures(knowledge, true).size() == 2);
    world.indexInheritedDecorations(*maps.findById(63), {first, second}, *global);
    CHECK(world.queryFeatures(knowledge, true).size() == 2);
    CHECK(world.queryFeatures(knowledge, false).size() == 1);
}


TEST_CASE("shared decoration rules preserve outdoor binding compatibility for existing container saves")
{
    DecorationEntry flour;
    flour.internalName = "floursac"; flour.hint = "Sack";
    const auto shared = resolveInteractiveDecorationBindingSpec(flour, "floursac");
    REQUIRE(shared);
    CHECK(shared->baseEventId == 1741);
    CHECK_FALSE(resolveInteractiveDecorationBindingSpec(flour, "floursac",
        InteractiveDecorationRuleSet::OutdoorBindings));
    DecorationEntry trash;
    trash.internalName = "trasheap"; trash.hint = "Trash heap";
    const auto indoor = resolveInteractiveDecorationBindingSpec(trash, "trasheap");
    const auto outdoor = resolveInteractiveDecorationBindingSpec(trash, "trasheap",
        InteractiveDecorationRuleSet::OutdoorBindings);
    REQUIRE(indoor);
    REQUIRE(outdoor);
    CHECK(indoor->baseEventId == 1748);
    CHECK(outdoor->baseEventId == 281);
    CHECK(outdoor->eventCount == 4);
}

TEST_CASE("decoration reward catalogs retain object coordinates through other source reindexing")
{
    OpenYAMM::Engine::AssetFileSystem assets;
    ItemTable items;
    MapStats maps;
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    REQUIRE(items.load(assets, guideTableRows("items.txt"), {}));
    REQUIRE(maps.loadFromRows(guideTableRows("map_stats.txt")));
    std::ifstream support(std::string(OPENYAMM_SOURCE_DIR) + "/assets_dev/engine/scripts/common/event_support.lua");
    REQUIRE(support);
    const std::string prefix{std::istreambuf_iterator<char>(support), std::istreambuf_iterator<char>()};
    const std::string code = prefix + R"(
        RegisterGlobalEvent(65310, 'Crystal', function() error('must not execute') end)
        SetGlobalItemRewards(65310, {2060})
    )";
    std::string error;
    const auto global = ScriptedEventProgram::loadFromLuaText(code,
        "crystal-catalog", ScriptedEventScope::Global, error);
    REQUIRE_MESSAGE(global, error);
    for (const std::string worldId : {"mm7", "mm8"})
    {
        CAPTURE(worldId);
        WorldDatabase world(maps, houses, npcs, teachers, worldId);
        const auto region = std::find_if(world.records().begin(), world.records().end(), [](const WorldRecord &record)
            { return record.kind == WorldRecordKind::Region && record.map; });
        REQUIRE(region != world.records().end());
        const MapStatsEntry map = *region->map;
        WorldDecorationSite first;
        first.site = {"inherited-decoration/2", 65310, 100, -200, 300};
        first.decorVarIndex = 2; first.eventCount = 1; first.initialEventId = 65310;
        WorldDecorationSite second = first;
        second.site.id = "inherited-decoration/3"; second.site.x = 400; second.decorVarIndex = 3;
        WorldDecorationSite depleted = first;
        depleted.decorVarIndex = 4; depleted.initiallyDepleted = true;
        WorldDecorationSite invalid = first;
        invalid.site.x = std::numeric_limits<float>::infinity();
        const std::vector<WorldDecorationSite> sites = {first, second, depleted, invalid};
        world.indexDecorationItemRewards(map, sites, *global, items);
        WorldKnowledge known;
        known.visitedMaps.insert(toLowerCopy(map.fileName));
        auto sources = world.itemSources(known, true);
        REQUIRE(sources.size() == 2);
        CHECK(sources[0]->kind == WorldItemSourceKind::DecorationReward);
        CHECK(sources[0]->decorationSlot == 2);
        CHECK(sources[0]->sourceIndex == 65310);
        REQUIRE(sources[0]->position);
        CHECK((*sources[0]->position)[0] == 100);
        CHECK((*sources[1]->position)[0] == 400);
        CHECK(worldItemSourceLabel(*sources[0]) == "Possible object reward");
        CHECK(world.itemSources(known, false).empty());
        world.indexMapItemSources(map, {}, items);
        world.indexSharedItemRewards(*global, items);
        world.indexDecorationItemRewards(map, sites, *global, items);
        CHECK(world.itemSources(known, true).size() == 2);
        WorldGuideSource source;
        source.pItems = &items; source.pWorld = &world; source.pWorldKnowledge = &known;
        const auto guide = buildWorldGuide(source, WorldGuideSection::Items, {}, true);
        const auto gem = std::find_if(guide.begin(), guide.end(), [](const WorldGuideEntry &entry)
            { return entry.id == "2060"; });
        REQUIRE(gem != guide.end());
        REQUIRE(gem->links.size() == 2);
        CHECK(gem->links[0].kind == WorldGuideLink::Kind::Feature);
        CHECK(gem->links[0].label.starts_with("Possible object reward:"));
        const auto replaced = ScriptedEventProgram::loadFromLuaText(code
            + "\nReplaceGlobalEvent(65310, '', function() end)", "replaced-crystal", ScriptedEventScope::Global, error);
        REQUIRE_MESSAGE(replaced, error);
        world.indexDecorationItemRewards(map, sites, *replaced, items);
        CHECK(world.itemSources(known, true).empty());
        CHECK(world.queryFeatures(known, true).empty());
    }
}

TEST_CASE("decoration reward receipts keep exact slots and reject corrupt pending object origins")
{
    EventRuntimeState state;
    state.activeDecorationContext = EventRuntimeState::ActiveDecorationContext{
        .decorVarIndex = 2, .baseEventId = 65310, .currentEventId = 65310, .eventCount = 1,
        .hideWhenCleared = true, .position = {{100, -200, 300}}};
    state.grantedItemIds = {2060};
    // ChangeEvent can clear the object before the handler grants an item.
    state.activeDecorationContext->currentEventId = 0;
    stageWorldItemReward(state, false, 0, 65310, true, 123);
    CHECK(worldObservedItemSources(state).empty());
    state.activeDecorationContext.reset();
    confirmWorldItemReward(state, false, 0);
    auto sources = worldObservedItemSources(state);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0].kind == WorldItemSourceKind::DecorationReward);
    CHECK(sources[0].decorationSlot == 2);
    CHECK(sources[0].sourceIndex == 65310);
    REQUIRE(sources[0].position);
    CHECK(*sources[0].position == std::array<int32_t, 3>{100, -200, 300});
    CHECK(worldItemSourceLabel(sources[0]) == "Received from object");
    state.activeDecorationContext = EventRuntimeState::ActiveDecorationContext{
        .decorVarIndex = 3, .baseEventId = 65310, .currentEventId = 65310, .eventCount = 1,
        .hideWhenCleared = true, .position = {{400, -200, 300}}};
    recordWorldItemReward(state, 2060, 65310, true, 124);
    sources = worldObservedItemSources(state);
    REQUIRE(sources.size() == 2);
    CHECK(sources[1].decorationSlot == 3);
    const auto received = state.namedMapVars;
    for (const auto &[field, value] : std::vector<std::pair<std::string, int32_t>>{
        {"decoration-slot", -1}, {"decoration-slot", 125}, {"global", 0}})
    {
        state.namedMapVars = received;
        stageWorldItemReward(state, false, 0, 65310, true, 125);
        state.namedMapVars["openyamm.item-grant/ids/0/" + field] = value;
        confirmWorldItemReward(state, false, 0);
        CHECK(worldObservedItemSources(state).size() == 2);
    }
    state.namedMapVars = received;
    stageWorldItemReward(state, false, 0, 65310, true, 125);
    state.namedMapVars.erase("openyamm.item-grant/ids/0/z");
    confirmWorldItemReward(state, false, 0);
    CHECK(worldObservedItemSources(state).size() == 2);
    state.namedMapVars = received;
    stageWorldItemReward(state, false, 0, 65310, true, 125);
    state.namedMapVars.erase("openyamm.item-grant/ids/0/decoration-slot");
    confirmWorldItemReward(state, false, 0);
    CHECK(worldObservedItemSources(state).size() == 2);
    for (const char *key : {"decoration-reward/125/65310/2060/time", "decoration-reward/2/0/2060/time",
        "decoration-reward/2/65536/2060/time", "decoration-reward/02/65310/2060/time",
        "decoration-reward/2/65310/02060/time", "decoration-reward/2/65310/2060/extra/time"})
        state.namedMapVars[std::string("openyamm.discovery.item/") + key] = 125;
    CHECK(worldObservedItemSources(state).size() == 2);
    // An unrelated callback or a map-local event cannot claim this inherited object's provenance.
    recordWorldItemReward(state, 2060, 65311, true, 126);
    recordWorldItemReward(state, 2060, 65310, false, 126);
    sources = worldObservedItemSources(state);
    REQUIRE(sources.size() == 4);
    CHECK(std::count_if(sources.begin(), sources.end(), [](const WorldItemSource &source)
        { return source.decorationSlot.has_value(); }) == 2);
}


TEST_CASE("world guide companions retain Inn members in Discovery and distinguish current from starting profiles")
{
    std::vector<std::vector<std::string>> rows(2);
    for (uint32_t id : {1, 2, 3})
    {
        std::vector<std::string> row(124);
        row[0] = std::to_string(id); row[1] = "Companion " + std::to_string(id); row[2] = "knight";
        row[7] = "5"; row[8] = "20"; row[123] = "Authored biography";
        rows.push_back(row);
    }
    RosterTable roster;
    REQUIRE(roster.loadFromRows(rows));
    Party::Snapshot seed;
    Character active;
    active.rosterId = 1; active.name = "Renamed champion"; active.className = "champion"; active.level = 27;
    seed.members.push_back(active);
    AdventurersInnMember waiting;
    waiting.character.rosterId = 2; waiting.character.name = "Waiting ally";
    waiting.character.className = "cavalier"; waiting.character.level = 18;
    seed.adventurersInnMembers.push_back(waiting);
    Party party;
    party.restoreSnapshot(seed);
    WorldGuideSource source;
    source.pRoster = &roster;
    source.pParty = &party;
    const WorldGuideKnowledge knowledge;
    const auto entries = buildWorldGuide(source, WorldGuideSection::Companions, knowledge, false);
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].id == "1");
    CHECK(entries[0].details.find("Travelling with the party") != std::string::npos);
    CHECK(entries[0].details.find("Renamed champion") != std::string::npos);
    CHECK(entries[0].details.find("Level 27") != std::string::npos);
    CHECK(entries[0].details.find("Starting profile: Companion 1") != std::string::npos);
    CHECK(entries[0].details.find("Level: 5") != std::string::npos);
    CHECK(entries[1].id == "2");
    CHECK(entries[1].details.find("In the Adventurer's Inn roster") != std::string::npos);
    CHECK(entries[1].details.find("Level 18") != std::string::npos);
    const auto guide = buildWorldGuide(source, WorldGuideSection::Companions, knowledge, true);
    REQUIRE(guide.size() == 3);
    CHECK(guide[1].details.find("Inn unlock not recorded yet") == std::string::npos);
    CHECK(knowledge.roster.empty());
    CHECK(party.memberCount() == 1);
    CHECK(party.adventurersInnMembers().size() == 1);
    source.pParty = nullptr;
    CHECK(buildWorldGuide(source, WorldGuideSection::Companions, knowledge, false).empty());
    const auto reference = buildWorldGuide(source, WorldGuideSection::Companions, knowledge, true);
    REQUIRE(reference.size() == 3);
    CHECK(reference[0].details.find("Load a campaign") != std::string::npos);
}

TEST_CASE("world guide companion contacts follow typed topics while Discovery uses actual recorded invitations")
{
    std::vector<std::vector<std::string>> rows(2);
    std::vector<std::string> rosterRow(124);
    rosterRow[0] = "2"; rosterRow[1] = "Roster definition name"; rosterRow[2] = "cleric"; rosterRow[7] = "5";
    rows.push_back(rosterRow);
    RosterTable roster;
    REQUIRE(roster.loadFromRows(rows));
    NpcDialogTable npcs;
    REQUIRE(npcs.loadTextsFromRows({{"202", "Invitation"}, {"203", "Party full"}}));
    REQUIRE(npcs.loadTopicsFromRows({{"602", "Roster Join Event", "", "", "", ""}}));
    std::vector<std::string> npcRow(16);
    npcRow[0] = "42"; npcRow[1] = "Different NPC alias"; npcRow[6] = "11";
    REQUIRE(npcs.loadNpcRows({npcRow}));
    npcs.resolveSpecialTopics(roster);
    REQUIRE(npcs.getRosterJoinOfferForTopic(602));
    MapStats maps;
    std::vector<std::string> mapRow(35);
    mapRow[0] = "1"; mapRow[1] = "Island"; mapRow[2] = "out01.odm"; mapRow[34] = "mm8";
    std::vector<std::string> remoteRow = mapRow;
    remoteRow[0] = "2"; remoteRow[1] = "Remote island"; remoteRow[2] = "out02.odm";
    REQUIRE(maps.loadFromRows({mapRow, remoteRow}));
    HouseTable houses;
    std::vector<std::string> house(24);
    house[0] = "11"; house[2] = "House"; house[3] = "1"; house[5] = "Contact house";
    REQUIRE(houses.loadFromRows({house}));
    MergedTeacherTopicTable teachers;
    std::ifstream input(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    REQUIRE(input.is_open());
    const std::string support{std::istreambuf_iterator<char>(input), {}};
    std::string error;
    const auto program = ScriptedEventProgram::loadFromLuaText(support
        + "evt.global[10]=function() error('Never run recruitment') end\n"
          "SetGlobalNpcTopicRefs(10, {{42, 602}})", "@companion-contacts", ScriptedEventScope::Global, error);
    REQUIRE_MESSAGE(program, error);
    WorldDatabase world(maps, houses, npcs, teachers, "mm8");
    WorldKnowledge knowledge;
    WorldGuideKnowledge referenceKnowledge;
    referenceKnowledge.roster.insert(2);
    WorldGuideSource source;
    source.pRoster = &roster; source.pNpcs = &npcs; source.pHouses = &houses; source.pMaps = &maps;
    source.pWorld = &world; source.pWorldKnowledge = &knowledge; source.pGlobalEvents = &*program;
    source.worldId = "mm8";
    const auto guide = buildWorldGuide(source, WorldGuideSection::Companions, referenceKnowledge, true);
    REQUIRE(guide.size() == 1);
    REQUIRE(guide[0].links.size() == 1);
    CHECK(guide[0].links[0].target == "mm8:npc/42");
    CHECK(guide[0].details.find("Different NPC alias") != std::string::npos);
    CHECK(buildWorldGuide(source, WorldGuideSection::Companions, referenceKnowledge, false)[0].links.empty());
    source.pGlobalEvents = nullptr;
    const auto remote = ScriptedEventProgram::loadFromLuaText(support
        + "evt.map[10]=function() error('Do not execute a remote callback') end\n"
          "SetMapNpcTopicRefs(10, {{42, 602}})", "@remote-companion-contacts", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(remote, error);
    const MapStatsEntry *pRemote = maps.findByFileName("out02.odm");
    REQUIRE(pRemote);
    world.indexMapNpcTopics(*pRemote, *remote);
    const auto remoteGuide = buildWorldGuide(source, WorldGuideSection::Companions, referenceKnowledge, true);
    REQUIRE(remoteGuide[0].links.size() == 1);
    // The assignment's map must never replace the NPC's independently authored residence.
    CHECK(remoteGuide[0].links[0].target == "mm8:npc/42");
    CHECK(remoteGuide[0].links[0].label.find("Possible scripted contact") != std::string::npos);
    CHECK(remoteGuide[0].links[0].label.find("Remote island") == std::string::npos);
    CHECK(buildWorldGuide(source, WorldGuideSection::Companions, referenceKnowledge, false)[0].links.empty());
    const auto removed = ScriptedEventProgram::loadFromLuaText(support
        + "evt.map[10]=function() end", "@removed-companion-contacts", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(removed, error);
    world.indexMapNpcTopics(*pRemote, *removed);
    CHECK(buildWorldGuide(source, WorldGuideSection::Companions, referenceKnowledge, true)[0].links.empty());
    CHECK(knowledge.visitedMaps.empty());
    CHECK(knowledge.variables.empty());
    EventRuntimeState observed;
    recordWorldNpcObservation(observed, 42, 11, {});
    recordWorldRosterOfferObservation(observed, 42, 2);
    knowledge.variables = observed.namedGlobalVars;
    WorldDatabase recorded(maps, houses, npcs, teachers, "mm8", {},
        worldObservedNpcs(npcs, teachers, knowledge.variables));
    source.pWorld = &recorded;
    CHECK(buildWorldGuide(source, WorldGuideSection::Companions, referenceKnowledge, false)[0].links.empty());
    knowledge.visitedMaps.insert("out01.odm");
    const auto known = buildWorldGuide(source, WorldGuideSection::Companions, referenceKnowledge, false);
    REQUIRE(known.size() == 1);
    REQUIRE(known[0].links.size() == 1);
    CHECK(known[0].details.find("Last observed recruitment contact") != std::string::npos);
    CHECK(knowledge.visitedMaps.size() == 1);
    CHECK_THROWS_AS(recordWorldRosterOfferObservation(observed, 42, 0), std::invalid_argument);
    observed.namedGlobalVars[worldNpcRosterOfferKey(42, 2)] = 7;
    CHECK_THROWS_AS(worldRosterOfferKnown(observed.namedGlobalVars, 42, 2), std::invalid_argument);
}
