#pragma once

#include "game/app/GameSettings.h"

#include <string>
#include <string_view>

namespace OpenYAMM::Game
{
// The menu edits only its player-facing fields; debug, startup and content settings retain their owner.
std::string menuSettingValue(const GameSettings &settings, std::string_view id);
bool setMenuSettingValue(GameSettings &settings, std::string_view id, const std::string &value);
bool sameMenuBinding(const InputBinding &left, const InputBinding &right);
bool reservedMenuBinding(const InputBinding &binding);
} // namespace OpenYAMM::Game
