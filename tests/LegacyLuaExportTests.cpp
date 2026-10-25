#include "doctest/doctest.h"
#include "engine/TextTable.h"

#include "tools/LegacyLuaExport.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/events/EventRuntime.h"

#include <algorithm>
#include <cctype>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
std::vector<uint8_t> readBinaryFixture(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    REQUIRE_MESSAGE(stream.good(), path.string().c_str());
    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

std::string extractLuaEvent(const std::string &lua, const std::string &eventRegistration)
{
    const size_t eventStart = lua.find(eventRegistration);
    REQUIRE(eventStart != std::string::npos);

    size_t eventEnd = lua.find("\n\nRegister", eventStart + eventRegistration.size());

    if (eventEnd == std::string::npos)
    {
        eventEnd = lua.size();
    }

    return lua.substr(eventStart, eventEnd - eventStart);
}

std::string extractTimerMetadata(
    const std::string &lua,
    uint16_t sourceEventId,
    size_t occurrence = 0)
{
    const std::string sourceText = "sourceEventId = " + std::to_string(sourceEventId);
    size_t sourcePosition = 0;

    for (size_t index = 0; index <= occurrence; ++index)
    {
        sourcePosition = lua.find(sourceText, sourcePosition);
        REQUIRE(sourcePosition != std::string::npos);
        ++sourcePosition;
    }

    const size_t lineStart = lua.rfind('{', sourcePosition);
    const size_t lineEnd = lua.find('\n', sourcePosition);
    REQUIRE(lineStart != std::string::npos);
    REQUIRE(lineEnd != std::string::npos);
    return lua.substr(lineStart, lineEnd - lineStart);
}

void appendTimerRecord(
    std::vector<uint8_t> &evtBytes,
    uint16_t eventId,
    uint8_t step,
    OpenYAMM::Game::EvtOpcode opcode,
    const std::array<uint8_t, 10> &descriptor)
{
    evtBytes.push_back(14);
    evtBytes.push_back(static_cast<uint8_t>(eventId & 0xffu));
    evtBytes.push_back(static_cast<uint8_t>(eventId >> 8));
    evtBytes.push_back(step);
    evtBytes.push_back(static_cast<uint8_t>(opcode));
    evtBytes.insert(evtBytes.end(), descriptor.begin(), descriptor.end());
}

void appendExitRecord(std::vector<uint8_t> &evtBytes, uint16_t eventId, uint8_t step)
{
    evtBytes.push_back(4);
    evtBytes.push_back(static_cast<uint8_t>(eventId & 0xffu));
    evtBytes.push_back(static_cast<uint8_t>(eventId >> 8));
    evtBytes.push_back(step);
    evtBytes.push_back(static_cast<uint8_t>(OpenYAMM::Game::EvtOpcode::Exit));
}

OpenYAMM::Game::ScriptedEventProgram exportPressureTrapProgram(
    const std::vector<uint8_t> &bytes,
    OpenYAMM::Game::LegacyEventVersion version,
    const std::string &mapName = "test.blv")
{
    using namespace OpenYAMM::Game;
    EvtProgram legacyProgram;
    REQUIRE(legacyProgram.loadFromBytes(bytes));
    LegacyLuaExportLookups lookups;
    lookups.sourceMapFile = mapName;
    const auto noteBytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/data_tables/english/autonote.txt");
    const auto noteTable = OpenYAMM::Engine::TextTable::parseTabSeparated(
        std::string(noteBytes.begin(), noteBytes.end()));
    REQUIRE(noteTable);
    for (size_t i = 0; i < noteTable->getRowCount(); ++i)
    {
        const auto row = noteTable->getRow(i);
        if (row.size() >= 2 && !row[0].empty() && std::isdigit(static_cast<unsigned char>(row[0][0])))
            lookups.autonoteTexts[uint32_t(std::stoul(row[0]))] = row[1];
    }
    const std::string lua = generateLegacyEventLuaChunk(
        legacyProgram, StrTable{}, lookups, LegacyLuaExportScope::Map, version)
        + generateLegacyEventItemRewardsLuaChunk(legacyProgram, StrTable{}, lookups,
            LegacyLuaExportScope::Map, version)
        + generateLegacyEventNpcLocationsLuaChunk(legacyProgram, StrTable{}, lookups,
            LegacyLuaExportScope::Map, version)
        + generateLegacyEventAutonoteRefsLuaChunk(legacyProgram, StrTable{}, lookups,
            LegacyLuaExportScope::Map, version);
    const std::vector<uint8_t> supportBytes = readBinaryFixture(
        std::filesystem::path(OPENYAMM_SOURCE_DIR) / "assets_dev/engine/scripts/common/event_support.lua");
    std::string error;
    const std::optional<ScriptedEventProgram> program = ScriptedEventProgram::loadFromLuaText(
        std::string(supportBytes.begin(), supportBytes.end()) + "\n" + lua,
        "pressure-trap-test", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program.has_value(), error);
    return *program;
}
}

TEST_CASE("levitate pressure trap export respects each worlds event semantics")
{
    using namespace OpenYAMM::Game;
    const std::filesystem::path worlds = std::filesystem::path(OPENYAMM_SOURCE_DIR) / "assets_dev/worlds";
    const ScriptedEventProgram mm7 = exportPressureTrapProgram(
        readBinaryFixture(worlds / "mm7/_legacy/events/7d10.EVT"), LegacyEventVersion::Mm7);
    CHECK(mm7.isLevitateSensitivePressurePlate(451, 0x84000102));
    CHECK(mm7.isLevitateSensitivePressurePlate(452, 0x84000100));
    CHECK_FALSE(mm7.isLevitateSensitivePressurePlate(501, 0x8c000100));

    const ScriptedEventProgram mm6 = exportPressureTrapProgram(
        readBinaryFixture(worlds / "mm6/_legacy/events/6D03.EVT"), LegacyEventVersion::Mm6, "6d03.blv");
    CHECK(mm6.isLevitateSensitivePressurePlate(21, 0x84000100));
    CHECK_FALSE(mm6.isLevitateSensitivePressurePlate(22, 0x8c000100));

    const ScriptedEventProgram puzzle = exportPressureTrapProgram(
        readBinaryFixture(worlds / "mm6/_legacy/events/6D08.EVT"), LegacyEventVersion::Mm6, "6d08.blv");
    for (uint16_t eventId : {22, 23, 25, 32, 34, 35, 36, 39, 40, 41, 42})
    {
        CHECK(puzzle.isLevitateSensitivePressurePlate(eventId, 0x84000100));
    }
    CHECK_FALSE(puzzle.isLevitateSensitivePressurePlate(24, 0x84000100));

    const ScriptedEventProgram encounter = exportPressureTrapProgram(
        readBinaryFixture(worlds / "mm7/_legacy/events/7out02.EVT"), LegacyEventVersion::Mm7);
    CHECK_FALSE(encounter.isLevitateSensitivePressurePlate(236, 0x04000100));

    const ScriptedEventProgram mm8 = exportPressureTrapProgram(
        readBinaryFixture(worlds / "mm8/_legacy/events/D26.EVT"), LegacyEventVersion::Mm8);
    CHECK(mm8.isLevitateSensitivePressurePlate(101, 0x8c000102));
    CHECK_FALSE(mm8.isLevitateSensitivePressurePlate(101, 0x84000102));
}

TEST_CASE("levitate pressure trap inference rejects benefits and reachable side effects")
{
    using namespace OpenYAMM::Game;
    std::vector<uint8_t> bytes;
    const auto appendRecord = [&](uint16_t eventId, uint8_t step, EvtOpcode opcode, std::vector<uint8_t> payload)
    {
        bytes.push_back(static_cast<uint8_t>(4 + payload.size()));
        bytes.push_back(static_cast<uint8_t>(eventId));
        bytes.push_back(0);
        bytes.push_back(step);
        bytes.push_back(static_cast<uint8_t>(opcode));
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    };
    std::vector<uint8_t> fireball(27, 0);
    fireball[0] = 6;
    fireball[2] = 5;
    std::vector<uint8_t> wizardEye = fireball;
    wizardEye[0] = 12;
    appendRecord(1, 0, EvtOpcode::CastSpell, fireball);
    appendExitRecord(bytes, 1, 1);
    appendRecord(1, 2, EvtOpcode::Set, {0x75, 0, 1, 0, 0, 0}); // Unreachable state change.
    appendRecord(2, 0, EvtOpcode::CastSpell, wizardEye);
    appendExitRecord(bytes, 2, 1);
    appendRecord(3, 0, EvtOpcode::CastSpell, fireball);
    appendRecord(3, 1, EvtOpcode::Set, {0x75, 0, 1, 0, 0, 0});
    appendExitRecord(bytes, 3, 2);
    appendRecord(4, 0, EvtOpcode::Compare, {0x75, 0, 1, 0, 0, 0, 3});
    appendRecord(4, 1, EvtOpcode::CastSpell, fireball);
    appendExitRecord(bytes, 4, 2);
    appendRecord(4, 3, EvtOpcode::Set, {0x75, 0, 1, 0, 0, 0});
    appendExitRecord(bytes, 4, 4);
    appendRecord(5, 0, EvtOpcode::CastSpell, fireball);
    appendRecord(5, 1, EvtOpcode::CastSpell, wizardEye);
    appendExitRecord(bytes, 5, 2);

    const ScriptedEventProgram program = exportPressureTrapProgram(bytes, LegacyEventVersion::Mm7);
    CHECK(program.isLevitateSensitivePressurePlate(1, 0x04000000));
    for (uint16_t eventId : {2, 3, 4, 5})
    {
        CHECK_FALSE(program.isLevitateSensitivePressurePlate(eventId, 0x04000000));
    }
}

TEST_CASE("legacy lua exporter preserves return for castle gloaming soul jar chest")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm7/_legacy/events/d03.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm7/_legacy/events/d03.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "Castle Gloaming";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm7);

    const size_t eventStart = lua.find("RegisterEvent(176");
    REQUIRE(eventStart != std::string::npos);
    const size_t eventEnd = lua.find("RegisterEvent(177", eventStart);
    REQUIRE(eventEnd != std::string::npos);
    const std::string eventLua = lua.substr(eventStart, eventEnd - eventStart);
    INFO(eventLua);

    const size_t chest0 = eventLua.find("evt.OpenChest(0)");
    const size_t qbit743 = eventLua.find("SetQBit(QBit(743))", chest0);
    const size_t qbit662 = eventLua.find("SetQBit(QBit(662))", qbit743);
    const size_t branchReturn = eventLua.find("return", qbit662);
    const size_t laterChest1 = eventLua.find("evt.OpenChest(1)", qbit662);

    REQUIRE(chest0 != std::string::npos);
    REQUIRE(qbit743 != std::string::npos);
    REQUIRE(qbit662 != std::string::npos);
    REQUIRE(branchReturn != std::string::npos);
    REQUIRE(laterChest1 != std::string::npos);
    CHECK(chest0 < qbit743);
    CHECK(qbit743 < qbit662);
    CHECK(qbit662 < branchReturn);
    CHECK(branchReturn < laterChest1);
}

TEST_CASE("legacy lua exporter separates timer continuation from direct event body")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm7/_legacy/events/7d35.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm7/_legacy/events/7d35.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "Nighon Tunnels";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm7);

    const size_t timerLineMatch = lua.find("intervalHalfMinutes = 5");
    REQUIRE(timerLineMatch != std::string::npos);
    const size_t timerLineStart = lua.rfind('{', timerLineMatch);
    const size_t timerLineEnd = lua.find('\n', timerLineMatch);
    REQUIRE(timerLineStart != std::string::npos);
    REQUIRE(timerLineEnd != std::string::npos);
    const std::string timerLine = lua.substr(timerLineStart, timerLineEnd - timerLineStart);
    INFO(timerLine);
    CHECK(timerLine.find("eventId = 452") == std::string::npos);

    const size_t timerEventIdStart = timerLine.find("eventId = ");
    REQUIRE(timerEventIdStart != std::string::npos);
    const size_t timerEventIdValueStart = timerEventIdStart + std::string("eventId = ").size();
    const size_t timerEventIdValueEnd = timerLine.find(',', timerEventIdValueStart);
    REQUIRE(timerEventIdValueEnd != std::string::npos);
    const std::string timerEventIdText =
        timerLine.substr(timerEventIdValueStart, timerEventIdValueEnd - timerEventIdValueStart);

    const size_t directEventStart = lua.find("RegisterEvent(452");
    REQUIRE(directEventStart != std::string::npos);
    const size_t directEventEnd = lua.find("RegisterEvent(454", directEventStart);
    REQUIRE(directEventEnd != std::string::npos);
    const std::string directEventLua = lua.substr(directEventStart, directEventEnd - directEventStart);
    INFO(directEventLua);
    CHECK(directEventLua.find("evt.MoveToMap(1232, 6896, -384") != std::string::npos);
    CHECK(directEventLua.find("evt.CastSpell(6") == std::string::npos);

    const std::string syntheticRegistration = "RegisterEvent(" + timerEventIdText;
    const size_t timerEventStart = lua.find(syntheticRegistration);
    REQUIRE(timerEventStart != std::string::npos);
    const size_t timerEventEnd = lua.find("\n\n", timerEventStart);
    REQUIRE(timerEventEnd != std::string::npos);
    const std::string timerEventLua = lua.substr(timerEventStart, timerEventEnd - timerEventStart);
    INFO(timerEventLua);
    CHECK(timerEventLua.find("evt.MoveToMap(1232, 6896, -384") == std::string::npos);
    CHECK(timerEventLua.find("evt.CastSpell(6, 7, 4, 13891") != std::string::npos);
    CHECK(timerEventLua.find("evt.CastSpell(6, 7, 4, 14618") != std::string::npos);
}

