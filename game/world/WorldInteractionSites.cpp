#include "game/world/WorldInteractionSites.h"
#include "game/outdoor/OutdoorMapData.h"
#include "game/indoor/IndoorMapData.h"
#include "game/FaceEnums.h"
#include "game/maps/MapDeltaData.h"
#include "game/gameplay/InteractiveDecorationRules.h"

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
template<typename Entity>
std::vector<WorldDecorationSite> inheritedDecorations(const std::vector<Entity> &entities,
    const DecorationTable &decorations, InteractiveDecorationRuleSet rules, const MapDeltaData *pInitialState)
{
    std::vector<WorldDecorationSite> result;
    uint8_t decorVarIndex = 0;
    constexpr uint8_t MaximumDecorationVariables = 125;
    const bool hasInitialState = pInitialState != nullptr
        && std::any_of(pInitialState->eventVariables.decorVars.begin(), pInitialState->eventVariables.decorVars.end(),
            [](uint8_t value) { return value != 0; });
    for (size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex)
    {
        const Entity &entity = entities[entityIndex];
        if (entity.scriptEventId() != 0) continue;
        const DecorationEntry *pEntry = decorations.resolveMapDecoration(entity.decorationListId, entity.name).pEntry;
        if (pEntry == nullptr) continue;
        const auto binding = resolveInteractiveDecorationBindingSpec(*pEntry, entity.name, rules);
        if (!binding || decorVarIndex >= MaximumDecorationVariables) continue;
        WorldDecorationSite decoration;
        decoration.decorVarIndex = decorVarIndex;
        decoration.site = {"inherited-decoration/" + std::to_string(decorVarIndex++), binding->baseEventId,
            float(entity.x), float(entity.y), float(entity.z)};
        decoration.eventCount = binding->eventCount;
        const uint32_t seed = makeInteractiveDecorationSeed(entityIndex, entity.decorationListId,
            entity.x, entity.y, entity.z);
        uint8_t initialState = binding->initialState;
        if (binding->useSeededInitialState) initialState = uint8_t(seed % binding->eventCount);
        else if (binding->family != InteractiveDecorationFamily::None)
            initialState = initialInteractiveDecorationState(binding->family, seed);
        if (hasInitialState) initialState = pInitialState->eventVariables.decorVars[decorVarIndex - 1];
        decoration.initiallyDepleted = interactiveDecorationIsCleared(initialState, binding->eventCount,
            binding->hideWhenCleared);
        if (initialState >= binding->eventCount) initialState = 0;
        decoration.initialEventId = uint16_t(uint32_t(binding->baseEventId) + initialState);
        decoration.name = !pEntry->hint.empty() && pEntry->hint != "0" ? pEntry->hint : "Decoration";
        switch (binding->family)
        {
        case InteractiveDecorationFamily::Barrel: decoration.name = "Barrel"; break;
        case InteractiveDecorationFamily::Cauldron: decoration.name = "Cauldron"; break;
        case InteractiveDecorationFamily::TrashHeap:
        case InteractiveDecorationFamily::MightAndMagicSixTrashHeap: decoration.name = "Trash heap"; break;
        case InteractiveDecorationFamily::CampFire: decoration.name = "Camp fire"; break;
        case InteractiveDecorationFamily::Cask: decoration.name = "Cask"; break;
        case InteractiveDecorationFamily::FlourSack: decoration.name = "Flour sack"; break;
        case InteractiveDecorationFamily::LargeBag: decoration.name = "Large bag"; break;
        case InteractiveDecorationFamily::Bucket: decoration.name = "Bucket"; break;
        case InteractiveDecorationFamily::Crystal: decoration.name = "Crystal"; break;
        case InteractiveDecorationFamily::None: break;
        }
        result.push_back(std::move(decoration));
    }
    return result;
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
std::vector<WorldDecorationSite> worldInheritedDecorationSites(const OutdoorMapData &map,
    const DecorationTable &decorations, const MapDeltaData *pInitialState)
{
    return inheritedDecorations(map.entities, decorations,
        InteractiveDecorationRuleSet::OutdoorBindings, pInitialState);
}

std::vector<WorldDecorationSite> worldInheritedDecorationSites(const IndoorMapData &map,
    const DecorationTable &decorations, const MapDeltaData *pInitialState)
{
    return inheritedDecorations(map.entities, decorations, InteractiveDecorationRuleSet::Shared, pInitialState);
}

}
