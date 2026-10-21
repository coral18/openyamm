#include "doctest/doctest.h"

#include "game/gameplay/GameMechanics.h"
#include "game/gameplay/GameplayRuntimeInterfaces.h"
#include "game/events/EventRuntime.h"
#include "game/gameplay/JournalQuestRuntime.h"
#include "game/items/ItemRuntime.h"
#include "game/maps/SaveGame.h"
#include "game/world/WorldItemSources.h"
#include "game/world/WorldMapKnowledge.h"
#include "game/world/WorldMapNotes.h"
#include "game/world/WorldMonsterKnowledge.h"
#include "game/world/WorldMapKnowledge.h"
#include "game/party/Party.h"
#include "game/party/SpellIds.h"
#include "game/tables/CharacterDollTable.h"
#include "game/tables/JournalQuestTable.h"
#include "tests/RegressionGameData.h"
#include "tests/PartySpellTestHarness.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>

namespace
{
OpenYAMM::Game::Character makeRegressionPartyMember(
    const std::string &name,
    const std::string &className,
    const std::string &portraitTextureName,
    uint32_t characterDataId)
{
    OpenYAMM::Game::Character member = {};
    member.name = name;
    member.className = className;
    member.role = className;
    member.portraitTextureName = portraitTextureName;
    member.characterDataId = characterDataId;
    member.birthYear = 1160;
    member.experience = 0;
    member.level = 1;
    member.skillPoints = 5;
    member.might = 14;
    member.intellect = 14;
    member.personality = 14;
    member.endurance = 14;
    member.speed = 14;
    member.accuracy = 14;
    member.luck = 14;
    member.maxHealth = 40;
    member.health = 40;
    member.maxSpellPoints = 20;
    member.spellPoints = 20;
    return member;
}

OpenYAMM::Game::PartySeed createRegressionPartySeed()
{
    OpenYAMM::Game::PartySeed seed = {};
    seed.gold = 200;
    seed.food = 7;
    seed.members.push_back(makeRegressionPartyMember("Ariel", "Knight", "PC01-01", 1));
    seed.members.push_back(makeRegressionPartyMember("Brom", "Cleric", "PC03-01", 3));
    seed.members.push_back(makeRegressionPartyMember("Cedric", "Druid", "PC05-01", 5));
    seed.members.push_back(makeRegressionPartyMember("Daria", "Sorcerer", "PC07-01", 7));
    return seed;
}

OpenYAMM::Game::InventoryItem makeInventoryItem(uint32_t objectDescriptionId, uint8_t width, uint8_t height)
{
    OpenYAMM::Game::InventoryItem item = {};
    item.objectDescriptionId = objectDescriptionId;
    item.quantity = 1;
    item.width = width;
    item.height = height;
    item.identified = true;
    return item;
}

OpenYAMM::Game::Party makeInventoryParty()
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    for (size_t memberIndex = 0; memberIndex < party.members().size(); ++memberIndex)
    {
        OpenYAMM::Game::Character *pMember = party.member(memberIndex);

        if (pMember != nullptr)
        {
            pMember->inventory.clear();
        }
    }

    return party;
}

void fillInventory(OpenYAMM::Game::Character &member, uint32_t itemId)
{
    member.inventory.clear();

    for (int gridY = 0; gridY < OpenYAMM::Game::Character::InventoryHeight; ++gridY)
    {
        for (int gridX = 0; gridX < OpenYAMM::Game::Character::InventoryWidth; ++gridX)
        {
            REQUIRE(member.addInventoryItemAt(
                makeInventoryItem(itemId, 1, 1),
                static_cast<uint8_t>(gridX),
                static_cast<uint8_t>(gridY)));
        }
    }
}

bool hasPendingSpeech(
    const OpenYAMM::Game::Party &party,
    size_t memberIndex,
    OpenYAMM::Game::SpeechId speechId)
{
    for (const OpenYAMM::Game::Party::PendingAudioRequest &request : party.pendingAudioRequests())
    {
        if (request.kind == OpenYAMM::Game::Party::PendingAudioRequest::Kind::Speech
            && request.memberIndex == memberIndex
            && request.speechId == speechId)
        {
            return true;
        }
    }

    return false;
}

bool hasPendingSound(const OpenYAMM::Game::Party &party, OpenYAMM::Game::SoundId soundId)
{
    for (const OpenYAMM::Game::Party::PendingAudioRequest &request : party.pendingAudioRequests())
    {
        if (request.kind == OpenYAMM::Game::Party::PendingAudioRequest::Kind::Sound
            && request.soundId == soundId)
        {
            return true;
        }
    }

    return false;
}
}

TEST_CASE("default party seed only creates the first test member")
{
    OpenYAMM::Game::Party party = {};
    party.reset();

    const std::vector<OpenYAMM::Game::Character> &members = party.members();

    REQUIRE_EQ(members.size(), 1u);
    CHECK_EQ(members[0].name, "Ariel");
    CHECK_EQ(members[0].portraitPictureId, 2u);
    CHECK_EQ(members[0].characterDataId, 3u);
}

TEST_CASE("event granted inventory items queue loot sound")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    OpenYAMM::Game::EventRuntimeState runtimeState = {};
    runtimeState.grantedItems.push_back(makeInventoryItem(42, 1, 1));

    party.applyEventRuntimeState(runtimeState, true);

    CHECK(hasPendingSound(party, OpenYAMM::Game::SoundId::Gold));
}

TEST_CASE("scripted item discoveries require successful delivery and pending origins survive save load")
{
    using namespace OpenYAMM::Game;
    Party party = makeInventoryParty();
    for (size_t i = 0; i < party.members().size(); ++i) fillInventory(*party.member(i), 43);
    EventRuntimeState state;
    state.mapFileName = "7out02.odm";
    state.grantedItems.push_back(makeInventoryItem(42, 1, 1));
    state.grantedItemIds.push_back(44);
    stageWorldItemReward(state, true, 0, 100, false, 123);
    stageWorldItemReward(state, false, 0, 101, true, 124);
    CHECK(worldObservedItemSources(state).empty());
    party.applyEventRuntimeState(state, true); // Full inventory: neither queued offer is a received reward.
    CHECK(worldObservedItemSources(state).empty());
    GameSaveData save;
    save.mapFileName = state.mapFileName;
    save.party = party.snapshot();
    save.outdoorWorld.eventRuntimeState = state;
    const auto path = std::filesystem::temp_directory_path() / "openyamm_pending_item_reward.oysav";
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
    auto loaded = loadGameDataFromPath(path, error);
    std::filesystem::remove(path);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    REQUIRE(loaded->outdoorWorld.eventRuntimeState.has_value());
    state = *loaded->outdoorWorld.eventRuntimeState;
    for (size_t i = 0; i < party.members().size(); ++i) party.member(i)->inventory.clear();
    party.applyEventRuntimeState(state, false); // Synchronizing state is not delivery either.
    CHECK(worldObservedItemSources(state).empty());
    party.applyEventRuntimeState(state, true);
    const auto rewards = worldObservedItemSources(state);
    REQUIRE(rewards.size() == 2);
    CHECK(rewards[0].itemId == 42);
    CHECK(rewards[0].kind == WorldItemSourceKind::MapReward);
    CHECK(rewards[0].sourceIndex == 100);
    CHECK(rewards[0].gameMinutes == 123);
    CHECK_FALSE(rewards[0].position);
    CHECK(rewards[1].itemId == 44);
    CHECK(rewards[1].kind == WorldItemSourceKind::SharedReward);
    CHECK(rewards[1].gameMinutes == 124);
    EventRuntimeState refilled;
    restoreWorldMapKnowledgeAfterRefill(state, refilled);
    CHECK(worldObservedItemSources(refilled).size() == 2);
    // Old saves without queued provenance never acquire a guessed giver or reward event.
    state.namedMapVars.clear();
    party.applyEventRuntimeState(state, true);
    CHECK(worldObservedItemSources(state).empty());
}

TEST_CASE("actual event arrival provenance and coordinates survive normal save serialization")
{
    using namespace OpenYAMM::Game;
    EventRuntimeState state;
    state.mapFileName = "7d06.blv";
    REQUIRE(recordWorldMapArrival(state, "world.mm7.map.7out03", 105, 100, -200, -512, 1440));
    GameSaveData save;
    save.currentSceneKind = SceneKind::Indoor;
    save.mapFileName = state.mapFileName;
    save.indoorScene.eventRuntimeState = state;
    const auto path = std::filesystem::temp_directory_path() / "openyamm_arrival.oysav";
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
    const auto loaded = loadGameDataFromPath(path, error);
    std::filesystem::remove(path);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    REQUIRE(loaded->indoorScene.eventRuntimeState.has_value());
    const auto arrivals = worldMapArrivals(*loaded->indoorScene.eventRuntimeState, loaded->mapFileName);
    REQUIRE(arrivals.size() == 1);
    CHECK(arrivals[0].sourceMapId == "world.mm7.map.7out03");
    CHECK(arrivals[0].eventId == 105);
    CHECK(arrivals[0].x == 100);
    CHECK(arrivals[0].y == -200);
    CHECK(arrivals[0].z == -512);
    CHECK(arrivals[0].gameMinutes == 1440);
}

TEST_CASE("recorded creature loot identity survives save and refill independently of current actors")
{
    using namespace OpenYAMM::Game;
    EventRuntimeState state;
    const std::string key = "openyamm.discovery.item/carried/3/540/";
    state.namedMapVars[key + "time"] = 100;
    state.namedMapVars[key + "x"] = 1;
    state.namedMapVars[key + "y"] = 2;
    state.namedMapVars[key + "z"] = 3;
    state.namedMapVars[key + "monster"] = 45;
    GameSaveData save;
    save.mapFileName = "out05.odm";
    save.outdoorWorld.eventRuntimeState = state;
    const auto path = std::filesystem::temp_directory_path() / "openyamm_typed_creature_loot.oysav";
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
    const auto loaded = loadGameDataFromPath(path, error);
    std::filesystem::remove(path);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    REQUIRE(loaded->outdoorWorld.eventRuntimeState.has_value());
    EventRuntimeState refilled;
    restoreWorldMapKnowledgeAfterRefill(*loaded->outdoorWorld.eventRuntimeState, refilled);
    const auto observations = worldObservedItemSources(refilled);
    REQUIRE(observations.size() == 1);
    CHECK(observations[0].monsterId == 45);
    CHECK(observations[0].itemId == 540);
    CHECK(observations[0].gameMinutes == 100);
}