TEST_CASE("legacy lua exporter emits New Sorpigal first-visit fountain refill timer")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/OUTE3.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/OutE3.str");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "New Sorpigal";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm6);

    const std::string fountainTimer = extractTimerMetadata(lua, 130);
    INFO(fountainTimer);
    CHECK(fountainTimer.find("eventId = 130") != std::string::npos);
    CHECK(fountainTimer.find("triggerStep = 0") != std::string::npos);
    CHECK(fountainTimer.find("origin = \"legacy\"") != std::string::npos);
    CHECK(fountainTimer.find("triggerKind = \"long\"") != std::string::npos);
    CHECK(fountainTimer.find("scheduleKind = \"daily\"") != std::string::npos);
    CHECK(fountainTimer.find("startHour = 0") != std::string::npos);
    CHECK(fountainTimer.find("startMinute = 0") != std::string::npos);
    CHECK(fountainTimer.find("startSecond = 1") != std::string::npos);

    const std::string dragonTowerTimer = extractTimerMetadata(lua, 230);
    INFO(dragonTowerTimer);
    CHECK(dragonTowerTimer.find("triggerKind = \"timer\"") != std::string::npos);
    CHECK(dragonTowerTimer.find("scheduleKind = \"interval\"") != std::string::npos);
    CHECK(dragonTowerTimer.find("intervalHalfMinutes = 10") != std::string::npos);
}

TEST_CASE("legacy lua exporter preserves every repeated long timer trigger")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm7/_legacy/events/7d14.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm7/_legacy/events/7d14.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        {},
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm7);

    const std::string weeklyTimer = extractTimerMetadata(lua, 196, 0);
    const std::string firstYearlyTimer = extractTimerMetadata(lua, 196, 1);
    const std::string secondYearlyTimer = extractTimerMetadata(lua, 196, 2);
    INFO(weeklyTimer);
    INFO(firstYearlyTimer);
    INFO(secondYearlyTimer);
    CHECK(weeklyTimer.find("triggerStep = 22") != std::string::npos);
    CHECK(weeklyTimer.find("scheduleKind = \"weekly\"") != std::string::npos);
    CHECK(firstYearlyTimer.find("triggerStep = 25") != std::string::npos);
    CHECK(firstYearlyTimer.find("scheduleKind = \"yearly\"") != std::string::npos);
    CHECK(secondYearlyTimer.find("triggerStep = 26") != std::string::npos);
    CHECK(secondYearlyTimer.find("scheduleKind = \"yearly\"") != std::string::npos);
}

TEST_CASE("legacy lua exporter decodes the complete timer interval word")
{
    const std::vector<uint8_t> evtBytes = {
        14,
        77, 0,
        0, static_cast<uint8_t>(OpenYAMM::Game::EvtOpcode::OnTimer),
        0, 0, 0, 0, 0, 0,
        1, 2,
        0, 0,
    };

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        {},
        {},
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm7);

    const std::string timer = extractTimerMetadata(lua, 77);
    INFO(timer);
    CHECK(timer.find("intervalHalfMinutes = 513") != std::string::npos);
}

TEST_CASE("legacy lua exporter preserves calendar precedence and daily offsets")
{
    std::vector<uint8_t> evtBytes;
    appendTimerRecord(
        evtBytes,
        70,
        0,
        OpenYAMM::Game::EvtOpcode::OnLongTimer,
        {2, 3, 4, 25, 61, 59, 0, 0, 0, 0});
    appendTimerRecord(
        evtBytes,
        71,
        0,
        OpenYAMM::Game::EvtOpcode::OnLongTimer,
        {0, 7, 8, 0, 0, 0, 0, 0, 0, 0});
    appendTimerRecord(
        evtBytes,
        72,
        0,
        OpenYAMM::Game::EvtOpcode::OnLongTimer,
        {0, 0, 9, 0, 0, 0, 0, 0, 0, 0});
    appendTimerRecord(
        evtBytes,
        73,
        0,
        OpenYAMM::Game::EvtOpcode::OnLongTimer,
        {0, 0, 0, 27, 61, 59, 0, 0, 0, 0});

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        {},
        {},
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm7);

    CHECK(extractTimerMetadata(lua, 70).find("scheduleKind = \"yearly\"") != std::string::npos);
    CHECK(extractTimerMetadata(lua, 71).find("scheduleKind = \"monthly\"") != std::string::npos);
    CHECK(extractTimerMetadata(lua, 72).find("scheduleKind = \"weekly\"") != std::string::npos);

    const std::string daily = extractTimerMetadata(lua, 73);
    CHECK(daily.find("scheduleKind = \"daily\"") != std::string::npos);
    CHECK(daily.find("startHour = 27") != std::string::npos);
    CHECK(daily.find("startMinute = 61") != std::string::npos);
    CHECK(daily.find("startSecond = 59") != std::string::npos);
}

TEST_CASE("legacy lua exporter emits every mixed timer trigger in one event")
{
    std::vector<uint8_t> evtBytes;
    appendTimerRecord(
        evtBytes,
        80,
        0,
        OpenYAMM::Game::EvtOpcode::OnTimer,
        {0, 0, 0, 0, 0, 0, 2, 0, 0, 0});
    appendExitRecord(evtBytes, 80, 1);
    appendTimerRecord(
        evtBytes,
        80,
        2,
        OpenYAMM::Game::EvtOpcode::OnLongTimer,
        {0, 0, 1, 0, 0, 0, 0, 0, 0, 0});
    appendExitRecord(evtBytes, 80, 3);
    appendTimerRecord(
        evtBytes,
        80,
        4,
        OpenYAMM::Game::EvtOpcode::OnTimer,
        {0, 0, 0, 0, 0, 0, 4, 0, 0, 0});
    appendExitRecord(evtBytes, 80, 5);

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        {},
        {},
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm7);

    const std::string first = extractTimerMetadata(lua, 80, 0);
    const std::string second = extractTimerMetadata(lua, 80, 1);
    const std::string third = extractTimerMetadata(lua, 80, 2);
    CHECK(first.find("triggerStep = 0") != std::string::npos);
    CHECK(first.find("triggerKind = \"timer\"") != std::string::npos);
    CHECK(second.find("triggerStep = 2") != std::string::npos);
    CHECK(second.find("triggerKind = \"long\"") != std::string::npos);
    CHECK(third.find("triggerStep = 4") != std::string::npos);
    CHECK(third.find("triggerKind = \"timer\"") != std::string::npos);
}

TEST_CASE("legacy lua exporter prefers house names over stale mouseover hints for house events")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/OUTB2.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/OUTB2.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "Blackshire";
    lookups.houseNames[34] = "Stout Heart Staff and Spear";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm6);

    const size_t eventStart = lua.find("RegisterEvent(2");
    REQUIRE(eventStart != std::string::npos);
    const size_t eventEnd = lua.find("RegisterEvent(3", eventStart);
    REQUIRE(eventEnd != std::string::npos);
    const std::string eventLua = lua.substr(eventStart, eventEnd - eventStart);
    INFO(eventLua);

    CHECK(eventLua.find("RegisterEvent(2, \"Stout Heart Staff and Spear\"") != std::string::npos);
    CHECK(eventLua.find("end, \"Stout Heart Staff and Spear\")") != std::string::npos);
    CHECK(eventLua.find("You pray at the shrine") == std::string::npos);
}

TEST_CASE("legacy lua exporter remaps outdoor dungeon transitions to current map house ids")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/OUTB1.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/OUTB1.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "Kriegspire";
    lookups.mapNamesByFile["6t7.blv"] = "Superior Temple of Baa";
    lookups.currentMapDungeonEntryHouseIdsByName["superior temple of baa"] = 435;

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm6);

    const std::string templeLua = extractLuaEvent(lua, "RegisterEvent(90");
    INFO(templeLua);
    CHECK(templeLua.find("evt.MoveToMap(2094, -19, 177, 337, 0, 0, 435, 1, \"6t7.blv\")")
        != std::string::npos);
    CHECK(templeLua.find("evt.MoveToMap(2094, -19, 177, 337, 0, 0, 179, 1, \"6t7.blv\")")
        == std::string::npos);
}

TEST_CASE("legacy lua exporter omits generated fallback titles")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/6T1.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/6T1.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "Temple of Baa";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm6);

    const std::string eventLua = extractLuaEvent(lua, "RegisterEvent(26");
    INFO(eventLua);
    CHECK(eventLua.find("RegisterEvent(26, nil, function()") != std::string::npos);
    CHECK(eventLua.find("Legacy event 26") == std::string::npos);
}

TEST_CASE("legacy lua exporter emits context action metadata")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/6T1.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/6T1.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "Temple of Baa";
    lookups.sourceMapFile = "6t1.blv";
    lookups.mapNamesByFile["outd3.odm"] = "New Sorpigal";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm6);

    CHECK(lua.find("contextActions = {") != std::string::npos);
    CHECK(lua.find("[1] = { kind = \"open_door\", source = \"title\" }") != std::string::npos);
    CHECK(lua.find("[28] = { kind = \"open_chest\", source = \"opcode\", chestIds = {2} }") != std::string::npos);
    CHECK(lua.find("[34] = { kind = \"fountain\", source = \"title\" }") != std::string::npos);
    CHECK(lua.find("[50] = { kind = \"leave_dungeon\", source = \"opcode\", targetMap = \"outd3.odm\"")
        != std::string::npos);
    CHECK(lua.find("targetName = \"New Sorpigal\"") != std::string::npos);
}

TEST_CASE("legacy lua exporter rewrites unrolled party member rewards to dynamic party loops")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/6T5.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/6T5.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "Temple of Baa";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm6);

    const std::string mightAltarLua = extractLuaEvent(lua, "RegisterEvent(16");
    INFO(mightAltarLua);
    CHECK(mightAltarLua.find("for _, player in ipairs(PartyMembers()) do") != std::string::npos);
    CHECK(mightAltarLua.find("evt.ForPlayer(player)") != std::string::npos);
    CHECK(mightAltarLua.find("evt.ForPlayer(Players.Member0)") == std::string::npos);
    CHECK(mightAltarLua.find("evt.ForPlayer(Players.Current)") == std::string::npos);
}

TEST_CASE("legacy lua exporter keeps the first-member luck altar exception in dynamic party loops")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/6T5.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/6T5.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "Temple of Baa";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm6);

    const std::string luckAltarLua = extractLuaEvent(lua, "RegisterEvent(21");
    INFO(luckAltarLua);
    CHECK(luckAltarLua.find("for _, player in ipairs(PartyMembers()) do") != std::string::npos);
    CHECK(luckAltarLua.find("if player == Players.Member0 then") != std::string::npos);
    CHECK(luckAltarLua.find("AddValue(BaseLuck, 2)") != std::string::npos);
    CHECK(luckAltarLua.find("AddValue(BaseLuck, 5)") != std::string::npos);
    CHECK(luckAltarLua.find("evt.ForPlayer(Players.Current)") == std::string::npos);
}

