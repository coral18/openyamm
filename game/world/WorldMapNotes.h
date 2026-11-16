#pragma once

#include "game/events/EventRuntime.h"
#include "game/party/Party.h"
#include <optional>

namespace OpenYAMM::Game
{
struct WorldMapNote
{
    JournalMapNote value;
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

std::vector<WorldMapNote> worldMapNotes(const Party &party, const EventRuntimeState &state,
    const std::string &mapFileName);
// Text, coordinates and ownership use the shared journal; the optional floor height is map-local Atlas metadata.
bool editWorldMapNote(Party &party, EventRuntimeState &state, const std::string &mapFileName,
    WorldMapNoteEdit &edit, std::string &error);
}