TEST_CASE("ground loot origins extend saves without changing the legacy core payload")
{
    using namespace OpenYAMM::Game;
    GameSaveData save;
    save.mapFileName = "out05.odm";
    OutdoorWorldRuntime::WorldItemState outside;
    outside.objectDescriptionId = 123;
    outside.lootMonsterId = 85;
    save.outdoorWorld.worldItems = {outside, {}};
    save.outdoorWorldStates["out02.odm"].worldItems = {outside};
    MapDeltaSpriteObject inside;
    inside.objectDescriptionId = 456;
    inside.lootMonsterId = 45;
    save.indoorScene.mapDeltaData.emplace();
    save.indoorScene.mapDeltaData->spriteObjects = {inside, {}};
    save.indoorSceneStates["d05.blv"].mapDeltaData.emplace();
    save.indoorSceneStates["d05.blv"].mapDeltaData->spriteObjects = {inside};
    const auto path = std::filesystem::temp_directory_path() / "openyamm_ground_loot_origins.oysav";
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
    const auto loaded = loadGameDataFromPath(path, error);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    CHECK(loaded->outdoorWorld.worldItems[0].lootMonsterId == 85);
    CHECK(loaded->outdoorWorld.worldItems[1].lootMonsterId == 0);
    CHECK(loaded->outdoorWorldStates.at("out02.odm").worldItems[0].lootMonsterId == 85);
    REQUIRE(loaded->indoorScene.mapDeltaData);
    CHECK(loaded->indoorScene.mapDeltaData->spriteObjects[0].lootMonsterId == 45);
    CHECK(loaded->indoorScene.mapDeltaData->spriteObjects[1].lootMonsterId == 0);
    CHECK(loaded->indoorSceneStates.at("d05.blv").mapDeltaData->spriteObjects[0].lootMonsterId == 45);
    const auto readBytes = [&]()
    {
        std::ifstream input(path, std::ios::binary);
        return std::vector<char>(std::istreambuf_iterator<char>(input), {});
    };
    const auto tagged = readBytes();
    save.outdoorWorld.worldItems[0].lootMonsterId = 0;
    save.outdoorWorldStates.at("out02.odm").worldItems[0].lootMonsterId = 0;
    save.indoorScene.mapDeltaData->spriteObjects[0].lootMonsterId = 0;
    save.indoorSceneStates.at("d05.blv").mapDeltaData->spriteObjects[0].lootMonsterId = 0;
    REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
    const auto legacy = readBytes();
    REQUIRE(tagged.size() > legacy.size());
    CHECK(std::equal(legacy.begin(), legacy.end(), tagged.begin()));
    const auto old = loadGameDataFromPath(path, error);
    REQUIRE_MESSAGE(old.has_value(), error);
    CHECK(old->outdoorWorld.worldItems[0].lootMonsterId == 0);
    CHECK(old->indoorScene.mapDeltaData->spriteObjects[0].lootMonsterId == 0);
    const auto reject = [&](const std::vector<char> &bytes, const std::string &message)
    {
        { std::ofstream output(path, std::ios::binary); output.write(bytes.data(), bytes.size()); }
        error.clear();
        CHECK_FALSE(loadGameDataFromPath(path, error).has_value());
        CHECK(error.find(message) != std::string::npos);
    };
    auto corrupted = tagged;
    corrupted.pop_back();
    reject(corrupted, "corrupted");
    corrupted = tagged;
    corrupted[legacy.size()] = '?';
    reject(corrupted, "corrupted");
    // Header: magic, packet count, length-prefixed namespace, schema, length-prefixed payload.
    const size_t schema = legacy.size() + 8 + 4 + 8 + std::string("openyamm.world-item-origins").size();
    corrupted = tagged;
    corrupted[schema] = 2;
    reject(corrupted, "unsupported save extension");
    const size_t firstRecord = schema + 4 + 8 + 4;
    corrupted = tagged;
    corrupted[firstRecord] = 4; // Unknown scene cannot attach an origin to an unrelated snapshot.
    reject(corrupted, "world item origin data is corrupted");
    corrupted = tagged;
    corrupted[firstRecord + 1 + 8] = 127; // Invalid object slot.
    reject(corrupted, "world item origin data is corrupted");
    corrupted = tagged;
    corrupted[firstRecord + 1 + 8 + 4] = 0; // Mismatched object signature.
    reject(corrupted, "world item origin data is corrupted");
    corrupted = tagged;
    corrupted[firstRecord + 1 + 8 + 4 + 2] = 0; // Invalid creature identity.
    reject(corrupted, "world item origin data is corrupted");
    corrupted = tagged;
    corrupted.push_back('?');
    reject(corrupted, "unexpected data");
    save.outdoorWorld.worldItems[0].lootMonsterId = -1;
    CHECK_FALSE(saveGameDataToPath(path, save, error));
    std::filesystem::remove(path);
}

TEST_CASE("empty chest observations survive normal save serialization without implying current loot state")
{
    using namespace OpenYAMM::Game;
    EventRuntimeState state;
    state.mapFileName = "7d06.blv";
    GameplayChestViewState view;
    view.chestId = 7;
    recordWorldChestView(state, view, 1440);
    GameSaveData save;
    save.currentSceneKind = SceneKind::Indoor;
    save.mapFileName = state.mapFileName;
    save.indoorScene.eventRuntimeState = state;
    const auto path = std::filesystem::temp_directory_path() / "openyamm_empty_chest.oysav";
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
    const auto loaded = loadGameDataFromPath(path, error);
    std::filesystem::remove(path);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    REQUIRE(loaded->indoorScene.eventRuntimeState.has_value());
    const auto observations = worldChestObservations(*loaded->indoorScene.eventRuntimeState);
    REQUIRE(observations.size() == 1);
    CHECK(observations[0].chestId == 7);
    CHECK(observations[0].visibleSlots == 0);
    CHECK(observations[0].empty);
    CHECK(observations[0].gameMinutes == 1440);
}

TEST_CASE("named monster observations survive normal save serialization and map refill")
{
    using namespace OpenYAMM::Game;
    EventRuntimeState state;
    state.mapFileName = "7d06.blv";
    recordWorldMonsterObservation(state, {3, 12, 100, -200, -512, true, 1440, 7});
    GameSaveData save;
    save.currentSceneKind = SceneKind::Indoor;
    save.mapFileName = state.mapFileName;
    save.indoorScene.eventRuntimeState = state;
    const auto path = std::filesystem::temp_directory_path() / "openyamm_named_monster.oysav";
    std::string error;
    REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
    const auto loaded = loadGameDataFromPath(path, error);
    std::filesystem::remove(path);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    REQUIRE(loaded->indoorScene.eventRuntimeState.has_value());
    EventRuntimeState refilled;
    restoreWorldMapKnowledgeAfterRefill(*loaded->indoorScene.eventRuntimeState, refilled);
    const auto observations = worldMonsterObservations(refilled);
    REQUIRE(observations.size() == 1);
    CHECK(observations[0].actorId == 3);
    CHECK(observations[0].monsterId == 12);
    CHECK(observations[0].uniqueNameIndex == 7);
    CHECK(observations[0].x == 100);
    CHECK(observations[0].y == -200);
    CHECK(observations[0].z == -512);
    CHECK(observations[0].dead);
    CHECK(observations[0].gameMinutes == 1440);
}

TEST_CASE("note elevation and unassigned legacy notes survive save and load independently")
{
    using namespace OpenYAMM::Game;
    EventRuntimeState state;
    state.mapFileName = "7d06.blv";
    WorldMapNoteEdit layered{state.mapFileName, 0, 100, 200, "Lower passage", false};
    layered.elevation = -512;
    WorldMapNoteEdit flat{state.mapFileName, 0, 300, 400, "Legacy flat note", false};
    std::string error;
    REQUIRE_MESSAGE(editWorldMapNote(state, state.mapFileName, layered, error), error);
    REQUIRE_MESSAGE(editWorldMapNote(state, state.mapFileName, flat, error), error);
    state.runtimeMapNotes[42] = {42, 10, 20, state.mapFileName, "Scripted landmark", true};
    state.namedMapVars["openyamm.map-notes.user/42/z"] = 1024; // No ownership: never assign script notes a user layer.
    GameSaveData save;
    save.currentSceneKind = SceneKind::Indoor;
    save.mapFileName = state.mapFileName;
    save.indoorScene.eventRuntimeState = state;
    const auto path = std::filesystem::temp_directory_path() / "openyamm_note_elevation.oysav";
    REQUIRE_MESSAGE(saveGameDataToPath(path, save, error), error);
    const auto loaded = loadGameDataFromPath(path, error);
    std::filesystem::remove(path);
    REQUIRE_MESSAGE(loaded.has_value(), error);
    REQUIRE(loaded->indoorScene.eventRuntimeState.has_value());
    EventRuntimeState restored = *loaded->indoorScene.eventRuntimeState;
    const auto notes = worldMapNotes(restored, state.mapFileName);
    REQUIRE(notes.size() == 3);
    CHECK(notes.back().elevation == -512);
    CHECK(notes.back().value.text == "Lower passage");
    CHECK_FALSE(notes[0].elevation);
    CHECK_FALSE(notes[1].elevation);
    layered.remove = true;
    REQUIRE_MESSAGE(editWorldMapNote(restored, state.mapFileName, layered, error), error);
    CHECK_FALSE(restored.namedMapVars.contains("openyamm.map-notes.user/" + std::to_string(layered.id) + "/z"));
    CHECK(worldMapNotes(restored, state.mapFileName).size() == 2);
}

TEST_CASE("party keeps global hired followers when applying a map-local event runtime state")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());
    party.addHiredNpcFollower({1001, 10, 300});
    party.addHiredNpcFollower({1002, 11, 400});

    OpenYAMM::Game::EventRuntimeState runtimeState = {};
    runtimeState.hiredNpcFollowers.push_back({1003, 12, 500});

    party.applyEventRuntimeState(runtimeState, false);

    OpenYAMM::Game::EventRuntimeState syncedRuntimeState = {};
    party.applyGlobalNpcStateTo(syncedRuntimeState);

    REQUIRE_EQ(syncedRuntimeState.hiredNpcFollowers.size(), 3u);
    CHECK_EQ(syncedRuntimeState.hiredNpcFollowers[0].npcId, 1001u);
    CHECK_EQ(syncedRuntimeState.hiredNpcFollowers[1].npcId, 1002u);
    CHECK_EQ(syncedRuntimeState.hiredNpcFollowers[2].npcId, 1003u);
    CHECK(syncedRuntimeState.unavailableNpcIds.contains(1001u));
    CHECK(syncedRuntimeState.unavailableNpcIds.contains(1002u));
    CHECK(syncedRuntimeState.unavailableNpcIds.contains(1003u));
}

TEST_CASE("party carries generated hired follower identity into a new map runtime")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::HiredNpcFollower follower = {};
    follower.npcId = 1184;
    follower.professionId = 20;
    follower.weeklyCost = 300;
    follower.name = "Generated Merchant";
    follower.pictureId = 123;
    party.addHiredNpcFollower(follower);

    OpenYAMM::Game::EventRuntimeState destinationRuntimeState = {};
    party.applyGlobalNpcStateTo(destinationRuntimeState);

    REQUIRE_EQ(destinationRuntimeState.hiredNpcFollowers.size(), 1u);
    CHECK_EQ(destinationRuntimeState.npcNameOverrides.at(1184), "Generated Merchant");
    CHECK_EQ(destinationRuntimeState.npcPictureOverrides.at(1184), 123u);
    CHECK_EQ(destinationRuntimeState.npcProfessionOverrides.at(1184), 20u);

    OpenYAMM::Game::GameSaveData saveData = {};
    saveData.mapFileName = "generated_follower_roundtrip.odm";
    saveData.party = party.snapshot();

    const std::filesystem::path savePath =
        std::filesystem::temp_directory_path() / "openyamm_generated_party_follower_roundtrip.oysav";
    std::string error;
    REQUIRE(OpenYAMM::Game::saveGameDataToPath(savePath, saveData, error));
    const std::optional<OpenYAMM::Game::GameSaveData> loaded =
        OpenYAMM::Game::loadGameDataFromPath(savePath, error);
    std::filesystem::remove(savePath);

    REQUIRE(loaded.has_value());
    REQUIRE_EQ(loaded->party.hiredNpcFollowers.size(), 1u);
    CHECK_EQ(loaded->party.hiredNpcFollowers.front().name, "Generated Merchant");
    CHECK_EQ(loaded->party.hiredNpcFollowers.front().pictureId, 123u);
}

