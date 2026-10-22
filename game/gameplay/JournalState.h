#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

namespace OpenYAMM::Game
{
struct EventRuntimeState;

// The unindexed E1 variable is an Intellect comparison, not a journal note.
std::optional<uint16_t> journalNoteId(uint32_t rawVariable);
int32_t journalNoteValue(const EventRuntimeState &state, uint16_t noteId);
void setJournalNoteValue(EventRuntimeState &state, uint16_t noteId, int32_t value);
void removeLegacyJournalNoteVariables(EventRuntimeState &state);

// One-time import: canonical globals win; the active map overrides cached legacy values.
void migrateLegacyJournalNotes(
    std::unordered_map<std::string, int32_t> &globals,
    std::span<const EventRuntimeState *const> cachedStates,
    const EventRuntimeState *pCurrentState);
} // namespace OpenYAMM::Game
