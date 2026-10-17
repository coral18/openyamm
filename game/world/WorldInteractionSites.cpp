#include "game/world/WorldInteractionSites.h"
#include "game/outdoor/OutdoorMapData.h"
#include "game/indoor/IndoorMapData.h"
#include "game/FaceEnums.h"

#include <algorithm>
#include <map>
#include <tuple>

namespace OpenYAMM::Game
{
namespace
{
template<typename Vertex>
void positionFromVertices(WorldInteractionSite &site, const std::vector<uint16_t> &ids,
                          const std::vector<Vertex> &vertices)
{
    std::vector<std::tuple<int, int, int>> positions;
    for (uint16_t id : ids)
        positions.emplace_back(vertices[id].x, vertices[id].y, vertices[id].z);
    std::sort(positions.begin(), positions.end());
    positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
    double x = 0, y = 0, z = 0;
    for (const auto &[vx, vy, vz] : positions)
    {
        x += vx; y += vy; z += vz;
    }
    site.x = float(x / positions.size()); site.y = float(y / positions.size()); site.z = float(z / positions.size());
}

template<typename Entity>
void appendDecorations(std::vector<WorldInteractionSite> &sites, const std::vector<Entity> &entities)
{
    for (size_t i = 0; i < entities.size(); ++i)
    {
        const Entity &entity = entities[i];
        if (entity.scriptEventId() != 0)
            sites.push_back({"decoration/" + std::to_string(i), entity.scriptEventId(),
                             float(entity.x), float(entity.y), float(entity.z)});
    }
}
template<typename Vertex, typename Face, typename EventId>
void appendSurfaceSites(std::vector<WorldInteractionSite> &sites, const std::vector<Vertex> &vertices,
                        const std::vector<Face> &faces, EventId eventIdForFace, const std::string &prefix)
{
    // Connected faces sharing an event are one object. Reusing an event elsewhere stays a separate site.
    std::map<uint16_t, std::vector<uint32_t>> facesByEvent;
    for (size_t i = 0; i < faces.size(); ++i)
        if (eventIdForFace(faces[i]) != 0 && !hasFaceAttribute(faces[i].attributes, FaceAttribute::Invisible))
            facesByEvent[eventIdForFace(faces[i])].push_back(uint32_t(i));
    for (const auto &[eventId, eventFaces] : facesByEvent)
    {
        const auto vertexKey = [&vertices](uint16_t vertex)
        { return std::tuple{vertices[vertex].x, vertices[vertex].y, vertices[vertex].z}; };
        std::map<std::tuple<int, int, int>, std::vector<uint32_t>> facesAtVertex;
        for (uint32_t face : eventFaces)
            for (uint16_t vertex : faces[face].vertexIndices)
                if (vertex < vertices.size())
                    facesAtVertex[vertexKey(vertex)].push_back(face);
        std::map<uint32_t, bool> visited;
        for (uint32_t first : eventFaces)
        {
            if (visited[first])
                continue;
            WorldInteractionSite site{prefix + "surface/" + std::to_string(first), eventId};
            site.surfacePrefix = prefix;
            std::vector<uint16_t> vertexIds;
            std::vector<uint32_t> pending{first};
            visited[first] = true;
            while (!pending.empty())
            {
                const uint32_t face = pending.back();
                pending.pop_back();
                site.faceIds.push_back(face);
                for (uint16_t vertex : faces[face].vertexIndices)
                    if (vertex < vertices.size())
                    {
                        vertexIds.push_back(vertex);
                        for (uint32_t neighbor : facesAtVertex.at(vertexKey(vertex)))
                            if (!visited[neighbor])
                            {
                                visited[neighbor] = true;
                                pending.push_back(neighbor);
                            }
                    }
            }
            if (!vertexIds.empty())
            {
                positionFromVertices(site, vertexIds, vertices);
                sites.push_back(std::move(site));
            }
        }
    }
}
}

std::vector<WorldInteractionSite> worldInteractionSites(const OutdoorMapData &map)
{
    std::vector<WorldInteractionSite> sites;
    for (size_t i = 0; i < map.bmodels.size(); ++i)
    {
        const OutdoorBModel &model = map.bmodels[i];
        appendSurfaceSites(sites, model.vertices, model.faces,
            [](const OutdoorBModelFace &face) { return face.cogTriggeredNumber; },
            "bmodel/" + std::to_string(i) + "/");
    }
    appendDecorations(sites, map.entities);
    return sites;
}

std::vector<WorldInteractionSite> worldInteractionSites(const IndoorMapData &map)
{
    std::vector<WorldInteractionSite> sites;
    appendSurfaceSites(sites, map.vertices, map.faces,
        [](const IndoorFace &face) { return face.cogTriggered; }, "");
    appendDecorations(sites, map.entities);
    return sites;
}
}