TEST_CASE("legacy lua exporter rewrites party loops that reset the selected player afterward")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/6D13.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/6D13.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "The Monolith";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm6);

    const std::string altarLua = extractLuaEvent(lua, "RegisterEvent(24");
    INFO(altarLua);
    CHECK(altarLua.find("for _, player in ipairs(PartyMembers()) do") != std::string::npos);
    CHECK(altarLua.find("evt.ForPlayer(Players.All)") != std::string::npos);
    CHECK(altarLua.find("evt.ForPlayer(Players.Member0)") == std::string::npos);
    CHECK(altarLua.find("evt.ForPlayer(Players.Member3)") == std::string::npos);
}

TEST_CASE("legacy lua exporter rewrites lincoln wetsuit checks to all active party members")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm7/_legacy/events/7d23.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm7/_legacy/events/7d23.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "The Lincoln";
    lookups.itemNames[1406] = "Wetsuit";
    lookups.mapNamesByFile["7out15.odm"] = "Shoals";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm7);

    const std::string leaveLincolnLua = extractLuaEvent(lua, "RegisterEvent(501");
    INFO(leaveLincolnLua);
    CHECK(leaveLincolnLua.find("local hasAllWetsuits = true") != std::string::npos);
    CHECK(leaveLincolnLua.find("for _, player in ipairs(PartyMembers()) do") != std::string::npos);
    CHECK(leaveLincolnLua.find("if not HasItem(1406) then -- Wetsuit") != std::string::npos);
    CHECK(leaveLincolnLua.find("evt.MoveToMap(-7005, 7856, 225, 128") != std::string::npos);
    CHECK(leaveLincolnLua.find("evt.ForPlayer(Players.Member0)") == std::string::npos);
    CHECK(leaveLincolnLua.find("evt.ForPlayer(Players.Current)") == std::string::npos);
}

TEST_CASE("legacy lua exporter rewrites lich ritual checks and promotions to all active party members")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm7/_legacy/events/Global.EVT");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Global,
        OpenYAMM::Game::LegacyEventVersion::Mm7);

    const std::string ritualLua = extractLuaEvent(lua, "RegisterGlobalEvent(847");
    INFO(ritualLua);
    CHECK(ritualLua.find("local hasAllLichJars = true") != std::string::npos);
    CHECK(ritualLua.find("for _, player in ipairs(PartyMembers()) do") != std::string::npos);
    CHECK(ritualLua.find("if not HasItem(1417) then -- Lich Jar") != std::string::npos);
    CHECK(ritualLua.find("SetValue(ClassId, 45)") != std::string::npos);
    CHECK(ritualLua.find("while HasItem(1417) do -- Lich Jar") != std::string::npos);
    CHECK(ritualLua.find("evt.SetNPCGreeting(388, 194)") != std::string::npos);
    CHECK(ritualLua.find("local function Step_0()") == std::string::npos);
    CHECK(ritualLua.find("evt.ForPlayer(Players.Member0)") == std::string::npos);
    CHECK(ritualLua.find("evt.ForPlayer(Players.Member3)") == std::string::npos);
}

TEST_CASE("legacy lua exporter keeps mm8 lich jar checks on necromancers")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm8/_legacy/events/Global.EVT");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.itemNames[628] = "Lich Jar";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Global,
        OpenYAMM::Game::LegacyEventVersion::Mm8);

    const std::string promotionLua = extractLuaEvent(lua, "RegisterGlobalEvent(89");
    INFO(promotionLua);
    CHECK(promotionLua.find("if IsAtLeast(ClassId, 44) then") != std::string::npos);
    CHECK(promotionLua.find("if not HasItem(628) then -- Lich Jar") != std::string::npos);
    CHECK(promotionLua.find("if not IsAtLeast(ClassId, 44) then") == std::string::npos);

    const std::string repeatPromotionLua = extractLuaEvent(lua, "RegisterGlobalEvent(738");
    INFO(repeatPromotionLua);
    CHECK(repeatPromotionLua.find("if IsAtLeast(ClassId, 44) then") != std::string::npos);
    CHECK(repeatPromotionLua.find("if not HasItem(628) then -- Lich Jar") != std::string::npos);
    CHECK(repeatPromotionLua.find("if not IsAtLeast(ClassId, 44) then") == std::string::npos);
}

TEST_CASE("legacy lua exporter preserves mm8 reagent item ranges")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm8/_legacy/events/Global.EVT");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Global,
        OpenYAMM::Game::LegacyEventVersion::Mm8);

    const std::string pureSpeedLua = extractLuaEvent(lua, "RegisterGlobalEvent(181");
    INFO(pureSpeedLua);
    CHECK(pureSpeedLua.find("evt.CheckItemsCount(200, 204, 4)") != std::string::npos);
    CHECK(pureSpeedLua.find("evt.CheckItemsCount(205, 209, 2)") != std::string::npos);
    CHECK(pureSpeedLua.find("evt.CheckItemsCount(210, 214, 1)") != std::string::npos);
    CHECK(pureSpeedLua.find("evt.RemoveItems(200, 204, 4)") != std::string::npos);
    CHECK(pureSpeedLua.find("evt.RemoveItems(205, 209, 2)") != std::string::npos);
    CHECK(pureSpeedLua.find("evt.RemoveItems(210, 214, 1)") != std::string::npos);
    CHECK(pureSpeedLua.find("DecorVar(") == std::string::npos);
}

TEST_CASE("legacy lua exporter labels mm8 summon item payloads as item ids")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm8/_legacy/events/Out01.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm8/_legacy/events/OUT01.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.itemNames[200] = "Widowsweep Berries";
    lookups.objectPayloadNames[200] = "Fae dust";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm8);

    const std::string palmTreeLua = extractLuaEvent(lua, "RegisterEvent(494");
    INFO(palmTreeLua);
    CHECK(palmTreeLua.find("evt.SummonItem(200, 3896, 8080, 544, 1000, 1, true) -- Widowsweep Berries")
        != std::string::npos);
    CHECK(palmTreeLua.find("-- Fae dust") == std::string::npos);
}

TEST_CASE("legacy lua exporter preserves mm8 indoor light group ids")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm8/_legacy/events/D05.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm8/_legacy/events/D05.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "Abandoned Temple";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm8);

    const std::string buttonLua = extractLuaEvent(lua, "RegisterEvent(106");
    INFO(buttonLua);
    CHECK(buttonLua.find("evt.SetTexture(5, \"t65b11b\")") != std::string::npos);
    CHECK(buttonLua.find("evt.SetLight(5, 0)") != std::string::npos);
    CHECK(buttonLua.find("evt.SetLight(5, 1)") != std::string::npos);
    CHECK(buttonLua.find("evt.SetLight(4, 0)") == std::string::npos);
}

TEST_CASE("legacy lua exporter keeps mm6 indoor light ids zero based")
{
    const std::filesystem::path sourceRoot = OPENYAMM_SOURCE_DIR;
    const std::vector<uint8_t> evtBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/SEWER.EVT");
    const std::vector<uint8_t> strBytes =
        readBinaryFixture(sourceRoot / "assets_dev/worlds/mm6/_legacy/events/SEWER.STR");

    OpenYAMM::Game::EvtProgram evtProgram = {};
    REQUIRE(evtProgram.loadFromBytes(evtBytes));

    OpenYAMM::Game::StrTable strTable = {};
    REQUIRE(strTable.loadFromBytes(strBytes));

    OpenYAMM::Game::LegacyLuaExportLookups lookups = {};
    lookups.mapName = "Sewer";

    const std::string lua = OpenYAMM::Game::generateLegacyEventLuaChunk(
        evtProgram,
        strTable,
        lookups,
        OpenYAMM::Game::LegacyLuaExportScope::Map,
        OpenYAMM::Game::LegacyEventVersion::Mm6);

    INFO(lua);
    CHECK(lua.find("evt.SetLight(0, 1)") != std::string::npos);
}

TEST_CASE("legacy item reward metadata follows reachable fixed grants and separates trigger continuations")
{
    using namespace OpenYAMM::Game;
    std::vector<uint8_t> bytes;
    const auto append = [&](uint16_t eventId, uint8_t step, EvtOpcode opcode, std::vector<uint8_t> payload)
    {
        bytes.insert(bytes.end(), {uint8_t(4 + payload.size()), uint8_t(eventId), uint8_t(eventId >> 8),
            step, uint8_t(opcode)});
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    };
    append(1, 0, EvtOpcode::Add, {0x11, 0, 42, 0, 0, 0});
    append(1, 1, EvtOpcode::GiveItem, {1, 0, 43, 0, 0, 0});
    appendExitRecord(bytes, 1, 2);
    append(1, 3, EvtOpcode::Add, {0x11, 0, 99, 0, 0, 0}); // Unreachable after return.
    append(2, 0, EvtOpcode::Compare, {0x10, 0, 1, 0, 0, 0, 3});
    append(2, 1, EvtOpcode::GiveItem, {1, 0, 44, 0, 0, 0});
    appendExitRecord(bytes, 2, 2);
    append(2, 3, EvtOpcode::Add, {0x11, 0, 45, 0, 0, 0});
    appendExitRecord(bytes, 2, 4);
    append(3, 0, EvtOpcode::Add, {0x11, 0, 49, 0, 0, 0});
    appendExitRecord(bytes, 3, 1);
    append(3, 5, EvtOpcode::OnMapReload, {});
    append(3, 6, EvtOpcode::Add, {0x11, 0, 50, 0, 0, 0});
    appendExitRecord(bytes, 3, 7);
    append(4, 0, EvtOpcode::GiveItem, {3, 0, 0, 0, 0, 0}); // Random treasure has no fixed source ID.
    appendExitRecord(bytes, 4, 1);
    for (LegacyEventVersion version : {LegacyEventVersion::Mm6, LegacyEventVersion::Mm7, LegacyEventVersion::Mm8})
    {
        const ScriptedEventProgram program = exportPressureTrapProgram(bytes, version);
        REQUIRE(program.itemRewards().contains(1));
        CHECK(program.itemRewards().at(1) == std::vector<uint32_t>{42, 43});
        CHECK(program.itemRewards().at(2) == std::vector<uint32_t>{44, 45});
        CHECK(program.itemRewards().at(3) == std::vector<uint32_t>{49});
        CHECK_FALSE(program.itemRewards().contains(4));
        REQUIRE(program.onLoadEventIds().size() == 1);
        CHECK(program.itemRewards().at(program.onLoadEventIds().front()) == std::vector<uint32_t>{50});
    }
}

TEST_CASE("item reward declarations bind to active handlers and validate item arrays")
{
    using namespace OpenYAMM::Game;
    const auto supportBytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string support(supportBytes.begin(), supportBytes.end());
    const auto load = [&](const std::string &code, ScriptedEventScope scope = ScriptedEventScope::Map)
    {
        std::string error;
        return ScriptedEventProgram::loadFromLuaText(support + "\n" + code, "item-reward-test", scope, error);
    };
    const std::string base = "RegisterEvent(10, '', function() end)\nSetMapItemRewards(10, {42, 41, 42})\n";
    auto program = load(base);
    REQUIRE(program);
    CHECK(program->itemRewards().at(10) == std::vector<uint32_t>{41, 42});
    program = load(base + "evt.map[10] = function() end\n");
    REQUIRE(program);
    CHECK(program->itemRewards().empty());
    program = load(base + "RegisterEvent(10, '', function() end)\nSetMapItemRewards(10, {43})\n");
    REQUIRE(program);
    CHECK(program->itemRewards().at(10) == std::vector<uint32_t>{43});
    program = load(base + "evt.map[10] = nil\n");
    REQUIRE(program);
    CHECK(program->itemRewards().empty());
    CHECK_FALSE(load("SetMapItemRewards(10, {42})"));
    for (const char *array : {"{0}", "{-1}", "{1.5}", "{'42'}", "{4294967296}",
        "{[2] = 42}", "{[1] = 42, key = 43}", "false"})
        CHECK_FALSE(load(std::string("RegisterEvent(10, '', function() end)\nSetMapItemRewards(10, ") + array + ")"));
    const std::string global = "RegisterGlobalEvent(10, '', function() end)\nSetGlobalItemRewards(10, {99})\n";
    program = load(base + global);
    REQUIRE(program);
    CHECK(program->itemRewards().at(10) == std::vector<uint32_t>{41, 42});
    program = load(base + global, ScriptedEventScope::Global);
    REQUIRE(program);
    CHECK(program->itemRewards().at(10) == std::vector<uint32_t>{99});
    for (const char *value : {"false", "nil"})
    {
        CHECK_FALSE(load(base + "evt.map = " + value));
        CHECK_FALSE(load(global + "evt.global = " + value, ScriptedEventScope::Global));
    }
}

