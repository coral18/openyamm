#pragma once

#include "game/render/SpriteAtlasCook.h"

namespace OpenYAMM::Game
{
// Host-only encoding. No encoders or source images are needed by the shipped game.
PreparedSpriteAtlasPage compressSpriteAtlasPage(SpriteAtlasSourcePage source, int maskChannels,
    const std::string &profile);
}
