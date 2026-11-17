#include "engine/AssetFileSystem.h"
#include "engine/Localization.h"
#include "engine/TextCatalog.h"
#include "engine/Utf8Text.h"
#include "game/world/WorldDatabase.h"
#include "game/world/WorldGuide.h"
#include "game/tables/MonsterTable.h"
#include "game/tables/ItemTable.h"
#include "game/tables/JournalQuestTable.h"
#include "game/tables/RosterTable.h"
#include "game/party/Party.h"

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

using namespace OpenYAMM;
using namespace OpenYAMM::Game;

namespace
{
std::string atlasPolishCatalog()
{
    std::ifstream input(std::filesystem::path(OPENYAMM_SOURCE_DIR) / "assets_dev/languages/pl/text/engine-atlas.po",
                        std::ios::binary);
    REQUIRE(input.is_open());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct AtlasLanguageFixture
{
    std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("openyamm-atlas-language-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Engine::AssetFileSystem assets;

    AtlasLanguageFixture()
    {
        const std::filesystem::path development = root / "assets_dev";
        std::filesystem::create_directories(development / "engine/data_tables");
        const std::filesystem::path pack = development / "languages/pl";
        std::filesystem::create_directories(pack / "text");
        std::ofstream(pack / "language.yml") << "name: Polski\nenglish_name: Polish\n";
        std::ofstream(pack / "text/engine-atlas.po", std::ios::binary) << atlasPolishCatalog();
        std::ofstream(pack / "text/engine-fixture.po", std::ios::binary)
            << "msgid \"Atlas Goblin\"\nmsgstr \"Żółty goblin\"\n\n"
               "msgid \"Castle Fixture\"\nmsgstr \"Żółty zamek\"\n\n"
               "msgid \"Knight\"\nmsgstr \"Rycerz\"\n";
        std::ofstream(pack / "text/engine-fixture-items.po", std::ios::binary)
            << "msgid \"Fixture item note.\"\nmsgstr \"Opis próbnego przedmiotu.\"\n\n"
               "msgid \"Atlas Spear\"\nmsgstr \"Żółta włócznia\"\n";
        Engine::InstalledContentOptions options;
        options.languageId = "pl";
        REQUIRE(assets.initialize(root, development, Engine::AssetScaleTier::X1,
                                  Engine::createUniformAssetScaleProfile(Engine::AssetScaleTier::X1), "mm7", options));
        REQUIRE(Engine::activateLanguage(assets, "en"));
    }

    ~AtlasLanguageFixture()
    {
        Engine::activateLanguage(assets, "en");
        std::filesystem::remove_all(root);
    }
};
} // namespace

TEST_CASE("Atlas language: Polish entries retain all formatting and have no empty translations")
{
    std::string error;
    const auto catalog = Engine::parsePoFile(atlasPolishCatalog(), error);
    REQUIRE_MESSAGE(catalog.has_value(), error);
    CHECK(catalog->entries.size() > 500);
    for (const Engine::PoEntry &entry : catalog->entries)
    {
        CAPTURE(entry.english);
        CHECK_FALSE(entry.fuzzy);
        REQUIRE_FALSE(entry.translations.empty());
        for (const std::string &translation : entry.translations)
        {
            CHECK_FALSE(translation.empty());
            CHECK(Engine::translationMismatch(entry.english, translation).empty());
        }
    }
}

TEST_CASE("Atlas language: translating the bestiary preserves discovery and definition identities")
{
    AtlasLanguageFixture language;
    MonsterTable monsters;
    std::vector<std::string> known(38, "0"), secret(38, "0");
    known[0] = "1";
    known[1] = "Atlas Goblin";
    known[2] = "Goblin A";
    known[4] = "40";
    secret[0] = "2";
    secret[1] = "Undiscovered secret";
    secret[2] = "Dragon A";
    REQUIRE(monsters.loadStatsFromRows({known, secret}));
    WorldGuideSource source;
    source.pMonsters = &monsters;
    source.worldId = "mm7";
    WorldGuideKnowledge knowledge;
    knowledge.monsters.insert(1);
    const auto english = buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false);
    REQUIRE(english.size() == 1);
    CHECK(english.front().title == "Atlas Goblin");
    REQUIRE(Engine::activateLanguage(language.assets, "pl"));
    const auto polish = buildWorldGuide(source, WorldGuideSection::Monsters, knowledge, false);
    REQUIRE(polish.size() == 1);
    CHECK(polish.front().id == english.front().id);
    CHECK(polish.front().title == "Żółty goblin");
    CHECK(polish.front().details.find("Poziom:") != std::string::npos);
    CHECK(polish.front().details.find("Undiscovered secret") == std::string::npos);
    CHECK(knowledge.monsters == std::unordered_set<int16_t>{1});
    CHECK(monsters.statsEntries().at(1).name == "Atlas Goblin");
}

TEST_CASE("Atlas language: Polish searches preserve map IDs and do not translate personal notes")
{
    AtlasLanguageFixture language;
    REQUIRE(Engine::activateLanguage(language.assets, "pl"));
    std::vector<std::string> row(35);
    row[0] = "63";
    row[1] = "Castle Fixture";
    row[2] = "7out02.odm";
    row[34] = "mm7";
    MapStats maps;
    REQUIRE(maps.loadFromRows({row}));
    HouseTable houses;
    NpcDialogTable npcs;
    MergedTeacherTopicTable teachers;
    WorldDatabase world(maps, houses, npcs, teachers, "mm7");
    WorldKnowledge knowledge;
    CHECK(world.query(knowledge, false, "ŻÓŁTY").empty());
    const MapStatsEntry &map = *maps.findById(63);
    knowledge.variables[WorldDatabase::mapDiscoveryKey(map)] = 1;
    const auto results = world.query(knowledge, false, "ŻÓŁTY");
    REQUIRE(results.size() == 1);
    CHECK(results.front()->id == WorldDatabase::mapId(map));
    CHECK(results.front()->map->name == "Castle Fixture");
    CHECK(world.recordDisplayName(*results.front(), knowledge, false) == "Żółty zamek");
    CHECK(Engine::Utf8::lower("ZAŻÓŁĆ GĘŚLĄ JAŹŃ") == "zażółć gęślą jaźń");
    WorldMapFeature note;
    note.kind = WorldMapFeatureKind::Note;
    note.title = "Back";
    CHECK(world.featureTitle(note, knowledge, true) == "Back");
}

TEST_CASE("Atlas language: Polish plurals, fallback and player names retain their meaning")
{
    AtlasLanguageFixture language;
    REQUIRE(Engine::activateLanguage(language.assets, "pl"));
    for (const uint64_t days : {1, 2, 5, 12, 22})
        CHECK(Engine::trn("{count} day", "{count} days", days, {{"count", days}}) ==
              std::to_string(days) + (days == 1 ? " dzień" : " dni"));
    CHECK(Engine::tr("An untranslated future Atlas message.") == "An untranslated future Atlas message.");
    std::vector<std::vector<std::string>> rows(2);
    std::vector<std::string> row(124);
    row[0] = "42";
    row[1] = "cleric";
    row[2] = "cleric";
    row[7] = "5";
    rows.push_back(row);
    RosterTable roster;
    REQUIRE(roster.loadFromRows(rows));
    Character recruit;
    recruit.rosterId = 42;
    recruit.name = "Knight";
    recruit.className = "knight";
    Party::Snapshot snapshot;
    snapshot.members = {recruit};
    Party party;
    party.restoreSnapshot(snapshot);
    WorldGuideSource source;
    source.pRoster = &roster;
    source.pParty = &party;
    source.worldId = "mm7";
    WorldGuideKnowledge knowledge;
    const auto entries = buildWorldGuide(source, WorldGuideSection::Companions, knowledge, false);
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().title == "Knight");
    CHECK(party.members().front().name == "Knight");
    CHECK(Engine::localize("Knight") == "Rycerz");
    REQUIRE(Engine::activateLanguage(language.assets, "en"));
    CHECK(Engine::tr("Interactive Atlas") == "Interactive Atlas");
}