TEST_CASE("reward metadata exports separately without changing callback code or redefining handlers")
{
    using namespace OpenYAMM::Game;
    const std::vector<uint8_t> bytes = {10, 1, 0, 0, uint8_t(EvtOpcode::GiveItem), 1, 0, 42, 0, 0, 0,
        4, 1, 0, 1, uint8_t(EvtOpcode::Exit)};
    EvtProgram legacy;
    REQUIRE(legacy.loadFromBytes(bytes));
    const auto support = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    for (const auto scope : {LegacyLuaExportScope::Map, LegacyLuaExportScope::Global})
    {
        const std::string handlers = generateLegacyEventLuaChunk(legacy, {}, {}, scope);
        const std::string metadata = generateLegacyEventItemRewardsLuaChunk(legacy, {}, {}, scope);
        CHECK(handlers.find("ItemRewards(") == std::string::npos);
        CHECK(metadata.find("RegisterEvent(") == std::string::npos);
        CHECK(metadata.find("RegisterGlobalEvent(") == std::string::npos);
        CHECK(metadata.find(scope == LegacyLuaExportScope::Map
            ? "SetMapItemRewards(1, {42})" : "SetGlobalItemRewards(1, {42})") != std::string::npos);
        const ScriptedEventScope programScope = scope == LegacyLuaExportScope::Map
            ? ScriptedEventScope::Map : ScriptedEventScope::Global;
        std::string error;
        const std::string prefix(support.begin(), support.end());
        const auto program = ScriptedEventProgram::loadFromLuaText(prefix + handlers + metadata,
            "separate-item-rewards", programScope, error);
        REQUIRE_MESSAGE(program.has_value(), error);
        CHECK(program->itemRewards().at(1) == std::vector<uint32_t>{42});
        CHECK_FALSE(ScriptedEventProgram::loadFromLuaText(prefix + metadata,
            "missing-item-reward-handler", programScope, error));
    }
}

TEST_CASE("NPC location declarations are typed callback-owned possibilities with isolated scopes")
{
    using namespace OpenYAMM::Game;
    const auto supportBytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string support(supportBytes.begin(), supportBytes.end());
    const auto load = [&](const std::string &code, ScriptedEventScope scope = ScriptedEventScope::Map)
    {
        std::string error;
        return ScriptedEventProgram::loadFromLuaText(support + "\n" + code, "npc-location-test", scope, error);
    };
    const std::string base = "RegisterEvent(10, '', function() error('must not execute') end)\n"
        "SetMapNpcLocations(10, {{42, 12}, {41, 11}, {42, 12}})\n";
    auto program = load(base);
    REQUIRE(program);
    const std::vector<ScriptedEventProgram::NpcLocation> expected{{41, 11}, {42, 12}};
    CHECK(program->npcLocations().at(10) == expected);
    program = load(base + "evt.map[10] = function() end\n");
    REQUIRE(program);
    CHECK(program->npcLocations().empty());
    program = load(base + "evt.map[10] = nil\n");
    REQUIRE(program);
    CHECK(program->npcLocations().empty());
    program = load(base + "ReplaceMapEvent(10, '', function() end)\nSetMapNpcLocations(10, {{43, 13}})\n");
    REQUIRE(program);
    CHECK(program->npcLocations().at(10) == std::vector<ScriptedEventProgram::NpcLocation>{{43, 13}});
    CHECK_FALSE(load("SetMapNpcLocations(10, {{42, 11}})"));
    for (const char *array : {"false", "{false}", "{{0, 11}}", "{{42, 0}}", "{{-1, 11}}", "{{42.5, 11}}",
        "{{'42', 11}}", "{{4294967296, 11}}", "{{42, 11, 12}}", "{{42}}", "{[2] = {42, 11}}",
        "{{42, 11, extra = 12}}", "{extra = {42, 11}}"})
        CHECK_FALSE(load(std::string("RegisterEvent(10, '', function() end)\nSetMapNpcLocations(10, ") + array + ")"));
    const std::string global = "RegisterGlobalEvent(10, '', function() end)\nSetGlobalNpcLocations(10, {{99, 19}})\n";
    program = load(base + global);
    REQUIRE(program);
    CHECK(program->npcLocations().at(10) == expected);
    program = load(base + global, ScriptedEventScope::Global);
    REQUIRE(program);
    CHECK(program->npcLocations().at(10) == std::vector<ScriptedEventProgram::NpcLocation>{{99, 19}});
}

TEST_CASE("legacy NPC location metadata respects reachability branches removals and trigger continuations")
{
    using namespace OpenYAMM::Game;
    std::vector<uint8_t> bytes;
    const auto append = [&](uint16_t eventId, uint8_t step, EvtOpcode opcode, std::vector<uint8_t> payload)
    {
        bytes.insert(bytes.end(), {uint8_t(4 + payload.size()), uint8_t(eventId), uint8_t(eventId >> 8),
            step, uint8_t(opcode)});
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    };
    append(1, 0, EvtOpcode::MoveNpc, {42, 0, 0, 0, 11, 0, 0, 0});
    appendExitRecord(bytes, 1, 1);
    append(1, 2, EvtOpcode::MoveNpc, {42, 0, 0, 0, 99, 0, 0, 0}); // Unreachable.
    append(2, 0, EvtOpcode::Compare, {0x10, 0, 1, 0, 0, 0, 3});
    append(2, 1, EvtOpcode::MoveNpc, {42, 0, 0, 0, 12, 0, 0, 0});
    appendExitRecord(bytes, 2, 2);
    append(2, 3, EvtOpcode::MoveNpc, {42, 0, 0, 0, 13, 0, 0, 0});
    appendExitRecord(bytes, 2, 4);
    append(3, 0, EvtOpcode::MoveNpc, {42, 0, 0, 0, 14, 0, 0, 0});
    appendExitRecord(bytes, 3, 1);
    append(3, 5, EvtOpcode::OnMapReload, {});
    append(3, 6, EvtOpcode::MoveNpc, {42, 0, 0, 0, 15, 0, 0, 0});
    appendExitRecord(bytes, 3, 7);
    append(4, 0, EvtOpcode::MoveNpc, {42, 0, 0, 0, 0, 0, 0, 0});
    appendExitRecord(bytes, 4, 1);
    for (LegacyEventVersion version : {LegacyEventVersion::Mm6, LegacyEventVersion::Mm7, LegacyEventVersion::Mm8})
    {
        const ScriptedEventProgram program = exportPressureTrapProgram(bytes, version);
        CHECK(program.npcLocations().at(1) == std::vector<ScriptedEventProgram::NpcLocation>{{42, 11}});
        CHECK(program.npcLocations().at(2) == std::vector<ScriptedEventProgram::NpcLocation>{{42, 12}, {42, 13}});
        CHECK(program.npcLocations().at(3) == std::vector<ScriptedEventProgram::NpcLocation>{{42, 14}});
        CHECK_FALSE(program.npcLocations().contains(4));
        REQUIRE(program.onLoadEventIds().size() == 1);
        CHECK(program.npcLocations().at(program.onLoadEventIds().front())
            == std::vector<ScriptedEventProgram::NpcLocation>{{42, 15}});
    }
}

TEST_CASE("journal reference declarations bind active callbacks and reject invalid note IDs")
{
    using namespace OpenYAMM::Game;
    const auto supportBytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string support(supportBytes.begin(), supportBytes.end());
    const auto load = [&](const std::string &code, ScriptedEventScope scope = ScriptedEventScope::Map)
    {
        std::string error;
        return ScriptedEventProgram::loadFromLuaText(support + "\n" + code, "journal-ref-test", scope, error);
    };
    const std::string base = "RegisterEvent(10, '', function() error('must not execute') end)\n"
        "SetMapAutonoteRefs(10, {43, 42, 43, 65535})\n";
    auto program = load(base);
    REQUIRE(program);
    CHECK(program->autonoteReferences().at(10) == std::vector<uint32_t>{42, 43, 65535});
    for (const char *replacement : {"evt.map[10] = nil\n", "evt.map[10] = function() end\n"})
    {
        program = load(base + replacement);
        REQUIRE(program);
        CHECK(program->autonoteReferences().empty());
    }
    program = load(base + "AppendMapEvent(10, function() end)\nSetMapAutonoteRefs(10, {44})\n");
    REQUIRE(program);
    CHECK(program->autonoteReferences().at(10) == std::vector<uint32_t>{44});
    program = load(base + "SetMapAutonoteRefs(10, {})\n");
    REQUIRE(program);
    CHECK(program->autonoteReferences().empty());
    CHECK_FALSE(load("SetMapAutonoteRefs(10, {42})"));
    for (const char *array : {"false", "{0}", "{-1}", "{'42'}", "{1.5}", "{65536}", "{4294967296}",
        "{[2] = 42}", "{42, key = 43}"})
        CHECK_FALSE(load(std::string("RegisterEvent(10, '', function() end)\nSetMapAutonoteRefs(10, ") + array + ")"));
    const std::string global = "RegisterGlobalEvent(10, '', function() end)\nSetGlobalAutonoteRefs(10, {45})\n";
    program = load(base + global, ScriptedEventScope::Global);
    REQUIRE(program);
    CHECK(program->autonoteReferences().at(10) == std::vector<uint32_t>{45});
    program = load(base + global);
    REQUIRE(program);
    CHECK(program->autonoteReferences().at(10) == std::vector<uint32_t>{42, 43, 65535});
}

TEST_CASE("legacy journal reference metadata follows learned notes branches and synthetic continuations")
{
    using namespace OpenYAMM::Game;
    std::vector<uint8_t> bytes;
    const auto append = [&](uint16_t eventId, uint8_t step, EvtOpcode opcode, std::vector<uint8_t> payload)
    {
        bytes.insert(bytes.end(), {uint8_t(4 + payload.size()), uint8_t(eventId), uint8_t(eventId >> 8),
            step, uint8_t(opcode)});
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    };
    append(1, 0, EvtOpcode::Add, {0xe1, 0, 42, 0, 0, 0});
    appendExitRecord(bytes, 1, 1);
    append(1, 2, EvtOpcode::Add, {0xe1, 0, 99, 0, 0, 0}); // Unreachable after return.
    append(2, 0, EvtOpcode::Compare, {0x10, 0, 1, 0, 0, 0, 3});
    append(2, 1, EvtOpcode::Set, {0xe1, 0, 43, 0, 0, 0});
    appendExitRecord(bytes, 2, 2);
    append(2, 3, EvtOpcode::Add, {0xe1, 0, 44, 0, 0, 0});
    appendExitRecord(bytes, 2, 4);
    append(3, 0, EvtOpcode::Add, {0xe1, 0, 45, 0, 0, 0});
    appendExitRecord(bytes, 3, 1);
    append(3, 5, EvtOpcode::OnMapReload, {});
    append(3, 6, EvtOpcode::Add, {0xe1, 0, 46, 0, 0, 0});
    appendExitRecord(bytes, 3, 7);
    append(4, 0, EvtOpcode::Subtract, {0xe1, 0, 47, 0, 0, 0}); // Clearing is not learning.
    appendExitRecord(bytes, 4, 1);
    append(5, 0, EvtOpcode::Add, {0xe1, 0, 0x90, 1, 0, 0}); // Reserved blank journal row 400.
    appendExitRecord(bytes, 5, 1);
    for (LegacyEventVersion version : {LegacyEventVersion::Mm6, LegacyEventVersion::Mm7, LegacyEventVersion::Mm8})
    {
        const ScriptedEventProgram program = exportPressureTrapProgram(bytes, version);
        CHECK(program.autonoteReferences().at(1) == std::vector<uint32_t>{42});
        CHECK(program.autonoteReferences().at(2) == std::vector<uint32_t>{43, 44});
        CHECK(program.autonoteReferences().at(3) == std::vector<uint32_t>{45});
        CHECK_FALSE(program.autonoteReferences().contains(4));
        CHECK_FALSE(program.autonoteReferences().contains(5));
        REQUIRE(program.onLoadEventIds().size() == 1);
        CHECK(program.autonoteReferences().at(program.onLoadEventIds().front()) == std::vector<uint32_t>{46});
    }
}