TEST_CASE("current quest journal entries come from party qbits and non-empty quest text")
{
    OpenYAMM::Game::JournalQuestTable questTable = {};
    REQUIRE(questTable.loadFromRows({
        {"1", "Recover the crystal.", "Shown while active.", "test"},
        {"2", "Find the hidden port.", "Not active.", "test"},
        {"3", "", "Internal completion bit.", "test"},
    }));

    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());
    party.setQuestBit(1, true);
    party.setQuestBit(3, true);
    party.setQuestBit(999, true);

    const std::vector<std::string> questTexts =
        OpenYAMM::Game::buildCurrentQuestTexts(questTable, party);

    REQUIRE_EQ(questTexts.size(), 1u);
    CHECK_EQ(questTexts[0], "Recover the crystal.");
}

TEST_CASE("journal quest table rejects malformed qbit ids")
{
    OpenYAMM::Game::JournalQuestTable questTable = {};

    CHECK_FALSE(questTable.loadFromRows({
        {"Q Bit", "Quest Note Text", "Notes", "Owner"},
        {"abc", "Broken quest.", "", "test"},
    }));
    CHECK(questTable.entries().empty());
}

TEST_CASE("journal quest table rejects duplicate qbit ids")
{
    OpenYAMM::Game::JournalQuestTable questTable = {};

    CHECK_FALSE(questTable.loadFromRows({
        {"Q Bit", "Quest Note Text", "Notes", "Owner"},
        {"10000", "First custom quest.", "", "test"},
        {"10000", "Duplicate custom quest.", "", "test"},
    }));
    CHECK(questTable.entries().empty());
}

TEST_CASE("conditional journal quest overlays use required and forbidden qbits")
{
    OpenYAMM::Game::JournalQuestTable questTable = {};
    REQUIRE(questTable.loadFromRows({
        {"90001", "Complete training.", "", "MM9", "", "90473"},
        {"90002", "Search the island.", "", "MM9", "90473", "90480"},
        {"90003", "Find a boat.", "", "MM9", "90473;90474", "90481"},
    }));

    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());
    CHECK(OpenYAMM::Game::buildCurrentQuestTexts(questTable, party) == std::vector<std::string>{"Complete training."});

    party.setQuestBit(90473, true);
    CHECK(questTable.qbitChangeRevealsQuest(90473, false, true, party));
    CHECK(OpenYAMM::Game::buildCurrentQuestTexts(questTable, party) == std::vector<std::string>{"Search the island."});

    party.setQuestBit(90474, true);
    CHECK(questTable.qbitChangeRevealsQuest(90474, false, true, party));
    CHECK(OpenYAMM::Game::buildCurrentQuestTexts(questTable, party)
        == std::vector<std::string>{"Search the island.", "Find a boat."});

    party.setQuestBit(90480, true);
    CHECK(OpenYAMM::Game::buildCurrentQuestTexts(questTable, party) == std::vector<std::string>{"Find a boat."});

    party.setQuestBit(90480, false);
    CHECK(questTable.qbitChangeRevealsQuest(90480, true, false, party));
}

TEST_CASE("party quest bits survive save data round trip")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());
    party.setQuestBit(6, true);
    party.setQuestBit(36, true);

    OpenYAMM::Game::GameSaveData saveData = {};
    saveData.mapFileName = "quest_roundtrip.odm";
    saveData.party = party.snapshot();

    const std::filesystem::path savePath =
        std::filesystem::temp_directory_path() / "openyamm_quest_bits_roundtrip.oysav";
    std::string error;
    REQUIRE(OpenYAMM::Game::saveGameDataToPath(savePath, saveData, error));

    const std::optional<OpenYAMM::Game::GameSaveData> loaded =
        OpenYAMM::Game::loadGameDataFromPath(savePath, error);
    std::filesystem::remove(savePath);

    REQUIRE(loaded.has_value());

    OpenYAMM::Game::Party restoredParty = {};
    restoredParty.seed(createRegressionPartySeed());
    restoredParty.restoreSnapshot(loaded->party);

    CHECK(restoredParty.hasQuestBit(6));
    CHECK(restoredParty.hasQuestBit(36));
    CHECK_FALSE(restoredParty.hasQuestBit(37));
}

TEST_CASE("held cursor item survives save data round trip")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::InventoryItem heldItem = {};
    heldItem.objectDescriptionId = 643;
    heldItem.quantity = 1;
    heldItem.width = 1;
    heldItem.height = 2;
    heldItem.identified = true;
    heldItem.currentCharges = 3;
    heldItem.maxCharges = 5;

    OpenYAMM::Game::GameSaveData saveData = {};
    saveData.mapFileName = "held_cursor_roundtrip.odm";
    saveData.party = party.snapshot();
    saveData.heldInventoryItemActive = true;
    saveData.heldInventoryItem = heldItem;
    saveData.heldInventoryItemGrabCellOffsetX = 1;
    saveData.heldInventoryItemGrabCellOffsetY = 2;
    saveData.heldInventoryItemGrabOffsetX = 9.0f;
    saveData.heldInventoryItemGrabOffsetY = 17.0f;

    const std::filesystem::path savePath =
        std::filesystem::temp_directory_path() / "openyamm_held_cursor_roundtrip.oysav";
    std::string error;
    REQUIRE(OpenYAMM::Game::saveGameDataToPath(savePath, saveData, error));

    const std::optional<OpenYAMM::Game::GameSaveData> loaded =
        OpenYAMM::Game::loadGameDataFromPath(savePath, error);
    std::filesystem::remove(savePath);

    REQUIRE(loaded.has_value());
    CHECK(loaded->heldInventoryItemActive);
    CHECK(loaded->heldInventoryItem.objectDescriptionId == heldItem.objectDescriptionId);
    CHECK(loaded->heldInventoryItem.currentCharges == heldItem.currentCharges);
    CHECK(loaded->heldInventoryItem.maxCharges == heldItem.maxCharges);
    CHECK(loaded->heldInventoryItemGrabCellOffsetX == 1);
    CHECK(loaded->heldInventoryItemGrabCellOffsetY == 2);
    CHECK(loaded->heldInventoryItemGrabOffsetX == 9.0f);
    CHECK(loaded->heldInventoryItemGrabOffsetY == 17.0f);
}

TEST_CASE("party continent reputations survive save data round trip")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());
    party.setContinentReputation(3, 28);
    party.setContinentReputation(4, -11);

    OpenYAMM::Game::GameSaveData saveData = {};
    saveData.mapFileName = "reputation_roundtrip.odm";
    saveData.party = party.snapshot();

    const std::filesystem::path savePath =
        std::filesystem::temp_directory_path() / "openyamm_continent_reputation_roundtrip.oysav";
    std::string error;
    REQUIRE(OpenYAMM::Game::saveGameDataToPath(savePath, saveData, error));

    const std::optional<OpenYAMM::Game::GameSaveData> loaded =
        OpenYAMM::Game::loadGameDataFromPath(savePath, error);
    std::filesystem::remove(savePath);

    REQUIRE(loaded.has_value());

    OpenYAMM::Game::Party restoredParty = {};
    restoredParty.seed(createRegressionPartySeed());
    restoredParty.restoreSnapshot(loaded->party);

    CHECK_EQ(restoredParty.continentReputation(3), 28);
    CHECK_EQ(restoredParty.continentReputation(4), -11);
}

TEST_CASE("named runtime globals survive save data round trip")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::GameSaveData saveData = {};
    saveData.mapFileName = "named_global_roundtrip.odm";
    saveData.party = party.snapshot();
    saveData.namedGlobalVars["MMerge.CrossContinents.GotFinalQuest"] = 1;
    saveData.namedGlobalVars["MMerge.CrossContinents.GotFQHint2"] = 1;
    saveData.namedGlobalVars["MMerge.CrossContinents.GotFQHint3"] = 1;
    saveData.namedGlobalVars["MMerge.BountyHunt.new_sorpigal.odm.Month"] = 2;
    saveData.namedGlobalVars["MMerge.BountyHunt.new_sorpigal.odm.MonsterId"] = 10;
    saveData.namedGlobalVars["MMerge.BountyHunt.new_sorpigal.odm.Done"] = 1;
    saveData.namedGlobalVars["MMerge.BountyHunt.new_sorpigal.odm.Claimed"] = 0;

    const std::filesystem::path savePath =
        std::filesystem::temp_directory_path() / "openyamm_named_globals_roundtrip.oysav";
    std::string error;
    REQUIRE(OpenYAMM::Game::saveGameDataToPath(savePath, saveData, error));

    const std::optional<OpenYAMM::Game::GameSaveData> loaded =
        OpenYAMM::Game::loadGameDataFromPath(savePath, error);
    std::filesystem::remove(savePath);

    REQUIRE(loaded.has_value());
    REQUIRE(loaded->namedGlobalVars.contains("MMerge.CrossContinents.GotFinalQuest"));
    CHECK_EQ(loaded->namedGlobalVars.at("MMerge.CrossContinents.GotFinalQuest"), 1);
    CHECK_EQ(loaded->namedGlobalVars.at("MMerge.CrossContinents.GotFQHint2"), 1);
    CHECK_EQ(loaded->namedGlobalVars.at("MMerge.CrossContinents.GotFQHint3"), 1);
    CHECK_EQ(loaded->namedGlobalVars.at("MMerge.BountyHunt.new_sorpigal.odm.Month"), 2);
    CHECK_EQ(loaded->namedGlobalVars.at("MMerge.BountyHunt.new_sorpigal.odm.MonsterId"), 10);
    CHECK_EQ(loaded->namedGlobalVars.at("MMerge.BountyHunt.new_sorpigal.odm.Done"), 1);
    CHECK_EQ(loaded->namedGlobalVars.at("MMerge.BountyHunt.new_sorpigal.odm.Claimed"), 0);
}

