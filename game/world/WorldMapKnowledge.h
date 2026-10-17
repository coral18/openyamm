#pragma once

namespace OpenYAMM::Game
{
struct EventRuntimeState;

// Map refills replace gameplay state, not the party's observations and annotations.
void restoreWorldMapKnowledgeAfterRefill(const EventRuntimeState &previous, EventRuntimeState &refilled);
}