TEST_CASE("site reference annotations bind callbacks without executing them")
{
    using namespace OpenYAMM::Game;
    const auto supportBytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string support(supportBytes.begin(), supportBytes.end());
    const auto load = [&](const std::string &code)
    {
        std::string error;
        return ScriptedEventProgram::loadFromLuaText(support + "\n" + code, "site-reference", ScriptedEventScope::Map,
            error);
    };
    const std::string base = "RegisterEvent(10, '', function() error('must not execute') end)\n"
        "SetMapSiteReference(10, 'Party gold +200')\n";
    auto program = load(base);
    REQUIRE(program);
    CHECK(program->siteReferences().at(10) == "Party gold +200");
    for (const char *replacement : {"evt.map[10] = nil\n", "evt.map[10] = function() end\n"})
    {
        program = load(base + replacement);
        REQUIRE(program);
        CHECK(program->siteReferences().empty());
    }
    program = load(base + "ReplaceMapEvent(10, '', function() end)\nSetMapSiteReference(10, 'Modded effect')");
    REQUIRE(program);
    CHECK(program->siteReferences().at(10) == "Modded effect");
    program = load(base + "SetMapSiteReference(10, '')");
    REQUIRE(program);
    CHECK(program->siteReferences().empty());
    CHECK_FALSE(load("SetMapSiteReference(10, 'No callback')"));
    CHECK_FALSE(load(base + "SetMapSiteReference(10, 123)"));
    CHECK_FALSE(load(base + "SetMapSiteReference(10, string.rep('x', 16385))"));
    CHECK_FALSE(load(base + "SetMapSiteReference(10, 'a' .. string.char(0) .. 'b')"));
    CHECK_FALSE(load(base + "SetMapSiteReference(-1, 'invalid')"));
    CHECK_FALSE(load(base + "evt.meta.map.siteReferences[10].handler = 42"));
}

TEST_CASE("global site references are callback owned and isolated from map references")
{
    using namespace OpenYAMM::Game;
    const auto bytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string support(bytes.begin(), bytes.end());
    const std::string base = "RegisterEvent(10, '', function() error('must not execute') end)\n"
        "SetMapSiteReference(10, 'Local')\n"
        "RegisterGlobalEvent(10, '', function() error('must not execute') end)\n"
        "SetGlobalSiteReference(10, 'Shared')\n";
    const auto load = [&](const std::string &code, ScriptedEventScope scope)
    {
        std::string error;
        return ScriptedEventProgram::loadFromLuaText(support + code, "global-site-references", scope, error);
    };
    const auto local = load(base, ScriptedEventScope::Map);
    auto global = load(base, ScriptedEventScope::Global);
    REQUIRE(local);
    REQUIRE(global);
    CHECK(local->siteReferences().at(10) == "Local");
    CHECK(global->siteReferences().at(10) == "Shared");
    global = load(base + "ReplaceGlobalEvent(10, '', function() end)", ScriptedEventScope::Global);
    REQUIRE(global);
    CHECK(global->siteReferences().empty());
    global = load(base + "ReplaceGlobalEvent(10, '', function() end)\nSetGlobalSiteReference(10, 'Modded')",
        ScriptedEventScope::Global);
    REQUIRE(global);
    CHECK(global->siteReferences().at(10) == "Modded");
    CHECK_FALSE(load("SetGlobalSiteReference(10, 'No handler')", ScriptedEventScope::Global));
    CHECK_FALSE(load(base + "SetGlobalSiteReference(10, 42)", ScriptedEventScope::Global));
}

TEST_CASE("site reference export follows branches and rejects incomplete or state-dependent projections")
{
    using namespace OpenYAMM::Game;
    std::vector<uint8_t> bytes;
    const auto append = [&](uint16_t eventId, uint8_t step, EvtOpcode opcode, std::vector<uint8_t> payload)
    {
        bytes.insert(bytes.end(), {uint8_t(4 + payload.size()), uint8_t(eventId), uint8_t(eventId >> 8),
            step, uint8_t(opcode)});
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    };
    append(1, 0, EvtOpcode::Compare, {0x15, 0, 99, 0, 0, 0, 3});
    append(1, 1, EvtOpcode::Add, {0x15, 0, 200, 0, 0, 0});
    appendExitRecord(bytes, 1, 2);
    appendExitRecord(bytes, 1, 3);
    append(1, 4, EvtOpcode::CastSpell, std::vector<uint8_t>(27, 0)); // Unreachable.
    append(2, 0, EvtOpcode::Add, {0x15, 0, 200, 0, 0, 0});
    append(2, 1, EvtOpcode::CastSpell, std::vector<uint8_t>(27, 0)); // Reject entire callback, not just this effect.
    appendExitRecord(bytes, 2, 2);
    append(3, 0, EvtOpcode::Compare, {0x15, 0, 99, 0, 0, 0, 0}); // Cycle.
    append(3, 1, EvtOpcode::Add, {0x15, 0, 200, 0, 0, 0});
    appendExitRecord(bytes, 3, 2);
    append(4, 0, EvtOpcode::Add, {0x15, 0, 200, 0, 0, 0});
    append(4, 1, EvtOpcode::Compare, {0x15, 0, 99, 0, 0, 0, 3}); // Compares mutated state.
    appendExitRecord(bytes, 4, 2);
    appendExitRecord(bytes, 4, 3);
    append(5, 0, EvtOpcode::Add, {0x27, 0, 2, 0, 0, 0}); // Actual Might is not a writable base stat.
    appendExitRecord(bytes, 5, 1);
    append(6, 0, EvtOpcode::Compare, {0x7b, 0, 2, 0, 0, 0, 4});
    append(6, 1, EvtOpcode::Add, {0x15, 0, 200, 0, 0, 0});
    append(6, 2, EvtOpcode::Set, {0x7b, 0, 1, 0, 0, 0}); // A boolean assignment cannot count two uses.
    appendExitRecord(bytes, 6, 3);
    appendExitRecord(bytes, 6, 4);
    append(7, 0, EvtOpcode::Compare, {0x15, 0, 99, 0, 0, 0, 2});
    append(7, 1, EvtOpcode::Jmp, {0}); // A comparison ladder's fallthrough returns to its own condition.
    appendExitRecord(bytes, 7, 2);
    EvtProgram events;
    REQUIRE(events.loadFromBytes(bytes));
    LegacyLuaExportLookups lookups;
    for (uint16_t id : {1, 2, 3, 4, 5, 6, 7}) lookups.eventTitles[id] = "Drink from the well";
    const std::string refs = generateLegacyEventSiteRefsLuaChunk(events, {}, lookups, LegacyLuaExportScope::Map);
    CHECK(refs.find("SetMapSiteReference(1,") != std::string::npos);
    CHECK(refs.find("Party gold +200") != std::string::npos);
    CHECK(refs.find("Party gold below 99") != std::string::npos);
    for (uint16_t id : {2, 3, 4, 5, 6, 7})
        CHECK(refs.find("SetMapSiteReference(" + std::to_string(id) + ",") == std::string::npos);
    // The full exporter must also terminate for a cyclic callback, using its existing labelled representation.
    const std::string cyclicCallbacks = generateLegacyEventLuaChunk(events, {}, lookups, LegacyLuaExportScope::Map);
    CHECK(cyclicCallbacks.find("RegisterEvent(3,") != std::string::npos);
    CHECK(cyclicCallbacks.size() < 65536);
    const auto supportBytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    std::string error;
    const auto cyclicProgram = ScriptedEventProgram::loadFromLuaText(
        std::string(supportBytes.begin(), supportBytes.end()) + "\n" + cyclicCallbacks,
        "@cyclic-export", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(cyclicProgram, error);
    CHECK(cyclicProgram->hasEvent(3));
    CHECK(cyclicProgram->hasEvent(7));
    // Inspect individual supported write encodings independently of that control-flow regression.
    std::vector<uint8_t> writes;
    const auto appendWrite = [&](uint16_t id, uint8_t step, EvtOpcode opcode, std::vector<uint8_t> payload)
    {
        writes.insert(writes.end(), {uint8_t(4 + payload.size()), uint8_t(id), 0, step, uint8_t(opcode)});
        writes.insert(writes.end(), payload.begin(), payload.end());
    };
    appendWrite(5, 0, EvtOpcode::Add, {0x27, 0, 2, 0, 0, 0});
    appendExitRecord(writes, 5, 1);
    appendWrite(6, 0, EvtOpcode::Set, {0x7b, 0, 1, 0, 0, 0});
    appendExitRecord(writes, 6, 1);
    EvtProgram writeEvents;
    REQUIRE(writeEvents.loadFromBytes(writes));
    const std::string callbacks = generateLegacyEventLuaChunk(writeEvents, {}, lookups, LegacyLuaExportScope::Map);
    CHECK(extractLuaEvent(callbacks, "RegisterEvent(5,").find("AddValue(ActualMight, 2)") != std::string::npos);
    CHECK(extractLuaEvent(callbacks, "RegisterEvent(6,").find("SetValue(MapVar(") != std::string::npos);
    const std::string globalRefs =
        generateLegacyEventSiteRefsLuaChunk(events, {}, lookups, LegacyLuaExportScope::Global);
    CHECK(globalRefs.find("SetGlobalSiteReference(1,") != std::string::npos);
    CHECK(globalRefs.find("Party gold +200") != std::string::npos);
    CHECK(globalRefs.find("SetMapSiteReference(") == std::string::npos);
}

TEST_CASE("Harmondale unnoted gold well reference uses the real callback thresholds")
{
    using namespace OpenYAMM::Game;
    const std::filesystem::path root =
        std::filesystem::path(OPENYAMM_SOURCE_DIR) / "assets_dev/worlds/mm7/_legacy/events";
    EvtProgram events;
    REQUIRE(events.loadFromBytes(readBinaryFixture(root / "7out02.EVT")));
    StrTable strings;
    REQUIRE(strings.loadFromBytes(readBinaryFixture(root / "7out02.STR")));
    LegacyLuaExportLookups lookups;
    lookups.eventTitles[223] = "Drink from the Well";
    const std::string refs = generateLegacyEventSiteRefsLuaChunk(events, strings, lookups,
        LegacyLuaExportScope::Map, LegacyEventVersion::Mm7);
    const size_t start = refs.find("SetMapSiteReference(223,");
    REQUIRE(start != std::string::npos);
    const std::string well = refs.substr(start, refs.find('\n', start) - start);
    CHECK(well.find("Party gold +200") != std::string::npos);
    CHECK(well.find("Banked gold below 99") != std::string::npos);
    CHECK(well.find("Party gold below 199") != std::string::npos);
    CHECK(well.find("Not used since this location reset") != std::string::npos);
    CHECK(well.find("Refreshing") == std::string::npos);
}

TEST_CASE("drink site reference kinds are callback owned and preserve explicit context overrides")
{
    using namespace OpenYAMM::Game;
    const auto supportBytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string support(supportBytes.begin(), supportBytes.end());
    const std::string base = R"(
        SetMapMetadata({contextActions = {[10] = {kind = 'generic_event', source = 'opcode'}}})
        RegisterEvent(10, 'Drink', function() error('must not execute') end)
        SetMapSiteReference(10, '', 'drink')
    )";
    const auto load = [&](const std::string &code)
    {
        std::string error;
        return ScriptedEventProgram::loadFromLuaText(support + code, "drink-context", ScriptedEventScope::Map, error);
    };
    auto program = load(base);
    REQUIRE(program);
    REQUIRE(program->getContextActionMetadata(10));
    CHECK(program->getContextActionMetadata(10)->kind == "drink");
    CHECK(program->siteReferences().empty());
    for (const char *pReplacement : {"evt.map[10] = function() end", "evt.map[10] = nil",
        "ReplaceMapEvent(10, '', function() end)"})
    {
        program = load(base + pReplacement);
        REQUIRE(program);
        const auto context = program->getContextActionMetadata(10);
        CHECK((!context || context->kind != "drink"));
    }
    program = load(base + "SetMapContextAction(10, {kind = 'well', source = 'script'})");
    REQUIRE(program);
    CHECK(program->getContextActionMetadata(10)->kind == "well");
    CHECK_FALSE(load(base + "SetMapSiteReference(10, '', 'teleport')"));
    CHECK_FALSE(load(base + "SetMapSiteReference(10, '', 1)"));
    CHECK_FALSE(load(base + "SetMapSiteReference(10, '', 'drink' .. string.char(0))"));
}

