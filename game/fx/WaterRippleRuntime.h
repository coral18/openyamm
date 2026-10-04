#pragma once

#include <bx/math.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace OpenYAMM::Game
{
constexpr size_t MaxWaterRipples = 64;
constexpr size_t MaxWaterRipplesPerEmitter = 4;
constexpr size_t MaxDrawWaterRipples = 4;
constexpr size_t MaxDrawWaterRipplesPerEmitter = 2;
constexpr float WaterRippleLifetime = 1.6f;
constexpr float WaterRippleMaxRadius = 154.0f;

struct WaterRipple
{
    bx::Vec3 position = {0.0f, 0.0f, 0.0f};
    float strength = 1.0f;
    double bornSeconds = 0.0;
    uint32_t sourceId = 0;
    int16_t sectorId = -1;
};

struct WaterRippleDrawSet
{
    // World XY, expanding radius, and faded strength: no lifetime math in the fragment shader.
    std::array<std::array<float, 4>, MaxDrawWaterRipples> rings = {};
    uint32_t count = 0;
};

// Presentation only: bounded storage, no collision queries, and no saved state.
class WaterRippleRuntime
{
public:
    void setEnabled(bool enabled)
    {
        if (m_enabled != enabled)
        {
            *this = {};
            m_enabled = enabled;
        }
    }

    bool enabled() const { return m_enabled; }
    double seconds() const { return m_seconds; }
    std::span<const WaterRipple> ripples() const { return {m_ripples.data(), m_count}; }

    void advance(float deltaSeconds, bool paused)
    {
        if (!m_enabled || paused)
        {
            return;
        }
        m_seconds += std::max(deltaSeconds, 0.0f);
        for (size_t index = 0; index < m_count;)
        {
            if (m_seconds - m_ripples[index].bornSeconds >= WaterRippleLifetime)
            {
                m_ripples[index] = m_ripples[--m_count];
            }
            else
            {
                ++index;
            }
        }
    }

    void observe(uint32_t sourceId, const bx::Vec3 &position, bool onWater, float bodyRadius,
        int16_t sectorId = -1)
    {
        if (!m_enabled)
        {
            return;
        }
        Emitter *pEmitter = nullptr;
        for (Emitter &emitter : m_emitters)
        {
            if (emitter.valid && emitter.sourceId == sourceId)
            {
                pEmitter = &emitter;
                break;
            }
        }
        if (!onWater)
        {
            if (pEmitter != nullptr)
            {
                pEmitter->valid = false;
            }
            return;
        }
        if (pEmitter == nullptr)
        {
            pEmitter = &*std::min_element(m_emitters.begin(), m_emitters.end(),
                [](const Emitter &first, const Emitter &second)
                {
                    return (first.valid ? first.seenSeconds : -1.0f)
                        < (second.valid ? second.seenSeconds : -1.0f);
                });
            *pEmitter = {position, 0.0f, sourceId, true, m_seconds, m_seconds};
            return;
        }
        const bx::Vec3 delta = bx::sub(position, pEmitter->position);
        const float distanceSquared = delta.x * delta.x + delta.y * delta.y;
        pEmitter->position = position;
        pEmitter->seenSeconds = m_seconds;
        // Teleports, jumps and surface-height changes must not leave a trail across the map.
        if (distanceSquared > 512.0f * 512.0f || std::abs(delta.z) > 8.0f)
        {
            pEmitter->distance = 0.0f;
            pEmitter->emittedSeconds = m_seconds;
            return;
        }
        pEmitter->distance += std::sqrt(distanceSquared);
        if (pEmitter->distance < 80.0f || m_seconds - pEmitter->emittedSeconds < 0.25f)
        {
            return;
        }
        size_t index = m_count;
        size_t sourceCount = 0;
        size_t oldestSourceIndex = m_count;
        for (size_t candidate = 0; candidate < m_count; ++candidate)
        {
            if (m_ripples[candidate].sourceId == sourceId)
            {
                ++sourceCount;
                if (oldestSourceIndex == m_count
                    || m_ripples[candidate].bornSeconds < m_ripples[oldestSourceIndex].bornSeconds)
                {
                    oldestSourceIndex = candidate;
                }
            }
        }
        if (sourceCount >= MaxWaterRipplesPerEmitter)
        {
            index = oldestSourceIndex;
        }
        else if (m_count < m_ripples.size())
        {
            ++m_count;
        }
        else
        {
            index = size_t(std::min_element(m_ripples.begin(), m_ripples.end(),
                [](const WaterRipple &first, const WaterRipple &second)
                { return first.bornSeconds < second.bornSeconds; }) - m_ripples.begin());
        }
        m_ripples[index] = {position, std::clamp(bodyRadius / 32.0f, 0.6f, 1.4f), m_seconds, sourceId, sectorId};
        pEmitter->distance = 0.0f;
        pEmitter->emittedSeconds = m_seconds;
    }

    WaterRippleDrawSet select(const bx::Vec3 &camera, float height,
        std::span<const std::array<bx::Vec3, 2>> patches, int16_t sectorId = -1, int16_t backSectorId = -1) const
    {
        WaterRippleDrawSet result;
        struct SourceRings
        {
            uint32_t sourceId = 0;
            std::array<size_t, MaxDrawWaterRipplesPerEmitter> indices = {MaxWaterRipples, MaxWaterRipples};
            std::array<float, MaxDrawWaterRipplesPerEmitter> distances = {};
        };
        std::array<SourceRings, MaxDrawWaterRipples> sources;
        size_t sourceCount = 0;
        for (size_t rippleIndex = 0; rippleIndex < m_count; ++rippleIndex)
        {
            const WaterRipple &ripple = m_ripples[rippleIndex];
            if (std::abs(ripple.position.z - height) > 4.0f
                || (sectorId >= 0 && ripple.sectorId >= 0
                    && ripple.sectorId != sectorId && ripple.sectorId != backSectorId))
            {
                continue;
            }
            const bx::Vec3 delta = bx::sub(camera, ripple.position);
            const float distanceSquared = bx::dot(delta, delta);
            if (distanceSquared > 4096.0f * 4096.0f)
            {
                continue;
            }
            const bool overlaps = std::any_of(patches.begin(), patches.end(), [&](const auto &patch)
            {
                return ripple.position.x + WaterRippleMaxRadius >= patch[0].x
                    && ripple.position.x - WaterRippleMaxRadius <= patch[1].x
                    && ripple.position.y + WaterRippleMaxRadius >= patch[0].y
                    && ripple.position.y - WaterRippleMaxRadius <= patch[1].y;
            });
            if (!overlaps)
            {
                continue;
            }
            size_t sourceIndex = 0;
            while (sourceIndex < sourceCount && sources[sourceIndex].sourceId != ripple.sourceId)
            {
                ++sourceIndex;
            }
            if (sourceIndex == sourceCount)
            {
                if (sourceCount < sources.size())
                {
                    ++sourceCount;
                }
                else
                {
                    sourceIndex = size_t(std::max_element(sources.begin(), sources.end(),
                        [](const SourceRings &first, const SourceRings &second)
                        { return first.distances[0] < second.distances[0]; }) - sources.begin());
                    if (distanceSquared >= sources[sourceIndex].distances[0])
                    {
                        continue;
                    }
                }
                sources[sourceIndex] = {};
                sources[sourceIndex].sourceId = ripple.sourceId;
            }
            SourceRings &source = sources[sourceIndex];
            if (source.indices[0] == MaxWaterRipples || distanceSquared < source.distances[0])
            {
                source.indices[1] = source.indices[0];
                source.distances[1] = source.distances[0];
                source.indices[0] = rippleIndex;
                source.distances[0] = distanceSquared;
            }
            else if (source.indices[1] == MaxWaterRipples || distanceSquared < source.distances[1])
            {
                source.indices[1] = rippleIndex;
                source.distances[1] = distanceSquared;
            }
        }
        const auto appendRing = [&](size_t index)
        {
            const WaterRipple &ripple = m_ripples[index];
            const float age = float(m_seconds - ripple.bornSeconds);
            const float fade = std::max(0.0f, 1.0f - age / WaterRippleLifetime);
            result.rings[result.count++] = {ripple.position.x, ripple.position.y,
                16.0f + age * 80.0f, ripple.strength * fade * fade};
        };
        // Give each nearby creature a ring before adding a second trail ring.
        for (size_t index = 0; index < sourceCount; ++index)
        {
            appendRing(sources[index].indices[0]);
        }
        while (result.count < MaxDrawWaterRipples)
        {
            size_t nearest = sourceCount;
            for (size_t index = 0; index < sourceCount; ++index)
            {
                if (sources[index].indices[1] != MaxWaterRipples
                    && (nearest == sourceCount || sources[index].distances[1] < sources[nearest].distances[1]))
                {
                    nearest = index;
                }
            }
            if (nearest == sourceCount)
            {
                break;
            }
            appendRing(sources[nearest].indices[1]);
            sources[nearest].indices[1] = MaxWaterRipples;
        }
        return result;
    }

private:
    struct Emitter
    {
        bx::Vec3 position = {0.0f, 0.0f, 0.0f};
        float distance = 0.0f;
        uint32_t sourceId = 0;
        bool valid = false;
        double seenSeconds = 0.0;
        double emittedSeconds = 0.0;
    };

    std::array<WaterRipple, MaxWaterRipples> m_ripples = {};
    std::array<Emitter, MaxWaterRipples> m_emitters = {};
    // Keep sub-millisecond frames advancing even after hours of uninterrupted gameplay.
    double m_seconds = 0.0;
    size_t m_count = 0;
    bool m_enabled = false;
};
}
