#pragma once

#include "game/gameplay/GameplayInputFrame.h"

#include <algorithm>

namespace OpenYAMM::Game
{
// Captures one scroll offset for the whole drag, including a gesture completed between input frames.
class MenuTouchScroll
{
public:
    void reset()
    {
        m_pScroll = nullptr;
    }

    void beginFrame(const GameplayInputFrame &input)
    {
        if (input.mobileTouchDragStarted || (!input.mobileTouchDragActive && !input.mobileTouchDragReleased))
        {
            reset();
        }
    }

    bool update(const GameplayInputFrame &input, bool startsInside, float scale, float maxScroll, float &scroll)
    {
        if (scale <= 0 || (!input.mobileTouchDragActive && !input.mobileTouchDragReleased))
        {
            return false;
        }
        if (m_pScroll == nullptr && input.mobileTouchDragStarted && startsInside && maxScroll > 0)
        {
            m_pScroll = &scroll;
            m_lastY = input.mobileTouchDragStartY;
        }
        if (m_pScroll != &scroll)
        {
            return false;
        }
        scroll = std::clamp(scroll + (m_lastY - input.pointerY) / scale, 0.0f, std::max(0.0f, maxScroll));
        m_lastY = input.pointerY;
        return true;
    }

private:
    float *m_pScroll = nullptr;
    float m_lastY = 0;
};
} // namespace OpenYAMM::Game