TEST_CASE("drink reference export handles condition application and personal use gates without executing events")
{
    using namespace OpenYAMM::Game;
    std::vector<uint8_t> bytes;
    const auto append = [&](uint16_t id, uint8_t step, EvtOpcode opcode, std::vector<uint8_t> payload)
    {
        bytes.insert(bytes.end(), {uint8_t(4 + payload.size()), uint8_t(id), 0, step, uint8_t(opcode)});
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    };
    append(1, 0, EvtOpcode::Compare, {0x23, 0, 35, 0, 0, 0, 3});
    append(1, 1, EvtOpcode::Set, {0x75, 0, 0, 0, 0, 0}); // Active engine selector: Paralysis, not numeric zero removal.
    appendExitRecord(bytes, 1, 2);
    appendExitRecord(bytes, 1, 3);
    append(2, 0, EvtOpcode::Compare, {0xe9, 0, 30, 0, 0, 0, 3});
    append(2, 1, EvtOpcode::Add, {0x19, 0, 25, 0, 0, 0});
    append(2, 2, EvtOpcode::Add, {0xe9, 0, 30, 0, 0, 0});
    appendExitRecord(bytes, 2, 3);
    appendExitRecord(bytes, 3, 0); // A known empty callback has no attribute or condition effects.
    append(4, 0, EvtOpcode::CastSpell, std::vector<uint8_t>(27, 0));
    appendExitRecord(bytes, 4, 1);
    append(5, 0, EvtOpcode::RandomGoTo, {1, 1, 3, 3, 0, 0});
    append(5, 1, EvtOpcode::Set, {0x71, 0, 0, 0, 0, 0});
    appendExitRecord(bytes, 5, 2);
    append(5, 3, EvtOpcode::Set, {0x73, 0, 0, 0, 0, 0});
    appendExitRecord(bytes, 5, 4);
    append(6, 0, EvtOpcode::Compare, {0x34, 0, 10, 0, 0, 0, 2});
    append(6, 1, EvtOpcode::Add, {0x3f, 0, 25, 0, 0, 0});
    appendExitRecord(bytes, 6, 2);
    EvtProgram events;
    REQUIRE(events.loadFromBytes(bytes));
    LegacyLuaExportLookups lookups;
    for (uint16_t id : {1, 2, 3, 4, 5, 6}) lookups.eventTitles[id] = "Take a Drink";
    const std::string references = generateLegacyEventSiteRefsLuaChunk(events, {}, lookups, LegacyLuaExportScope::Map);
    CHECK(references.find("Can apply: Paralysis") != std::string::npos);
    CHECK(references.find("Base Endurance below 35") != std::string::npos);
    CHECK(references.find("Not yet used by this character") != std::string::npos);
    CHECK(references.find("Temporary Might +25") != std::string::npos);
    CHECK(references.find("Body resistance below 10") != std::string::npos);
    CHECK(references.find("Body resistance bonus +25") != std::string::npos);
    CHECK(references.find("No attribute or condition change") != std::string::npos);
    CHECK(references.find("Can apply: Medium poison") != std::string::npos);
    CHECK(references.find("Can apply: Severe poison") != std::string::npos);
    CHECK(references.find("their probabilities are not shown") != std::string::npos);
    CHECK(references.find("SetMapSiteReference(4, \"\", \"drink\")") != std::string::npos);
    const auto supportBytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string callbacks = generateLegacyEventLuaChunk(events, {}, lookups, LegacyLuaExportScope::Map);
    std::string error;
    const auto program = ScriptedEventProgram::loadFromLuaText(std::string(supportBytes.begin(), supportBytes.end())
        + callbacks + references, "drink-export", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program, error);
    REQUIRE(program->getContextActionMetadata(1));
    CHECK(program->getContextActionMetadata(1)->kind == "drink");
    CHECK(program->getContextActionMetadata(4)->kind == "drink");
}

TEST_CASE("typed interaction references bind active callbacks and validate semantic fields")
{
    using namespace OpenYAMM::Game;
    const auto bytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string support(bytes.begin(), bytes.end());
    const auto load = [&](const std::string &code)
    {
        std::string error;
        return ScriptedEventProgram::loadFromLuaText(support + code, "interaction-reference",
            ScriptedEventScope::Map, error);
    };
    const std::string handler = "RegisterEvent(1, 'Door', function() error('must not execute') end)\n";
    const std::string definition = "SetMapInteractionReference(1, {kind = 'enter_dungeon', itemId = 610, "
        "openedMapFlag = 'CrystalOpened', targetMap = 'd10.blv'})\n";
    auto program = load(handler + definition);
    REQUIRE(program);
    REQUIRE(program->interactionReferences().contains(1));
    CHECK(program->interactionReferences().at(1).itemId == 610);
    REQUIRE(program->getContextActionMetadata(1));
    CHECK(program->getContextActionMetadata(1)->kind == "enter_dungeon");
    CHECK(program->getContextActionMetadata(1)->targetMap == "d10.blv");
    for (const char *pReplacement : {"evt.map[1] = nil", "evt.map[1] = function() end",
        "ReplaceMapEvent(1, '', function() end)", "SetMapInteractionReference(1, nil)"})
    {
        program = load(handler + definition + pReplacement);
        REQUIRE(program);
        CHECK(program->interactionReferences().empty());
    }
    CHECK_FALSE(load(definition));
    for (const char *pFields : {"kind = 'enter_dungeon', itemId = 610, targetMap = 'd10.blv'",
        "kind = 'enter_dungeon', itemId = true, openedMapFlag = 'Opened', targetMap = 'd10.blv'",
        "kind = 'enter_dungeon', itemId = 1.5, openedMapFlag = 'Opened', targetMap = 'd10.blv'",
        "kind = 'enter_dungeon', itemId = 610, openedMapFlag = 'Opened', targetMap = '../d10.blv'",
        "kind = 'enter_dungeon', itemId = 610, openedMapFlag = 'Opened/', targetMap = 'd10.blv'",
        "kind = 'enter_dungeon', itemId = 610, openedMapFlag = 'Opened', targetMap = 'd10.blv', typo = 1",
        "kind = 'stone_to_flesh', itemId = 339, npcId = 42, spriteId = 20",
        "kind = 'rescue', itemId = 1431, npcId = 400, spriteId = 1, completionQBit = 0"})
        CHECK_FALSE(load(handler + "SetMapInteractionReference(1, {" + pFields + "})"));
    program = load(handler + definition + "SetMapContextAction(1, {kind = 'well', source = 'script'})");
    REQUIRE(program);
    CHECK(program->getContextActionMetadata(1)->kind == "well");
    CHECK_FALSE(program->getContextActionMetadata(1)->targetMap);
}

TEST_CASE("passage site groups belong to current callbacks and reject malformed or overlapping members")
{
    using namespace OpenYAMM::Game;
    const auto bytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string support(bytes.begin(), bytes.end());
    const auto load = [&](const std::string &code)
    {
        std::string error;
        return ScriptedEventProgram::loadFromLuaText(support + code, "site-groups", ScriptedEventScope::Map, error);
    };
    const std::string handler = "RegisterEvent(10, 'Door', function() error('must not execute') end)\n";
    const std::string declaration = "SetMapSiteGroups(10, {{'surface/0', 'surface/2'}, "
        "{'bmodel/1/surface/0', 'bmodel/2/surface/0'}})\n";
    auto program = load(handler + declaration);
    REQUIRE(program);
    REQUIRE(program->siteGroups().contains(10));
    REQUIRE(program->siteGroups().at(10).size() == 2);
    CHECK(program->siteGroups().at(10)[0] == std::vector<std::string>{"surface/0", "surface/2"});
    for (const char *pReplacement : {"evt.map[10] = nil", "evt.map[10] = function() end",
        "ReplaceMapEvent(10, '', function() end)", "SetMapSiteGroups(10, {})", "SetMapSiteGroups(10, nil)"})
    {
        program = load(handler + declaration + pReplacement);
        REQUIRE(program);
        CHECK(program->siteGroups().empty());
    }
    CHECK_FALSE(load(declaration));
    for (const char *pValue : {"false", "{'surface/0', 'surface/1'}", "{{'surface/0'}}",
        "{{'surface/0', 'surface/0'}}", "{{'surface/0', 'surface/1'}, {'surface/1', 'surface/2'}}",
        "{{'surface/0', 1}}", "{{'surface/0', '../surface/2'}}",
        "{{'surface/0', 'surface/01'}}", "{{'surface/0', 'surface/4294967296'}}",
        "{{'surface/0', 'bmodel/0/decoration/1'}}", "{{'surface/0', 'invented/1'}}",
        "{[1] = {'surface/0', 'surface/1'}, [3] = {'surface/2', 'surface/3'}}",
        "{{'surface/0', 'surface/1', extra = 'surface/2'}}"})
        CHECK_FALSE(load(handler + "SetMapSiteGroups(10, " + pValue + ")"));
    program = load(handler + declaration + "ReplaceMapEvent(10, '', function() end)\n"
        "SetMapSiteGroups(10, {{'surface/4', 'surface/5'}})");
    REQUIRE(program);
    CHECK(program->siteGroups().at(10)[0][0] == "surface/4");
}

TEST_CASE("gate references are callback-owned and opening named networks preserves only local event provenance")
{
    using namespace OpenYAMM::Game;
    std::string error;
    const auto load = [&](const std::string &code, ScriptedEventScope scope = ScriptedEventScope::Map)
    { return ScriptedEventProgram::loadFromLuaText(code, "@gate-api", scope, error); };
    const auto bytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string support(bytes.begin(), bytes.end());
    const std::string handler = "evt.map[10] = function() end\n";
    const std::string reference = "SetMapInteractionReference(10, {kind='dimension_door', network='test.gate'})\n";
    auto program = load(support + handler + reference);
    REQUIRE_MESSAGE(program, error);
    CHECK(program->interactionReferences().at(10).network == "test.gate");
    for (const char *pChanged : {"evt.map[10]=function() end", "evt.map[10]=nil"})
    {
        program = load(support + handler + reference + pChanged);
        REQUIRE(program);
        CHECK(program->interactionReferences().empty());
    }
    for (const char *pFields : {"kind='dimension_door', network=''", "kind='dimension_door', network='bad/path'",
        "kind='dimension_door', network='bad..path'", "kind='dimension_door', network='.bad'",
        "kind='dimension_door', network='test.gate', itemId=1"})
        CHECK_FALSE(load(support + handler + "SetMapInteractionReference(10, {" + pFields + "})"));
    EventRuntime runtime;
    EventRuntimeState state;
    state.mapFileName = "7d29.blv";
    program = load("evt.map[10] = function() evt.OpenDimensionDoor('test.gate') end");
    REQUIRE(program);
    REQUIRE(runtime.executeEventById(program, std::nullopt, 10, state));
    CHECK(state.pendingDimensionDoorOverlay);
    CHECK(state.pendingDimensionDoorNetwork == "test.gate");
    CHECK(state.pendingDimensionDoorEventId == 10);
    program = load("evt.global[10] = function() evt.OpenDimensionDoor() end", ScriptedEventScope::Global);
    REQUIRE(program);
    REQUIRE(runtime.executeEventById(std::nullopt, program, 10, state));
    CHECK(state.pendingDimensionDoorOverlay);
    CHECK(state.pendingDimensionDoorNetwork.empty());
    CHECK(state.pendingDimensionDoorEventId == 0);
    for (const char *pArgument : {"'bad/path'", "'bad..network'", "'.bad'", "''", "12"})
    {
        program = load("evt.map[10] = function() evt.OpenDimensionDoor(" + std::string(pArgument) + ") end");
        REQUIRE(program);
        state.pendingDimensionDoorOverlay = true;
        state.pendingDimensionDoorNetwork = "test.gate";
        state.pendingDimensionDoorEventId = 10;
        CHECK_FALSE(runtime.executeEventById(program, std::nullopt, 10, state));
        CHECK_FALSE(state.pendingDimensionDoorOverlay);
        CHECK(state.pendingDimensionDoorNetwork.empty());
        CHECK(state.pendingDimensionDoorEventId == 0);
    }
    program = load("evt.map[10] = function() evt.OpenDimensionDoor('test.gate'); evt.ClearDimensionDoorOverlay() end");
    REQUIRE(program);
    REQUIRE(runtime.executeEventById(program, std::nullopt, 10, state));
    CHECK_FALSE(state.pendingDimensionDoorOverlay);
    CHECK(state.pendingDimensionDoorNetwork.empty());
    CHECK(state.pendingDimensionDoorEventId == 0);
}


