#include "game/ui/screens/AtlasScreen.h"
#include "game/data/GameDataRepository.h"
#include "game/data/GameDataLoader.h"
#include "game/gameplay/MasteryTeacherDialog.h"
#include "game/StringUtils.h"
#include "game/maps/MapAssetLoader.h"
#include "game/ui/GameplayMinimapTransform.h"
#include "game/events/EvtEnums.h"

#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <iostream>
#include <cmath>

namespace OpenYAMM::Game
{
namespace
{
std::string initialAtlasWorld(const Engine::AssetFileSystem &assets, const MapAssetInfo *pCurrentMap)
{
    const std::string world = pCurrentMap != nullptr ? pCurrentMap->map.worldId : assets.getActiveWorldId();
    return assets.exists("engine/world/atlas/" + world + ".yml") ? world : "mm7";
}
WorldNpcState atlasNpcState(const GameDataRepository &data, const WorldKnowledge &knowledge,
    bool guide, bool hasParty)
{
    return guide || !hasParty ? knowledge.currentNpcs
        : worldObservedNpcs(data.npcDialogTable(), data.mergedTeacherTopicTable(), knowledge.variables);
}
} // namespace

AtlasScreen::AtlasScreen(const Engine::AssetFileSystem &assets, const GameDataRepository &data,
    WorldKnowledge knowledge, std::function<void()> close, bool paused, GameAudioSystem *audio,
    const MapAssetInfo *pCurrentMap, const Party *pParty, NoteWriter writeNote)
    : MenuDesignScreen(assets, paused, audio), m_knowledge(std::move(knowledge)),
      m_presentation(loadWorldAtlasPresentation(assets, initialAtlasWorld(assets, pCurrentMap))),
      m_world(data.mapStats(), data.houseTable(), data.npcDialogTable(), data.mergedTeacherTopicTable(),
              m_presentation.worldId, m_presentation.maps, atlasNpcState(data, m_knowledge, false, pParty != nullptr)),
      m_data(data), m_pParty(pParty),
      m_writeNote(std::move(writeNote)), m_close(std::move(close)), m_paused(paused)
{
    loadDesign("menu/main_menu");
    m_world.indexMapNotes(m_knowledge);
    for (const std::string &file : assets.enumerate("engine/world/atlas"))
        if (std::filesystem::path(file).extension() == ".yml")
            m_availableWorlds.push_back(std::filesystem::path(file).stem().string());
    m_referenceKnowledge.recipes = m_knowledge.autonoteIds;
    for (const auto &[file, observations] : m_knowledge.monsterObservations)
        for (const WorldMonsterObservation &observation : observations)
            m_referenceKnowledge.monsters.insert(observation.monsterId);
    if (m_pParty != nullptr)
    {
        for (const JournalQuestEntry &quest : m_data.journalQuestTable().entries())
        {
            if (m_data.journalQuestTable().isVisible(quest, *m_pParty))
            {
                m_referenceKnowledge.activeQuests.insert(quest.qbitId);
                m_referenceKnowledge.quests.insert(quest.qbitId);
            }
            const auto known = m_knowledge.variables.find(JournalQuestTable::discoveryKey(quest.qbitId));
            if (known != m_knowledge.variables.end() && known->second > 0)
                m_referenceKnowledge.quests.insert(quest.qbitId);
        }
        m_referenceKnowledge.hasParty = true;
        m_referenceKnowledge.arcomageWinCount = m_pParty->arcomageWinCount();
        m_referenceKnowledge.arcomageLossCount = m_pParty->arcomageLossCount();
        for (size_t i = 0; i < m_referenceKnowledge.arenaWins.size(); ++i)
            m_referenceKnowledge.arenaWins[i] = m_pParty->eventVariableValue(uint16_t(EvtVariable::ArenaWinsPage) + i);
        for (const auto &[id, house] : m_data.houseTable().entries())
        {
            const std::string key = WorldDatabase::houseDiscoveryKey(id);
            if (m_knowledge.variables.contains(key) && m_knowledge.variables.at(key) > 0)
                m_referenceKnowledge.houses.insert(id);
            if (m_pParty->hasArcomageWinAt(id))
            {
                m_referenceKnowledge.arcomageWins.insert(id);
                // A saved first victory proves the tavern was visited, including in older saves.
                m_knowledge.variables[key] = 1;
            }
        }
        for (const ItemDefinition &item : m_data.itemTable().entries())
            if (m_pParty->hasEverOwnedItem(item.itemId) || m_pParty->hasItemAnywhere(item.itemId))
                m_referenceKnowledge.items.insert(item.itemId);
        for (size_t i = 0; i < m_pParty->memberCount(); ++i)
            if (const Character *pMember = m_pParty->member(i))
            {
                m_referenceKnowledge.classes.insert(canonicalClassName(pMember->className));
                for (const SpellEntry &spell : m_data.spellTable().entries())
                    if (pMember->knowsSpell(uint32_t(spell.id)))
                        m_referenceKnowledge.spells.insert(uint32_t(spell.id));
            }
        for (const RosterEntry *pMember : m_data.rosterTable().getEntriesSortedById())
            if (m_pParty->hasRosterMember(pMember->id))
                m_referenceKnowledge.roster.insert(pMember->id);
    }
    indexQuestLocations();
    if (pCurrentMap != nullptr && pCurrentMap->outdoorMapData && pCurrentMap->localEventProgram)
    {
        m_world.indexMapLocations(*pCurrentMap->outdoorMapData, *pCurrentMap->localEventProgram);
        m_world.indexMapMonsters(pCurrentMap->map, *pCurrentMap->outdoorMapData, m_data.monsterTable(),
            m_knowledge.monsterObservations[toLowerCopy(pCurrentMap->map.fileName)]);
        m_indexedMaps.insert(WorldDatabase::mapId(pCurrentMap->map));
    }
    else if (pCurrentMap != nullptr && pCurrentMap->indoorMapData && pCurrentMap->localEventProgram)
    {
        m_world.indexMapLocations(pCurrentMap->map, *pCurrentMap->indoorMapData, *pCurrentMap->localEventProgram);
        m_world.indexMapMonsters(pCurrentMap->map, *pCurrentMap->indoorMapData, m_data.monsterTable(),
            m_knowledge.monsterObservations[toLowerCopy(pCurrentMap->map.fileName)]);
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
    if (m_placingNote && event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)
    {
        m_placingNote = false;
        m_suppressEscape = true;
        return;
    }
    if (!m_searchEditing && !m_noteEditorOpen)
        return;
    std::string &text = m_noteEditorOpen ? m_noteDraft.text : m_search;
    if (event.type == SDL_EVENT_TEXT_INPUT)
    {
        const std::string input = event.text.text;
        if (text.size() + input.size() <= (m_noteEditorOpen ? 512u : 128u))
            text += input;
        m_scroll = 0;
    }
    if (event.type == SDL_EVENT_KEY_DOWN)
    {
        if (event.key.scancode == SDL_SCANCODE_BACKSPACE && !text.empty())
        {
            size_t start = text.size() - 1;
            while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xc0) == 0x80)
                --start;
            text.erase(start);
            m_scroll = 0;
        }
        if (event.key.scancode == SDL_SCANCODE_ESCAPE)
        {
            if (m_noteEditorOpen)
                closeNoteEditor();
            m_searchEditing = false;
            setTextEditing(false);
            m_suppressEscape = true;
        }
        else if (event.key.scancode == SDL_SCANCODE_RETURN && !m_noteEditorOpen)
        {
            m_searchEditing = false;
            setTextEditing(false);
        }
    }
}

