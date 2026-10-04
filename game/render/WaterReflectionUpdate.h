#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace OpenYAMM::Game
{
// Static reflections can reuse their texture between animation refreshes. Uncovered
// camera and content changes render immediately, including while gameplay is paused.
class WaterReflectionUpdate
{
public:
    bool prepare(const float *pViewProjection, float height, uint16_t size, float seconds,
        uint64_t visualRevision, bool buildings, bool viewCovered = false, bool billboards = false)
    {
        constexpr float AnimationInterval = 1.0f / 30.0f;
        const bool update = m_seconds < 0.0f || seconds < m_seconds || seconds - m_seconds >= AnimationInterval
            || m_height != height || m_size != size || m_visualRevision != visualRevision || m_buildings != buildings
            || m_billboards != billboards
            || (!viewCovered && !std::equal(m_viewProjection.begin(), m_viewProjection.end(), pViewProjection));
        if (update)
        {
            std::copy_n(pViewProjection, m_viewProjection.size(), m_viewProjection.begin());
            m_seconds = seconds;
            m_height = height;
            m_size = size;
            m_visualRevision = visualRevision;
            m_buildings = buildings;
            m_billboards = billboards;
        }
        return update;
    }

private:
    std::array<float, 16> m_viewProjection = {};
    float m_seconds = -1.0f;
    float m_height = 0.0f;
    uint64_t m_visualRevision = 0;
    uint16_t m_size = 0;
    bool m_buildings = false;
    bool m_billboards = false;
};
}