TEST_CASE("connection targets are strict callback-owned possibilities without executing travel")
{
    using namespace OpenYAMM::Game;
    const auto bytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string support(bytes.begin(), bytes.end());
    std::string error;
    const auto load = [&](const std::string &code)
    {
        return ScriptedEventProgram::loadFromLuaText(support + code, "@connection-api", ScriptedEventScope::Map, error);
    };
    const std::string handler = "evt.map[10] = function() error('Must not execute travel') end\n";
    const std::string targets = "SetMapConnectionTargets(10, {'mdt01.blv', 'mdr01.blv'})\n";
    auto program = load(handler + targets);
    REQUIRE_MESSAGE(program, error);
    REQUIRE(program->getContextActionMetadata(10));
    CHECK(program->getContextActionMetadata(10)->kind == "passage");
    CHECK(program->getContextActionMetadata(10)->targetMaps.size() == 2);
    CHECK_FALSE(program->getContextActionMetadata(10)->targetMap);
    for (const char *pReplacement : {"evt.map[10] = function() end", "evt.map[10] = nil",
        "ReplaceMapEvent(10, '', function() end)", "SetMapConnectionTargets(10, nil)",
        "SetMapConnectionTargets(10, {})"})
    {
        program = load(handler + targets + pReplacement);
        REQUIRE(program);
        CHECK(program->connectionTargets().empty());
    }
    for (const char *pMaps : {"{'../map.blv'}", "{'MAP.BLV'}", "{'test.blv', 'test.blv'}", "{1}",
        "{[1]='a.blv', [3]='b.blv'}", "{bad='a.blv'}", "{'a.lua'}", "{'a.blv', false}", "'a.blv'"})
        CHECK_FALSE(load(handler + "SetMapConnectionTargets(10, " + pMaps + ")"));
    CHECK_FALSE(load("SetMapConnectionTargets(10, {'a.blv'})"));
}

TEST_CASE("connection target export retains both real Barrow lever routes instead of the first branch")
{
    using namespace OpenYAMM::Game;
    const auto root = std::filesystem::path(OPENYAMM_SOURCE_DIR);
    EvtProgram events;
    REQUIRE(events.loadFromBytes(readBinaryFixture(root / "assets_dev/worlds/mm7/_legacy/events/MDK01.EVT")));
    LegacyLuaExportLookups lookups;
    lookups.sourceMapFile = "mdk01.blv";
    const std::string references = generateLegacyEventSiteRefsLuaChunk(events, {}, lookups,
        LegacyLuaExportScope::Map, LegacyEventVersion::Mm7);
    const auto support = readBinaryFixture(root / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string callbacks = generateLegacyEventLuaChunk(events, {}, lookups,
        LegacyLuaExportScope::Map, LegacyEventVersion::Mm7);
    std::string error;
    const auto program = ScriptedEventProgram::loadFromLuaText(
        std::string(support.begin(), support.end()) + callbacks + references,
        "@barrow-connection-targets", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program, error);
    const auto metadata = program->getContextActionMetadata(502);
    REQUIRE(metadata);
    REQUIRE(metadata->targetMaps.size() == 2);
    CHECK_FALSE(metadata->targetMap);
    CHECK_FALSE(metadata->targetName);
    EventRuntime runtime;
    for (uint8_t setting : {1, 2})
    {
        EventRuntimeState state;
        state.mapFileName = "mdk01.blv";
        state.mapVars[2] = setting;
        REQUIRE(runtime.executeEventById(program, std::nullopt, 502, state));
        REQUIRE(state.pendingMapMove);
        const std::string expected = setting == 1 ? "mdt01.blv" : "mdr01.blv";
        CHECK(state.pendingMapMove->mapName == expected);
        CHECK(std::find(metadata->targetMaps.begin(), metadata->targetMaps.end(), expected)
            != metadata->targetMaps.end());
        CHECK(state.mapVars[2] == setting);
    }
}


namespace
{
struct ConnectionReferenceFixture
{
    std::vector<uint8_t> bytes;
    OpenYAMM::Game::LegacyLuaExportLookups lookups;

    ConnectionReferenceFixture()
    {
        lookups.sourceMapFile = "source.blv";
        lookups.mapNamesByFile = {{"a.blv", "First destination"}, {"b.blv", "Second destination"}};
        lookups.questNotes[519] = "The council has recognized the party.";
        lookups.itemNames[619] = "Pirate Leader's Key";
    }
    void append(uint8_t step, OpenYAMM::Game::EvtOpcode opcode, const std::vector<uint8_t> &payload)
    {
        bytes.insert(bytes.end(), {uint8_t(4 + payload.size()), 10, 0, step, uint8_t(opcode)});
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    }
    void move(uint8_t step, const std::string &file)
    {
        std::vector<uint8_t> payload(26);
        payload.insert(payload.end(), file.begin(), file.end());
        payload.push_back(0);
        append(step, OpenYAMM::Game::EvtOpcode::MoveToMap, payload);
    }
    std::optional<OpenYAMM::Game::ScriptedEventProgram> program(bool realCallbacks = false) const
    {
        using namespace OpenYAMM::Game;
        EvtProgram legacy;
        REQUIRE(legacy.loadFromBytes(bytes));
        const auto support = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
            / "assets_dev/engine/scripts/common/event_support.lua");
        const std::string reference = generateLegacyEventSiteRefsLuaChunk(legacy, {}, lookups,
            LegacyLuaExportScope::Map, LegacyEventVersion::Mm7);
        std::string error;
        const std::string callbacks = realCallbacks
            ? generateLegacyEventLuaChunk(legacy, {}, lookups, LegacyLuaExportScope::Map, LegacyEventVersion::Mm7)
            : "evt.map[10] = function() error('Metadata must not execute travel') end\n";
        const auto result = ScriptedEventProgram::loadFromLuaText(std::string(support.begin(), support.end())
            + callbacks + reference,
            "@connection-requirements", ScriptedEventScope::Map, error);
        REQUIRE_MESSAGE(result, error);
        return result;
    }
};
}

TEST_CASE("connection requirement references retain complete positive and negative paths and inventory scope")
{
    using namespace OpenYAMM::Game;
    for (bool allMembers : {false, true})
    {
        ConnectionReferenceFixture f;
        f.append(0, EvtOpcode::ForPartyMember, {uint8_t(allMembers ? 5 : 4)});
        f.append(1, EvtOpcode::Compare, {0x10, 0, 7, 2, 0, 0, 5}); // QBit 519.
        f.move(2, "a.blv");
        f.append(3, EvtOpcode::Exit, {});
        f.append(5, EvtOpcode::Compare, {0x11, 0, 0x6b, 2, 0, 0, 8}); // Item 619.
        f.move(6, "a.blv");
        f.append(7, EvtOpcode::Exit, {});
        f.move(8, "b.blv");
        f.append(9, EvtOpcode::Exit, {});
        const auto program = f.program();
        REQUIRE(program->siteReferences().contains(10));
        const std::string &text = program->siteReferences().at(10);
        CHECK(text.find("Destination: First destination") != std::string::npos);
        CHECK(text.find("Destination: Second destination") != std::string::npos);
        CHECK(text.find("Campaign record absent: The council has recognized the party.") != std::string::npos);
        CHECK(text.find("Campaign record present: The council has recognized the party.") != std::string::npos);
        CHECK(text.find(allMembers ? "A party member carries: Pirate Leader's Key"
            : "Selected character carries: Pirate Leader's Key") != std::string::npos);
        CHECK(text.find(allMembers ? "No party member carries: Pirate Leader's Key"
            : "Selected character does not carry: Pirate Leader's Key") != std::string::npos);
        CHECK(text.find("not current availability") != std::string::npos);
        CHECK(text.find("Other gameplay effects are not listed") != std::string::npos);
        CHECK(text.find("QBit") == std::string::npos);
        CHECK(text.find("a.blv") == std::string::npos);
        CHECK(text.find("Any one of these alternatives:") != std::string::npos);
        EventRuntime runtime;
        const auto executable = f.program(true);
        for (bool quest : {false, true})
            for (bool item : {false, true})
            {
                Party::Snapshot seed;
                seed.members.resize(2);
                Party party;
                party.restoreSnapshot(seed);
                REQUIRE(party.memberCount() == 2);
                party.setQuestBit(519, quest);
                if (item)
                {
                    InventoryItem key;
                    key.objectDescriptionId = 619;
                    REQUIRE(party.member(1)->addInventoryItem(key));
                }
                EventRuntimeState state;
                state.mapFileName = "source.blv";
                REQUIRE(runtime.executeEventById(executable, std::nullopt, 10, state, &party));
                REQUIRE(state.pendingMapMove);
                REQUIRE(state.pendingMapMove->mapName);
                CHECK(*state.pendingMapMove->mapName == (quest && item && allMembers ? "b.blv" : "a.blv"));
                CHECK(party.hasQuestBit(519) == quest);
            }
    }
}

TEST_CASE("connection requirement references reject incomplete or state-dependent path explanations")
{
    using namespace OpenYAMM::Game;
    for (const std::string variation : {"unknown predicate", "mutated before predicate", "cycle", "unmapped target",
        "missing quest text", "unknown action", "unconditional", "two moves"})
    {
        ConnectionReferenceFixture f;
        const bool mutated = variation == "mutated before predicate";
        if (mutated) f.append(0, EvtOpcode::Set, {0x10, 0, 7, 2, 0, 0});
        const uint8_t selector = variation == "unknown predicate" ? 0x7d : 0x10; // Unlabelled map state.
        if (variation != "unconditional")
            f.append(1, EvtOpcode::Compare, {selector, 0, 7, 2, 0, 0, 5});
        if (variation == "unknown action") f.append(2, EvtOpcode::CastSpell, std::vector<uint8_t>(22));
        f.move(3, "a.blv");
        f.append(4, EvtOpcode::Exit, {});
        if (variation == "cycle") f.append(5, EvtOpcode::Jmp, {5});
        else f.move(5, variation == "unmapped target" ? "missing.blv" : "b.blv");
        if (variation == "two moves") f.move(6, "a.blv");
        f.append(7, EvtOpcode::Exit, {});
        if (variation == "missing quest text") f.lookups.questNotes.clear();
        const auto program = f.program();
        CHECK_MESSAGE(program->siteReferences().empty(), variation);
        CHECK_FALSE(program->connectionTargets().empty());
    }
}


TEST_CASE("connection references omit entry bookkeeping and prune contradictory repeated predicates")
{
    using namespace OpenYAMM::Game;
    ConnectionReferenceFixture f;
    f.append(0, EvtOpcode::Compare, {0x10, 0, 7, 2, 0, 0, 4});
    f.append(1, EvtOpcode::Set, {0x10, 0, 7, 2, 0, 0});
    f.move(2, "a.blv");
    f.append(3, EvtOpcode::Exit, {});
    f.move(4, "a.blv");
    f.append(5, EvtOpcode::Exit, {});
    CHECK(f.program()->siteReferences().empty());
    f.bytes.clear();
    f.append(0, EvtOpcode::Compare, {0x10, 0, 7, 2, 0, 0, 4});
    f.append(1, EvtOpcode::Exit, {});
    f.append(4, EvtOpcode::Compare, {0x10, 0, 7, 2, 0, 0, 7});
    f.move(5, "b.blv"); // Unreachable: requires the same quest bit both set and unset.
    f.append(6, EvtOpcode::Exit, {});
    f.move(7, "a.blv");
    f.append(8, EvtOpcode::Exit, {});
    const auto program = f.program();
    REQUIRE(program->siteReferences().contains(10));
    CHECK(program->siteReferences().at(10).find("Second destination") == std::string::npos);
    CHECK(program->siteReferences().at(10).find("First destination") != std::string::npos);
}

TEST_CASE("connection requirement references describe exact gold boundaries and remapped campaign records")
{
    using namespace OpenYAMM::Game;
    ConnectionReferenceFixture f;
    f.append(0, EvtOpcode::Compare, {0x15, 0, 100, 0, 0, 0, 3});
    f.append(1, EvtOpcode::Exit, {});
    f.move(3, "a.blv");
    f.append(4, EvtOpcode::Exit, {});
    const auto program = f.program(true);
    REQUIRE(program->siteReferences().contains(10));
    CHECK(program->siteReferences().at(10).find("Party gold at least 100") != std::string::npos);
    EventRuntime runtime;
    for (int gold : {99, 100})
    {
        Party::Snapshot seed;
        seed.members.resize(1);
        seed.gold = gold;
        Party party;
        party.restoreSnapshot(seed);
        EventRuntimeState state;
        REQUIRE(runtime.executeEventById(program, std::nullopt, 10, state, &party));
        CHECK(state.pendingMapMove.has_value() == (gold == 100));
        CHECK(party.gold() == gold);
    }
    f.bytes.clear();
    f.lookups.qbitRemaps[99] = 519;
    f.append(0, EvtOpcode::Compare, {0x10, 0, 99, 0, 0, 0, 3});
    f.append(1, EvtOpcode::Exit, {});
    f.move(3, "a.blv");
    f.append(4, EvtOpcode::Exit, {});
    REQUIRE(f.program()->siteReferences().contains(10));
    CHECK(f.program()->siteReferences().at(10).find("The council has recognized the party.") != std::string::npos);
}


