#pragma once

#include "game/ui/GameplayHudGeometry.h"

#include <array>
#include <cstddef>
#include <vector>

namespace OpenYAMM::Game
{
struct HudArcMeshInput
{
    float x;
    float y;
    float width;
    float height;
    float strokeWidth;
    float startDegrees;
    float sweepDegrees;
    float begin;
    float end;

    bool operator==(const HudArcMeshInput &) const = default;
};

struct HudArcMeshVertex
{
    float x;
    float y;
    float z;
    float u;
    float v;
};

struct HudArcMesh
{
    std::vector<HudArcMeshVertex> vertices;
    std::vector<uint16_t> indices;
};

// CPU geometry only: textures, colours and draw order remain live. Exact geometry inputs avoid
// stale meters after HP/recovery changes or resizing, and the bound covers continuously animated arcs.
class HudArcMeshCache
{
public:
    const HudArcMesh &get(const HudArcMeshInput &input)
    {
        for (size_t i = 0; i < m_size; ++i)
        {
            if (m_entries[i].input == input)
            {
                return m_entries[i].mesh;
            }
        }
        const size_t index = m_size < m_entries.size() ? m_size++ : m_nextReplacement++ % m_entries.size();
        Entry &entry = m_entries[index];
        entry.input = input;
        build(input, entry.mesh);
        return entry.mesh;
    }

    void clear()
    {
        m_entries = {};
        m_size = 0;
        m_nextReplacement = 0;
    }

private:
    static void build(const HudArcMeshInput &input, HudArcMesh &mesh)
    {
        const HudArcRange range = hudArcRange(input.sweepDegrees, input.begin, input.end);
        if (range.segments == 0 || input.width <= 0 || input.height <= 0 || input.strokeWidth <= 0
            || !std::isfinite(input.startDegrees))
        {
            mesh.vertices.clear();
            mesh.indices.clear();
            return;
        }
        mesh.vertices.resize(4 * (range.segments + 1));
        mesh.indices.resize(18 * range.segments);
        const float cx = input.x + input.width * 0.5f;
        const float cy = input.y + input.height * 0.5f;
        const bool closed = std::abs(input.sweepDegrees) == 360 && range.end - range.begin == 1;
        const HudArcEdgeProfile edge = hudArcEdgeProfile(input.strokeWidth);
        // Preserve the material's one-pixel edge fade and the seam-free UVs of a closed rim.
        constexpr float outerV = 0.5f / HudArcTextureHeight;
        const float coreV = (0.5f + HudArcEdgeTexels * edge.coreOpacity) / HudArcTextureHeight;
        for (uint16_t i = 0; i <= range.segments; ++i)
        {
            const float t = float(i) / range.segments;
            const float angle = input.startDegrees + input.sweepDegrees * (range.begin + (range.end - range.begin) * t);
            const HudArcCrossSection outer =
                hudArcCrossSection(input.width * 0.5f, input.height * 0.5f, angle, edge.outerWidth);
            const HudArcCrossSection core =
                hudArcCrossSection(input.width * 0.5f, input.height * 0.5f, angle, edge.coreWidth);
            const float u = closed ? 0.5f : t;
            mesh.vertices[4 * i] = {cx + outer.innerX, cy + outer.innerY, 0, u, 1 - outerV};
            mesh.vertices[4 * i + 1] = {cx + core.innerX, cy + core.innerY, 0, u, 1 - coreV};
            mesh.vertices[4 * i + 2] = {cx + core.outerX, cy + core.outerY, 0, u, coreV};
            mesh.vertices[4 * i + 3] = {cx + outer.outerX, cy + outer.outerY, 0, u, outerV};
            if (i < range.segments)
            {
                for (uint16_t row = 0; row < 3; ++row)
                {
                    const uint16_t base = 4 * i + row;
                    const uint16_t index = 18 * i + 6 * row;
                    mesh.indices[index] = base;
                    mesh.indices[index + 1] = base + 1;
                    mesh.indices[index + 2] = base + 4;
                    mesh.indices[index + 3] = base + 1;
                    mesh.indices[index + 4] = base + 5;
                    mesh.indices[index + 5] = base + 4;
                }
            }
        }
    }

    struct Entry
    {
        HudArcMeshInput input = {};
        HudArcMesh mesh;
    };
    std::array<Entry, 64> m_entries;
    size_t m_size = 0;
    size_t m_nextReplacement = 0;
};
}