void AtlasScreen::closeNoteEditor()
{
    m_noteEditorOpen = false;
    m_noteError.clear();
    setTextEditing(false);
}

void AtlasScreen::saveNote(bool remove)
{
    m_noteDraft.remove = remove;
    if (!m_writeNote || !m_writeNote(m_noteDraft, m_noteError))
        return;
    auto &notes = m_knowledge.notes[toLowerCopy(m_noteDraft.mapFileName)];
    std::erase_if(notes, [this](const WorldMapNote &note) { return note.value.id == m_noteDraft.id; });
    if (!remove)
        notes.push_back({{m_noteDraft.id, m_noteDraft.x, m_noteDraft.y,
                         m_noteDraft.mapFileName, m_noteDraft.text, true}, true});
    m_world.indexMapNotes(m_knowledge);
    m_selectedFeature = remove ? "" : m_map + "/note/" + std::to_string(m_noteDraft.id);
    m_featureView = true;
    m_filter.reset(); m_search.clear(); m_siteSelection.clear(); m_scroll = 0;
    closeNoteEditor();
}

void AtlasScreen::drawNoteEditor()
{
    textInRect(canvasRect(80, 87, 690, 35), "Map note", "fondamento", 23, 0xffbcddeeu);
    const WorldRecord *pMap = m_world.find(m_map);
    textInRect(canvasRect(80, 125, 690, 25), (pMap != nullptr ? pMap->name() : "Map") + " | "
        + std::to_string(m_noteDraft.x) + ", " + std::to_string(m_noteDraft.y), "menu_lucida", 11);
    const Rect field = canvasRect(80, 160, 690, 125);
    drawSolidRect(field, 0xff18160fu); outline(field, 0xff8dc5e1u);
    if (button("atlas-note-text", field, "", "text_field"))
        setTextEditing(true);
    setDesignClip(field);
    textInRect(canvasRect(88, 165, 674, 115), m_noteDraft.text + "_", "menu_arrus", 12,
               0xffc6dfeau, false, true);
    setDesignClip(std::nullopt);
    textInRect(canvasRect(80, 287, 690, 32), m_noteError.empty()
        ? "Your notes are shared with the Map Book. Escape cancels changes." : m_noteError,
        "menu_lucida", 11);
    if (button("atlas-note-save", canvasRect(80, 330, 155, 30), "Save note"))
        saveNote(false);
    if (button("atlas-note-cancel", canvasRect(260, 330, 155, 30), "Cancel"))
        closeNoteEditor();
    if (m_noteDraft.id != 0 && button("atlas-note-delete", canvasRect(615, 330, 155, 30), "Delete note"))
        saveNote(true);
}

