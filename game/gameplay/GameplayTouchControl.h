#pragma once

namespace OpenYAMM::Game
{
enum class GameplayTouchRole
{
    Hud,
    Inspect,
    FlyUp,
    FlyDown
};

struct GameplayTouchControl
{
    GameplayTouchRole role = GameplayTouchRole::Hud;
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;

    bool contains(float pointX, float pointY) const
    {
        return pointX >= x && pointX < x + width && pointY >= y && pointY < y + height;
    }
};
}
