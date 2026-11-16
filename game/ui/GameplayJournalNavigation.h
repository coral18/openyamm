#pragma once

#include "game/ui/GameplayOverlayTypes.h"

#include <algorithm>
#include <array>
#include <span>
#include <utility>

namespace OpenYAMM::Game
{
using GameplayJournalMenuTarget = std::pair<const char *, GameplayJournalPointerTargetType>;

inline constexpr std::array<GameplayJournalMenuTarget, 5> GameplayJournalFourTabs = {{
    {"JournalMainViewMap", GameplayJournalPointerTargetType::MainMapView},
    {"JournalMainViewQuests", GameplayJournalPointerTargetType::MainQuestsView},
    {"JournalMainViewStory", GameplayJournalPointerTargetType::MainStoryView},
    {"JournalMainViewNotes", GameplayJournalPointerTargetType::MainNotesView},
    {"JournalCloseButton", GameplayJournalPointerTargetType::CloseButton},
}};
inline constexpr std::array<GameplayJournalMenuTarget, 6> GameplayJournalFiveTabs = {{
    {"JournalTabs5Map", GameplayJournalPointerTargetType::MainMapView},
    {"JournalTabs5Quests", GameplayJournalPointerTargetType::MainQuestsView},
    {"JournalTabs5Story", GameplayJournalPointerTargetType::MainStoryView},
    {"JournalTabs5Notes", GameplayJournalPointerTargetType::MainNotesView},
    {"JournalTabs5Progress", GameplayJournalPointerTargetType::MainProgressView},
    {"JournalCloseButton", GameplayJournalPointerTargetType::CloseButton},
}};

inline std::span<const GameplayJournalMenuTarget> gameplayJournalMenuTargets(bool progressTab)
{
    return progressTab ? std::span<const GameplayJournalMenuTarget>(GameplayJournalFiveTabs)
        : std::span<const GameplayJournalMenuTarget>(GameplayJournalFourTabs);
}

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
