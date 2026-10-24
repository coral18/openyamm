#include "game/ui/screens/AtlasScreen.h"
#include "game/data/GameDataLoader.h"
#include "game/data/GameDataRepository.h"
#include "game/maps/MapAssetLoader.h"
#include "game/StringUtils.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace OpenYAMM::Game
{
void AtlasScreen::stepConnectionScan()
{
    if (!m_guide || m_connectionCatalogComplete) return;
    if (m_connectionScanMaps.empty())
    {
        for (const WorldRecord &record : m_world.records())
            if (record.kind == WorldRecordKind::Region || record.kind == WorldRecordKind::Dungeon)
                m_connectionScanMaps.push_back(record.map);
        if (m_connectionScanMaps.empty())
        {
            m_connectionCatalogComplete = true;
            return;
        }
    }
    const MapStatsEntry &entry = *m_connectionScanMaps[m_connectionScanIndex++];
    try
    {
        MapAssetInfo map;
        map.map = entry;
        std::string error;
        if (!GameDataLoader::loadEventPrograms(assetFileSystem(), map, error) || !map.localEventProgram)
            throw std::runtime_error(error.empty() ? "Missing connection program" : error);
        m_world.indexMapConnections(entry, *map.localEventProgram);
    }
    catch (const std::exception &error)
    {
        ++m_connectionScanFailures;
        std::cerr << "Atlas connection catalog failed for " << entry.fileName << ": " << error.what() << '\n';
    }
    m_connectionCatalogComplete = m_connectionScanIndex == m_connectionScanMaps.size();
}

void AtlasScreen::drawConnectionGraph()
{
    stepConnectionScan();
    const WorldConnectionGraph graph = buildWorldConnectionGraph(m_world, m_knowledge, m_map, m_guide);
    if (std::none_of(graph.nodes.begin(), graph.nodes.end(), [&](const MapStatsEntry *pMap)
        {
            return WorldDatabase::mapId(*pMap) == m_graphNode;
        })) m_graphNode = m_map;
    const Rect panel = canvasRect(25, 121, 544, 270);
    drawSolidRect(panel, 0xff18160fu);
    outline(panel, 0xff597284u);
    std::string status = "Solid: travelled   Dashed: possible   Select a location to highlight its routes";
    if (m_guide && !m_connectionCatalogComplete)
        status = "Reading connections " + std::to_string(m_connectionScanIndex) + "/"
            + std::to_string(m_connectionScanMaps.size());
    if (m_connectionScanFailures > 0) status = "Partial connection catalog - some map programs could not be read";
    textInRect(canvasRect(35, 124, 524, 18), status, "menu_lucida", 9);

    constexpr size_t NodesPerPage = 20, EdgesPerPage = 7;
    const size_t pages = std::max(size_t(1), (graph.nodes.size() + NodesPerPage - 1) / NodesPerPage);
    m_graphPage = std::min(m_graphPage, pages - 1);
    std::map<std::string, Rect> nodes;
    for (size_t index = m_graphPage * NodesPerPage;
        index < std::min(graph.nodes.size(), (m_graphPage + 1) * NodesPerPage); ++index)
    {
        const size_t slot = index % NodesPerPage;
        nodes.emplace(WorldDatabase::mapId(*graph.nodes[index]),
            canvasRect(38 + float(slot % 4) * 132, 149 + float(slot / 4) * 40, 114, 28));
    }
    // Draw the selected edge last; parallel and opposite directions retain distinct lanes.
    for (bool selectedPass : {false, true})
        for (size_t index = 0; index < graph.edges.size(); ++index)
        {
            const WorldConnection &edge = graph.edges[index];
            const bool selected = edge.id == m_graphEdge;
            if (selected != selectedPass || !nodes.contains(edge.sourceId) || !nodes.contains(edge.destinationId))
                continue;
            const Rect &a = nodes.at(edge.sourceId), &b = nodes.at(edge.destinationId);
            const bool incident = edge.sourceId == m_graphNode || edge.destinationId == m_graphNode;
            const uint32_t color = selected ? 0xff8dc5e1u
                : ((edge.observedGameMinutes ? 0x0091a990u : 0x00cfb487u)
                    | (incident ? 0xaa000000u : 0x18000000u));
            const float stroke = (selected ? 2.0f : 0.9f) * designScale();
            float x1 = a.x + a.width / 2, y1 = a.y + a.height / 2;
            float x2 = b.x + b.width / 2, y2 = b.y + b.height / 2;
            const float length = std::hypot(x2 - x1, y2 - y1);
            if (length < designScale())
            {
                const float x = a.x + a.width, y = a.y + a.height / 2, inset = 7 * designScale();
                drawLine(x - inset, a.y, x - inset, a.y - inset, stroke, color);
                drawLine(x - inset, a.y - inset, x + inset, a.y - inset, stroke, color);
                drawLine(x + inset, a.y - inset, x + inset, y, stroke, color);
                drawLine(x + inset, y, x, y, stroke, color);
                drawLine(x, y, x + inset / 2, y - inset / 2, stroke, color);
                drawLine(x, y, x + inset / 2, y + inset / 2, stroke, color);
                continue;
            }
            const float dx = (x2 - x1) / length, dy = (y2 - y1) / length;
            size_t rank = 0, count = 0;
            for (size_t other = 0; other < graph.edges.size(); ++other)
            {
                const WorldConnection &candidate = graph.edges[other];
                if ((candidate.sourceId == edge.sourceId && candidate.destinationId == edge.destinationId)
                    || (candidate.sourceId == edge.destinationId && candidate.destinationId == edge.sourceId))
                {
                    if (other < index) ++rank;
                    ++count;
                }
            }
            const float lane = (float(rank) - float(count - 1) / 2) * 4 * designScale()
                * (edge.sourceId < edge.destinationId ? 1.0f : -1.0f);
            const float trim = std::min(std::abs(dx) > 0.001f ? a.width / (2 * std::abs(dx)) : length,
                std::abs(dy) > 0.001f ? a.height / (2 * std::abs(dy)) : length) + 2 * designScale();
            x1 += dx * trim - dy * lane; y1 += dy * trim + dx * lane;
            x2 -= dx * trim + dy * lane; y2 -= dy * trim - dx * lane;
            const float distance = std::hypot(x2 - x1, y2 - y1);
            if (edge.observedGameMinutes) drawLine(x1, y1, x2, y2, stroke, color);
            else for (float offset = 0; offset < distance; offset += 9 * designScale())
            {
                const float end = std::min(distance, offset + 5 * designScale());
                drawLine(x1 + dx * offset, y1 + dy * offset, x1 + dx * end, y1 + dy * end, stroke, color);
            }
            const float tipX = x1 + (x2 - x1) * 0.65f, tipY = y1 + (y2 - y1) * 0.65f;
            const float arrow = 5 * designScale();
            drawLine(tipX, tipY, tipX - dx * arrow - dy * arrow / 2,
                tipY - dy * arrow + dx * arrow / 2, stroke, color);
            drawLine(tipX, tipY, tipX - dx * arrow + dy * arrow / 2,
                tipY - dy * arrow - dx * arrow / 2, stroke, color);
        }
    for (const MapStatsEntry *pMap : graph.nodes)
    {
        const std::string id = WorldDatabase::mapId(*pMap);
        if (!nodes.contains(id)) continue;
        const Rect &rect = nodes.at(id);
        if (button("atlas-graph-node-" + id, rect, "", "button", true, id == m_graphNode))
        {
            m_graphNode = id;
            m_graphEdge.clear();
            m_graphEdgePage = 0;
        }
        textInRect(rect, pMap->name, "menu_lucida", 9, 0xffc6dfeau, true);
    }
    if (button("atlas-graph-previous", canvasRect(35, 360, 68, 25), "<", "button", m_graphPage > 0))
        --m_graphPage;
    textInRect(canvasRect(112, 360, 275, 25), "Locations " + std::to_string(graph.nodes.size()) + " | Page "
        + std::to_string(m_graphPage + 1) + "/" + std::to_string(pages), "menu_lucida", 10, 0xffc6dfeau, true);
    if (button("atlas-graph-focus", canvasRect(400, 360, 77, 25), "Focus", "button", m_graphNode != m_map))
    {
        m_map = m_graphNode; m_selected = m_map;
        m_graphEdge.clear(); m_graphPage = m_graphEdgePage = 0;
    }
    if (button("atlas-graph-next", canvasRect(489, 360, 68, 25), ">", "button", m_graphPage + 1 < pages))
        ++m_graphPage;

    std::vector<const WorldConnection *> incident;
    const std::string needle = toLowerCopy(m_search);
    for (const WorldConnection &edge : graph.edges)
        if (edge.sourceId == m_graphNode || edge.destinationId == m_graphNode)
        {
            const std::string text = m_world.mapDefinition(edge.sourceId)->name + " "
                + m_world.mapDefinition(edge.destinationId)->name + " " + std::to_string(edge.eventId);
            if (needle.empty() || toLowerCopy(text).find(needle) != std::string::npos) incident.push_back(&edge);
        }
    const size_t edgePages = std::max(size_t(1), (incident.size() + EdgesPerPage - 1) / EdgesPerPage);
    m_graphEdgePage = std::min(m_graphEdgePage, edgePages - 1);
    const MapStatsEntry *pSelectedNode = m_world.mapDefinition(m_graphNode);
    textInRect(canvasRect(589, 89, 151, 25), (pSelectedNode != nullptr ? pSelectedNode->name : "Routes")
        + " | " + std::to_string(m_graphEdgePage + 1) + "/" + std::to_string(edgePages), "menu_lucida", 10);
    if (button("atlas-graph-routes-previous", canvasRect(747, 89, 35, 25), "<", "button", m_graphEdgePage > 0))
    {
        --m_graphEdgePage;
        m_graphEdge.clear();
    }
    if (button("atlas-graph-routes-next", canvasRect(787, 89, 35, 25), ">", "button", m_graphEdgePage + 1 < edgePages))
    {
        ++m_graphEdgePage;
        m_graphEdge.clear();
    }
    if (std::none_of(incident.begin(), incident.end(), [&](const WorldConnection *pEdge)
        {
            return pEdge->id == m_graphEdge;
        }))
        m_graphEdge = incident.empty() ? "" : incident[m_graphEdgePage * EdgesPerPage]->id;
    for (size_t index = m_graphEdgePage * EdgesPerPage;
        index < std::min(incident.size(), (m_graphEdgePage + 1) * EdgesPerPage); ++index)
    {
        const WorldConnection &edge = *incident[index];
        const float y = 121 + float(index % EdgesPerPage) * 39;
        if (button("atlas-graph-route-" + edge.id, canvasRect(589, y, 233, 35), "", "button",
            true, edge.id == m_graphEdge)) m_graphEdge = edge.id;
        const bool outgoing = edge.sourceId == m_graphNode;
        const MapStatsEntry *pOther = m_world.mapDefinition(outgoing ? edge.destinationId : edge.sourceId);
        textInRect(canvasRect(596, y + 2, 219, 15), std::string(outgoing ? "To: " : "From: ") + pOther->name,
            "menu_lucida", 10);
        size_t rank = 0, count = 0;
        for (const WorldConnection &other : graph.edges)
            if (other.sourceId == edge.sourceId && other.destinationId == edge.destinationId)
            {
                if (other.id < edge.id) ++rank;
                ++count;
            }
        std::string caption = edge.observedGameMinutes ? "Travelled | Day "
            + std::to_string(*edge.observedGameMinutes / (24 * 60) + 1) : "Possible route";
        if (count > 1) caption += " | Passage " + std::to_string(rank + 1) + "/" + std::to_string(count);
        textInRect(canvasRect(596, y + 18, 219, 13), caption, "menu_lucida", 9);
    }
    const auto selected = std::find_if(incident.begin(), incident.end(), [&](const WorldConnection *pEdge)
        {
            return pEdge->id == m_graphEdge;
        });
    std::string details = m_guide ? "Select a location or route. Possibilities can depend on quest and lever states."
        : "Only recorded journeys are shown. A journey does not establish a return passage.";
    if (selected == incident.end())
    {
        textInRect(canvasRect(34, 407, 778, 43), details, "menu_arrus", 11);
        return;
    }
    const WorldConnection &edge = **selected;
    const MapStatsEntry &source = *m_world.mapDefinition(edge.sourceId);
    const MapStatsEntry &destination = *m_world.mapDefinition(edge.destinationId);
    details = source.name + " -> " + destination.name + "\n";
    details += edge.observedGameMinutes ? "Travelled | Day "
        + std::to_string(*edge.observedGameMinutes / (24 * 60) + 1) + ". Availability may change."
        : "Possible route. Conditions may apply.";
    details += "\nArrows show direction; a return passage is not implied.";
    textInRect(canvasRect(34, 407, 470, 43), details, "menu_arrus", 11);
    const auto open = [&](const MapStatsEntry &map, bool arrival)
    {
        if (const WorldRecord *pRecord = m_world.find(WorldDatabase::mapId(map))) select(*pRecord);
        m_connectionView = false;
        m_featureView = true;
        m_connectionJumpEvent = arrival ? 0 : edge.eventId;
        m_connectionJumpMap = arrival ? "" : edge.sourceId;
        m_connectionJumpDestination = arrival ? "" : edge.destinationId;
        if (arrival && edge.observedGameMinutes)
            m_selectedFeature = edge.destinationId + "/arrival/" + edge.sourceId
                + "/event/" + std::to_string(edge.eventId);
    };
    if (button("atlas-graph-departure", canvasRect(514, 416, 145, 28), "Departure map")) open(source, false);
    if (button("atlas-graph-destination", canvasRect(669, 416, 145, 28), "Destination map")) open(destination, true);
}
}
