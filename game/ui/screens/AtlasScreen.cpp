#include "game/ui/screens/AtlasScreen.h"
#include "game/data/GameDataRepository.h"
#include "game/data/GameDataLoader.h"
#include "game/gameplay/MasteryTeacherDialog.h"
#include "game/StringUtils.h"
#include "game/maps/MapAssetLoader.h"
#include "game/ui/GameplayMinimapTransform.h"

#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <iostream>

namespace OpenYAMM::Game
{
namespace
{
std::vector<WorldMapPresentation> loadPresentation(const Engine::AssetFileSystem &assets)
{
    const auto text = assets.readTextFile("world/atlas/mm7.yml");
    if (!text)
        throw std::runtime_error("Missing MM7 atlas presentation package");
    const auto root = YAML::Load(*text);
    if (root["schema"].as<int>() != 1 || root["world"].as<std::string>() != "mm7")
        throw std::runtime_error("Unsupported atlas presentation schema");
    std::vector<WorldMapPresentation> result;
    for (const auto &node : root["maps"])
    {
        WorldMapPresentation map;
        map.fileName = node["file"].as<std::string>();
        map.areaId = node["area"].as<int>(0);
        map.parentFileName = node["parent"].as<std::string>("");
        if (node["position"])
        {
            if (!node["position"].IsSequence() || node["position"].size() != 2)
                throw std::runtime_error("Invalid atlas map position");
            map.position = {node["position"][0].as<float>(), node["position"][1].as<float>()};
        }
        result.push_back(std::move(map));
    }
    return result;
}
} // namespace

AtlasScreen::AtlasScreen(const Engine::AssetFileSystem &assets, const GameDataRepository &data,
    WorldKnowledge knowledge, std::function<void()> close, bool paused, GameAudioSystem *audio,
    const MapAssetInfo *pCurrentMap, const Party *pParty)
    : MenuDesignScreen(assets, paused, audio), m_knowledge(std::move(knowledge)),
      m_world(data.mapStats(), data.houseTable(), data.npcDialogTable(), data.mergedTeacherTopicTable(),
              "mm7", loadPresentation(assets)), m_data(data), m_pParty(pParty),
      m_close(std::move(close)), m_paused(paused)
{
    loadDesign("menu/main_menu");
    if (pCurrentMap != nullptr && pCurrentMap->outdoorMapData && pCurrentMap->localEventProgram)
    {
        m_world.indexMapLocations(*pCurrentMap->outdoorMapData, *pCurrentMap->localEventProgram);
        m_indexedMaps.insert(WorldDatabase::mapId(pCurrentMap->map));
    }
    if (m_knowledge.partyPosition)
        for (const auto &record : m_world.records())
            if ((record.kind == WorldRecordKind::Region || record.kind == WorldRecordKind::Dungeon)
                && toLowerCopy(record.map->fileName) == m_knowledge.partyPosition->mapFileName)
            {
                select(record);
                break;
            }
}

AtlasScreen::~AtlasScreen()
{
    if (bgfx::isValid(m_floorPlanTexture))
        bgfx::destroy(m_floorPlanTexture);
    if (bgfx::isValid(m_fogTexture))
        bgfx::destroy(m_fogTexture);
}

AppMode AtlasScreen::mode() const { return m_paused ? AppMode::PauseMenu : AppMode::MainMenu; }
void AtlasScreen::onExit() { setTextEditing(false); }

void AtlasScreen::handleSdlEvent(const SDL_Event &event)
{
    if (!m_searchEditing)
        return;
    if (event.type == SDL_EVENT_TEXT_INPUT)
    {
        if (m_search.size() < 128)
            m_search += event.text.text;
        m_scroll = 0;
    }
    if (event.type == SDL_EVENT_KEY_DOWN)
    {
        if (event.key.scancode == SDL_SCANCODE_BACKSPACE && !m_search.empty())
        {
            // Erase a complete UTF-8 code point.
            size_t start = m_search.size() - 1;
            while (start > 0 && (static_cast<unsigned char>(m_search[start]) & 0xc0) == 0x80)
                --start;
            m_search.erase(start);
            m_scroll = 0;
        }
        if (event.key.scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_ESCAPE)
        {
            m_searchEditing = false;
            setTextEditing(false);
            m_suppressEscape = true;
        }
    }
}

void AtlasScreen::select(const WorldRecord &record)
{
    m_selected = record.id;
    if (record.kind == WorldRecordKind::Region || record.kind == WorldRecordKind::Dungeon)
    {
        m_map = record.id;
        m_zoom = 1;
        m_panX = m_panY = m_scroll = 0;
        m_search.clear();
        m_filter.reset();
        m_siteSelection.clear();
    }
}

void AtlasScreen::drawScreen(float)
{
    beginDesign("Atlas");
    drawSolidRect({0, 0, float(frameWidth()), float(frameHeight())}, 0xff100e09u);
    skin("frame", canvasRect(8, 8, 837, 464), true);
    textInRect(canvasRect(25, 15, 325, 30), "Interactive Atlas - Antagarich", "fondamento", 20, 0xffbcddeeu);
    if (button("atlas-mode", canvasRect(526, 17, 177, 27), m_guide ? "Guide Mode: ON" : "Discovery Mode"))
    {
        m_guide = !m_guide;
        if (const auto *selected = m_world.find(m_selected); selected && !m_world.visible(*selected, m_knowledge, m_guide))
            m_selected.clear();
        if (const auto *map = m_world.find(m_map); map && !m_world.visible(*map, m_knowledge, m_guide))
            m_map.clear();
        m_scroll = 0;
        m_zoom = 1;
        m_panX = m_panY = 0;
        m_siteSelection.clear();
    }
    if (button("atlas-close", canvasRect(716, 17, 107, 27), "Back")
        || (!m_suppressEscape && !m_searchEditing && keyPressed(SDL_SCANCODE_ESCAPE)))
    {
        m_close();
        return;
    }
    m_suppressEscape = false;
    const Rect search = canvasRect(25, 53, 274, 28);
    if (m_searchEditing && (keyPressed(SDL_SCANCODE_TAB) || (leftMouseJustPressed() && !pointerInside(search))))
    {
        m_searchEditing = false;
        setTextEditing(false);
    }
    if (button("atlas-search", search, "", "text_field"))
    {
        m_searchEditing = true;
        setTextEditing(true);
    }
    textInRect(canvasRect(35, 57, 253, 20), m_search.empty() ? (m_guide ? "Search the world..." : "Search discovered places...") : m_search + (m_searchEditing ? "_" : ""),
        "menu_lucida", 11);
    const std::pair<const char *, std::optional<WorldRecordKind>> filters[] = {
        {"All", {}}, {"Regions", WorldRecordKind::Region}, {"Dungeons", WorldRecordKind::Dungeon},
        {"Buildings", WorldRecordKind::House}, {"NPCs", WorldRecordKind::Npc},
        {"Trainers", WorldRecordKind::Trainer}, {"Travel", WorldRecordKind::Travel}};
    for (int i = 0; i < 7; ++i)
    {
        const Rect rect = canvasRect(312 + i * 73, 53, 69, 28);
        if (button("atlas-filter-" + std::to_string(i), rect, filters[i].first,
                   "button", true, m_filter == filters[i].second))
        {
            m_filter = filters[i].second;
            m_scroll = 0;
            m_siteSelection.clear();
        }
        if (m_filter == filters[i].second)
            outline(rect, 0xff8dc5e1u);
    }
    if (button("atlas-world", canvasRect(25, 89, 132, 25), "Antagarich", "button", true, m_map.empty()))
    {
        m_map.clear();
        m_selected.clear();
        m_siteSelection.clear();
        m_zoom = 1;
        m_panX = m_panY = m_scroll = 0;
    }
    if (const auto *map = m_world.find(m_map))
    {
        textInRect(canvasRect(168, 89, 403, 25), map->name(), "fondamento", 14);
        if (map->kind == WorldRecordKind::Region)
            textInRect(canvasRect(375, 94, 195, 16), "B Buildings   T Trainers   D Dungeons",
                       "menu_lucida", 8, 0xffc6dfeau);
    }
    indexKnownRegions();
    const std::string listHeading = !m_siteSelection.empty() ? "Selected markers (clear)"
        : m_filter == WorldRecordKind::Travel ? (m_guide ? "All recorded routes" : "Known routes")
        : m_map.empty() ? (m_guide ? "All recorded locations" : "Your party's discoveries")
        : (m_guide ? "Locations and residents here" : "Discovered here");
    if (m_siteSelection.empty())
        textInRect(canvasRect(589, 89, 233, 25), listHeading, "menu_lucida", 11);
    else if (button("atlas-clear-sites", canvasRect(589, 89, 233, 25), listHeading))
    {
        m_siteSelection.clear();
        m_scroll = 0;
    }
    drawMap();
    drawList();
    drawDetails();
}

void AtlasScreen::indexKnownRegions()
{
    for (const WorldRecord &record : m_world.records())
    {
        if (record.kind != WorldRecordKind::Region || !m_world.visible(record, m_knowledge, m_guide)
            || !m_indexedMaps.insert(record.id).second)
            continue;
        try
        {
            auto map = MapAssetLoader().load(assetFileSystem(), *record.map,
                m_data.monsterTable(), m_data.objectTable(), MapLoadPurpose::HeadlessGameplay);
            std::string error;
            if (!map || !map->outdoorMapData || !GameDataLoader::loadEventPrograms(assetFileSystem(), *map, error)
                || !map->localEventProgram)
                throw std::runtime_error(error.empty() ? "Missing geometry or event metadata" : error);
            m_world.indexMapLocations(*map->outdoorMapData, *map->localEventProgram);
        }
        catch (const std::exception &error)
        {
            m_locationError = "Some map markers are unavailable in this content package.";
            std::cerr << "World locations could not be indexed for " << record.map->fileName
                      << ": " << error.what() << '\n';
        }
    }
}

bool AtlasScreen::drawLocationMarkers(const WorldRecord &map, const Rect &image, const Rect &viewport)
{
    struct Pin
    {
        std::string id;
        float x = 0, y = 0;
        size_t sites = 0;
        std::vector<const WorldRecord *> records;
    };
    std::vector<Pin> pins;
    const float spacing = 26 * designScale();
    for (const WorldMapLocation &location : m_world.locations())
    {
        if (location.mapId != map.id || !m_world.locationVisible(location, m_knowledge, m_guide))
            continue;
        auto records = m_world.recordsAtLocation(location, m_knowledge, m_guide);
        std::erase_if(records, [this](const WorldRecord *pRecord)
        { return (m_filter && pRecord->kind != *m_filter)
            || (!m_search.empty()
                && toLowerCopy(pRecord->searchText()).find(toLowerCopy(m_search)) == std::string::npos); });
        if (records.empty())
            continue;
        const auto uv = gameplayMinimapWorldToUv({}, location.x, location.y);
        const float x = image.x + uv.x * image.width, y = image.y + uv.y * image.height;
        if (x < viewport.x || x > viewport.x + viewport.width || y < viewport.y || y > viewport.y + viewport.height)
            continue;
        auto pin = std::find_if(pins.begin(), pins.end(), [&](const Pin &other)
        { return std::hypot(x - other.x, y - other.y) < spacing; });
        if (pin == pins.end())
        {
            pins.push_back({location.id, x, y, 0, {}});
            pin = std::prev(pins.end());
        }
        pin->x = (pin->x * pin->sites + x) / (pin->sites + 1);
        pin->y = (pin->y * pin->sites + y) / (pin->sites + 1);
        ++pin->sites;
        for (const WorldRecord *pRecord : records)
            if (std::find(pin->records.begin(), pin->records.end(), pRecord) == pin->records.end())
                pin->records.push_back(pRecord);
    }
    bool hovered = false;
    for (const Pin &pin : pins)
    {
        const auto trainer = std::find_if(pin.records.begin(), pin.records.end(), [](const WorldRecord *pRecord)
        { return pRecord->kind == WorldRecordKind::Trainer; });
        const auto house = std::find_if(pin.records.begin(), pin.records.end(), [](const WorldRecord *pRecord)
        { return pRecord->kind == WorldRecordKind::House; });
        const WorldRecord *pPrimary = trainer != pin.records.end() ? *trainer
            : house != pin.records.end() ? *house : pin.records.front();
        const bool dungeon = pPrimary->kind == WorldRecordKind::Dungeon;
        const char *label = dungeon ? "D" : trainer != pin.records.end() ? "T" : "B";
        const std::string caption = pin.sites > 1 ? std::to_string(pin.sites) + " locations - select to inspect"
            : pPrimary->name();
        const Rect marker{pin.x - 12 * designScale(), pin.y - 12 * designScale(),
                          24 * designScale(), 24 * designScale()};
        const std::string buttonId = "atlas-site-" + pin.id;
        hovered = hovered || pointerInside(marker);
        if (button(buttonId, marker, pin.sites > 1 ? std::to_string(pin.sites) : label, "button_square"))
        {
            if (dungeon && pin.records.size() == 1)
            {
                select(*pPrimary);
                return true;
            }
            m_siteSelection.clear();
            for (const WorldRecord *pRecord : pin.records)
                m_siteSelection.insert(pRecord->id);
            m_selected = pPrimary->id;
            m_scroll = 0;
        }
        outline(marker, trainer != pin.records.end() ? 0xff96c798u : dungeon ? 0xff8dc5e1u : 0xff8dc8dfu);
        if (pointerInside(marker) || focused(buttonId))
        {
            Rect text{pin.x - 105 * designScale(), pin.y - 35 * designScale(),
                      210 * designScale(), 20 * designScale()};
            text.x = std::clamp(text.x, viewport.x, viewport.x + viewport.width - text.width);
            text.y = std::max(text.y, viewport.y);
            drawSolidRect(text, 0xed100e09u);
            textInRect(text, caption, "menu_arrus", 11, 0xffc6dfeau, true);
        }
    }
    return hovered;
}

void AtlasScreen::drawMap()
{
    const Rect viewport = canvasRect(25, 121, 544, 270);
    drawSolidRect(viewport, 0xff18160fu);
    outline(viewport, 0xff597284u);
    const WorldRecord *map = m_world.find(m_map);
    const bool dungeon = map != nullptr && map->kind == WorldRecordKind::Dungeon;
    const std::string texture = map == nullptr ? "antagrich.png"
        : std::filesystem::path(map->map->fileName).stem().string();
    const WorldFloorPlanRaster *floorPlan = nullptr;
    std::optional<TextureSize> size;
    std::string floorPlanKey;
    if (dungeon)
    {
        floorPlanKey = map->id + (m_guide ? ":guide" : ":discovery");
        if (m_floorPlanKey != floorPlanKey)
        {
            m_floorPlanKey = floorPlanKey;
            m_floorPlan = {};
            if (bgfx::isValid(m_floorPlanTexture))
                bgfx::destroy(m_floorPlanTexture);
            m_floorPlanTexture = BGFX_INVALID_HANDLE;
            const auto path = MapAssetLoader::findAssetPath(assetFileSystem(), map->map->worldId, map->map->fileName);
            const auto bytes = path ? assetFileSystem().readBinaryFile(*path) : std::nullopt;
            const auto geometry = bytes ? IndoorMapDataLoader().loadFromBytes(*bytes) : std::nullopt;
            if (geometry)
            {
                const auto knowledge = m_knowledge.floorPlans.find(toLowerCopy(map->map->fileName));
                m_floorPlan = buildWorldFloorPlan(*geometry,
                    knowledge == m_knowledge.floorPlans.end() ? WorldFloorPlanKnowledge{} : knowledge->second, m_guide);
            }
            if (!m_floorPlan.pixelsBgra.empty())
                m_floorPlanTexture = bgfx::createTexture2D(uint16_t(m_floorPlan.width), uint16_t(m_floorPlan.height),
                    false, 1, bgfx::TextureFormat::BGRA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
                    bgfx::copy(m_floorPlan.pixelsBgra.data(), uint32_t(m_floorPlan.pixelsBgra.size())));
        }
        floorPlan = &m_floorPlan;
        if (floorPlan->pixelsBgra.empty())
        {
            textInRect(canvasRect(66, 185, 466, 115), m_guide
                ? "No dungeon geometry is available in this content package."
                : "No explored floor plan yet. Explore this dungeon to reveal its map.",
                "menu_arrus", 14, 0xffc6dfeau, true);
            return;
        }
        size = TextureSize{float(floorPlan->width), float(floorPlan->height)};
    }
    else
        size = textureSize(texture);
    if (!size)
    {
        textInRect(canvasRect(66, 185, 466, 115),
            "The region's map image is unavailable in this content package.",
            "menu_arrus", 14, 0xffc6dfeau, true);
        return;
    }
    if (pointerInside(viewport) && mouseWheelDelta() != 0)
        m_zoom = std::clamp(m_zoom + mouseWheelDelta() * 0.2f, 1.0f, 3.0f);
    const float fit = std::min(viewport.width / size->width, viewport.height / size->height);
    const float width = size->width * fit * m_zoom, height = size->height * fit * m_zoom;
    m_panX = std::clamp(m_panX, -std::max(0.0f, (width - viewport.width) / 2), std::max(0.0f, (width - viewport.width) / 2));
    m_panY = std::clamp(m_panY, -std::max(0.0f, (height - viewport.height) / 2), std::max(0.0f, (height - viewport.height) / 2));
    const Rect image{viewport.x + (viewport.width - width) / 2 + m_panX,
        viewport.y + (viewport.height - height) / 2 + m_panY, width, height};
    setDesignClip(viewport);
    if (floorPlan != nullptr)
        drawTextureHandle(m_floorPlanTexture, image);
    else
        drawTexture(texture, image);
    if (map != nullptr && !dungeon && !m_guide)
    {
        if (m_fogMap != map->id)
        {
            m_fogMap = map->id;
            if (bgfx::isValid(m_fogTexture))
                bgfx::destroy(m_fogTexture);
            const auto knowledge = m_knowledge.regionMaps.find(toLowerCopy(map->map->fileName));
            const auto fog = buildWorldRegionFog(knowledge == m_knowledge.regionMaps.end()
                ? WorldRegionMapKnowledge{} : knowledge->second, GameplayMinimapState{});
            m_fogTexture = bgfx::createTexture2D(WorldRegionFog::Size, WorldRegionFog::Size, false, 1,
                bgfx::TextureFormat::BGRA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
                    | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT,
                bgfx::copy(fog.pixelsBgra.data(), uint32_t(fog.pixelsBgra.size())));
        }
        drawTextureHandle(m_fogTexture, image);
    }
    bool markerHovered = map != nullptr && !dungeon ? drawLocationMarkers(*map, image, viewport) : false;
    if (map == nullptr)
    {
        for (const auto &region : m_world.records())
        {
            if (!region.position || !m_world.visible(region, m_knowledge, m_guide))
                continue;
            const float x = image.x + region.position->first * image.width;
            const float y = image.y + region.position->second * image.height;
            const Rect marker{x - 14 * designScale(), y - 14 * designScale(), 28 * designScale(), 28 * designScale()};
            markerHovered = markerHovered || pointerInside(marker);
            if (button("atlas-pin-" + region.id, marker, "", "button_square"))
            {
                select(region);
                setDesignClip(std::nullopt);
                return;
            }
            drawEllipseOutline({x - 4 * designScale(), y - 4 * designScale(), 8 * designScale(), 8 * designScale()},
                               2 * designScale(), 0xff8dc5e1u);
            if (pointerInside(marker) || focused("atlas-pin-" + region.id))
            {
                Rect caption{x - 80 * designScale(), y - 36 * designScale(), 160 * designScale(), 20 * designScale()};
                caption.x = std::clamp(caption.x, viewport.x, viewport.x + viewport.width - caption.width);
                caption.y = std::max(caption.y, viewport.y);
                drawSolidRect(caption, 0xed100e09u);
                textInRect(caption, region.name(), "menu_arrus", 12, 0xffc6dfeau, true);
            }
        }
    }
    if (map != nullptr)
        drawPartyArrow(*map, image, floorPlan);
    if (leftMouseJustPressed() && pointerInside(viewport) && !markerHovered)
    {
        m_dragging = true;
        m_dragX = mouseX(); m_dragY = mouseY();
    }
    if (m_dragging && leftMouseDown())
    {
        m_panX += mouseX() - m_dragX; m_panY += mouseY() - m_dragY;
        m_dragX = mouseX(); m_dragY = mouseY();
    }
    if (!leftMouseDown())
        m_dragging = false;
    setDesignClip(std::nullopt);
    if (button("atlas-zoom-out", canvasRect(490, 357, 30, 27), "-")) m_zoom = std::max(1.0f, m_zoom - 0.25f);
    if (button("atlas-zoom-in", canvasRect(527, 357, 30, 27), "+")) m_zoom = std::min(3.0f, m_zoom + 0.25f);
}

void AtlasScreen::drawPartyArrow(const WorldRecord &map, const Rect &image, const WorldFloorPlanRaster *floorPlan)
{
    if (!m_knowledge.partyPosition || toLowerCopy(map.map->fileName) != m_knowledge.partyPosition->mapFileName)
        return;
    const auto &party = *m_knowledge.partyPosition;
    std::pair<float, float> uv;
    if (floorPlan != nullptr)
        uv = floorPlan->worldToUv(party.x, party.y);
    else
    {
        const auto point = gameplayMinimapWorldToUv(GameplayMinimapState{}, party.x, party.y);
        uv = {point.x, point.y};
    }
    if (uv.first < 0 || uv.first > 1 || uv.second < 0 || uv.second > 1)
        return;
    const std::string arrow = "MAPDIR" + std::to_string(gameplayMinimapArrowIndex(party.yawRadians) + 1);
    if (const auto size = textureSize(arrow))
    {
        const float width = 20 * designScale(), height = width * size->height / std::max(1.0f, size->width);
        drawTexture(arrow, {image.x + uv.first * image.width - width / 2,
                            image.y + uv.second * image.height - height / 2, width, height});
    }
}

void AtlasScreen::drawList()
{
    const Rect viewport = canvasRect(588, 121, 235, 270);
    auto records = m_world.query(m_knowledge, m_guide, m_search, m_filter, m_map,
                                 m_filter == WorldRecordKind::Travel);
    if (!m_siteSelection.empty())
        std::erase_if(records, [this](const WorldRecord *pRecord) { return !m_siteSelection.contains(pRecord->id); });
    if (m_map.empty() && m_search.empty() && !m_filter)
        std::erase_if(records, [](const WorldRecord *record)
        {
            return !record->position || (record->kind != WorldRecordKind::Region && record->kind != WorldRecordKind::Dungeon);
        });
    scrollViewport(viewport, records.size() * 38 * designScale(), m_scroll, 38);
    setDesignClip(viewport);
    for (size_t i = 0; i < records.size(); ++i)
    {
        const Rect row{viewport.x, viewport.y + i * 38 * designScale() - m_scroll, viewport.width - 12 * designScale(), 34 * designScale()};
        const auto &record = *records[i];
        if (button("atlas-record-" + record.id, row, "", "button", true, m_selected == record.id))
        {
            select(record);
            break;
        }
        if (m_selected == record.id)
            outline(row, 0xff8dc5e1u);
        textInRect({row.x + 7 * designScale(), row.y + 2 * designScale(), row.width - 14 * designScale(), 18 * designScale()},
            record.name(), "menu_arrus", 12);
        const std::string subtitle = record.kind == WorldRecordKind::Travel
            ? "From " + record.map->name + " - " + std::to_string(record.walkingRoute
                ? record.walkingRoute->travelDays : record.route->travelDays) + " days"
            : std::string(WorldDatabase::kindName(record.kind)) + (record.house != nullptr ? " - " + record.map->name : "");
        textInRect({row.x + 7 * designScale(), row.y + 19 * designScale(), row.width - 14 * designScale(), 12 * designScale()},
            subtitle, "menu_lucida", 9, 0xffa9bbb7u);
    }
    revealFocus(viewport, m_scroll);
    setDesignClip(std::nullopt);
    if (records.empty())
        textInRect(viewport, m_guide ? "No matching records." : "No matching discoveries.\nVisit places and meet their residents to reveal them.",
                   "menu_arrus", 14, 0xffc6dfeau, true);
}

void AtlasScreen::drawDetails()
{
    const Rect details = canvasRect(25, 403, 798, 52);
    outline(details, 0xff597284u);
    const auto *record = m_world.find(m_selected);
    std::string text = m_guide ? "Guide Mode reveals the database without changing your discoveries."
        : "Only visited places and encountered residents are shown. Guide Mode reveals the reference database.";
    if (record != nullptr)
    {
        text = record->name();
        if (record->house != nullptr)
        {
            text += " | " + record->map->name + " | " + record->house->type;
            if (record->kind == WorldRecordKind::House)
                text += " | Hours " + std::to_string(record->house->openHour) + ":00 - " + std::to_string(record->house->closeHour) + ":00";
        }
        if (record->teacher != nullptr)
        {
            if (m_pParty != nullptr)
            {
                const auto evaluation = evaluateMasteryTeacherTopic(record->teacher->topicId, *m_pParty,
                    m_data.classSkillTable(), m_data.npcDialogTable(), &m_data.mergedTeacherTopicTable());
                if (evaluation)
                    text += " | " + std::to_string(evaluation->cost) + " gold\nSelected character: "
                        + evaluation->displayText;
            }
            else
            {
                if (record->teacher->requiredSkill != 0)
                    text += " | Skill " + std::to_string(record->teacher->requiredSkill);
                if (record->teacher->requiredGold != 0)
                    text += " | " + std::to_string(record->teacher->requiredGold) + " gold";
                text += "\nSelect a party in-game to check its training requirements.";
            }
        }
        if (record->route != nullptr)
        {
            text += " | " + std::to_string(record->route->travelDays) + " days | ";
            const char *days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
            for (size_t day = 0; day < 7; ++day)
                if (record->route->daysAvailable[day]) text += std::string(days[day]) + " ";
            if (record->route->requiredQBit != 0) text += "| Quest unlock required";
        }
        if (record->walkingRoute != nullptr)
        {
            text += " | Walking from " + record->map->name + " | "
                + std::to_string(record->walkingRoute->travelDays) + " days";
            if (!record->walkingRoute->requiredQuestBitsAny.empty()) text += " | Quest unlock required";
            if (record->walkingRoute->requiredOriginSurface == MapTransitionSurfaceRequirement::Water)
                text += " | Water access required";
        }
    }
    if (!m_locationError.empty())
        text += "\n" + m_locationError;
    textInRect(canvasRect(34, 407, 778, 43), text, "menu_arrus", 12);
}
} // namespace OpenYAMM::Game
