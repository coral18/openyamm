#pragma once

#include "game/ui/MenuScreenBase.h"

namespace OpenYAMM::Game
{
class ControllerPointerOverlay : public MenuScreenBase
{
public:
    explicit ControllerPointerOverlay(const Engine::AssetFileSystem &assets);
    AppMode mode() const override;

private:
    void drawScreen(float deltaSeconds) override;
};
}