TEST_CASE("Atlas language: item reading text translates without rewriting equipment kinds or item data")
{
    AtlasLanguageFixture language;
    ItemTable items;
    std::vector<std::string> row(17);
    row[0] = "1";
    row[2] = "Atlas Spear";
    row[3] = "250";
    row[4] = "Weapon1or2";
    row[5] = "Spear";
    row[6] = "1d6";
    row[10] = "Spear";
    row[16] = "Fixture item note.";
    REQUIRE(items.load(language.assets, {row}, {}));
    WorldGuideSource source;
    source.pItems = &items;
    source.worldId = "mm7";
    WorldGuideKnowledge knowledge;
    knowledge.identifiedItems.insert(1);
    REQUIRE(Engine::activateLanguage(language.assets, "pl"));
    const auto entries = buildWorldGuide(source, WorldGuideSection::Items, knowledge, false);
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().title == "Żółta włócznia");
    CHECK(entries.front().details.find("Opis próbnego przedmiotu.") != std::string::npos);
    CHECK(entries.front().details.find("Broń jedno- lub dwuręczna") != std::string::npos);
    CHECK(entries.front().details.find("Weapon1or2") == std::string::npos);
    REQUIRE(items.get(1) != nullptr);
    CHECK(items.get(1)->equipStat == "Weapon1or2");
    CHECK(items.get(1)->notes == "Fixture item note.");
    WorldSiteObservation observation;
    observation.items.push_back({1, 1, 1});
    CHECK(describeWorldSiteObservation(observation, nullptr, &items, {1}).find("Żółta włócznia") != std::string::npos);
    JournalQuestTable quests;
    REQUIRE(quests.loadFromRows({{"3", "Find the spear", "", ""}}));
    REQUIRE(quests.loadLocationRows({{"3", "mm7", "objective", "item", "1", "journal"}}));
    source.pQuests = &quests;
    const auto questEntries = buildWorldGuide(source, WorldGuideSection::Quests, knowledge, true);
    REQUIRE(questEntries.size() == 1);
    CHECK(questEntries.front().details.find("Żółta włócznia") != std::string::npos);
    REQUIRE(questEntries.front().links.size() == 1);
    CHECK(questEntries.front().links.front().label.find("Żółta włócznia") != std::string::npos);
    CHECK(questEntries.front().links.front().target == "1");
    CHECK(items.get(1)->name == "Atlas Spear");
}