TEST_CASE("outdoor location reset metadata survives save data round trip")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::GameSaveData saveData = {};
    saveData.currentSceneKind = OpenYAMM::Game::SceneKind::Outdoor;
    saveData.mapFileName = "out02.odm";
    saveData.party = party.snapshot();
    saveData.hasOutdoorRuntimeState = true;
    saveData.outdoorWorld.locationInfo.respawnCount = 3;
    saveData.outdoorWorld.locationInfo.lastRespawnDay = 124;
    saveData.outdoorWorld.locationInfo.reputation = -7;
    saveData.outdoorWorld.locationInfo.alertStatus = 1;
    saveData.outdoorWorld.locationTime.lastVisitTime = 987654;
    saveData.outdoorWorld.locationTime.skyTextureName = "plansky";
    saveData.outdoorWorld.hasLocationTime = true;
    saveData.outdoorWorld.faceAttributes = {0x20000000u, 0x00002000u, 0u};

    OpenYAMM::Game::ScriptedEventTimerState timer = {};
    timer.definition.scope = OpenYAMM::Game::ScriptedEventScope::Map;
    timer.definition.origin = OpenYAMM::Game::ScriptedEventTimerOrigin::Legacy;
    timer.definition.triggerKind = OpenYAMM::Game::ScriptedEventTimerTriggerKind::LongTimer;
    timer.definition.scheduleKind = OpenYAMM::Game::ScriptedEventTimerScheduleKind::Daily;
    timer.definition.eventId = 65000;
    timer.definition.sourceEventId = 130;
    timer.definition.triggerStep = 7;
    timer.definition.repeating = true;
    timer.definition.startSecond = 1;
    timer.nextAlarmGameMinutes = 1440.0 + 1.0 / 60.0;
    timer.eligibleGameMinutes = 540.5;
    saveData.outdoorWorld.timers.push_back(timer);
    saveData.outdoorWorldStates["out02.odm"] = saveData.outdoorWorld;
    saveData.outdoorWorldStates["legacy_absent.odm"] = {};

    const std::filesystem::path savePath =
        std::filesystem::temp_directory_path() / "openyamm_outdoor_location_info_roundtrip.oysav";
    std::string error;
    REQUIRE(OpenYAMM::Game::saveGameDataToPath(savePath, saveData, error));

    const std::optional<OpenYAMM::Game::GameSaveData> loaded =
        OpenYAMM::Game::loadGameDataFromPath(savePath, error);
    std::filesystem::remove(savePath);

    REQUIRE(loaded.has_value());
    REQUIRE(loaded->hasOutdoorRuntimeState);
    CHECK_EQ(loaded->outdoorWorld.locationInfo.respawnCount, 3);
    CHECK_EQ(loaded->outdoorWorld.locationInfo.lastRespawnDay, 124);
    CHECK_EQ(loaded->outdoorWorld.locationInfo.reputation, -7);
    CHECK_EQ(loaded->outdoorWorld.locationInfo.alertStatus, 1);
    CHECK(loaded->outdoorWorld.hasLocationTime);
    CHECK_EQ(loaded->outdoorWorld.locationTime.lastVisitTime, 987654);
    CHECK_EQ(loaded->outdoorWorld.locationTime.skyTextureName, "plansky");
    CHECK_EQ(loaded->outdoorWorld.faceAttributes, saveData.outdoorWorld.faceAttributes);
    REQUIRE_EQ(loaded->outdoorWorld.timers.size(), 1u);
    CHECK_EQ(loaded->outdoorWorld.timers[0].definition.eventId, 65000);
    CHECK_EQ(loaded->outdoorWorld.timers[0].definition.sourceEventId, 130);
    CHECK_EQ(loaded->outdoorWorld.timers[0].definition.triggerStep, 7);
    CHECK_EQ(
        loaded->outdoorWorld.timers[0].definition.triggerKind,
        OpenYAMM::Game::ScriptedEventTimerTriggerKind::LongTimer);
    CHECK_EQ(loaded->outdoorWorld.timers[0].nextAlarmGameMinutes, doctest::Approx(1440.0 + 1.0 / 60.0));
    REQUIRE(loaded->outdoorWorldStates.contains("out02.odm"));
    CHECK_EQ(loaded->outdoorWorldStates.at("out02.odm").locationInfo.lastRespawnDay, 124);
    CHECK_EQ(loaded->outdoorWorldStates.at("out02.odm").locationTime.lastVisitTime, 987654);
    CHECK_EQ(
        loaded->outdoorWorldStates.at("out02.odm").faceAttributes,
        saveData.outdoorWorld.faceAttributes);
    REQUIRE(loaded->outdoorWorldStates.contains("legacy_absent.odm"));
    CHECK_FALSE(loaded->outdoorWorldStates.at("legacy_absent.odm").hasLocationTime);
}

TEST_CASE("party ever owned item ids survive save data round trip")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    CHECK_FALSE(party.hasEverOwnedItem(2170));
    REQUIRE(party.tryGrantItem(2170));
    CHECK(party.hasEverOwnedItem(2170));
    CHECK(party.hasItemAnywhere(2170));

    REQUIRE(party.removeItem(2170));
    CHECK(party.hasEverOwnedItem(2170));
    CHECK_FALSE(party.hasItemAnywhere(2170));

    OpenYAMM::Game::GameSaveData saveData = {};
    saveData.mapFileName = "ever_owned_roundtrip.odm";
    saveData.party = party.snapshot();

    const std::filesystem::path savePath =
        std::filesystem::temp_directory_path() / "openyamm_ever_owned_roundtrip.oysav";
    std::string error;
    REQUIRE(OpenYAMM::Game::saveGameDataToPath(savePath, saveData, error));

    const std::optional<OpenYAMM::Game::GameSaveData> loaded =
        OpenYAMM::Game::loadGameDataFromPath(savePath, error);
    std::filesystem::remove(savePath);

    REQUIRE(loaded.has_value());

    OpenYAMM::Game::Party restoredParty = {};
    restoredParty.seed(createRegressionPartySeed());
    restoredParty.restoreSnapshot(loaded->party);

    CHECK(restoredParty.hasEverOwnedItem(2170));
    CHECK_FALSE(restoredParty.hasItemAnywhere(2170));
}

TEST_CASE("party records mm7 arcomage champion qbit after all antagarich taverns")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    party.recordArcomageWin(239, 0, 0);
    CHECK_FALSE(party.hasQuestBit(750));

    for (uint32_t houseId = 240; houseId < 252; ++houseId)
    {
        party.recordArcomageWin(houseId, 0, 0);
    }

    CHECK_FALSE(party.hasQuestBit(750));

    party.recordArcomageWin(252, 0, 0);
    CHECK(party.hasQuestBit(750));
}

TEST_CASE("party records mm8 arcomage champion qbit after all jadame taverns")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    party.recordArcomageWin(227, 0, 0);
    CHECK_FALSE(party.hasQuestBit(174));

    for (uint32_t houseId = 228; houseId < 238; ++houseId)
    {
        party.recordArcomageWin(houseId, 0, 0);
    }

    CHECK_FALSE(party.hasQuestBit(174));

    party.recordArcomageWin(238, 0, 0);
    CHECK(party.hasQuestBit(174));
}

TEST_CASE("party item ownership queries include the held item")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    const OpenYAMM::Game::InventoryItem item = makeInventoryItem(2119, 1, 1);

    CHECK_FALSE(party.hasEverOwnedItem(2119));
    CHECK_FALSE(party.hasItemAnywhere(2119));

    party.setHeldItemForQueries(item);
    CHECK(party.hasEverOwnedItem(2119));
    CHECK(party.hasItemAnywhere(2119));

    party.clearHeldItemForQueries();
    CHECK(party.hasEverOwnedItem(2119));
    CHECK_FALSE(party.hasItemAnywhere(2119));
}

TEST_CASE("party grants inventory items starting with active member")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    REQUIRE(party.setActiveMemberIndex(2));

    const OpenYAMM::Game::InventoryItem item = makeInventoryItem(2119, 1, 1);
    size_t recipientMemberIndex = 0;
    REQUIRE(party.tryGrantInventoryItem(item, &recipientMemberIndex));

    CHECK_EQ(recipientMemberIndex, 2u);
    CHECK_EQ(party.member(0)->inventoryItemCount(), 0u);
    CHECK_EQ(party.member(1)->inventoryItemCount(), 0u);
    CHECK_EQ(party.member(2)->inventoryItemCount(), 1u);
    CHECK_EQ(party.member(3)->inventoryItemCount(), 0u);
}

TEST_CASE("party grant falls through from full active inventory to next member")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    REQUIRE(party.setActiveMemberIndex(2));
    REQUIRE(party.member(2) != nullptr);
    fillInventory(*party.member(2), 109);

    const OpenYAMM::Game::InventoryItem item = makeInventoryItem(2119, 1, 1);
    size_t recipientMemberIndex = 0;
    REQUIRE(party.tryGrantInventoryItem(item, &recipientMemberIndex));

    CHECK_EQ(recipientMemberIndex, 3u);
    CHECK_EQ(party.member(2)->inventoryItemCount(), party.member(2)->inventoryCapacity());
    CHECK_EQ(party.member(3)->inventoryItemCount(), 1u);
}

TEST_CASE("party grant item ids use active member inventory rotation")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    REQUIRE(party.setActiveMemberIndex(1));
    REQUIRE(party.member(1) != nullptr);
    fillInventory(*party.member(1), 109);

    REQUIRE(party.tryGrantItem(2119));

    CHECK_EQ(party.member(0)->inventoryItemCount(), 0u);
    CHECK_EQ(party.member(1)->inventoryItemCount(), party.member(1)->inventoryCapacity());
    CHECK_EQ(party.member(2)->inventoryItemCount(), 1u);
    CHECK_EQ(party.member(3)->inventoryItemCount(), 0u);
}

TEST_CASE("party restore records currently held inventory as ever owned")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    REQUIRE(party.tryGrantItem(2122));

    OpenYAMM::Game::Party::Snapshot snapshot = party.snapshot();
    snapshot.everOwnedItemIds.clear();

    OpenYAMM::Game::Party restoredParty = {};
    restoredParty.seed(createRegressionPartySeed());
    restoredParty.restoreSnapshot(snapshot);

    CHECK(restoredParty.hasEverOwnedItem(2122));
    CHECK(restoredParty.hasItemAnywhere(2122));
}

TEST_CASE("monster target selection prefers matching living members")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    const std::optional<size_t> targetIndex = party.chooseMonsterAttackTarget(0x0010, 1);

    REQUIRE(targetIndex.has_value());
    CHECK_EQ(*targetIndex, 1u);
}

TEST_CASE("monster target selection skips invalid preferred members")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::Character *pCleric = party.member(1);
    OpenYAMM::Game::Character *pKnight = party.member(0);

    REQUIRE(pCleric != nullptr);
    REQUIRE(pKnight != nullptr);

    pCleric->conditions.set(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Unconscious));
    pKnight->conditions.set(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Dead));
    pKnight->health = 0;

    const std::optional<size_t> targetIndex = party.chooseMonsterAttackTarget(0x0010, 7);

    REQUIRE(targetIndex.has_value());
    CHECK_NE(*targetIndex, 0u);
    CHECK_NE(*targetIndex, 1u);
}

TEST_CASE("default seed monster target selection matches female preference")
{
    REQUIRE_MESSAGE(
        OpenYAMM::Tests::regressionGameDataLoaded(),
        OpenYAMM::Tests::regressionGameDataFailure().c_str());

    OpenYAMM::Game::Party party = {};
    party.setCharacterDollTable(&OpenYAMM::Tests::regressionGameData().characterDollTable);
    party.setClassMultiplierTable(&OpenYAMM::Tests::regressionGameData().classMultiplierTable);
    party.seed(OpenYAMM::Game::Party::createDefaultSeed());

    const std::optional<size_t> targetIndex = party.chooseMonsterAttackTarget(0x0400, 3);

    REQUIRE(targetIndex.has_value());
    CHECK_EQ(*targetIndex, 0u);
}

TEST_CASE("party damage sets unconscious within endurance threshold")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);

    pMember->health = 10;
    pMember->endurance = 14;

    REQUIRE(party.applyDamageToMember(0, 12, ""));
    CHECK(pMember->conditions.test(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Unconscious)));
    CHECK_FALSE(pMember->conditions.test(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Dead)));
}

