#pragma once

#include "game/ui/GameplayOverlayTypes.h"

#include <algorithm>
#include <array>
#include <span>
#include <utility>

namespace OpenYAMM::Game
{
inline constexpr std::array<std::pair<const char *, GameplayJournalPointerTargetType>, 6> GameplayJournalMainTargets = {{
    {"JournalMainViewMap", GameplayJournalPointerTargetType::MainMapView},
    {"JournalMainViewQuests", GameplayJournalPointerTargetType::MainQuestsView},
    {"JournalMainViewStory", GameplayJournalPointerTargetType::MainStoryView},
    {"JournalMainViewNotes", GameplayJournalPointerTargetType::MainNotesView},
    {"JournalAtlasButton", GameplayJournalPointerTargetType::InteractiveAtlas},
    {"JournalCloseButton", GameplayJournalPointerTargetType::CloseButton},
}};

inline GameplayJournalPointerTargetType nextJournalMenuFocus(GameplayJournalPointerTargetType current,
    std::span<const GameplayJournalPointerTargetType> available, bool reverse)
{
    if (available.empty()) return GameplayJournalPointerTargetType::None;
    const auto position = std::find(available.begin(), available.end(), current);
    if (position == available.end()) return reverse ? available.back() : available.front();
    const size_t index = size_t(position - available.begin());
    return available[(index + (reverse ? available.size() - 1 : 1)) % available.size()];
}
}
