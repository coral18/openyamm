#include "game/gameplay/JournalState.h"

#include "game/events/EventRuntime.h"

#include <algorithm>
#include <map>

namespace OpenYAMM::Game
{
namespace
{
constexpr uint32_t JournalTag = 0x00E1u;
constexpr uint32_t LegacyJournalTag = 0x00DFu;
constexpr const char *MigrationKey = "openyamm.migration.journal-notes-v1";
constexpr const char *NotePrefix = "openyamm.journal.note/";

std::string noteKey(uint16_t id)
{
    return std::string(NotePrefix) + std::to_string(id);
}

std::map<uint16_t, int32_t> legacyNotes(const EventRuntimeState &state)
{
    std::map<uint16_t, int32_t> notes;
    for (const auto &[rawId, value] : state.variables)
    {
        if (const std::optional<uint16_t> id = journalNoteId(rawId))
        {
            // Prefer the canonical E1 alias, including explicit clears, regardless of hash iteration order.
            const uint32_t canonical = (uint32_t(*id) << 16) | JournalTag;
            const auto current = state.variables.find(canonical);
            notes[*id] = (current != state.variables.end() ? current->second : value) != 0 ? 1 : 0;
        }
    }
    return notes;
}
}

std::optional<uint16_t> journalNoteId(uint32_t rawVariable)
{
    const uint32_t tag = rawVariable & 0xFFFFu;
    const uint32_t index = rawVariable >> 16;
    if (index == 0 || (tag != JournalTag && tag != LegacyJournalTag))
        return std::nullopt;
    return uint16_t(index);
}

int32_t journalNoteValue(const EventRuntimeState &state, uint16_t noteId)
{
    if (noteId == 0)
        return 0;
    if (const auto value = state.namedGlobalVars.find(noteKey(noteId)); value != state.namedGlobalVars.end())
        return value->second;
    // Compatibility for raw runtime fixtures and states before the explicit save migration.
    for (const uint32_t tag : {JournalTag, LegacyJournalTag})
        if (const auto value = state.variables.find((uint32_t(noteId) << 16) | tag); value != state.variables.end())
            return value->second;
    return 0;
}

void setJournalNoteValue(EventRuntimeState &state, uint16_t noteId, int32_t value)
{
    if (noteId == 0)
        return;
    state.namedGlobalVars[noteKey(noteId)] = value != 0 ? 1 : 0;
    state.variables.erase((uint32_t(noteId) << 16) | JournalTag);
    state.variables.erase((uint32_t(noteId) << 16) | LegacyJournalTag);
}

void removeLegacyJournalNoteVariables(EventRuntimeState &state)
{
    std::erase_if(state.variables, [](const auto &entry) { return journalNoteId(entry.first).has_value(); });
}

void migrateLegacyJournalNotes(
    std::unordered_map<std::string, int32_t> &globals,
    std::span<const EventRuntimeState *const> cachedStates,
    const EventRuntimeState *pCurrentState)
{
    if (const auto marker = globals.find(MigrationKey); marker != globals.end() && marker->second == 1)
        return;
    std::map<uint16_t, int32_t> notes;
    for (const EventRuntimeState *pState : cachedStates)
    {
        if (pState == nullptr)
            continue;
        for (const auto &[id, value] : legacyNotes(*pState))
            notes[id] = std::max(notes[id], value);
    }
    if (pCurrentState != nullptr)
        for (const auto &[id, value] : legacyNotes(*pCurrentState))
            notes[id] = value;
    int32_t imported = 0;
    for (const auto &[id, value] : notes)
        if (globals.try_emplace(noteKey(id), value).second)
            ++imported;
    globals[std::string(MigrationKey) + "/imported"] = imported;
    globals[MigrationKey] = 1;
}
} // namespace OpenYAMM::Game