TEST_CASE("party damage sets dead below endurance threshold")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);

    pMember->health = 10;
    pMember->endurance = 5;

    REQUIRE(party.applyDamageToMember(0, 20, ""));
    CHECK(pMember->conditions.test(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Dead)));
    CHECK_FALSE(pMember->conditions.test(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Unconscious)));
}

TEST_CASE("party damage queues mmmerge dying and last standing reactions")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    for (size_t memberIndex = 2; memberIndex < party.members().size(); ++memberIndex)
    {
        OpenYAMM::Game::Character *pMember = party.member(memberIndex);
        REQUIRE(pMember != nullptr);
        pMember->health = 0;
        pMember->conditions.set(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Unconscious));
    }

    OpenYAMM::Game::Character *pTargetMember = party.member(1);
    REQUIRE(pTargetMember != nullptr);
    pTargetMember->health = 10;
    pTargetMember->endurance = 5;

    REQUIRE(party.applyDamageToMember(1, 20, ""));
    CHECK(hasPendingSpeech(party, 1, OpenYAMM::Game::SpeechId::Dying));
    CHECK(hasPendingSpeech(party, 0, OpenYAMM::Game::SpeechId::LastPersonStanding));
}

TEST_CASE("party damage queues mmmerge fall and preservation reactions")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);
    pMember->health = 10;
    pMember->endurance = 5;
    party.applyCharacterBuff(
        0,
        OpenYAMM::Game::CharacterBuffId::Preservation,
        60.0f,
        1,
        0,
        0,
        OpenYAMM::Game::SkillMastery::None,
        0);

    REQUIRE(party.applyDamageToMember(0, 20, "falling"));
    CHECK(hasPendingSpeech(party, 0, OpenYAMM::Game::SpeechId::Falling));
    CHECK(hasPendingSpeech(party, 0, OpenYAMM::Game::SpeechId::CheatedDeath));
}

TEST_CASE("party fall damage uses OE integer health scaling")
{
    OpenYAMM::Game::Party party = makeInventoryParty();

    OpenYAMM::Game::Character *pFirstMember = party.member(0);
    OpenYAMM::Game::Character *pSecondMember = party.member(1);
    REQUIRE(pFirstMember != nullptr);
    REQUIRE(pSecondMember != nullptr);
    pFirstMember->maxHealth = 45;
    pFirstMember->health = 45;
    pSecondMember->maxHealth = 45;
    pSecondMember->health = 45;
    pSecondMember->featherFalling = true;

    OpenYAMM::Game::OutdoorMovementEffects effects = {};
    effects.maxFallDamageDistance = 768.0f;
    party.applyMovementEffects(effects);

    CHECK(pFirstMember->health == 33);
    CHECK(pSecondMember->health == 45);
}

TEST_CASE("party condition acquisition queues mmmerge condition reactions")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    REQUIRE(party.applyMemberCondition(0, OpenYAMM::Game::CharacterCondition::Drunk));
    REQUIRE(party.applyMemberCondition(1, OpenYAMM::Game::CharacterCondition::Insane));
    REQUIRE(party.applyMemberCondition(2, OpenYAMM::Game::CharacterCondition::Cursed));
    REQUIRE(party.applyMemberCondition(3, OpenYAMM::Game::CharacterCondition::Fear));

    CHECK(hasPendingSpeech(party, 0, OpenYAMM::Game::SpeechId::Drunk));
    CHECK(hasPendingSpeech(party, 1, OpenYAMM::Game::SpeechId::Insane));
    CHECK(hasPendingSpeech(party, 2, OpenYAMM::Game::SpeechId::Cursed));
    CHECK(hasPendingSpeech(party, 3, OpenYAMM::Game::SpeechId::AfraidSilent));
}

TEST_CASE("aoe damage can move unconscious member to dead")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);

    pMember->health = -1;
    pMember->endurance = 3;
    pMember->conditions.set(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Unconscious));

    CHECK_FALSE(party.applyDamageToMember(0, 5, ""));
    REQUIRE(party.applyDamageToMember(0, 5, "", true));
    CHECK_EQ(pMember->health, -6);
    CHECK(pMember->conditions.test(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Dead)));
    CHECK_FALSE(pMember->conditions.test(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Unconscious)));
}

TEST_CASE("debug damage immunity preserves hit handling while leaving health unchanged")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);

    pMember->health = 10;
    pMember->endurance = 5;
    party.setDebugDamageImmune(true);

    REQUIRE(party.applyDamageToMember(0, 20, "debug hit"));
    CHECK_EQ(pMember->health, 10);
    CHECK_FALSE(pMember->conditions.test(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Dead)));
    CHECK_FALSE(pMember->conditions.test(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Unconscious)));
    CHECK_EQ(party.lastStatus(), "debug hit");
}

TEST_CASE("gameplay selection blocks impairing conditions")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);

    const std::array<OpenYAMM::Game::CharacterCondition, 6> blockedConditions = {{
        OpenYAMM::Game::CharacterCondition::Asleep,
        OpenYAMM::Game::CharacterCondition::Paralyzed,
        OpenYAMM::Game::CharacterCondition::Unconscious,
        OpenYAMM::Game::CharacterCondition::Dead,
        OpenYAMM::Game::CharacterCondition::Petrified,
        OpenYAMM::Game::CharacterCondition::Eradicated,
    }};

    for (OpenYAMM::Game::CharacterCondition condition : blockedConditions)
    {
        pMember->conditions.reset();
        pMember->conditions.set(static_cast<size_t>(condition));
        CHECK_FALSE(party.canSelectMemberInGameplay(0));
    }

    pMember->conditions.reset();
    pMember->conditions.set(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Weak));
    CHECK(party.canSelectMemberInGameplay(0));

    pMember->conditions.reset();
    pMember->health = 0;
    CHECK_FALSE(party.canSelectMemberInGameplay(0));

    pMember->health = 10;
    pMember->recoverySecondsRemaining = 0.5f;
    CHECK(party.canSelectMemberInGameplay(0));
    CHECK_FALSE(OpenYAMM::Game::GameMechanics::canTakeGameplayAction(*pMember));
}

TEST_CASE("party shared experience uses learning and skips incapacitated members")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::Character *pFirst = party.member(0);
    OpenYAMM::Game::Character *pSecond = party.member(1);
    OpenYAMM::Game::Character *pThird = party.member(2);
    OpenYAMM::Game::Character *pFourth = party.member(3);
    REQUIRE(pFirst != nullptr);
    REQUIRE(pSecond != nullptr);
    REQUIRE(pThird != nullptr);
    REQUIRE(pFourth != nullptr);

    pFirst->skills["Learning"] = {"Learning", 5, OpenYAMM::Game::SkillMastery::Normal};
    pFourth->skills["Learning"] = {"Learning", 10, OpenYAMM::Game::SkillMastery::Grandmaster};
    pSecond->conditions.set(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Unconscious));
    pThird->conditions.set(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Eradicated));

    const uint32_t totalGrantedExperience = party.grantSharedExperience(200);

    CHECK_EQ(pFirst->experience, 114u);
    CHECK_EQ(pSecond->experience, 0u);
    CHECK_EQ(pThird->experience, 0u);
    CHECK_EQ(pFourth->experience, 159u);
    CHECK_EQ(totalGrantedExperience, 273u);
}

TEST_CASE("party-wide utility skills use the best actable member")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    OpenYAMM::Game::Character *pActiveMember = party.member(0);
    OpenYAMM::Game::Character *pExpert = party.member(1);
    OpenYAMM::Game::Character *pGrandmaster = party.member(2);
    REQUIRE(pActiveMember != nullptr);
    REQUIRE(pExpert != nullptr);
    REQUIRE(pGrandmaster != nullptr);

    pActiveMember->skills["RepairItem"] = {"RepairItem", 1, OpenYAMM::Game::SkillMastery::Normal};
    pExpert->skills["RepairItem"] = {"RepairItem", 20, OpenYAMM::Game::SkillMastery::Expert};
    pGrandmaster->skills["RepairItem"] = {"RepairItem", 1, OpenYAMM::Game::SkillMastery::Grandmaster};

    const std::optional<size_t> bestRepairMember = party.bestPartyWideUtilitySkillMemberIndex("RepairItem");
    REQUIRE(bestRepairMember.has_value());
    CHECK_EQ(*bestRepairMember, 2u);

    pGrandmaster->conditions.set(static_cast<size_t>(OpenYAMM::Game::CharacterCondition::Unconscious));
    const std::optional<size_t> bestActableRepairMember = party.bestPartyWideUtilitySkillMemberIndex("RepairItem");
    REQUIRE(bestActableRepairMember.has_value());
    CHECK_EQ(*bestActableRepairMember, 1u);

    pExpert->skills["DisarmTraps"] = {"DisarmTraps", 4, OpenYAMM::Game::SkillMastery::Expert};
    pExpert->skills["Merchant"] = {"Merchant", 4, OpenYAMM::Game::SkillMastery::Expert};
    pExpert->skills["IdentifyItem"] = {"IdentifyItem", 4, OpenYAMM::Game::SkillMastery::Expert};
    pExpert->skills["IdentifyMonster"] = {"IdentifyMonster", 4, OpenYAMM::Game::SkillMastery::Expert};
    pExpert->skills["Alchemy"] = {"Alchemy", 4, OpenYAMM::Game::SkillMastery::Expert};
    pExpert->skills["Perception"] = {"Perception", 4, OpenYAMM::Game::SkillMastery::Expert};

    CHECK(OpenYAMM::Game::Party::isPartyWideUtilitySkill("DisarmTraps"));
    CHECK(OpenYAMM::Game::Party::isPartyWideUtilitySkill("Merchant"));
    CHECK(OpenYAMM::Game::Party::isPartyWideUtilitySkill("IdentifyItem"));
    CHECK(OpenYAMM::Game::Party::isPartyWideUtilitySkill("IdentifyMonster"));
    CHECK(OpenYAMM::Game::Party::isPartyWideUtilitySkill("Alchemy"));
    CHECK(OpenYAMM::Game::Party::isPartyWideUtilitySkill("Perception"));
    CHECK_FALSE(OpenYAMM::Game::Party::isPartyWideUtilitySkill("Armsmaster"));
    CHECK(party.bestPartyWideUtilitySkillValue("DisarmTraps") > 0);
    CHECK(party.bestPartyWideUtilitySkillValue("Merchant") > 0);
    CHECK(party.bestPartyWideUtilitySkillValue("IdentifyItem") > 0);
    CHECK(party.bestPartyWideUtilitySkillValue("IdentifyMonster") > 0);
    CHECK(party.bestPartyWideUtilitySkillValue("Alchemy") > 0);
    CHECK(party.bestPartyWideUtilitySkillValue("Perception") > 0);
}

