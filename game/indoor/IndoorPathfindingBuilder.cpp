#include "game/indoor/IndoorPathfindingBuilder.h"

#include "game/FaceEnums.h"

namespace OpenYAMM::Game
{
namespace
{
PathFacetKind convertPathFacetKind(const IndoorFaceGeometryData &geometry)
{
    if (geometry.isPortal)
    {
        return PathFacetKind::Portal;
    }

    switch (geometry.kind)
    {
        case IndoorFaceKind::Floor:
            return PathFacetKind::Floor;
        case IndoorFaceKind::Ceiling:
            return PathFacetKind::Ceiling;
        case IndoorFaceKind::Wall:
            return PathFacetKind::Wall;
        case IndoorFaceKind::Unknown:
        default:
            return PathFacetKind::Generic;
    }
}

PathFacet buildPathFacet(const IndoorFaceGeometryData &geometry)
{
    PathFacet facet = {};
    facet.kind = convertPathFacetKind(geometry);
    facet.attributes.portal = geometry.isPortal;
    facet.attributes.invisible = hasFaceAttribute(geometry.attributes, FaceAttribute::Invisible);
    facet.attributes.untouchable = hasFaceAttribute(geometry.attributes, FaceAttribute::Untouchable);
    // Horizontal portals connect sectors; they provide no physical floor or overhead support.
    facet.walkableFloor = geometry.isWalkable && !geometry.isPortal;
    facet.blocking = !geometry.isPortal && !facet.attributes.untouchable;
    facet.sourceId = static_cast<int32_t>(geometry.faceIndex);
    facet.vertices.reserve(geometry.vertices.size());

    for (const bx::Vec3 &vertex : geometry.vertices)
    {
        facet.vertices.push_back({vertex.x, vertex.y, vertex.z});
    }

    return facet;
}
}

IndoorPathMapBuildResult IndoorPathfindingBuilder::buildPathMap(
    const IndoorMapData &indoorMapData,
    const std::vector<IndoorVertex> &vertices,
    const MapDeltaData *pMapDeltaData,
    IndoorFaceGeometryCache *pGeometryCache,
    float spatialGridCellSize
)
{
    IndoorFaceGeometryCache localGeometryCache;
    IndoorFaceGeometryCache *pActiveGeometryCache = pGeometryCache;

    if (pActiveGeometryCache == nullptr)
    {
        localGeometryCache.reset(indoorMapData.faces.size());
        pActiveGeometryCache = &localGeometryCache;
    }

    pActiveGeometryCache->setAttributeOverrides(pMapDeltaData);

    IndoorPathMapBuildResult result = {};
    result.sourceFaceCount = indoorMapData.faces.size();
    std::vector<PathFacet> pathFacets;
    pathFacets.reserve(indoorMapData.faces.size());

    for (size_t faceIndex = 0; faceIndex < indoorMapData.faces.size(); ++faceIndex)
    {
        const IndoorFaceGeometryData *pGeometry =
            pActiveGeometryCache->geometryForFace(indoorMapData, vertices, faceIndex);

        if (pGeometry == nullptr || !pGeometry->hasPlane || pGeometry->vertices.size() < 3)
        {
            ++result.skippedFaceCount;
            continue;
        }

        pathFacets.push_back(buildPathFacet(*pGeometry));
    }

    result.pathFacetCount = pathFacets.size();
    result.pathMap.setFacets(std::move(pathFacets));
    result.pathMap.buildSpatialGrid(spatialGridCellSize);
    return result;
}
}