TEST_CASE("site reference dependencies invalidate stale controller descriptions without executing callbacks")
{
    using namespace OpenYAMM::Game;
    const auto bytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string support(bytes.begin(), bytes.end());
    const std::string handlers = "evt.map[10]=function() error('exit must not execute') end\n"
        "evt.map[11]=function() error('lever must not execute') end\n"
        "evt.map[12]=function() error('second controller must not execute') end\n";
    const auto load = [&](const std::string &code)
    {
        std::string error;
        return ScriptedEventProgram::loadFromLuaText(support + handlers + code, "@reference-dependencies",
            ScriptedEventScope::Map, error);
    };
    const std::string reference = "SetMapSiteReference(10, 'Lever controls the exit', nil, {11, 12})\n";
    auto program = load(reference);
    REQUIRE(program);
    CHECK(program->siteReferences().at(10) == "Lever controls the exit");
    for (const std::string replacement : {"evt.map[10]=nil", "evt.map[11]=nil", "evt.map[12]=function() end",
        "ReplaceMapEvent(11, '', function() end)"})
    {
        program = load(reference + replacement);
        REQUIRE(program);
        CHECK(program->siteReferences().empty());
    }
    program = load(reference + "evt.map[13]=function() end");
    REQUIRE(program);
    CHECK(program->siteReferences().contains(10));
    program = load(reference + "evt.map[11]=function() end\nSetMapSiteReference(10,'Updated',nil,{11})");
    REQUIRE(program);
    CHECK(program->siteReferences().at(10) == "Updated");
    for (const char *pDependencies : {"false", "'11'", "{11, 11}", "{10}", "{13}", "{0}", "{65536}",
        "{11.5}", "{[1]=11,[3]=12}", "{named=11}", "{11,false}"})
        CHECK_FALSE(load("SetMapSiteReference(10,'text',nil," + std::string(pDependencies) + ")"));
    CHECK_FALSE(load(reference + "evt.meta.map.siteReferences[10].dependencies[11]=false"));
    CHECK_FALSE(load(reference + "evt.meta.map.siteReferences[10].dependencies[0]=function() end"));
}

TEST_CASE("Barrow routing requirements bind both the exit and its complete two-position lever")
{
    using namespace OpenYAMM::Game;
    const auto root = std::filesystem::path(OPENYAMM_SOURCE_DIR);
    const auto bytes = readBinaryFixture(root / "assets_dev/worlds/mm7/_legacy/events/MDK01.EVT");
    EvtProgram events;
    REQUIRE(events.loadFromBytes(bytes));
    LegacyLuaExportLookups lookups;
    lookups.sourceMapFile = "mdk01.blv";
    lookups.eventTitles[451] = "Lever";
    lookups.mapNamesByFile = {{"out11.odm", "Barrow Downs"}, {"mdt01.blv", "Barrow IX"},
        {"mdr01.blv", "Barrow X"}};
    const auto support = readBinaryFixture(root / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string callbacks = generateLegacyEventLuaChunk(events, {}, lookups,
        LegacyLuaExportScope::Map, LegacyEventVersion::Mm7);
    const std::string refs = generateLegacyEventSiteRefsLuaChunk(events, {}, lookups,
        LegacyLuaExportScope::Map, LegacyEventVersion::Mm7);
    const auto load = [&](const std::string &overlay)
    {
        std::string error;
        const auto result = ScriptedEventProgram::loadFromLuaText(std::string(support.begin(), support.end())
            + callbacks + refs + overlay, "@barrow-controls", ScriptedEventScope::Map, error);
        REQUIRE_MESSAGE(result, error);
        return result;
    };
    const auto program = load("");
    REQUIRE(program->siteReferences().contains(502));
    const std::string &text = program->siteReferences().at(502);
    CHECK(text.find("Destination: Barrow IX\nRequirements (all):\n- Controlling lever: first position")
        != std::string::npos);
    CHECK(text.find("Destination: Barrow X\nRequirements (all):\n- Controlling lever: second position")
        != std::string::npos);
    CHECK(text.find("MapVar") == std::string::npos);
    CHECK(refs.find(", nil, {451})") != std::string::npos);
    for (const std::string overlay : {"evt.map[451]=function() end", "evt.map[451]=nil"})
    {
        const auto replaced = load(overlay);
        CHECK_FALSE(replaced->siteReferences().contains(502));
        CHECK(replaced->connectionTargets().at(502).size() == 2);
    }
    EventRuntime runtime;
    for (uint8_t position : {0, 1, 2, 255})
    {
        EventRuntimeState state;
        state.mapFileName = "mdk01.blv";
        state.mapVars[2] = position;
        REQUIRE(runtime.executeEventById(program, std::nullopt, 502, state));
        REQUIRE(state.pendingMapMove);
        CHECK(state.pendingMapMove->mapName == (position < 2 ? "mdt01.blv" : "mdr01.blv"));
        CHECK(state.mapVars[2] == position);
    }
    // A second source of this control state makes its inferred ownership ambiguous.
    std::vector<uint8_t> competing = bytes;
    competing.insert(competing.end(), {10, 0x60, 0xea, 0, uint8_t(EvtOpcode::Set), 0x7d, 0, 3, 0, 0, 0});
    appendExitRecord(competing, 60000, 1);
    EvtProgram modified;
    REQUIRE(modified.loadFromBytes(competing));
    CHECK(generateLegacyEventSiteRefsLuaChunk(modified, {}, lookups, LegacyLuaExportScope::Map,
        LegacyEventVersion::Mm7).find("Controlling lever") == std::string::npos);
}


TEST_CASE("Zokarr routing references describe random destinations without claiming a lever requirement")
{
    using namespace OpenYAMM::Game;
    const auto root = std::filesystem::path(OPENYAMM_SOURCE_DIR);
    EvtProgram events;
    REQUIRE(events.loadFromBytes(readBinaryFixture(root / "assets_dev/worlds/mm7/_legacy/events/7D13.EVT")));
    LegacyLuaExportLookups lookups;
    lookups.sourceMapFile = "7d13.blv";
    lookups.mapNamesByFile = {{"mdk02.blv", "Barrow IV"}, {"mdt02.blv", "Barrow VI"}};
    const std::string refs = generateLegacyEventSiteRefsLuaChunk(events, {}, lookups,
        LegacyLuaExportScope::Map, LegacyEventVersion::Mm7);
    const auto support = readBinaryFixture(root / "assets_dev/engine/scripts/common/event_support.lua");
    std::string error;
    const auto program = ScriptedEventProgram::loadFromLuaText(std::string(support.begin(), support.end())
        + "evt.map[501]=function() error('Do not roll the destination while reading metadata') end\n" + refs,
        "@random-connection-reference", ScriptedEventScope::Map, error);
    REQUIRE_MESSAGE(program, error);
    REQUIRE(program->siteReferences().contains(501));
    const std::string &text = program->siteReferences().at(501);
    CHECK(text.find("Destination: Barrow IV") != std::string::npos);
    CHECK(text.find("Destination: Barrow VI") != std::string::npos);
    CHECK(text.find("at random; probabilities are not shown") != std::string::npos);
    CHECK(text.find("Controlling lever") == std::string::npos);
}

TEST_CASE("NPC topic references are typed callback-owned possibilities and do not execute dialogue")
{
    using namespace OpenYAMM::Game;
    const auto bytes = readBinaryFixture(std::filesystem::path(OPENYAMM_SOURCE_DIR)
        / "assets_dev/engine/scripts/common/event_support.lua");
    const std::string support(bytes.begin(), bytes.end());
    for (bool global : {false, true})
    {
        const std::string scope = global ? "global" : "map";
        const std::string setter = global ? "SetGlobalNpcTopicRefs" : "SetMapNpcTopicRefs";
        const std::string handler = "evt." + scope + "[10]=function() error('Must not execute dialogue') end\n";
        const auto load = [&](const std::string &code)
        {
            std::string error;
            return ScriptedEventProgram::loadFromLuaText(support + handler + code, "@npc-topic-references",
                global ? ScriptedEventScope::Global : ScriptedEventScope::Map, error);
        };
        const std::string refs = setter + "(10, {{49, 611}, {28, 602}, {28, 602}})\n"
            + (global ? "SetGlobalQBitGrants" : "SetMapQBitGrants") + "(10, {430})\n";
        auto program = load(refs);
        REQUIRE(program);
        CHECK(program->qbitGrants().at(10) == std::vector<uint32_t>{430});
        CHECK(program->npcTopicReferences().at(10)
            == std::vector<ScriptedEventProgram::NpcTopicReference>{{28, 602}, {49, 611}});
        for (const char *pReplacement : {"nil", "function() end"})
        {
            program = load(refs + "evt." + scope + "[10]=" + pReplacement);
            REQUIRE(program);
            CHECK(program->npcTopicReferences().empty());
            CHECK(program->qbitGrants().empty());
        }
        for (const char *pRefs : {"false", "{{28, 0}}", "{{0, 602}}", "{{28, 602, 7}}", "{{28, true}}",
            "{[1]={28, 602},[3]={49, 611}}", "{{4294967296, 602}}"})
            CHECK_FALSE(load(setter + "(10," + pRefs + ")"));
        REQUIRE(load(setter + "(10,{})"));
        CHECK(load(setter + "(10,{})")->npcTopicReferences().empty());
    }
}

TEST_CASE("NPC topic reference export follows reachable branches and omits cleared or unreachable topics")
{
    using namespace OpenYAMM::Game;
    std::vector<uint8_t> bytes;
    const auto append = [&](uint16_t event, uint8_t step, EvtOpcode opcode, std::vector<uint8_t> payload)
    {
        bytes.insert(bytes.end(), {uint8_t(4 + payload.size()), uint8_t(event), uint8_t(event >> 8),
            step, uint8_t(opcode)});
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    };
    append(10, 0, EvtOpcode::Compare, {0x10, 0, 1, 0, 0, 0, 3});
    append(10, 1, EvtOpcode::SetNpcTopic, {28, 0, 0, 0, 2, 0x5a, 2, 0, 0}); // Topic 602.
    appendExitRecord(bytes, 10, 2);
    append(10, 3, EvtOpcode::SetNpcTopic, {49, 0, 0, 0, 0, 0x63, 2, 0, 0}); // Topic 611.
    appendExitRecord(bytes, 10, 4);
    append(10, 5, EvtOpcode::SetNpcTopic, {99, 0, 0, 0, 0, 0x64, 2, 0, 0}); // Unreachable.
    append(11, 0, EvtOpcode::SetNpcTopic, {28, 0, 0, 0, 2, 0, 0, 0, 0}); // Clears a topic.
    appendExitRecord(bytes, 11, 1);
    append(12, 0, EvtOpcode::Set, {0x10, 0, 0xae, 1, 0, 0}); // Grants QBit 430.
    appendExitRecord(bytes, 12, 1);
    append(13, 0, EvtOpcode::Subtract, {0x10, 0, 0xae, 1, 0, 0}); // Clearing is not a grant.
    appendExitRecord(bytes, 13, 1);
    EvtProgram events;
    REQUIRE(events.loadFromBytes(bytes));
    const std::string refs = generateLegacyEventNpcTopicRefsLuaChunk(events, {}, {}, LegacyLuaExportScope::Global);
    CHECK(refs.find("SetGlobalNpcTopicRefs(10, {{28, 602}, {49, 611}})") != std::string::npos);
    CHECK(refs.find("99, 612") == std::string::npos);
    CHECK(refs.find("SetGlobalNpcTopicRefs(11") == std::string::npos);
    CHECK(refs.find("SetGlobalQBitGrants(12, {430})") != std::string::npos);
    CHECK(refs.find("SetGlobalQBitGrants(13") == std::string::npos);
}