TEST_CASE("inventory identify and repair use best party utility skill")
{
    REQUIRE_MESSAGE(
        OpenYAMM::Tests::regressionGameDataLoaded(),
        OpenYAMM::Tests::regressionGameDataFailure().c_str());

    const OpenYAMM::Game::ItemDefinition *pDifficultItemDefinition = nullptr;

    for (const OpenYAMM::Game::ItemDefinition &itemDefinition : OpenYAMM::Tests::regressionGameData().itemTable.entries())
    {
        if (itemDefinition.itemId != 0
            && itemDefinition.identifyRepairDifficulty > 20
            && OpenYAMM::Game::ItemRuntime::requiresIdentification(itemDefinition))
        {
            pDifficultItemDefinition = &itemDefinition;
            break;
        }
    }

    REQUIRE(pDifficultItemDefinition != nullptr);

    OpenYAMM::Game::Party party = makeInventoryParty();
    party.setItemTable(&OpenYAMM::Tests::regressionGameData().itemTable);

    OpenYAMM::Game::Character *pActiveMember = party.member(0);
    OpenYAMM::Game::Character *pSkilledMember = party.member(1);
    REQUIRE(pActiveMember != nullptr);
    REQUIRE(pSkilledMember != nullptr);

    pActiveMember->skills["IdentifyItem"] = {"IdentifyItem", 1, OpenYAMM::Game::SkillMastery::Normal};
    pActiveMember->skills["RepairItem"] = {"RepairItem", 1, OpenYAMM::Game::SkillMastery::Normal};
    pSkilledMember->skills["IdentifyItem"] = {"IdentifyItem", 1, OpenYAMM::Game::SkillMastery::Grandmaster};
    pSkilledMember->skills["RepairItem"] = {"RepairItem", 1, OpenYAMM::Game::SkillMastery::Grandmaster};

    OpenYAMM::Game::InventoryItem item = {};
    item.objectDescriptionId = pDifficultItemDefinition->itemId;
    item.quantity = 1;
    item.width = std::max<uint8_t>(1, pDifficultItemDefinition->inventoryWidth);
    item.height = std::max<uint8_t>(1, pDifficultItemDefinition->inventoryHeight);
    item.identified = false;
    item.broken = true;
    REQUIRE(pActiveMember->addInventoryItemAt(item, 0, 0));

    std::string statusText;
    CHECK(party.tryIdentifyMemberInventoryItem(0, 0, 0, 0, statusText));
    const OpenYAMM::Game::InventoryItem *pItem = pActiveMember->inventoryItemAt(0, 0);
    REQUIRE(pItem != nullptr);
    CHECK(pItem->identified);

    CHECK(party.tryRepairMemberInventoryItem(0, 0, 0, 0, statusText));
    pItem = pActiveMember->inventoryItemAt(0, 0);
    REQUIRE(pItem != nullptr);
    CHECK_FALSE(pItem->broken);
}

TEST_CASE("Scholar hireling identifies inventory and equipment without character skills")
{
    REQUIRE_MESSAGE(
        OpenYAMM::Tests::regressionGameDataLoaded(),
        OpenYAMM::Tests::regressionGameDataFailure().c_str());

    const OpenYAMM::Game::ItemTable &itemTable = OpenYAMM::Tests::regressionGameData().itemTable;
    const OpenYAMM::Game::ItemDefinition *pDefinition = nullptr;
    for (const OpenYAMM::Game::ItemDefinition &definition : itemTable.entries())
    {
        if (definition.itemId != 0 && definition.equipStat == "Armor" && definition.identifyRepairDifficulty > 20
            && OpenYAMM::Game::ItemRuntime::requiresIdentification(definition))
        {
            pDefinition = &definition;
            break;
        }
    }
    REQUIRE(pDefinition != nullptr);

    OpenYAMM::Game::Party party = makeInventoryParty();
    party.setItemTable(&itemTable);
    for (size_t memberIndex = 0; memberIndex < party.members().size(); ++memberIndex)
    {
        party.member(memberIndex)->skills.clear();
    }
    OpenYAMM::Game::InventoryItem item = {};
    item.objectDescriptionId = pDefinition->itemId;
    item.quantity = 1;
    item.width = std::max<uint8_t>(1, pDefinition->inventoryWidth);
    item.height = std::max<uint8_t>(1, pDefinition->inventoryHeight);
    item.identified = false;
    REQUIRE(party.member(0)->addInventoryItemAt(item, 0, 0));

    std::string statusText;
    CHECK_FALSE(party.tryIdentifyMemberInventoryItem(0, 0, 0, 0, statusText));
    party.addHiredNpcFollower({1001, 1, 100});
    CHECK_FALSE(party.canIdentifyItem(*pDefinition));
    party.addHiredNpcFollower({1002, 4, 100});
    CHECK(party.tryIdentifyMemberInventoryItem(0, 0, 0, 0, statusText));
    CHECK(party.memberInventoryItem(0, 0, 0)->identified);
    CHECK_FALSE(party.tryIdentifyMemberInventoryItem(0, 0, 0, 0, statusText));
    party.member(0)->equipment.armor = pDefinition->itemId;
    party.member(0)->equipmentRuntime.armor.identified = false;
    CHECK(party.tryIdentifyEquippedItem(0, OpenYAMM::Game::EquipmentSlot::Armor, 0, statusText));
    CHECK(party.member(0)->equipmentRuntime.armor.identified);
    party.removeHiredNpcFollower(1002);
    CHECK_FALSE(party.canIdentifyItem(*pDefinition));
    party.member(0)->equipmentRuntime.armor.identified = false;
    CHECK_FALSE(party.tryIdentifyEquippedItem(0, OpenYAMM::Game::EquipmentSlot::Armor, 0, statusText));
}

TEST_CASE("member experience mutation clamps like OE")
{
    OpenYAMM::Game::Party party = {};
    party.seed(createRegressionPartySeed());

    CHECK_EQ(party.setMemberExperience(0, 3999999990u), 3999999990u);
    CHECK_EQ(party.addExperienceToMember(0, 1000), 4000000000u);
    CHECK_EQ(party.addExperienceToMember(0, -5000000000ll), 0u);
}

TEST_CASE("innate racial ability mastery grants its spells")
{
    SUBCASE("Dark Elf")
    {
        OpenYAMM::Game::Character darkElf = makeRegressionPartyMember("Elsbeth", "DarkElf", "PC12-01", 12);
        darkElf.skills["DarkElfAbility"] = {"DarkElfAbility", 1, OpenYAMM::Game::SkillMastery::Normal};
        darkElf.synchronizeInnateAbilitySpells();

        CHECK(darkElf.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Glamour)));
        CHECK_FALSE(darkElf.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::TravelersBoon)));
        CHECK(darkElf.setSkillMastery("DarkElfAbility", OpenYAMM::Game::SkillMastery::Expert));
        CHECK(darkElf.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::TravelersBoon)));
        CHECK_FALSE(darkElf.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Blind)));
        CHECK(darkElf.setSkillMastery("DarkElfAbility", OpenYAMM::Game::SkillMastery::Master));
        CHECK(darkElf.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Blind)));
        CHECK_FALSE(darkElf.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::DarkfireBolt)));
        CHECK(darkElf.setSkillMastery("DarkElfAbility", OpenYAMM::Game::SkillMastery::Grandmaster));
        CHECK(darkElf.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::DarkfireBolt)));
    }

    SUBCASE("Vampire")
    {
        OpenYAMM::Game::Character vampire = makeRegressionPartyMember("Vampira", "Vampire", "PC10-01", 10);
        vampire.skills["VampireAbility"] = {"VampireAbility", 1, OpenYAMM::Game::SkillMastery::Normal};
        vampire.synchronizeInnateAbilitySpells();

        CHECK(vampire.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Lifedrain)));
        CHECK_FALSE(vampire.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Levitate)));
        CHECK(vampire.setSkillMastery("VampireAbility", OpenYAMM::Game::SkillMastery::Expert));
        CHECK(vampire.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Levitate)));
    }

    SUBCASE("Dragon")
    {
        OpenYAMM::Game::Character dragon = makeRegressionPartyMember("Duroth", "Dragon", "PC13-01", 13);
        dragon.skills["DragonAbility"] = {"DragonAbility", 10, OpenYAMM::Game::SkillMastery::Normal};

        CHECK(dragon.setSkillMastery("DragonAbility", OpenYAMM::Game::SkillMastery::Expert));
        CHECK(dragon.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Fear)));
        CHECK(dragon.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::FlameBlast)));
        CHECK_FALSE(dragon.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Flight)));
        CHECK_FALSE(dragon.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::WingBuffet)));

        CHECK(dragon.setSkillMastery("DragonAbility", OpenYAMM::Game::SkillMastery::Master));
        CHECK(dragon.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Flight)));
        CHECK_FALSE(dragon.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::WingBuffet)));

        CHECK(dragon.setSkillMastery("DragonAbility", OpenYAMM::Game::SkillMastery::Grandmaster));
        CHECK(dragon.knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::WingBuffet)));
    }
}

TEST_CASE("new and restored Dark Elves repair innate spell ownership")
{
    OpenYAMM::Game::Character darkElf = makeRegressionPartyMember("Elspeth", "DarkElf", "PC12-01", 12);
    darkElf.raceId = 2;
    darkElf.skills["DarkElfAbility"] = {"DarkElfAbility", 1, OpenYAMM::Game::SkillMastery::Normal};

    OpenYAMM::Game::PartySeed seed = {};
    seed.members.push_back(darkElf);
    OpenYAMM::Game::Party party = {};
    party.seed(seed);

    OpenYAMM::Game::Character *pCreatedDarkElf = party.member(0);
    REQUIRE(pCreatedDarkElf != nullptr);
    CHECK(pCreatedDarkElf->knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Glamour)));

    pCreatedDarkElf->knownSpellIds.clear();
    pCreatedDarkElf->skills["DarkElfAbility"].mastery = OpenYAMM::Game::SkillMastery::Expert;
    const OpenYAMM::Game::Party::Snapshot oldSaveSnapshot = party.snapshot();

    OpenYAMM::Game::Party restoredParty = {};
    restoredParty.restoreSnapshot(oldSaveSnapshot);
    const OpenYAMM::Game::Character *pRestoredDarkElf = restoredParty.member(0);
    REQUIRE(pRestoredDarkElf != nullptr);
    CHECK(pRestoredDarkElf->knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Glamour)));
    CHECK(pRestoredDarkElf->knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::TravelersBoon)));
    CHECK_FALSE(pRestoredDarkElf->knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Blind)));
}

