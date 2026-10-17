#include "game/world/WorldMapKnowledge.h"
#include "game/events/EventRuntime.h"

namespace OpenYAMM::Game
{
void restoreWorldMapKnowledgeAfterRefill(const EventRuntimeState &previous, EventRuntimeState &refilled)
{
    refilled.runtimeMapNotes = previous.runtimeMapNotes;
    for (const auto &[key, value] : previous.namedMapVars)
        if (key.starts_with("openyamm.discovery.") || key.starts_with("openyamm.map-notes.user/"))
            refilled.namedMapVars[key] = value;
}
}
