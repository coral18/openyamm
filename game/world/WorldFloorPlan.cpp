#include "game/world/WorldFloorPlan.h"
#include "game/indoor/IndoorMapData.h"
#include "game/FaceEnums.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace OpenYAMM::Game
{
WorldFloorPlanRaster buildWorldFloorPlan(const IndoorMapData &map,
    const WorldFloorPlanKnowledge &knowledge, bool guide)
{
    WorldFloorPlanRaster raster;
    std::vector<const IndoorOutline *> lines;
    double minX = std::numeric_limits<double>::max(), minY = minX;
    double maxX = std::numeric_limits<double>::lowest(), maxY = maxX;
    const auto attributes = [&](size_t face)
    {
        return !guide && face < knowledge.faceAttributes.size()
            ? knowledge.faceAttributes[face] : map.faces[face].attributes;
    };
    for (size_t i = 0; i < map.outlines.size(); ++i)
    {
        const auto &line = map.outlines[i];
        if (line.vertex1Id >= map.vertices.size() || line.vertex2Id >= map.vertices.size()
            || line.face1Id >= map.faces.size() || line.face2Id >= map.faces.size())
            continue;
        const auto a = attributes(line.face1Id), b = attributes(line.face2Id);
        if (hasFaceAttribute(a, FaceAttribute::Invisible) || hasFaceAttribute(b, FaceAttribute::Invisible)
            || (!guide && (knowledge.invisibleFaces.contains(line.face1Id)
                        || knowledge.invisibleFaces.contains(line.face2Id))))
            continue;
        // Legacy reveal bits are most-significant-bit first, not little-endian bit indices.
        const bool revealed = (i / 8 < knowledge.visibleOutlines.size()
            && (knowledge.visibleOutlines[i / 8] & (1u << (7u - i % 8))))
            || hasFaceAttribute(a, FaceAttribute::SeenByParty) || hasFaceAttribute(b, FaceAttribute::SeenByParty);
        if (!guide && !revealed)
            continue;
        lines.push_back(&line);
        for (auto id : {line.vertex1Id, line.vertex2Id})
        {
            minX = std::min(minX, double(map.vertices[id].x)); maxX = std::max(maxX, double(map.vertices[id].x));
            minY = std::min(minY, double(map.vertices[id].y)); maxY = std::max(maxY, double(map.vertices[id].y));
        }
    }
    raster.outlineCount = lines.size();
    if (lines.empty())
        return raster;
    raster.pixelsBgra.resize(size_t(raster.width) * raster.height * 4);
    for (size_t p = 0; p < raster.pixelsBgra.size(); p += 4)
    {
        raster.pixelsBgra[p] = 15; raster.pixelsBgra[p + 1] = 22;
        raster.pixelsBgra[p + 2] = 24; raster.pixelsBgra[p + 3] = 255;
    }
    constexpr double padding = 24;
    const double scale = std::min((raster.width - 2 * padding) / std::max(1.0, maxX - minX),
                                  (raster.height - 2 * padding) / std::max(1.0, maxY - minY));
    const auto project = [&](const IndoorVertex &vertex)
    {
        return std::pair{int(std::lround(raster.width / 2.0 + (vertex.x - (minX + maxX) / 2) * scale)),
                         int(std::lround(raster.height / 2.0 - (vertex.y - (minY + maxY) / 2) * scale))};
    };
    for (const auto *line : lines)
    {
        const auto [x0, y0] = project(map.vertices[line->vertex1Id]);
        const auto [x1, y1] = project(map.vertices[line->vertex2Id]);
        const int steps = std::max({1, std::abs(x1 - x0), std::abs(y1 - y0)});
        for (int step = 0; step <= steps; ++step)
        {
            const int x = int(std::lround(x0 + double(x1 - x0) * step / steps));
            const int y = int(std::lround(y0 + double(y1 - y0) * step / steps));
            if (x < 0 || y < 0 || x >= raster.width || y >= raster.height)
                continue;
            const size_t p = (size_t(y) * raster.width + x) * 4;
            raster.pixelsBgra[p] = 190; raster.pixelsBgra[p + 1] = 211; raster.pixelsBgra[p + 2] = 223;
        }
    }
    return raster;
}
} // namespace OpenYAMM::Game
