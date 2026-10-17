#pragma once

#include "game/tables/MapStats.h"

namespace OpenYAMM::Game
{
// Canonical snapshots own map state. Filename entries are legacy aliases, used only when the canonical entry
// is absent. Never merge the two: an alias can contain stale or incompatible state.
template<typename Snapshots>
auto findMapSnapshot(Snapshots &snapshots, const MapStatsEntry &map)
{
    auto found = map.canonicalId.empty() ? snapshots.end() : snapshots.find(map.canonicalId);
    if (found == snapshots.end())
        found = snapshots.find(map.fileName);
    return found != snapshots.end() ? &found->second : nullptr;
}
}
