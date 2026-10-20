#pragma once

#include "game/events/EventRuntime.h"
#include <optional>

namespace OpenYAMM::Game
{
struct WorldMapNote
{
    EventRuntimeState::RuntimeMapNote value;
    bool userOwned = false;
    std::optional<int32_t> elevation;
};

struct WorldMapNoteEdit
{
    std::string mapFileName;
    uint32_t id = 0; // Zero allocates a new user note; returned to the caller on success.
    int32_t x = 0, y = 0;
    std::string text;
    bool remove = false;
    std::optional<int32_t> elevation;
};

std::vector<WorldMapNote> worldMapNotes(const EventRuntimeState &state, const std::string &mapFileName);
// Map scope comes from the owning live map or saved snapshot; the runtime context is not serialized.
// Reuses runtimeMapNotes/save serialization. Ownership is a namespaced map-local variable.
// User IDs use the upper uint32 half; occupied IDs are never replaced, including foreign/scripted notes.
bool editWorldMapNote(EventRuntimeState &state, const std::string &mapFileName,
                      WorldMapNoteEdit &edit, std::string &error);
}