TEST_CASE("default party seed grants first member full spell access and preserves inventory")
{
    REQUIRE_MESSAGE(
        OpenYAMM::Tests::regressionGameDataLoaded(),
        OpenYAMM::Tests::regressionGameDataFailure().c_str());

    OpenYAMM::Game::Party party = {};
    party.setClassMultiplierTable(&OpenYAMM::Tests::regressionGameData().classMultiplierTable);
    party.seed(OpenYAMM::Game::Party::createDefaultSeed());

    const OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);

    static constexpr std::array<const char *, 12> ExpectedSpellSkills = {{
        "FireMagic",
        "AirMagic",
        "WaterMagic",
        "EarthMagic",
        "SpiritMagic",
        "MindMagic",
        "BodyMagic",
        "LightMagic",
        "DarkMagic",
        "DarkElfAbility",
        "VampireAbility",
        "DragonAbility",
    }};

    REQUIRE_EQ(party.members().size(), 1u);

    for (const char *pSkillName : ExpectedSpellSkills)
    {
        const OpenYAMM::Game::CharacterSkill *pSkill = pMember->findSkill(pSkillName);
        REQUIRE(pSkill != nullptr);
        CHECK(pSkill->level == 200u);
        CHECK(pSkill->mastery == OpenYAMM::Game::SkillMastery::Grandmaster);
    }

    CHECK(pMember->knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::TorchLight)));
    CHECK(pMember->knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Incinerate)));
    CHECK(pMember->knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Starburst)));
    CHECK(pMember->knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::SoulDrinker)));
    CHECK(pMember->knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::DarkfireBolt)));
    CHECK(pMember->knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::Mistform)));
    CHECK(pMember->knowsSpell(OpenYAMM::Game::spellIdValue(OpenYAMM::Game::SpellId::WingBuffet)));

    for (const std::string &skillName : OpenYAMM::Game::allCanonicalSkillNames())
    {
        const OpenYAMM::Game::CharacterSkill *pSkill = pMember->findSkill(skillName);
        REQUIRE(pSkill != nullptr);
        CHECK(pSkill->level == 200);
        CHECK(pSkill->mastery == OpenYAMM::Game::SkillMastery::Grandmaster);
    }

    CHECK_EQ(pMember->maxHealth, 2042);
    CHECK_EQ(pMember->health, pMember->maxHealth);
    CHECK_EQ(pMember->maxSpellPoints, 3039);
    CHECK_EQ(pMember->spellPoints, pMember->maxSpellPoints);

    static constexpr std::array<uint32_t, 12> ExpectedInventoryIds = {{
        401,
        410,
        411,
        400,
        741,
        222,
        223,
        266,
        253,
        656,
        2106,
        2125,
    }};

    for (uint32_t itemId : ExpectedInventoryIds)
    {
        const bool found = std::any_of(
            pMember->inventory.begin(),
            pMember->inventory.end(),
            [itemId](const OpenYAMM::Game::InventoryItem &item)
            {
                return item.objectDescriptionId == itemId;
            });

        CHECK(found);
    }
}

TEST_CASE("default party reset does not seed secondary test members")
{
    REQUIRE_MESSAGE(
        OpenYAMM::Tests::regressionGameDataLoaded(),
        OpenYAMM::Tests::regressionGameDataFailure().c_str());

    OpenYAMM::Game::Party party = {};
    party.setItemTable(&OpenYAMM::Tests::regressionGameData().itemTable);
    party.reset();

    CHECK_EQ(party.members().size(), 1u);
    CHECK(party.member(1) == nullptr);
    CHECK(party.member(2) == nullptr);
}

TEST_CASE("inventory auto placement uses grid rules")
{
    OpenYAMM::Game::Character member = {};

    REQUIRE(member.addInventoryItem(makeInventoryItem(104, 2, 2)));
    REQUIRE(member.addInventoryItem(makeInventoryItem(111, 1, 2)));
    REQUIRE(member.addInventoryItem(makeInventoryItem(25, 2, 3)));
    REQUIRE(member.addInventoryItem(makeInventoryItem(94, 3, 2)));

    const OpenYAMM::Game::InventoryItem *pShield = member.inventoryItemAt(0, 0);
    const OpenYAMM::Game::InventoryItem *pFullHelm = member.inventoryItemAt(0, 2);
    const OpenYAMM::Game::InventoryItem *pFangedBlade = member.inventoryItemAt(0, 4);
    const OpenYAMM::Game::InventoryItem *pBreastplate = member.inventoryItemAt(0, 7);

    REQUIRE(pShield != nullptr);
    REQUIRE(pFullHelm != nullptr);
    REQUIRE(pFangedBlade != nullptr);
    REQUIRE(pBreastplate != nullptr);
    CHECK_EQ(pShield->objectDescriptionId, 104u);
    CHECK_EQ(pFullHelm->objectDescriptionId, 111u);
    CHECK_EQ(pFangedBlade->objectDescriptionId, 25u);
    CHECK_EQ(pBreastplate->objectDescriptionId, 94u);
}

TEST_CASE("inventory auto placement fills columns vertically")
{
    OpenYAMM::Game::Character member = {};

    for (int index = 0; index < 20; ++index)
    {
        REQUIRE(member.addInventoryItem(makeInventoryItem(109, 1, 1)));
    }

    CHECK(member.inventoryItemAt(0, 0) != nullptr);
    CHECK(member.inventoryItemAt(0, 8) != nullptr);
    CHECK(member.inventoryItemAt(1, 0) != nullptr);
    CHECK(member.inventoryItemAt(1, 8) != nullptr);
    CHECK(member.inventoryItemAt(2, 0) != nullptr);
    CHECK(member.inventoryItemAt(2, 1) != nullptr);
    CHECK(member.inventoryItemAt(2, 2) == nullptr);
}

TEST_CASE("inventory move accepts rejects and swaps by grid rules")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);
    REQUIRE(pMember->addInventoryItemAt(makeInventoryItem(104, 2, 2), 0, 0));
    REQUIRE(pMember->addInventoryItemAt(makeInventoryItem(111, 1, 2), 0, 2));

    OpenYAMM::Game::InventoryItem heldItem = {};
    REQUIRE(party.takeItemFromMemberInventoryCell(0, 0, 0, heldItem));

    std::optional<OpenYAMM::Game::InventoryItem> replacedItem;

    REQUIRE(party.tryPlaceItemInMemberInventoryCell(0, heldItem, 4, 4, replacedItem));
    CHECK_FALSE(replacedItem.has_value());

    const OpenYAMM::Game::InventoryItem *pMovedItem = pMember->inventoryItemAt(4, 4);
    REQUIRE(pMovedItem != nullptr);
    CHECK_EQ(pMovedItem->objectDescriptionId, 104u);

    REQUIRE(party.takeItemFromMemberInventoryCell(0, 4, 4, heldItem));
    CHECK_FALSE(party.tryPlaceItemInMemberInventoryCell(0, heldItem, 13, 8, replacedItem));
    REQUIRE(party.tryPlaceItemInMemberInventoryCell(0, heldItem, 0, 2, replacedItem));
    REQUIRE(replacedItem.has_value());
    CHECK_EQ(replacedItem->objectDescriptionId, 111u);

    pMovedItem = pMember->inventoryItemAt(0, 2);
    REQUIRE(pMovedItem != nullptr);
    CHECK_EQ(pMovedItem->objectDescriptionId, 104u);
}

TEST_CASE("inventory swap stores held item in first free slot when target origin is blocked")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);
    REQUIRE(pMember->addInventoryItemAt(makeInventoryItem(109, 1, 1), 0, 0));
    REQUIRE(pMember->addInventoryItemAt(makeInventoryItem(111, 1, 1), 1, 0));

    std::optional<OpenYAMM::Game::InventoryItem> replacedItem;
    const OpenYAMM::Game::InventoryItem heldItem = makeInventoryItem(104, 2, 2);

    REQUIRE(party.tryPlaceItemInMemberInventoryCell(0, heldItem, 0, 0, replacedItem));
    REQUIRE(replacedItem.has_value());
    CHECK_EQ(replacedItem->objectDescriptionId, 109u);

    const OpenYAMM::Game::InventoryItem *pFallbackPlacedItem = pMember->inventoryItemAt(0, 1);
    REQUIRE(pFallbackPlacedItem != nullptr);
    CHECK_EQ(pFallbackPlacedItem->objectDescriptionId, 104u);

    const OpenYAMM::Game::InventoryItem *pBlockingItem = pMember->inventoryItemAt(1, 0);
    REQUIRE(pBlockingItem != nullptr);
    CHECK_EQ(pBlockingItem->objectDescriptionId, 111u);
}

TEST_CASE("inventory swap restores clicked item when held item has no fallback slot")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);

    for (int gridY = 0; gridY < OpenYAMM::Game::Character::InventoryHeight; ++gridY)
    {
        for (int gridX = 0; gridX < OpenYAMM::Game::Character::InventoryWidth; ++gridX)
        {
            const uint32_t itemId = gridX == 0 && gridY == 0 ? 109u : 111u;
            REQUIRE(pMember->addInventoryItemAt(
                makeInventoryItem(itemId, 1, 1),
                static_cast<uint8_t>(gridX),
                static_cast<uint8_t>(gridY)));
        }
    }

    std::optional<OpenYAMM::Game::InventoryItem> replacedItem;
    const OpenYAMM::Game::InventoryItem heldItem = makeInventoryItem(104, 2, 2);

    CHECK_FALSE(party.tryPlaceItemInMemberInventoryCell(0, heldItem, 0, 0, replacedItem));
    CHECK_FALSE(replacedItem.has_value());

    const OpenYAMM::Game::InventoryItem *pRestoredItem = pMember->inventoryItemAt(0, 0);
    REQUIRE(pRestoredItem != nullptr);
    CHECK_EQ(pRestoredItem->objectDescriptionId, 109u);
    CHECK_EQ(pMember->inventoryItemCount(), pMember->inventoryCapacity());
}

TEST_CASE("inventory cross member move and full destination rejection")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    OpenYAMM::Game::Character *pSourceMember = party.member(0);
    OpenYAMM::Game::Character *pDestinationMember = party.member(1);
    REQUIRE(pSourceMember != nullptr);
    REQUIRE(pDestinationMember != nullptr);
    REQUIRE(pSourceMember->addInventoryItemAt(makeInventoryItem(111, 1, 2), 0, 0));

    OpenYAMM::Game::InventoryItem heldItem = {};
    REQUIRE(party.takeItemFromMemberInventoryCell(0, 0, 0, heldItem));

    std::optional<OpenYAMM::Game::InventoryItem> replacedItem;
    REQUIRE(party.tryPlaceItemInMemberInventoryCell(1, heldItem, 0, 0, replacedItem));
    CHECK_FALSE(replacedItem.has_value());

    const OpenYAMM::Game::InventoryItem *pTransferredItem = pDestinationMember->inventoryItemAt(0, 0);
    REQUIRE(pTransferredItem != nullptr);
    CHECK_EQ(pTransferredItem->objectDescriptionId, 111u);

    OpenYAMM::Game::Character *pFullMember = party.member(2);
    REQUIRE(pFullMember != nullptr);

    for (int gridY = 0; gridY < OpenYAMM::Game::Character::InventoryHeight; ++gridY)
    {
        for (int gridX = 0; gridX < OpenYAMM::Game::Character::InventoryWidth; ++gridX)
        {
            REQUIRE(pFullMember->addInventoryItemAt(
                makeInventoryItem(109, 1, 1),
                static_cast<uint8_t>(gridX),
                static_cast<uint8_t>(gridY)));
        }
    }

    heldItem = makeInventoryItem(104, 2, 2);
    CHECK_FALSE(party.tryPlaceItemInMemberInventoryCell(2, heldItem, 0, 0, replacedItem));
    CHECK_EQ(heldItem.objectDescriptionId, 104u);
    CHECK_EQ(pFullMember->inventoryItemCount(), pFullMember->inventoryCapacity());
}

