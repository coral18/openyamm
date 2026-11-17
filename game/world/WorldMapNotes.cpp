#include "engine/Localization.h"
#include "game/world/WorldMapNotes.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <limits>

namespace OpenYAMM::Game
{
namespace
{
std::string elevationKey(uint32_t id)
{
    return "openyamm.map-notes.height/" + std::to_string(id);
}
} // namespace

std::vector<WorldMapNote> worldMapNotes(const Party &party, const EventRuntimeState &state,
                                        const std::string &mapFileName)
{
    std::vector<WorldMapNote> result;
    for (const JournalMapNote &note : party.journalMapNotes())
        if (note.id != 0 && !note.text.empty() && !note.mapFileName.empty() &&
            toLowerCopy(note.mapFileName) == toLowerCopy(mapFileName))
        {
            const bool owned = note.personal;
            const auto height = state.namedMapVars.find(elevationKey(note.id));
            result.push_back(
                {note, owned,
                 owned && height != state.namedMapVars.end() ? std::optional<int32_t>(height->second) : std::nullopt});
        }
    std::sort(result.begin(), result.end(),
              [](const WorldMapNote &a, const WorldMapNote &b) { return a.value.id < b.value.id; });
    return result;
}

bool editWorldMapNote(Party &party, EventRuntimeState &state, const std::string &mapFileName, WorldMapNoteEdit &edit,
                      std::string &error)
{
    error.clear();
    if (edit.mapFileName.empty() || toLowerCopy(edit.mapFileName) != toLowerCopy(mapFileName))
    {
        error = std::string(Engine::trNoop("This note belongs to another map."));
        return false;
    }
    const JournalMapNote *pExisting = party.findJournalMapNote(mapFileName, edit.id);
    if (edit.id != 0 && (pExisting == nullptr || !pExisting->personal))
    {
        error = std::string(Engine::trNoop("Only your own notes can be changed."));
        return false;
    }
    if (edit.remove)
    {
        if (edit.id == 0)
        {
            error = std::string(Engine::trNoop("Select a note to delete."));
            return false;
        }
        party.removeJournalMapNote(mapFileName, edit.id);
        state.namedMapVars.erase(elevationKey(edit.id));
        return true;
    }
    const size_t begin = edit.text.find_first_not_of(" \t\r\n");
    const size_t end = edit.text.find_last_not_of(" \t\r\n");
    if (begin == std::string::npos || edit.text.size() > 512 || edit.text.find('\0') != std::string::npos)
    {
        error = std::string(Engine::trNoop("The note is empty or too long."));
        return false;
    }
    std::string text = edit.text.substr(begin, end - begin + 1);
    uint32_t id = edit.id;
    if (id == 0)
    {
        id = PersonalJournalMapNoteIdBase;
        while (party.findJournalMapNote(mapFileName, id) != nullptr)
        {
            if (id == std::numeric_limits<uint32_t>::max())
            {
                error = std::string(Engine::trNoop("No user note IDs are available."));
                return false;
            }
            ++id;
        }
    }
    party.setJournalMapNote({id, edit.x, edit.y, toLowerCopy(mapFileName), text, false, true});
    if (edit.elevation)
        state.namedMapVars[elevationKey(id)] = *edit.elevation;
    else
        state.namedMapVars.erase(elevationKey(id));
    edit.id = id;
    edit.mapFileName = toLowerCopy(edit.mapFileName);
    edit.text = std::move(text);
    return true;
}
} // namespace OpenYAMM::Game
