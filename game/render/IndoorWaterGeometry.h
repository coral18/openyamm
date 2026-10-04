#pragma once

#include "game/FaceEnums.h"
#include "game/render/WaterRenderer.h"
#include "game/tables/SurfaceMaterialTable.h"

#include <algorithm>
#include <cmath>

namespace OpenYAMM::Game
{
inline bool isIndoorWaterSurface(uint32_t attributes, SurfaceMaterialSemantic semantic)
{
    return !hasFaceAttribute(attributes, FaceAttribute::Lava)
        && !hasFaceAttribute(attributes, FaceAttribute::IndoorSky)
        && semantic != SurfaceMaterialSemantic::Lava
        && (hasFaceAttribute(attributes, FaceAttribute::Fluid) || semantic == SurfaceMaterialSemantic::Water);
}

inline bool isIndoorWaterPool(uint32_t attributes, SurfaceMaterialSemantic semantic, const bx::Vec3 &normal)
{
    // Animated water walls, waterfalls and undersides are artwork, not a reflective pool surface.
    // Keep their native texture animation instead of replacing it with an unreflected flat water tint.
    return isIndoorWaterSurface(attributes, semantic) && normal.z > 0.0f
        && normal.z * normal.z >= bx::dot(normal, normal) * 0.99f;
}

inline std::array<float, 2> indoorWaterFlow(const std::array<bx::Vec3, 3> &positions,
    const std::array<std::array<float, 2>, 3> &uvs, float flowU, float flowV)
{
    const bx::Vec3 edge1 = bx::sub(positions[1], positions[0]);
    const bx::Vec3 edge2 = bx::sub(positions[2], positions[0]);
    const float u1 = uvs[1][0] - uvs[0][0];
    const float v1 = uvs[1][1] - uvs[0][1];
    const float u2 = uvs[2][0] - uvs[0][0];
    const float v2 = uvs[2][1] - uvs[0][1];
    const float determinant = u1 * v2 - v1 * u2;
    const bx::Vec3 normal = bx::cross(edge1, edge2);
    if (std::abs(determinant) < 0.000001f || bx::dot(normal, normal) < 0.0001f)
    {
        return {};
    }
    const bx::Vec3 velocity = bx::mul(bx::add(
        bx::mul(bx::sub(bx::mul(edge1, v2), bx::mul(edge2, v1)), flowU),
        bx::mul(bx::sub(bx::mul(edge2, u1), bx::mul(edge1, u2)), flowV)), 1.0f / determinant);
    const bx::Vec3 unitNormal = bx::normalize(normal);
    const bx::Vec3 tangent = std::abs(unitNormal.z) > 0.99f ? bx::Vec3{1.0f, 0.0f, 0.0f}
        : bx::normalize(bx::cross(bx::Vec3{0.0f, 0.0f, 1.0f}, unitNormal));
    const bx::Vec3 bitangent = bx::cross(unitNormal, tangent);
    return {bx::dot(velocity, tangent) / 2048.0f, bx::dot(velocity, bitangent) / 2048.0f};
}

// Partition horizontal levels so a waterfall or a stepped pool cannot acquire an incorrect planar reflection.
inline std::vector<WaterSurfaceGeometry> buildIndoorWaterGeometry(std::span<const WaterVertex> vertices,
    int16_t sectorId, int16_t backSectorId)
{
    std::vector<WaterSurfaceGeometry> geometry;
    for (size_t index = 0; index + 2 < vertices.size(); index += 3)
    {
        const WaterVertex &a = vertices[index];
        const WaterVertex &b = vertices[index + 1];
        const WaterVertex &c = vertices[index + 2];
        const bx::Vec3 normal = bx::cross(bx::Vec3{b.x - a.x, b.y - a.y, b.z - a.z},
            bx::Vec3{c.x - a.x, c.y - a.y, c.z - a.z});
        if (bx::dot(normal, normal) < 0.0001f)
        {
            continue;
        }
        const bool planar = std::abs(a.z - b.z) < 0.01f && std::abs(a.z - c.z) < 0.01f;
        const float height = a.z;
        std::vector<WaterSurfaceGeometry>::iterator found = std::find_if(geometry.begin(), geometry.end(),
            [&](const WaterSurfaceGeometry &surface)
            { return surface.planar == planar && (!planar || std::abs(surface.height - height) < 0.01f); });
        if (found == geometry.end())
        {
            WaterSurfaceGeometry surface;
            surface.height = height;
            surface.planar = planar;
            surface.sectorId = sectorId;
            surface.backSectorId = backSectorId;
            surface.patches.push_back({bx::Vec3{a.x, a.y, a.z}, bx::Vec3{a.x, a.y, a.z}});
            geometry.push_back(std::move(surface));
            found = geometry.end() - 1;
        }
        const bx::Vec3 unitNormal = bx::normalize(normal);
        for (size_t corner = 0; corner < 3; ++corner)
        {
            WaterVertex vertex = vertices[index + corner];
            vertex.normalX = unitNormal.x;
            vertex.normalY = unitNormal.y;
            vertex.normalZ = unitNormal.z;
            found->vertices.push_back(vertex);
            bx::Vec3 &lo = found->patches.front()[0];
            bx::Vec3 &hi = found->patches.front()[1];
            lo = {std::min(lo.x, vertex.x), std::min(lo.y, vertex.y), std::min(lo.z, vertex.z)};
            hi = {std::max(hi.x, vertex.x), std::max(hi.y, vertex.y), std::max(hi.z, vertex.z)};
        }
    }
    return geometry;
}
}
