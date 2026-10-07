#include "game/world/WorldDatabase.h"
#include "game/events/ScriptedEventProgram.h"
#include "game/outdoor/OutdoorMapData.h"
#include "game/FaceEnums.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <map>

namespace OpenYAMM::Game
{
void WorldDatabase::indexMapLocations(const OutdoorMapData &map, const ScriptedEventProgram &events)
{
    const WorldRecord *pSource = nullptr;
    for (const WorldRecord &record : m_records)
        if (record.kind == WorldRecordKind::Region && record.map->worldId == map.worldId
            && toLowerCopy(record.map->fileName) == toLowerCopy(map.fileName))
            pSource = &record;
    if (pSource == nullptr)
        return;

    std::erase_if(m_locations, [pSource](const WorldMapLocation &location) { return location.mapId == pSource->id; });
    const auto target = [&](uint16_t eventId) -> const WorldRecord *
    {
        const auto metadata = events.getContextActionMetadata(eventId);
        if (!metadata || metadata->hidden || (metadata->source != "opcode" && metadata->source != "script"))
            return nullptr;
        for (const WorldRecord &record : m_records)
        {
            if (metadata->kind == "enter_house" && metadata->houseId
                && record.kind == WorldRecordKind::House && record.house->id == *metadata->houseId
                && record.map == pSource->map)
                return &record;
            if (metadata->kind == "enter_dungeon" && metadata->targetMap
                && record.kind == WorldRecordKind::Dungeon
                && toLowerCopy(record.map->fileName) == toLowerCopy(*metadata->targetMap))
                return &record;
        }
        return nullptr;
    };
    const auto add = [&](const std::string &site, uint16_t eventId, double x, double y)
    {
        const WorldRecord *pTarget = target(eventId);
        if (pTarget == nullptr || x < -32768 || x > 32768 || y < -32768 || y > 32768)
            return;
        m_locations.push_back({pSource->id + "/site/" + site + "/event/" + std::to_string(eventId),
                               pSource->id, pTarget->id, eventId, float(x), float(y)});
    };
    for (size_t i = 0; i < map.bmodels.size(); ++i)
    {
        const OutdoorBModel &model = map.bmodels[i];
        // Group the vertices of each interactive door, not the building's bounding box.
        std::map<uint16_t, std::vector<uint16_t>> verticesByEvent;
        for (const OutdoorBModelFace &face : model.faces)
            if (face.cogTriggeredNumber != 0 && (face.attributes & faceAttributeBit(FaceAttribute::Invisible)) == 0)
                for (uint16_t vertex : face.vertexIndices)
                    if (vertex < model.vertices.size())
                        verticesByEvent[face.cogTriggeredNumber].push_back(vertex);
        for (auto &[eventId, vertices] : verticesByEvent)
        {
            std::sort(vertices.begin(), vertices.end());
            vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
            double x = 0, y = 0;
            for (uint16_t vertex : vertices)
            {
                x += model.vertices[vertex].x;
                y += model.vertices[vertex].y;
            }
            if (!vertices.empty())
                add("bmodel/" + std::to_string(i), eventId, x / vertices.size(), y / vertices.size());
        }
    }
    for (size_t i = 0; i < map.entities.size(); ++i)
    {
        const OutdoorEntity &entity = map.entities[i];
        if (entity.scriptEventId() != 0)
            add("decoration/" + std::to_string(i), entity.scriptEventId(), entity.x, entity.y);
    }
}

const std::vector<WorldMapLocation> &WorldDatabase::locations() const { return m_locations; }

bool WorldDatabase::locationVisible(const WorldMapLocation &location, const WorldKnowledge &knowledge, bool guide) const
{
    const WorldRecord *pSource = find(location.mapId);
    const WorldRecord *pTarget = find(location.recordId);
    if (pSource == nullptr || pTarget == nullptr)
        return false;
    if (guide)
        return true;
    if (!knowledge.knows(*pSource))
        return false;
    if (pTarget->kind == WorldRecordKind::Dungeon)
    {
        const auto exploration = knowledge.regionMaps.find(toLowerCopy(pSource->map->fileName));
        return exploration != knowledge.regionMaps.end()
            && worldRegionPointExplored(exploration->second, location.x, location.y);
    }
    return !recordsAtLocation(location, knowledge, false).empty();
}

std::vector<const WorldRecord *> WorldDatabase::recordsAtLocation(const WorldMapLocation &location,
    const WorldKnowledge &knowledge, bool guide) const
{
    std::vector<const WorldRecord *> result;
    const WorldRecord *pTarget = find(location.recordId);
    if (pTarget == nullptr)
        return result;
    for (const WorldRecord &record : m_records)
        if (record.kind != WorldRecordKind::Travel
            && (&record == pTarget || (pTarget->house != nullptr && record.house == pTarget->house))
            && visible(record, knowledge, guide))
            result.push_back(&record);
    std::sort(result.begin(), result.end(), [](const WorldRecord *a, const WorldRecord *b)
    { return a->name() < b->name(); });
    return result;
}
} // namespace OpenYAMM::Game
