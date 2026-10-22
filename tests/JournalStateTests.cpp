#include "doctest/doctest.h"

#include "game/events/EventRuntime.h"
#include "game/gameplay/JournalState.h"

#include <array>

using namespace OpenYAMM::Game;

namespace
{
constexpr uint32_t Note = (206u << 16) | 0x00E1u;
constexpr uint32_t LegacyNote = (206u << 16) | 0x00DFu;
constexpr const char *NoteKey = "openyamm.journal.note/206";
constexpr const char *Migration = "openyamm.migration.journal-notes-v1";
}

TEST_CASE("JournalState aliases share party knowledge and preserve explicit clears")
{
    CHECK(journalNoteId(Note) == 206);
    CHECK(journalNoteId(LegacyNote) == 206);
    CHECK_FALSE(journalNoteId(0x00E1u));
    CHECK_FALSE(journalNoteId(0x00DFu));
    CHECK_FALSE(journalNoteId((206u << 16) | 0x00E2u));
    CHECK(journalNoteId((65535u << 16) | 0x00E1u) == 65535);

    EventRuntimeState state;
    state.variables[LegacyNote] = 206;
    CHECK(journalNoteValue(state, 206) == 206);
    state.variables[Note] = 0;
    CHECK(journalNoteValue(state, 206) == 0);
    setJournalNoteValue(state, 206, 206);
    CHECK(journalNoteValue(state, 206) == 1);
    CHECK_FALSE(state.variables.contains(Note));
    CHECK_FALSE(state.variables.contains(LegacyNote));
    setJournalNoteValue(state, 206, 0);
    state.variables[Note] = 206; // A stale legacy cache cannot resurrect a canonical clear.
    CHECK(journalNoteValue(state, 206) == 0);
    removeLegacyJournalNoteVariables(state);
    CHECK_FALSE(state.variables.contains(Note));
    setJournalNoteValue(state, 0, 1);
    CHECK(journalNoteValue(state, 0) == 0);
    CHECK(state.namedGlobalVars.size() == 1);
}

TEST_CASE("JournalState event operations persist between maps without awarding character stats")
{
    EventRuntimeState first;
    first.variables[26] = 15;
    const auto note = EventRuntime::decodeVariable(Note);
    const auto alias = EventRuntime::decodeVariable(LegacyNote);
    EventRuntime::addVariableValue(first, note, 206, nullptr, {});
    CHECK(EventRuntime::getVariableValue(first, alias, nullptr, std::nullopt) == 1);
    CHECK_FALSE(first.variables.contains(Note));
    CHECK(first.variables.at(26) == 15);
    const size_t feedback = first.portraitFxRequests.size();
    EventRuntime::addVariableValue(first, alias, 206, nullptr, {});
    CHECK(first.portraitFxRequests.size() == feedback);

    EventRuntimeState next;
    next.namedGlobalVars = first.namedGlobalVars;
    CHECK(EventRuntime::getVariableValue(next, note, nullptr, std::nullopt) == 1);
    EventRuntime::subtractVariableValue(next, alias, 0, nullptr, {});
    CHECK(journalNoteValue(next, 206) == 1);
    EventRuntime::subtractVariableValue(next, alias, 1, nullptr, {});
    CHECK(EventRuntime::getVariableValue(next, note, nullptr, std::nullopt) == 0);
    EventRuntime::setVariableValue(next, alias, 42, nullptr, {});
    CHECK(journalNoteValue(next, 206) == 1);
    EventRuntime::setVariableValue(next, note, 0, nullptr, {});
    CHECK(journalNoteValue(next, 206) == 0);
}

TEST_CASE("JournalState migration imports cached discoveries even when unrelated global variables exist")
{
    EventRuntimeState cached;
    cached.variables[Note] = 206;
    cached.variables[0x00E1u] = 9; // Unindexed Intellect selector is not a note.
    cached.variables[(400u << 16) | 0x00E1u] = 400; // Blank-text internal flags still persist.
    EventRuntimeState current;
    current.variables[25] = 10;
    const std::array<const EventRuntimeState *, 2> states = {&cached, &current};
    std::unordered_map<std::string, int32_t> globals = {{"openyamm.discovery.map/out01.odm", 1}};
    migrateLegacyJournalNotes(globals, states, &current);
    CHECK(globals.at(NoteKey) == 1);
    CHECK(globals.at("openyamm.journal.note/400") == 1);
    CHECK_FALSE(globals.contains("openyamm.journal.note/0"));
    CHECK(globals.at(Migration) == 1);
    CHECK(globals.at(std::string(Migration) + "/imported") == 2);
    CHECK(current.variables.at(25) == 10);
    CHECK(cached.variables.at(Note) == 206); // Input save is immutable.
    CHECK(current.namedGlobalVars.empty());
}

TEST_CASE("JournalState migration resolves legacy conflicts deterministically and never resurrects clears")
{
    EventRuntimeState cached;
    cached.variables[LegacyNote] = 206;
    EventRuntimeState emptyCache;
    emptyCache.variables[Note] = 0;
    EventRuntimeState current;
    const std::array<const EventRuntimeState *, 3> states = {&emptyCache, nullptr, &cached};
    std::unordered_map<std::string, int32_t> globals;
    migrateLegacyJournalNotes(globals, states, &current);
    CHECK(globals.at(NoteKey) == 1);

    globals.clear();
    current.variables[Note] = 0;
    current.variables[LegacyNote] = 206;
    migrateLegacyJournalNotes(globals, states, &current);
    CHECK(globals.at(NoteKey) == 0); // Active explicit E1 clear wins over DF and cached knowledge.
    current.variables[Note] = 206;
    migrateLegacyJournalNotes(globals, states, &current);
    CHECK(globals.at(NoteKey) == 0); // Versioned migration runs once.

    globals = {{NoteKey, 0}};
    migrateLegacyJournalNotes(globals, states, &current);
    CHECK(globals.at(NoteKey) == 0); // Canonical global clear wins even without migration marker.
    CHECK(globals.at(std::string(Migration) + "/imported") == 0);
    globals = {{NoteKey, 1}};
    current.variables[Note] = 0;
    migrateLegacyJournalNotes(globals, states, &current);
    CHECK(globals.at(NoteKey) == 1);
}