TEST_CASE("inventory member auto transfer places item in first free slot")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    OpenYAMM::Game::Character *pDestinationMember = party.member(1);
    OpenYAMM::Game::Character *pFullMember = party.member(2);
    REQUIRE(pDestinationMember != nullptr);
    REQUIRE(pFullMember != nullptr);
    REQUIRE(pDestinationMember->addInventoryItemAt(makeInventoryItem(109, 1, 1), 0, 0));

    const OpenYAMM::Game::InventoryItem heldItem = makeInventoryItem(104, 2, 2);
    REQUIRE(party.tryAutoPlaceItemInMemberInventory(1, heldItem));

    const OpenYAMM::Game::InventoryItem *pTransferredItem = pDestinationMember->inventoryItemAt(0, 1);
    REQUIRE(pTransferredItem != nullptr);
    CHECK_EQ(pTransferredItem->objectDescriptionId, 104u);
    CHECK_EQ(pTransferredItem->gridX, 0u);
    CHECK_EQ(pTransferredItem->gridY, 1u);

    for (int gridY = 0; gridY < OpenYAMM::Game::Character::InventoryHeight; ++gridY)
    {
        for (int gridX = 0; gridX < OpenYAMM::Game::Character::InventoryWidth; ++gridX)
        {
            REQUIRE(pFullMember->addInventoryItemAt(
                makeInventoryItem(109, 1, 1),
                static_cast<uint8_t>(gridX),
                static_cast<uint8_t>(gridY)));
        }
    }

    CHECK_FALSE(party.tryAutoPlaceItemInMemberInventory(2, heldItem));
    CHECK_EQ(party.lastStatus(), "inventory full");
    CHECK_EQ(pFullMember->inventoryItemCount(), pFullMember->inventoryCapacity());
}

TEST_CASE("MM9 temporary primary stat reward applies to every member and expires")
{
    OpenYAMM::Game::Party party = makeInventoryParty();

    party.applyTemporaryPrimaryStatBonus(0, 2, 3000.0f);
    for (const OpenYAMM::Game::Character &member : party.members())
    {
        CHECK_EQ(member.magicalBonuses.might, 2);
        CHECK_EQ(member.timedPrimaryStatBonuses[0].power, 2);
    }

    party.advanceTimedStates(2999.0f);
    CHECK_EQ(party.member(0)->magicalBonuses.might, 2);
    party.advanceTimedStates(1.0f);
    for (const OpenYAMM::Game::Character &member : party.members())
    {
        CHECK_EQ(member.magicalBonuses.might, 0);
        CHECK_EQ(member.timedPrimaryStatBonuses[0].power, 0);
    }
}

TEST_CASE("MM9 temporary primary stat reward survives save round trip")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    party.applyTemporaryPrimaryStatBonus(4, 1, 3000.0f);

    OpenYAMM::Game::GameSaveData saveData = {};
    saveData.mapFileName = "mm9_stat_reward_roundtrip.blv";
    saveData.party = party.snapshot();
    const std::filesystem::path savePath =
        std::filesystem::temp_directory_path() / "openyamm_mm9_stat_reward_roundtrip.oysav";
    std::string error;
    REQUIRE(OpenYAMM::Game::saveGameDataToPath(savePath, saveData, error));

    const std::optional<OpenYAMM::Game::GameSaveData> loaded =
        OpenYAMM::Game::loadGameDataFromPath(savePath, error);
    std::filesystem::remove(savePath);
    REQUIRE(loaded.has_value());

    OpenYAMM::Game::Party restoredParty = makeInventoryParty();
    restoredParty.restoreSnapshot(loaded->party);
    for (const OpenYAMM::Game::Character &member : restoredParty.members())
    {
        CHECK_EQ(member.magicalBonuses.speed, 1);
        CHECK_EQ(member.timedPrimaryStatBonuses[4].remainingSeconds, doctest::Approx(3000.0f));
    }
}

TEST_CASE("temporary event bonuses survive save round trip and remain rest-cleared")
{
    OpenYAMM::Game::Party party = makeInventoryParty();
    OpenYAMM::Game::Character *pMember = party.member(0);
    REQUIRE(pMember != nullptr);
    pMember->temporaryEventBonuses.intellect = 15;
    pMember->temporaryEventBonuses.resistances.fire = 25;
    party.rebuildMagicalBonusesFromBuffs();

    OpenYAMM::Game::GameSaveData saveData = {};
    saveData.mapFileName = "temporary_event_bonus_roundtrip.odm";
    saveData.party = party.snapshot();
    const std::filesystem::path savePath =
        std::filesystem::temp_directory_path() / "openyamm_temporary_event_bonus_roundtrip.oysav";
    std::string error;
    REQUIRE(OpenYAMM::Game::saveGameDataToPath(savePath, saveData, error));

    const std::optional<OpenYAMM::Game::GameSaveData> loaded =
        OpenYAMM::Game::loadGameDataFromPath(savePath, error);
    std::filesystem::remove(savePath);
    REQUIRE(loaded.has_value());

    OpenYAMM::Game::Party restoredParty = makeInventoryParty();
    restoredParty.restoreSnapshot(loaded->party);
    const OpenYAMM::Game::Character *pRestoredMember = restoredParty.member(0);
    REQUIRE(pRestoredMember != nullptr);
    CHECK_EQ(pRestoredMember->temporaryEventBonuses.intellect, 15);
    CHECK_EQ(pRestoredMember->magicalBonuses.intellect, 15);
    CHECK_EQ(pRestoredMember->magicalBonuses.resistances.fire, 25);

    restoredParty.restAndHealAll();
    CHECK_EQ(pRestoredMember->temporaryEventBonuses.intellect, 0);
    CHECK_EQ(pRestoredMember->magicalBonuses.intellect, 0);
    CHECK_EQ(pRestoredMember->magicalBonuses.resistances.fire, 0);
}


TEST_CASE("rendered creature discovery uses camera geometry LOS for living sprites and corpses")
{
    using namespace OpenYAMM::Game;
    class SightRuntime : public OpenYAMM::Tests::PartySpellTestWorldRuntime
    {
    public:
        bool clear = false;
        bool killed = false;
        float movementX = 0;
        std::optional<float> clearTargetX;
        mutable uint32_t checks = 0;
        mutable GameplayWorldPoint source, target;
        bool actorRuntimeState(size_t index, GameplayRuntimeActorState &actor) const override
        {
            if (!PartySpellTestWorldRuntime::actorRuntimeState(index, actor)) return false;
            actor.preciseX += movementX;
            if (index == 0 && killed) actor.isDead = true;
            return true;
        }
        bool hasWorldLineOfSight(const GameplayWorldPoint &from, const GameplayWorldPoint &to) const override
        {
            ++checks;
            source = from; target = to;
            return clear && (!clearTargetX || to.x == *clearTargetX);
        }
    } runtime;
    EventRuntimeState state;
    state.mapFileName = "out01.odm";
    runtime.bindEventRuntimeState(&state);
    GameplayRuntimeActorState actor;
    actor.actorId = 42; actor.monsterId = 12; actor.preciseX = 100; actor.preciseY = -200; actor.preciseZ = 300;
    const size_t visible = runtime.addActor(actor);
    actor.actorId = 43;
    runtime.addActor(actor); // Not submitted by the main view.
    actor.actorId = 44; actor.isInvisible = true;
    const size_t invisible = runtime.addActor(actor);
    actor.actorId = 45; actor.isInvisible = false; actor.isDead = true;
    const size_t dead = runtime.addActor(actor);
    const std::array<float, 3> eye = {1, 2, 399};
    WorldMonsterSightCandidate candidate{visible, {100, -200, 320}};
    WorldMonsterSightTracker tracker;
    recordWorldVisibleActorObservation(tracker, runtime, candidate, eye, 100);
    CHECK(state.namedMapVars.empty());
    CHECK(runtime.checks == 1);
    runtime.clear = true;
    recordWorldVisibleActorObservation(tracker, runtime, candidate, eye, 101);
    CHECK(state.namedMapVars.empty());
    CHECK(runtime.checks == 1);
    recordWorldVisibleActorObservation(tracker, runtime, candidate, eye, 1100);
    auto observed = worldMonsterObservations(state);
    REQUIRE(observed.size() == 1);
    CHECK(observed[0].actorId == 42);
    CHECK(observed[0].monsterId == 12);
    CHECK(observed[0].x == 100);
    CHECK(observed[0].y == -200);
    CHECK(runtime.source.z == 399);
    CHECK(runtime.target.z == 320);
    CHECK(runtime.checks == 2);
    recordWorldVisibleActorObservation(tracker, runtime, candidate, eye, 2100);
    recordWorldVisibleActorObservation(tracker, runtime, {invisible, candidate.point}, eye, 2100);
    recordWorldVisibleActorObservation(tracker, runtime, {999, candidate.point}, eye, 2100);
    CHECK(runtime.checks == 2);
    runtime.movementX = 25;
    candidate.point[0] = 125;
    runtime.clear = false;
    recordWorldVisibleActorObservation(tracker, runtime, candidate, eye, 2200);
    CHECK(worldMonsterObservations(state)[0].x == 100);
    runtime.clear = true;
    recordWorldVisibleActorObservation(tracker, runtime, candidate, eye, 2201);
    CHECK(worldMonsterObservations(state)[0].x == 100);
    recordWorldVisibleActorObservation(tracker, runtime, candidate, eye, 3200);
    CHECK(worldMonsterObservations(state)[0].x == 125);
    CHECK(runtime.checks == 4);

    runtime.killed = true;
    candidate.point[2] = 305; // Actual corpse sprite, not the standing actor's collision height.
    runtime.clear = false;
    recordWorldVisibleActorObservation(tracker, runtime, candidate, eye, 4200);
    CHECK_FALSE(worldMonsterObservations(state)[0].dead); // Death behind cover cannot update knowledge.
    runtime.clear = true;
    recordWorldVisibleActorObservation(tracker, runtime, candidate, eye, 5200);
    CHECK(worldMonsterObservations(state)[0].dead);
    CHECK(runtime.target.z == 305);
    CHECK(runtime.source.z == 399);
    CHECK_FALSE(runtime.partyAttackActorFacts(visible, true)); // No valid combat target is needed.
    recordWorldVisibleActorObservation(tracker, runtime, {dead, {125, -200, 305}}, eye, 5216);
    observed = worldMonsterObservations(state);
    REQUIRE(observed.size() == 2);
    CHECK(observed[1].actorId == 45);
    CHECK(observed[1].dead);
    const auto beforeInvalid = state.namedMapVars;
    candidate.point[0] = std::numeric_limits<float>::quiet_NaN();
    recordWorldVisibleActorObservation(tracker, runtime, candidate, eye, 9000);
    CHECK(state.namedMapVars == beforeInvalid);

    SightRuntime crowded;
    EventRuntimeState crowdState;
    crowdState.mapFileName = "crowded.blv";
    crowded.bindEventRuntimeState(&crowdState);
    crowded.clear = true;
    std::vector<WorldMonsterSightCandidate> candidates;
    actor.isDead = false;
    for (uint32_t i = 0; i < 301; ++i)
    {
        actor.actorId = i;
        candidates.push_back({crowded.addActor(actor), {float(i), 0, 320}});
    }
    crowded.clearTargetX = 300;
    WorldMonsterSightTracker fair;
    for (uint64_t frame = 0; frame < 200; ++frame)
    {
        const uint32_t before = crowded.checks;
        fair.observeRenderedActors(crowded, candidates, eye, frame * 16);
        CHECK(crowded.checks - before <= 2);
    }
    const auto crowdObserved = worldMonsterObservations(crowdState);
    REQUIRE(crowdObserved.size() == 1);
    CHECK(crowdObserved[0].actorId == 300);
}
