#include "game/ui/ControllerPointerOverlay.h"

#include <algorithm>

namespace OpenYAMM::Game
{
ControllerPointerOverlay::ControllerPointerOverlay(const Engine::AssetFileSystem &assets)
    : MenuScreenBase(assets)
{
    setRenderViewId(249);
    setClearBackground(false);
}

AppMode ControllerPointerOverlay::mode() const
{
    return AppMode::PauseMenu;
}

void ControllerPointerOverlay::drawScreen(float)
{
    const float unit = std::max(1.0f, frameHeight() / 900.0f);
    const float x = mouseX(), y = mouseY();
    drawSolidRect({x - 9 * unit, y - 2 * unit, 18 * unit, 4 * unit}, 0xff15100bu);
    drawSolidRect({x - 2 * unit, y - 9 * unit, 4 * unit, 18 * unit}, 0xff15100bu);
    drawSolidRect({x - 8 * unit, y - unit, 16 * unit, 2 * unit}, 0xffdfeaf0u);
    drawSolidRect({x - unit, y - 8 * unit, 2 * unit, 16 * unit}, 0xffdfeaf0u);
}
}