void AtlasScreen::select(const WorldRecord &record)
{
    m_placingNote = false;
    m_selectedFeature.clear();
    m_selected = record.id;
    const std::string destination = WorldDatabase::mapId(*record.map);
    if (m_map != destination)
    {
        m_map = destination;
        m_zoom = 1;
        m_panX = m_panY = m_scroll = 0;
        m_siteSelection.clear();
    }
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

std::string AtlasScreen::regionTexture(const WorldRecord &record) const
{
    const std::string base = "worlds/" + record.map->worldId + "/icons/"
        + std::filesystem::path(record.map->fileName).stem().string();
    for (const char *pExtension : {".png", ".bmp", ".pcx"})
        if (assetFileSystem().exists(base + pExtension))
            return base + pExtension;
    return base;
}

void AtlasScreen::rebuildWorld()
{
    m_world = WorldDatabase(m_data.mapStats(), m_data.houseTable(), m_data.npcDialogTable(),
        m_data.mergedTeacherTopicTable(), m_presentation.worldId, m_presentation.maps,
        atlasNpcState(m_data, m_knowledge, m_guide, m_pParty != nullptr));
    m_world.indexMapNotes(m_knowledge);
    indexQuestLocations();
    m_indexedMaps.clear();
    m_floorPlanKey.clear();
}

void AtlasScreen::selectWorld(const std::string &worldId)
{
    m_placingNote = false;
    if (worldId == m_presentation.worldId)
        return;
    try
    {
        WorldAtlasPresentation presentation = loadWorldAtlasPresentation(assetFileSystem(), worldId);
        WorldDatabase world(m_data.mapStats(), m_data.houseTable(), m_data.npcDialogTable(),
                            m_data.mergedTeacherTopicTable(), worldId, presentation.maps,
                            atlasNpcState(m_data, m_knowledge, m_guide, m_pParty != nullptr));
        m_world = std::move(world);
        m_world.indexMapNotes(m_knowledge);
        m_selectedFeature.clear();
        m_referenceDirty = true;
        m_referenceSelected.clear();
        m_referenceScroll = m_referenceDetailScroll = 0;
        m_presentation = std::move(presentation);
        indexQuestLocations();
        m_trainerMastery = 0;
        m_indexedMaps.clear();
        m_siteSelection.clear();
        m_map.clear(); m_selected.clear(); m_search.clear();
        m_filter.reset();
        m_floorPlanKey.clear(); m_fogMap.clear(); m_locationError.clear();
        m_zoom = 1;
        m_panX = m_panY = m_scroll = 0;
        m_dragging = false;
    }
    catch (const std::exception &error)
    {
        m_locationError = "Could not open the selected world's atlas.";
        std::cerr << "Atlas catalog error: " << error.what() << '\n';
    }
}

void AtlasScreen::drawScreen(float)
{
    beginDesign("Atlas");
    drawSolidRect({0, 0, float(frameWidth()), float(frameHeight())}, 0xff100e09u);
    skin("frame", canvasRect(8, 8, 837, 464), true);
    if (m_noteEditorOpen)
    {
        drawNoteEditor();
        return;
    }
    textInRect(canvasRect(25, 15, 245, 30), "Interactive Atlas", "fondamento", 20, 0xffbcddeeu);
    for (size_t i = 0; i < m_availableWorlds.size(); ++i)
        if (button("atlas-catalog-" + m_availableWorlds[i], canvasRect(285 + i * 115, 17, 105, 27),
                   m_availableWorlds[i], "button", true, m_presentation.worldId == m_availableWorlds[i]))
        {
            selectWorld(m_availableWorlds[i]);
            return;
        }
    if (button("atlas-reference", canvasRect(205, 17, 70, 27), m_referenceOpen ? "Map" : "Guidebook"))
    {
        m_placingNote = false;
        m_referenceOpen = !m_referenceOpen;
        m_referenceDirty = true;
        m_search.clear();
        m_referenceScroll = m_referenceDetailScroll = 0;
        m_searchEditing = false;
        setTextEditing(false);
    }
    if (button("atlas-mode", canvasRect(526, 17, 177, 27), m_guide ? "Guide Mode: ON" : "Discovery Mode"))
    {
        m_guide = !m_guide;
        rebuildWorld();
        if (const WorldRecord *pSelected = m_world.find(m_selected);
            pSelected != nullptr && pSelected->npc != nullptr && m_world.visible(*pSelected, m_knowledge, m_guide))
            select(*pSelected);
        m_referenceDirty = true;
        m_referenceSelected.clear();
        m_referenceScroll = m_referenceDetailScroll = 0;
        if (const auto *selected = m_world.find(m_selected);
            selected == nullptr || !m_world.visible(*selected, m_knowledge, m_guide))
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
    if (m_referenceOpen)
    {
        drawReference();
        return;
    }
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
    for (int i = 0; i < 8; ++i)
    {
        const Rect rect = canvasRect(312 + i * 64, 53, 61, 28);
        if (button("atlas-filter-" + std::to_string(i), rect, i == 7 ? "Sites" : filters[i].first,
                   "button", true, i == 7 ? m_featureView : !m_featureView && m_filter == filters[i].second))
        {
            m_featureView = i == 7;
            m_filter = i == 7 ? std::nullopt : filters[i].second;
            m_selectedFeature.clear();
            m_trainerMastery = 0;
            m_scroll = 0;
            m_siteSelection.clear();
        }
        if (i == 7 ? m_featureView : !m_featureView && m_filter == filters[i].second)
            outline(rect, 0xff8dc5e1u);
    }
    if (button("atlas-world", canvasRect(25, 89, 132, 25), m_presentation.name, "button", true, m_map.empty()))
    {
        m_map.clear();
        m_selected.clear();
        m_selectedFeature.clear();
        m_siteSelection.clear();
        m_zoom = 1;
        m_panX = m_panY = m_scroll = 0;
    }
    if (const auto *map = m_world.find(m_map))
    {
        textInRect(canvasRect(168, 89, 403, 25), map->name(), "fondamento", 14);
        if (m_featureView)
            textInRect(canvasRect(375, 94, 195, 16),
                       "C Chests W Water S Shrines O Obelisks P Passages M Monsters N Notes",
                       "menu_lucida", 7, 0xffc6dfeau);
        else if (map->kind == WorldRecordKind::Region)
            textInRect(canvasRect(375, 94, 195, 16), "B Buildings   T Trainers   D Dungeons",
                       "menu_lucida", 8, 0xffc6dfeau);
    }
    indexKnownRegions();
    const std::string listHeading = !m_siteSelection.empty() ? "Selected markers (clear)"
        : m_filter == WorldRecordKind::Travel ? (m_guide ? "All recorded routes" : "Known routes")
        : m_map.empty() ? (m_guide ? "All recorded locations" : "Your party's discoveries")
        : (m_guide ? "Locations and residents here" : "Discovered here");
    const WorldRecord *pNoteMap = m_world.find(m_map);
    const bool canAddNote = m_writeNote && pNoteMap != nullptr
        && m_knowledge.visitedMaps.contains(toLowerCopy(pNoteMap->map->fileName));
    if (m_siteSelection.empty())
        textInRect(canvasRect(589, 89, canAddNote ? 115 : 233, 25), listHeading, "menu_lucida", 11);
    else if (button("atlas-clear-sites", canvasRect(589, 89, canAddNote ? 115 : 233, 25), listHeading))
    {
        m_siteSelection.clear();
        m_scroll = 0;
    }
    if (canAddNote && button("atlas-new-note", canvasRect(711, 89, 110, 25),
                             m_placingNote ? "Cancel pin" : "Add note"))
    {
        m_placingNote = !m_placingNote;
        m_searchEditing = false;
        setTextEditing(false);
    }
    if (m_placingNote)
        textInRect(canvasRect(34, 407, 778, 43), "Click or tap the map to place your note. Escape cancels.",
                   "menu_arrus", 12);
    drawMap();
    if (m_featureView)
        drawFeatureList();
    else
        drawList();
    if (!m_placingNote)
        drawDetails();
}

void AtlasScreen::indexKnownRegions()
{
    for (const WorldRecord &record : m_world.records())
    {
        if ((record.kind != WorldRecordKind::Region
             && (record.kind != WorldRecordKind::Dungeon || record.id != m_map))
            || !m_world.visible(record, m_knowledge, m_guide)
            || !m_indexedMaps.insert(record.id).second)
            continue;
        try
        {
            auto map = MapAssetLoader().load(assetFileSystem(), *record.map,
                m_data.monsterTable(), m_data.objectTable(), MapLoadPurpose::HeadlessGameplay);
            std::string error;
            if (!map || !GameDataLoader::loadEventPrograms(assetFileSystem(), *map, error)
                || !map->localEventProgram)
                throw std::runtime_error(error.empty() ? "Missing geometry or event metadata" : error);
            if (map->outdoorMapData)
            {
                m_world.indexMapLocations(*map->outdoorMapData, *map->localEventProgram);
                m_world.indexMapMonsters(map->map, *map->outdoorMapData, m_data.monsterTable(),
                    m_knowledge.monsterObservations[toLowerCopy(map->map.fileName)]);
            }
            else if (map->indoorMapData)
            {
                m_world.indexMapLocations(map->map, *map->indoorMapData, *map->localEventProgram);
                m_world.indexMapMonsters(map->map, *map->indoorMapData, m_data.monsterTable(),
                    m_knowledge.monsterObservations[toLowerCopy(map->map.fileName)]);
            }
            else
                throw std::runtime_error("Missing map geometry");
        }
        catch (const std::exception &error)
        {
            m_locationError = "Some map markers are unavailable in this content package.";
            std::cerr << "World locations could not be indexed for " << record.map->fileName
                      << ": " << error.what() << '\n';
        }
    }
}

bool AtlasScreen::drawLocationMarkers(const WorldRecord &map, const Rect &image, const Rect &viewport,
    const WorldFloorPlanRaster *pFloorPlan)
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
            || (m_trainerMastery != 0 && pRecord->teacher != nullptr && pRecord->teacher->mastery != m_trainerMastery)
            || (!m_search.empty()
                && toLowerCopy(pRecord->searchText()).find(toLowerCopy(m_search)) == std::string::npos); });
        if (records.empty())
            continue;
        const auto point = pFloorPlan != nullptr ? pFloorPlan->worldToUv(location.x, location.y)
            : [&]()
            {
                const auto uv = gameplayMinimapWorldToUv({}, location.x, location.y);
                return std::pair{uv.x, uv.y};
            }();
        const float x = image.x + point.first * image.width, y = image.y + point.second * image.height;
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
        const Rect marker{pin.x - 6 * designScale(), pin.y - 6 * designScale(),
                          12 * designScale(), 12 * designScale()};
        const Rect hitArea{pin.x - 12 * designScale(), pin.y - 12 * designScale(),
                           24 * designScale(), 24 * designScale()};
        const std::string buttonId = "atlas-site-" + pin.id;
        hovered = hovered || pointerInside(hitArea);
        if (button(buttonId, hitArea, "", "button_square", true, false, &marker))
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
        textInRect(marker, pin.sites > 1 ? std::to_string(pin.sites) : label, "menu_lucida", 8,
                   0xffc6dfeau, true);
        outline(marker, trainer != pin.records.end() ? 0xff96c798u : dungeon ? 0xff8dc5e1u : 0xff8dc8dfu);
        if (pointerInside(hitArea) || focused(buttonId))
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
    const bool schematic = map == nullptr && m_presentation.overviewTexture.empty();
    const std::string texture = map == nullptr ? m_presentation.overviewTexture : regionTexture(*map);
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
    else if (schematic)
        size = TextureSize{1000, 600};
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
    else if (!schematic)
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
    bool markerHovered = false;
    if (map != nullptr && !m_placingNote)
        markerHovered = m_featureView ? drawFeatureMarkers(*map, image, viewport, floorPlan)
            : drawLocationMarkers(*map, image, viewport, floorPlan);
    if (map == nullptr)
    {
        for (const auto &region : m_world.records())
        {
            if (!region.position || !m_world.visible(region, m_knowledge, m_guide))
                continue;
            const float x = image.x + region.position->first * image.width;
            const float y = image.y + region.position->second * image.height;
            float tileHeight = 50;
            if (schematic)
            {
                for (const WorldRecord &other : m_world.records())
                    if (other.position && other.id != region.id && m_world.visible(other, m_knowledge, m_guide)
                        && std::abs(other.position->first - region.position->first) < 0.08f)
                    {
                        const float distance = std::abs(other.position->second - region.position->second)
                            * image.height / designScale();
                        tileHeight = std::min(tileHeight, std::max(16.0f, distance - 18));
                    }
                const Rect tile{x - 33 * designScale(), y - tileHeight / 2 * designScale(),
                                66 * designScale(), tileHeight * designScale()};
                if (m_guide && region.kind == WorldRecordKind::Region)
                {
                    const std::string regionalTexture = regionTexture(region);
                    if (textureSize(regionalTexture))
                        drawTexture(regionalTexture, tile);
                }
                outline(tile, 0xff597284u);
                textInRect({tile.x - 15 * designScale(), tile.y + tile.height,
                            tile.width + 30 * designScale(), 16 * designScale()}, region.name(),
                           "menu_lucida", 8, 0xffc6dfeau, true);
            }
            const Rect marker{x - 5 * designScale(), y - 5 * designScale(), 10 * designScale(), 10 * designScale()};
            const Rect hitArea{x - (schematic ? 33 : 14) * designScale(),
                               y - (schematic ? tileHeight / 2 : 14) * designScale(),
                               (schematic ? 66 : 28) * designScale(), (schematic ? tileHeight : 28) * designScale()};
            markerHovered = markerHovered || pointerInside(hitArea);
            if (button("atlas-pin-" + region.id, hitArea, "", "button_square", true, false, &marker))
            {
                select(region);
                setDesignClip(std::nullopt);
                return;
            }
            drawEllipseOutline({x - 2 * designScale(), y - 2 * designScale(), 4 * designScale(), 4 * designScale()},
                               designScale(), 0xff8dc5e1u);
            if (pointerInside(hitArea) || focused("atlas-pin-" + region.id))
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
    if (m_placingNote && map != nullptr && leftMouseJustPressed()
        && pointerInside(viewport) && pointerInside(image)
        && !pointerInside(canvasRect(490, 357, 67, 27)))
    {
        const float u = (mouseX() - image.x) / image.width, v = (mouseY() - image.y) / image.height;
        const auto point = floorPlan != nullptr ? floorPlan->uvToWorld(u, v) : [&]()
        {
            const GameplayMinimapPoint world = gameplayMinimapUvToWorld({}, u, v);
            return std::pair{world.x, world.y};
        }();
        m_noteDraft = {map->map->fileName, 0, int32_t(std::lround(point.first)),
                       int32_t(std::lround(point.second)), {}, false};
        m_noteError.clear(); m_placingNote = false; m_noteEditorOpen = true; m_dragging = false;
        setTextEditing(true);
    }
    else if (!m_noteEditorOpen && leftMouseJustPressed() && pointerInside(viewport) && !markerHovered)
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
    const float zoomX = schematic ? 34 : 490;
    if (button("atlas-zoom-out", canvasRect(zoomX, 357, 30, 27), "-")) m_zoom = std::max(1.0f, m_zoom - 0.25f);
    if (button("atlas-zoom-in", canvasRect(zoomX + 37, 357, 30, 27), "+")) m_zoom = std::min(3.0f, m_zoom + 0.25f);
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
    if (m_trainerMastery != 0)
        std::erase_if(records, [this](const WorldRecord *pRecord)
        { return pRecord->teacher != nullptr && pRecord->teacher->mastery != m_trainerMastery; });
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

bool AtlasScreen::drawFeatureMarkers(const WorldRecord &map, const Rect &image, const Rect &viewport,
                                    const WorldFloorPlanRaster *pFloorPlan)
{
    struct Pin
    {
        float x = 0, y = 0;
        std::vector<const WorldMapFeature *> features;
    };
    std::vector<Pin> pins;
    for (const WorldMapFeature *pFeature : m_world.queryFeatures(m_knowledge, m_guide, m_search, map.id))
    {
        const auto point = pFloorPlan != nullptr ? pFloorPlan->worldToUv(pFeature->site.x, pFeature->site.y)
            : [&]()
            {
                const auto uv = gameplayMinimapWorldToUv({}, pFeature->site.x, pFeature->site.y);
                return std::pair{uv.x, uv.y};
            }();
        const float x = image.x + point.first * image.width, y = image.y + point.second * image.height;
        if (x < viewport.x || x > viewport.x + viewport.width || y < viewport.y || y > viewport.y + viewport.height)
            continue;
        auto pin = std::find_if(pins.begin(), pins.end(), [&](const Pin &other)
        { return std::hypot(x - other.x, y - other.y) < 22 * designScale(); });
        if (pin == pins.end())
        {
            pins.push_back({x, y, {}});
            pin = std::prev(pins.end());
        }
        pin->features.push_back(pFeature);
    }
    bool hovered = false;
    for (const Pin &pin : pins)
    {
        const WorldMapFeature &feature = *pin.features.front();
        const Rect marker{pin.x - 5 * designScale(), pin.y - 5 * designScale(),
                          10 * designScale(), 10 * designScale()};
        const Rect hit{pin.x - 12 * designScale(), pin.y - 12 * designScale(),
                       24 * designScale(), 24 * designScale()};
        const std::string id = "atlas-feature-pin-" + feature.id;
        const bool selected = std::any_of(pin.features.begin(), pin.features.end(),
            [this](const WorldMapFeature *pFeature) { return pFeature->id == m_selectedFeature; });
        hovered = hovered || pointerInside(hit);
        if (button(id, hit, "", "button_square", true, selected, &marker))
        {
            m_siteSelection.clear();
            for (const WorldMapFeature *pFeature : pin.features)
                m_siteSelection.insert(pFeature->id);
            m_selectedFeature = feature.id;
            m_scroll = 0;
        }
        const char *labels[] = {"C", "W", "S", "O", "P", "M", "N"};
        textInRect(marker, pin.features.size() > 1 ? std::to_string(pin.features.size()) : labels[size_t(feature.kind)],
                   "menu_lucida", 7, 0xffc6dfeau, true);
        outline(marker, selected ? 0xff99d3eeu : 0xff8dc5e1u);
        if (pointerInside(hit) || focused(id))
        {
            Rect caption{pin.x - 100 * designScale(), pin.y - 28 * designScale(),
                         200 * designScale(), 19 * designScale()};
            caption.x = std::clamp(caption.x, viewport.x, viewport.x + viewport.width - caption.width);
            caption.y = std::max(caption.y, viewport.y);
            drawSolidRect(caption, 0xed100e09u);
            textInRect(caption, pin.features.size() > 1 ? std::to_string(pin.features.size()) + " sites"
                : feature.title, "menu_arrus", 11, 0xffc6dfeau, true);
        }
    }
    return hovered;
}

void AtlasScreen::drawFeatureList()
{
    const Rect viewport = canvasRect(588, 121, 235, 270);
    auto features = m_world.queryFeatures(m_knowledge, m_guide, m_search, m_map);
    if (!m_siteSelection.empty())
        std::erase_if(features, [this](const WorldMapFeature *pFeature)
        { return !m_siteSelection.contains(pFeature->id); });
    scrollViewport(viewport, features.size() * 38 * designScale(), m_scroll, 38);
    setDesignClip(viewport);
    for (size_t i = 0; i < features.size(); ++i)
    {
        const Rect row{viewport.x, viewport.y + i * 38 * designScale() - m_scroll,
                       viewport.width - 12 * designScale(), 34 * designScale()};
        if (row.y + row.height < viewport.y || row.y > viewport.y + viewport.height)
            continue;
        const WorldMapFeature &feature = *features[i];
        if (button("atlas-feature-" + feature.id, row, "", "button", true, m_selectedFeature == feature.id))
        {
            m_selectedFeature = feature.id;
            m_map = feature.mapId;
            m_panX = m_panY = 0; m_zoom = 1;
        }
        textInRect({row.x + 7 * designScale(), row.y, row.width - 14 * designScale(), 18 * designScale()},
                   feature.title, "menu_arrus", 12);
        const WorldRecord *pMap = m_world.find(feature.mapId);
        textInRect({row.x + 7 * designScale(), row.y + 19 * designScale(),
                    row.width - 14 * designScale(), 12 * designScale()},
                   std::string(worldMapFeatureKindName(feature.kind)) + " - " + pMap->name(),
                   "menu_lucida", 9, 0xffa9bbb7u);
    }
    revealFocus(viewport, m_scroll);
    setDesignClip(std::nullopt);
    if (features.empty())
        textInRect(viewport, m_guide ? "No matching sites in the indexed maps. Select a dungeon to index its sites."
            : "No discovered sites here. Explore the region or interact with objects in the dungeon.",
            "menu_arrus", 13, 0xffc6dfeau, true);
}

void AtlasScreen::indexQuestLocations()
{
    WorldGuideSource source;
    source.pQuests = &m_data.journalQuestTable();
    source.pMaps = &m_data.mapStats();
    source.pHouses = &m_data.houseTable();
    source.worldId = m_presentation.worldId;
    for (const WorldGuideEntry &entry : buildWorldGuide(source, WorldGuideSection::Quests,
        m_referenceKnowledge, false))
    {
        for (const WorldGuideLink &link : entry.links)
        {
            if (link.kind != WorldGuideLink::Kind::Location || !link.informed) continue;
            if (const WorldRecord *pLocation = m_world.find(link.target))
            {
                m_knowledge.informedRecords.insert(pLocation->id);
                m_knowledge.informedRecords.insert(WorldDatabase::mapId(*pLocation->map));
            }
        }
    }
}

void AtlasScreen::rebuildReference()
{
    const WorldGuideSource source{&m_data.itemTable(), &m_data.spellTable(), &m_data.classMultiplierTable(),
        &m_data.classSkillTable(), &m_data.mergedPotionSettingTable(), &m_data.mergedReagentSettingTable(),
        &m_data.potionMixingTable(), &m_data.potionNoteTable(), &m_data.rosterTable(),
        &m_data.houseTable(), &m_data.mapStats(), m_presentation.worldId, &m_data.monsterTable(),
        &m_data.journalQuestTable(), &m_data.npcDialogTable(), &m_world, &m_knowledge};
    m_referenceEntries = buildWorldGuide(source, m_referenceSection, m_referenceKnowledge, m_guide);
    m_referenceDirty = false;
}

void AtlasScreen::drawReference()
{
    const std::pair<const char *, WorldGuideSection> sections[] = {
        {"Items", WorldGuideSection::Items}, {"Spells", WorldGuideSection::Spells},
        {"Classes", WorldGuideSection::Classes}, {"Alchemy", WorldGuideSection::Alchemy},
        {"Companions", WorldGuideSection::Companions}, {"Progress", WorldGuideSection::Progress},
        {"Bestiary", WorldGuideSection::Monsters}, {"Quests", WorldGuideSection::Quests}};
    for (size_t i = 0; i < std::size(sections); ++i)
        if (button("atlas-ref-section-" + std::to_string(i), canvasRect(25 + i * 100, 53, 94, 28),
                   sections[i].first, "button", true, m_referenceSection == sections[i].second))
        {
            m_referenceSection = sections[i].second;
            m_referenceSelected.clear(); m_search.clear();
            m_referenceDirty = true;
            m_referenceScroll = m_referenceDetailScroll = 0;
        }
    if (m_referenceDirty)
        rebuildReference();
    const Rect search = canvasRect(25, 89, 245, 25);
    if (m_searchEditing && (keyPressed(SDL_SCANCODE_TAB) || (leftMouseJustPressed() && !pointerInside(search))))
    {
        m_searchEditing = false;
        setTextEditing(false);
    }
    if (button("atlas-ref-search", search, "", "text_field"))
    {
        m_searchEditing = true;
        setTextEditing(true);
    }
    textInRect(canvasRect(32, 91, 229, 20), m_search.empty() ? "Search this section..." : m_search,
               "menu_lucida", 11);
    textInRect(canvasRect(285, 89, 535, 25), m_presentation.name + " - active game data", "fondamento", 14);
    const Rect list = canvasRect(25, 121, 245, 270), details = canvasRect(285, 121, 535, 270);
    drawSolidRect(list, 0xff18160fu); outline(list, 0xff597284u);
    drawSolidRect(details, 0xff18160fu); outline(details, 0xff597284u);
    std::vector<const WorldGuideEntry *> entries;
    for (const WorldGuideEntry &entry : m_referenceEntries)
        if (m_search.empty() || toLowerCopy(entry.title + " " + entry.details).find(toLowerCopy(m_search))
            != std::string::npos)
            entries.push_back(&entry);
    if (!m_search.empty())
        std::stable_sort(entries.begin(), entries.end(), [this](const WorldGuideEntry *a, const WorldGuideEntry *b)
        {
            const std::string query = toLowerCopy(m_search);
            return (toLowerCopy(a->title).find(query) != std::string::npos)
                > (toLowerCopy(b->title).find(query) != std::string::npos);
        });
    const float rowHeight = 33 * designScale();
    scrollViewport(list, entries.size() * rowHeight, m_referenceScroll);
    setDesignClip(list);
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const Rect row{list.x + 3 * designScale(), list.y + i * rowHeight - m_referenceScroll,
                       list.width - 7 * designScale(), rowHeight - 2 * designScale()};
        if (row.y + row.height < list.y || row.y > list.y + list.height)
            continue;
        const WorldGuideEntry &entry = *entries[i];
        if (button("atlas-ref-entry-" + entry.id, row, "", "button", true, m_referenceSelected == entry.id))
        {
            m_referenceSelected = entry.id;
            m_referenceDetailScroll = m_referenceContentHeight = 0;
        }
        textInRect({row.x + 6 * designScale(), row.y, row.width - 12 * designScale(), row.height},
                   entry.title, "menu_arrus", 12);
        if (m_referenceSelected == entry.id)
            outline(row, 0xff8dc5e1u);
    }
    setDesignClip(std::nullopt);
    const auto selected = std::find_if(entries.begin(), entries.end(),
        [this](const WorldGuideEntry *pEntry) { return pEntry->id == m_referenceSelected; });
    if (selected != entries.end())
    {
        const WorldGuideEntry &entry = **selected;
        scrollViewport(details, m_referenceContentHeight, m_referenceDetailScroll);
        setDesignClip(details);
        const Rect text{details.x + 8 * designScale(), details.y + 8 * designScale() - m_referenceDetailScroll,
                        details.width - 24 * designScale(), 50000 * designScale()};
        float height = 0;
        textInRect(text, entry.details, "menu_arrus", 12, 0xffc6dfeau, false, true, &height);
        float y = text.y + height + 12 * designScale();
        for (size_t i = 0; i < entry.links.size(); ++i)
        {
            const WorldGuideLink &link = entry.links[i];
            const Rect row{text.x, y, text.width, 26 * designScale()};
            if (row.y + row.height >= details.y && row.y <= details.y + details.height
                && button("atlas-ref-link-" + std::to_string(i), row, link.label))
            {
                if (link.kind == WorldGuideLink::Kind::Trainer)
                {
                    m_referenceOpen = false;
                    m_featureView = false;
                    m_map.clear(); m_siteSelection.clear(); m_selected.clear();
                    m_filter = WorldRecordKind::Trainer;
                    m_trainerMastery = link.mastery;
                    m_search = link.target;
                    m_scroll = 0;
                }
                else if (link.kind == WorldGuideLink::Kind::Location)
                {
                    const WorldRecord *pLocation = m_world.find(link.target);
                    if (pLocation != nullptr && m_world.visible(*pLocation, m_knowledge, m_guide))
                    {
                        m_referenceOpen = false;
                        m_featureView = false;
                        m_search.clear(); m_filter.reset(); m_trainerMastery = 0;
                        m_siteSelection.clear();
                        select(*pLocation);
                    }
                }
                else
                {
                    m_referenceSection = link.kind == WorldGuideLink::Kind::Item
                        ? WorldGuideSection::Items : WorldGuideSection::Alchemy;
                    m_referenceSelected = link.target;
                    m_referenceDirty = true;
                    m_search.clear();
                    m_referenceScroll = m_referenceDetailScroll = m_referenceContentHeight = 0;
                }
                setDesignClip(std::nullopt);
                return;
            }
            y += 29 * designScale();
        }
        m_referenceContentHeight = height + (entry.links.size() * 29 + 24) * designScale();
        setDesignClip(std::nullopt);
    }
    else
        textInRect(details, entries.empty() ? (m_guide ? "No records in this section."
            : "Nothing known yet. Guide Mode opens the full reference catalog.")
            : "Select a record to inspect its details.", "menu_arrus", 13, 0xffc6dfeau, true);
    textInRect(canvasRect(34, 407, 778, 43), m_guide
        ? "Reference definitions follow the loaded game tables, including enabled mods."
        : "Discovery shows encountered items, learned spells, current classes, known recipes, "
          "recruited companions, observed monsters and recorded quests.",
        "menu_arrus", 11);
}

void AtlasScreen::drawDetails()
{
    const Rect details = canvasRect(25, 403, 798, 52);
    outline(details, 0xff597284u);
    if (m_featureView)
    {
        const auto features = m_world.queryFeatures(m_knowledge, m_guide, m_search, m_map);
        const auto selected = std::find_if(features.begin(), features.end(), [this](const WorldMapFeature *pFeature)
        { return pFeature->id == m_selectedFeature
            && (m_siteSelection.empty() || m_siteSelection.contains(pFeature->id)); });
        std::string text = "Select a site to inspect it. "
            "C Chests, W Wells/Fountains, S Shrines, O Obelisks, P Passages, M Monsters, N Notes.";
        const WorldRecord *pDestination = nullptr;
        int16_t creatureId = 0;
        const WorldMapFeature *pNote = nullptr;
        if (selected != features.end())
        {
            const WorldMapFeature &feature = **selected;
            text = feature.title + " | " + worldMapFeatureKindName(feature.kind);
            if (feature.kind == WorldMapFeatureKind::Connection)
            {
                for (const WorldRecord &record : m_world.records())
                    if ((record.kind == WorldRecordKind::Region || record.kind == WorldRecordKind::Dungeon)
                        && toLowerCopy(record.map->fileName) == toLowerCopy(feature.targetMap)
                        && m_world.visible(record, m_knowledge, m_guide))
                        pDestination = &record;
                text += feature.targetMap.empty() ? "\nDestination not mapped"
                    : pDestination != nullptr ? "\nDeclared connection: " + pDestination->name()
                    : m_guide ? "\nDestination outside this catalog" : "\nDestination undiscovered";
            }
            else if (feature.kind == WorldMapFeatureKind::Chest)
                text += "\nContainers at this interaction: " + std::to_string(feature.chestIds.size());
            else if (feature.kind == WorldMapFeatureKind::Note)
            {
                text += feature.userNote ? "\nYour note" : "\nRecorded by the party";
                if (feature.userNote && m_writeNote)
                    pNote = &feature;
            }
            else if (feature.monster)
            {
                const WorldMonsterFeature &monster = *feature.monster;
                if (monster.observed)
                    text += std::string("\nLast observed: ") + (monster.dead ? "defeated" : "alive")
                        + " | Day " + std::to_string(monster.gameMinutes / (24 * 60) + 1);
                else
                    text += "\nAuthored group: " + std::to_string(monster.minCount) + " - "
                        + std::to_string(monster.maxCount) + " creatures. Not a live population count.";
                if (!monster.definitions.empty())
                    creatureId = monster.definitions.front();
            }
        }
        if (!m_locationError.empty())
            text += "\n" + m_locationError;
        textInRect(canvasRect(34, 407, pDestination != nullptr || creatureId > 0 || pNote != nullptr ? 625 : 778, 43),
                   text, "menu_arrus", 11);
        if (pNote != nullptr && button("atlas-edit-note", canvasRect(669, 416, 145, 28), "Edit note"))
        {
            const WorldRecord *pMap = m_world.find(pNote->mapId);
            m_noteDraft = {pMap->map->fileName, pNote->noteId, int32_t(pNote->site.x), int32_t(pNote->site.y),
                           pNote->title, false};
            m_noteError.clear(); m_noteEditorOpen = true;
            m_searchEditing = false;
            setTextEditing(true);
        }
        if (creatureId > 0 && button("atlas-feature-creature", canvasRect(669, 416, 145, 28), "Creature details"))
        {
            m_referenceOpen = true;
            m_referenceSection = WorldGuideSection::Monsters;
            m_referenceSelected = std::to_string(creatureId);
            m_referenceDirty = true;
            m_search.clear();
            m_referenceScroll = m_referenceDetailScroll = m_referenceContentHeight = 0;
        }
        if (pDestination != nullptr && button("atlas-feature-destination", canvasRect(669, 416, 145, 28),
                                             "Open connected map"))
        {
            select(*pDestination);
            m_siteSelection.clear();
        }
        return;
    }
    const auto *record = m_world.find(m_selected);
    std::string text = m_guide ? "Guide Mode reveals the database without changing your discoveries."
        : "Known places and encountered residents are shown. Quest information does not reveal unexplored terrain.";
    if (record != nullptr)
    {
        text = record->name();
        if (record->lastObservedNpc)
            text += " | Last encountered here";
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
