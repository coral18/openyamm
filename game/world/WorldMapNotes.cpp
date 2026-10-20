#include "game/world/WorldMapNotes.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <limits>

namespace OpenYAMM::Game
{
namespace
{
std::string ownershipKey(uint32_t id)
{
    return "openyamm.map-notes.user/" + std::to_string(id);
}

bool userOwns(const EventRuntimeState &state, uint32_t id)
{
    const auto found = state.namedMapVars.find(ownershipKey(id));
    return id >= 0x80000000u && found != state.namedMapVars.end() && found->second == 1;
}

std::string elevationKey(uint32_t id)
{
    return ownershipKey(id) + "/z";
}
}

std::vector<WorldMapNote> worldMapNotes(const EventRuntimeState &state, const std::string &mapFileName)
{
    std::vector<WorldMapNote> result;
    for (const auto &[id, note] : state.runtimeMapNotes)
        if (id != 0 && note.id == id && note.active && !note.text.empty() && !note.mapFileName.empty()
            && toLowerCopy(note.mapFileName) == toLowerCopy(mapFileName))
        {
            const bool owned = userOwns(state, id);
            const auto height = state.namedMapVars.find(elevationKey(id));
            result.push_back({note, owned, owned && height != state.namedMapVars.end()
                ? std::optional<int32_t>(height->second) : std::nullopt});
        }
    std::sort(result.begin(), result.end(), [](const WorldMapNote &a, const WorldMapNote &b)
    { return a.value.id < b.value.id; });
    return result;
}

bool editWorldMapNote(EventRuntimeState &state, const std::string &mapFileName,
                      WorldMapNoteEdit &edit, std::string &error)
{
    error.clear();
    if (edit.mapFileName.empty() || toLowerCopy(edit.mapFileName) != toLowerCopy(mapFileName))
    {
        error = "This note belongs to another map.";
        return false;
    }
    const auto found = state.runtimeMapNotes.find(edit.id);
    if (edit.id != 0 && (found == state.runtimeMapNotes.end() || !userOwns(state, edit.id)
        || toLowerCopy(found->second.mapFileName) != toLowerCopy(edit.mapFileName)))
    {
        error = "Only your own notes can be changed.";
        return false;
    }
    if (edit.remove)
    {
        if (edit.id == 0)
        {
            error = "Select a note to delete.";
            return false;
        }
        state.runtimeMapNotes.erase(edit.id);
        state.namedMapVars.erase(ownershipKey(edit.id));
        state.namedMapVars.erase(elevationKey(edit.id));
        return true;
    }
    const size_t begin = edit.text.find_first_not_of(" \t\r\n");
    const size_t end = edit.text.find_last_not_of(" \t\r\n");
    if (begin == std::string::npos || edit.text.size() > 512 || edit.text.find('\0') != std::string::npos)
    {
        error = "The note is empty or too long.";
        return false;
    }
    std::string text = edit.text.substr(begin, end - begin + 1);
    uint32_t id = edit.id;
    if (id == 0)
    {
        id = std::numeric_limits<uint32_t>::max();
        while (id >= 0x80000000u && (state.runtimeMapNotes.contains(id)
            || state.namedMapVars.contains(ownershipKey(id)) || state.namedMapVars.contains(elevationKey(id))))
            --id;
        if (id < 0x80000000u)
        {
            error = "No user note IDs are available.";
            return false;
        }
    }
    state.runtimeMapNotes[id] = {id, edit.x, edit.y, toLowerCopy(edit.mapFileName), text, true};
    state.namedMapVars[ownershipKey(id)] = 1;
    if (edit.elevation) state.namedMapVars[elevationKey(id)] = *edit.elevation;
    else state.namedMapVars.erase(elevationKey(id));
    edit.id = id;
    edit.mapFileName = toLowerCopy(edit.mapFileName);
    edit.text = std::move(text);
    return true;
}
}
