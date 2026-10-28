#include "game/world/WorldFloorPlan.h"
#include "game/indoor/IndoorMapData.h"
#include "game/indoor/IndoorGeometryUtils.h"
#include "game/FaceEnums.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

namespace OpenYAMM::Game
{
namespace
{
uint32_t planFaceAttributes(const IndoorMapData &map, const WorldFloorPlanKnowledge &knowledge, bool guide,
    size_t face)
{
    return !guide && face < knowledge.faceAttributes.size() ? knowledge.faceAttributes[face]
        : map.faces[face].attributes;
}

bool floorProjectionsOverlap(const IndoorFaceGeometryData &a, const IndoorFaceGeometryData &b)
{
    // Separating axes reject touching room edges as well as disjoint convex face projections.
    const auto separatedOnEdge = [&](const bx::Vec3 &first, const bx::Vec3 &second)
    {
        const double x = double(first.y) - second.y, y = double(second.x) - first.x;
        if (x == 0 && y == 0) return false;
        const auto interval = [&](const IndoorFaceGeometryData &face)
        {
            double lower = std::numeric_limits<double>::max(), upper = -lower;
            for (const bx::Vec3 &vertex : face.vertices)
            {
                const double value = vertex.x * x + vertex.y * y;
                lower = std::min(lower, value);
                upper = std::max(upper, value);
            }
            return std::pair{lower, upper};
        };
        const auto [aMin, aMax] = interval(a);
        const auto [bMin, bMax] = interval(b);
        return aMax <= bMin || bMax <= aMin;
    };
    for (const IndoorFaceGeometryData *pFace : {&a, &b})
        for (size_t i = 0; i < pFace->vertices.size(); ++i)
            if (separatedOnEdge(pFace->vertices[i], pFace->vertices[(i + 1) % pFace->vertices.size()])) return false;
    return true;
}

void refineKnownFloorBands(const IndoorMapData &map, const WorldFloorPlanKnowledge &knowledge, bool guide,
    const std::unordered_set<uint32_t> &knownFaces, std::unordered_set<int32_t> &refined)
{
    if (knownFaces.empty()) return;
    IndoorFaceGeometryCache cache(map.faces.size());
    std::map<int32_t, std::vector<const IndoorFaceGeometryData *>> bands;
    for (uint32_t face : knownFaces)
    {
        const uint32_t attributes = planFaceAttributes(map, knowledge, guide, face);
        if (hasFaceAttribute(attributes, FaceAttribute::IsPortal)
            || hasFaceAttribute(attributes, FaceAttribute::Untouchable)) continue;
        const IndoorFaceGeometryData *pGeometry = cache.geometryForFace(map, map.vertices, face);
        if (pGeometry == nullptr || pGeometry->kind != IndoorFaceKind::Floor
            || pGeometry->minZ != pGeometry->maxZ || pGeometry->vertices.empty()) continue;
        bands[*worldFloorPlanLayer(pGeometry->minZ)].push_back(pGeometry);
    }
    for (auto &[band, floors] : bands)
    {
        std::sort(floors.begin(), floors.end(),
            [](const IndoorFaceGeometryData *pLeft, const IndoorFaceGeometryData *pRight)
            { return pLeft->minX < pRight->minX; });
        for (size_t i = 0; i < floors.size() && !refined.contains(band); ++i)
            for (size_t j = i + 1; j < floors.size(); ++j)
            {
                const IndoorFaceGeometryData &a = *floors[i], &b = *floors[j];
                if (b.minX >= a.maxX) break;
                if (a.minY >= b.maxY || b.minY >= a.maxY || std::abs(a.minZ - b.minZ) < 128) continue;
                if (floorProjectionsOverlap(a, b))
                {
                    refined.insert(band);
                    break;
                }
            }
    }
}
}

std::string WorldFloorPlanLayer::name() const
{
    return std::to_string(index) + (subdivision ? "." + std::to_string(subdivision) : "");
}

std::optional<int32_t> worldFloorPlanLayer(float z)
{
    if (!std::isfinite(z)) return std::nullopt;
    const double layer = std::floor((double(z) + 256) / 512);
    if (layer < INT32_MIN || layer > INT32_MAX) return std::nullopt;
    return int32_t(layer);
}

std::optional<WorldFloorPlanLayer> worldFloorPlanLayerForHeight(float z, bool subdivide)
{
    const std::optional<int32_t> band = worldFloorPlanLayer(z);
    if (!band) return std::nullopt;
    const double lower = double(*band) * 512 - 256;
    return WorldFloorPlanLayer{*band, subdivide ? uint8_t(1 + std::floor((double(z) - lower) / 128)) : uint8_t(0)};
}

bool worldFloorPlanLayerMatches(std::optional<WorldFloorPlanLayer> layer, float z)
{
    return !layer || worldFloorPlanLayerForHeight(z, layer->subdivision != 0) == layer;
}

std::optional<int32_t> worldFloorPlanLayerElevation(WorldFloorPlanLayer layer)
{
    if (layer.subdivision > 4) return std::nullopt;
    const int64_t center = int64_t(layer.index) * 512
        + (layer.subdivision ? int64_t(layer.subdivision - 1) * 128 - 192 : 0);
    const int32_t height = int32_t(std::clamp(center, int64_t(INT32_MIN), int64_t(INT32_MAX)));
    return worldFloorPlanLayerMatches(layer, float(height)) ? std::optional<int32_t>(height) : std::nullopt;
}

std::optional<WorldFloorPlanLayer> WorldFloorPlanRaster::layerAtHeight(float z) const
{
    const std::optional<int32_t> band = worldFloorPlanLayer(z);
    return band ? worldFloorPlanLayerForHeight(z, refinedBands.contains(*band)) : std::nullopt;
}

WorldFloorPlanRaster buildWorldFloorPlan(const IndoorMapData &map,
    const WorldFloorPlanKnowledge &knowledge, bool guide,
    const std::vector<std::pair<float, float>> &knownPositions, std::optional<WorldFloorPlanLayer> layer)
{
    if (layer && layer->subdivision > 4) throw std::invalid_argument("Invalid dungeon height subdivision");
    WorldFloorPlanRaster raster;
    std::vector<const IndoorOutline *> lines;
    std::unordered_set<uint32_t> knownFaces;
    double minX = std::numeric_limits<double>::max(), minY = minX;
    double maxX = std::numeric_limits<double>::lowest(), maxY = maxX;
    const auto attributes = [&](size_t face)
    {
        return planFaceAttributes(map, knowledge, guide, face);
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
        knownFaces.insert(line.face1Id);
        knownFaces.insert(line.face2Id);
        for (auto id : {line.vertex1Id, line.vertex2Id})
        {
            minX = std::min(minX, double(map.vertices[id].x)); maxX = std::max(maxX, double(map.vertices[id].x));
            minY = std::min(minY, double(map.vertices[id].y)); maxY = std::max(maxY, double(map.vertices[id].y));
        }
    }
    refineKnownFloorBands(map, knowledge, guide, knownFaces, raster.refinedBands);
    if (layer && !raster.refinedBands.contains(layer->index)) layer->subdivision = 0;
    raster.selectedLayer = layer;
    for (const IndoorOutline *pLine : lines) raster.layers.push_back(*raster.layerAtHeight(pLine->z));
    std::erase_if(lines, [&](const IndoorOutline *pLine) { return !worldFloorPlanLayerMatches(layer, pLine->z); });
    raster.outlineCount = lines.size();
    std::sort(raster.layers.begin(), raster.layers.end());
    raster.layers.erase(std::unique(raster.layers.begin(), raster.layers.end()), raster.layers.end());
    // A remembered position can be known before any nearby wall outline. It frames an otherwise blank map,
    // without using undiscovered geometry to infer its bounds or awarding any reveal bits.
    for (const auto &[x, y] : knownPositions)
        if (std::isfinite(x) && std::isfinite(y))
        {
            minX = std::min(minX, double(x)); maxX = std::max(maxX, double(x));
            minY = std::min(minY, double(y)); maxY = std::max(maxY, double(y));
        }
    if (minX > maxX || minY > maxY)
        return raster;
    raster.pixelsBgra.resize(size_t(raster.width) * raster.height * 4);
    for (size_t p = 0; p < raster.pixelsBgra.size(); p += 4)
    {
        raster.pixelsBgra[p] = 15; raster.pixelsBgra[p + 1] = 22;
        raster.pixelsBgra[p + 2] = 24; raster.pixelsBgra[p + 3] = 255;
    }
    constexpr double padding = 40;
    const double minimumSpan = raster.layers.empty() ? 512.0 : 1.0;
    const double scale = std::min((raster.width - 2 * padding) / std::max(minimumSpan, maxX - minX),
                                  (raster.height - 2 * padding) / std::max(minimumSpan, maxY - minY));
    raster.centerX = (minX + maxX) / 2;
    raster.centerY = (minY + maxY) / 2;
    raster.pixelsPerWorldUnit = scale;
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
std::pair<float, float> WorldFloorPlanRaster::worldToUv(float x, float y) const
{
    return {float(0.5 + (x - centerX) * pixelsPerWorldUnit / width),
            float(0.5 - (y - centerY) * pixelsPerWorldUnit / height)};
}
std::pair<float, float> WorldFloorPlanRaster::uvToWorld(float u, float v) const
{
    if (pixelsPerWorldUnit <= 0)
        return {float(centerX), float(centerY)};
    return {float(centerX + (u - 0.5) * width / pixelsPerWorldUnit),
            float(centerY - (v - 0.5) * height / pixelsPerWorldUnit)};
}
} // namespace OpenYAMM::Game
