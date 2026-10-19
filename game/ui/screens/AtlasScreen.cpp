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
std::string atlasTravelDays(int64_t days)
{
    return std::to_string(days) + (days == 1 ? " day" : " days");
}
WorldTransportState atlasTransportState(const GameDataRepository &data, const WorldKnowledge &knowledge,
    bool guide, bool hasParty)
{
    return guide || !hasParty ? knowledge.currentTransport
        : worldObservedTransport(data.houseTable(), data.mapStats(), knowledge.variables);
}
} // namespace

AtlasScreen::AtlasScreen(const Engine::AssetFileSystem &assets, const GameDataRepository &data,
    WorldKnowledge knowledge, std::function<void()> close, bool paused, GameAudioSystem *audio,
    const MapAssetInfo *pCurrentMap, const Party *pParty, NoteWriter writeNote)
    : MenuDesignScreen(assets, paused, audio), m_knowledge(std::move(knowledge)),
      m_presentation(loadWorldAtlasPresentation(assets, initialAtlasWorld(assets, pCurrentMap))),
      m_world(data.mapStats(), data.houseTable(), data.npcDialogTable(), data.mergedTeacherTopicTable(),
              m_presentation.worldId, m_presentation.maps, atlasNpcState(data, m_knowledge, false, pParty != nullptr),
              atlasTransportState(data, m_knowledge, false, pParty != nullptr)),
      m_data(data), m_pParty(pParty),
      m_writeNote(std::move(writeNote)), m_close(std::move(close)), m_paused(paused)
{
    loadDesign("menu/main_menu");
    if (!m_world.transportIssues().empty())
    {
        m_locationError = "Some saved transport offers are unavailable.";
        for (const std::string &issue : m_world.transportIssues()) std::cerr << issue << '\n';
    }
    m_world.indexMapNotes(m_knowledge);
    m_world.indexObservedItemSources(m_knowledge, m_data.itemTable());
    for (const std::string &file : assets.enumerate("engine/world/atlas"))
        if (std::filesystem::path(file).extension() == ".yml")
            m_availableWorlds.push_back(std::filesystem::path(file).stem().string());
    m_referenceKnowledge.recipes = m_knowledge.autonoteIds;
    for (const auto &[file, observations] : m_knowledge.itemObservations)
        for (const WorldItemSource &observation : observations)
            m_referenceKnowledge.items.insert(observation.itemId);
    for (const auto &[file, observations] : m_knowledge.monsterObservations)
        for (const WorldMonsterObservation &observation : observations)
            m_referenceKnowledge.monsters.insert(observation.monsterId);
    if (m_pParty != nullptr)
    {
        const MapBoundaryEdge edges[] = {MapBoundaryEdge::North, MapBoundaryEdge::South,
            MapBoundaryEdge::East, MapBoundaryEdge::West};
        for (const MapStatsEntry &map : m_data.mapStats().getEntries())
            for (MapBoundaryEdge edge : edges)
                if (const auto &route = *map.edgeTransition(edge); route)
                    for (uint32_t bit : route->requiredQuestBitsAny)
                        if (m_pParty->hasQuestBit(bit)) m_knowledge.questBits.insert(bit);
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
            for (const JournalQuestLocation &location : quest.locations)
            {
                const std::string key = JournalQuestTable::locationDiscoveryKey(quest.qbitId, location);
                const auto clue = m_knowledge.variables.find(key);
                if (clue != m_knowledge.variables.end() && clue->second > 0)
                    m_referenceKnowledge.questLocations.insert(key);
                for (const std::vector<uint32_t> *pBits : {&location.requiredQBitIds, &location.forbiddenQBitIds})
                    for (uint32_t bit : *pBits)
                        if (m_pParty->hasQuestBit(bit)) m_referenceKnowledge.questBits.insert(bit);
            }
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
                m_referenceKnowledge.races.insert(pMember->raceId);
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
    m_world.indexObservedItemSources(m_knowledge, m_data.itemTable());
    m_selectedFeature = remove ? "" : m_map + "/note/" + std::to_string(m_noteDraft.id);
    m_floorPlanKey.clear();
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
    if (record.kind == WorldRecordKind::Travel && m_map.empty())
        return;
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
        atlasNpcState(m_data, m_knowledge, m_guide, m_pParty != nullptr),
        atlasTransportState(m_data, m_knowledge, m_guide, m_pParty != nullptr));
    m_world.indexMapNotes(m_knowledge);
    m_world.indexObservedItemSources(m_knowledge, m_data.itemTable());
    indexQuestLocations();
    m_itemSourceMaps.clear(); m_itemSourceScanIndex = 0;
    m_itemSourceScanActive = m_itemSourcesComplete = false;
    m_itemSourceScanFailures = 0;
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
                            atlasNpcState(m_data, m_knowledge, m_guide, m_pParty != nullptr),
                            atlasTransportState(m_data, m_knowledge, m_guide, m_pParty != nullptr));
        m_world = std::move(world);
        m_world.indexMapNotes(m_knowledge);
        m_world.indexObservedItemSources(m_knowledge, m_data.itemTable());
        m_selectedFeature.clear();
        m_referenceDirty = true;
        m_referenceSelected.clear();
        m_referenceScroll = m_referenceDetailScroll = 0;
        m_itemSourceMaps.clear(); m_itemSourceScanIndex = 0;
        m_itemSourceScanActive = m_itemSourcesComplete = false;
        m_itemSourceScanFailures = 0;
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
                       "C Chests W Water S Shrines O Obelisks P Passages M Monsters N Notes I Items",
                       "menu_lucida", 7, 0xffc6dfeau);
        else if (map->kind == WorldRecordKind::Region)
            textInRect(canvasRect(375, 94, 195, 16), m_filter == WorldRecordKind::Travel
                ? "C Coaches   B Boats" : "B Buildings   T Trainers   D Dungeons",
                       "menu_lucida", 8, 0xffc6dfeau);
    }
    if (m_map.empty() && m_filter == WorldRecordKind::Travel)
        textInRect(canvasRect(168, 89, 403, 25),
            "Walk: solid   Coach: dashed   Boat: dotted   |   Arrows show direction", "menu_lucida", 9);
    indexKnownRegions();
    const std::string listHeading = !m_siteSelection.empty() ? "Clear selection"
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

void AtlasScreen::indexMapData(MapAssetInfo &map)
{
    const std::optional<MapDeltaData> &delta = map.outdoorMapData
        ? map.outdoorMapDeltaData : map.indoorMapDeltaData;
    if (delta)
        m_world.indexMapItemSources(map.map, buildWorldItemSources(*delta,
            m_data.itemTable(), m_data.objectTable()), m_data.itemTable());
    std::string error;
    if (!GameDataLoader::loadEventPrograms(assetFileSystem(), map, error) || !map.localEventProgram)
        throw std::runtime_error(error.empty() ? "Missing event metadata" : error);
    const auto &observations = m_knowledge.monsterObservations[toLowerCopy(map.map.fileName)];
    if (map.outdoorMapData)
    {
        m_world.indexMapLocations(*map.outdoorMapData, *map.localEventProgram);
        m_world.indexMapMonsters(map.map, *map.outdoorMapData, m_data.monsterTable(), observations);
    }
    else if (map.indoorMapData)
    {
        m_world.indexMapLocations(map.map, *map.indoorMapData, *map.localEventProgram);
        m_world.indexMapMonsters(map.map, *map.indoorMapData, m_data.monsterTable(), observations);
    }
    else
        throw std::runtime_error("Missing map geometry");
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
            if (!map) throw std::runtime_error("Missing map data");
            indexMapData(*map);
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
        auto records = m_world.recordsAtLocation(location, m_knowledge, m_guide,
            m_filter == WorldRecordKind::Travel);
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
        const bool transport = pPrimary->kind == WorldRecordKind::Travel;
        const char *label = transport ? (toLowerCopy(pPrimary->house->type) == "boats" ? "B" : "C")
            : dungeon ? "D" : trainer != pin.records.end() ? "T" : "B";
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

void AtlasScreen::drawTravelLinks(const Rect &image)
{
    const std::vector<WorldTravelLink> links = m_world.travelLinks(m_knowledge, m_guide, m_search);
    const bool selectedRoute = std::any_of(links.begin(), links.end(),
        [this](const WorldTravelLink &link) { return link.recordId == m_selected; });
    // Draw the inspected route last, keeping the full network subdued behind it.
    for (bool selectedPass : {false, true})
    {
        for (size_t i = 0; i < links.size(); ++i)
        {
            const WorldTravelLink &link = links[i];
            if ((m_selected == link.recordId) != selectedPass) continue;
            size_t rank = 0, count = 0;
            for (size_t j = 0; j < links.size(); ++j)
                if ((links[j].sourceId == link.sourceId && links[j].destinationId == link.destinationId)
                    || (links[j].sourceId == link.destinationId && links[j].destinationId == link.sourceId))
                {
                    if (j < i) ++rank;
                    ++count;
                }
            float x1 = image.x + link.source.first * image.width;
            float y1 = image.y + link.source.second * image.height;
            float x2 = image.x + link.destination.first * image.width;
            float y2 = image.y + link.destination.second * image.height;
            const float length = std::hypot(x2 - x1, y2 - y1);
            if (length < designScale()) continue;
            const float dx = (x2 - x1) / length, dy = (y2 - y1) / length;
            const float side = link.sourceId < link.destinationId ? 1.0f : -1.0f;
            const float lane = (float(rank) - (float(count) - 1) / 2) * 5 * designScale() * side;
            x1 -= dy * lane; x2 -= dy * lane;
            y1 += dx * lane; y2 += dx * lane;
            const bool selected = m_selected == link.recordId;
            const uint32_t baseColor = selected ? 0xff8dc5e1u : link.locked ? 0xff53698bu
                : link.kind == WorldTravelKind::Boat ? 0xffcfb487u
                : link.kind == WorldTravelKind::Coach ? 0xff8ebed6u : 0xff91a990u;
            const uint32_t alpha = selected ? 0xff000000u : selectedRoute ? 0x55000000u : 0x99000000u;
            const uint32_t color = (baseColor & 0x00ffffffu) | alpha;
            const float thickness = (selected ? 2.5f : 1.0f) * designScale();
            const auto segment = [&](float startX, float startY, float endX, float endY, float stroke)
            {
                // Keep routes readable over snow, terrain and the schematic's region thumbnails.
                if (selected)
                    drawLine(startX, startY, endX, endY, stroke + 1.4f * designScale(), 0xe0161712u);
                drawLine(startX, startY, endX, endY, stroke, color);
            };
            if (link.kind == WorldTravelKind::Walking)
                segment(x1, y1, x2, y2, thickness);
            else
            {
                const float dash = (link.kind == WorldTravelKind::Boat ? 2.0f : 7.0f) * designScale();
                const float gap = 4 * designScale();
                for (float offset = 0; offset < length; offset += dash + gap)
                {
                    const float end = std::min(offset + dash, length);
                    segment(x1 + dx * offset, y1 + dy * offset,
                        x1 + dx * end, y1 + dy * end, thickness);
                }
            }
            const float tipX = x1 + (x2 - x1) * 0.62f, tipY = y1 + (y2 - y1) * 0.62f;
            const float arrow = (selected ? 7.0f : 5.0f) * designScale();
            segment(tipX, tipY, tipX - dx * arrow - dy * arrow / 2,
                tipY - dy * arrow + dx * arrow / 2, designScale());
            segment(tipX, tipY, tipX - dx * arrow + dy * arrow / 2,
                tipY - dy * arrow - dx * arrow / 2, designScale());
        }
    }
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
                std::vector<std::pair<float, float>> positions;
                for (const WorldMapFeature *pFeature : m_world.queryFeatures(m_knowledge, m_guide, "", map->id))
                    positions.emplace_back(pFeature->site.x, pFeature->site.y);
                if (m_knowledge.partyPosition
                    && m_knowledge.partyPosition->mapFileName == toLowerCopy(map->map->fileName))
                    positions.emplace_back(m_knowledge.partyPosition->x, m_knowledge.partyPosition->y);
                m_floorPlan = buildWorldFloorPlan(*geometry,
                    knowledge == m_knowledge.floorPlans.end() ? WorldFloorPlanKnowledge{} : knowledge->second,
                    m_guide, positions);
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
    if (floorPlan != nullptr && floorPlan->outlineCount == 0)
        textInRect(canvasRect(34, 126, 200, 34),
            "Recorded positions only.\nExplore to reveal dungeon outlines.", "menu_lucida", 10);
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
        if (m_filter == WorldRecordKind::Travel)
            drawTravelLinks(image);
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
            ? "From " + record.map->name + " - " + (record.walkingRoute != nullptr ? ""
                : record.transport->quote ? "quoted " : "base ") + atlasTravelDays(record.walkingRoute
                    ? record.walkingRoute->travelDays : record.transport->quote
                        ? record.transport->quote->travelDays : int64_t(record.transport->route.travelDays))
            : std::string(WorldDatabase::kindName(record.kind)) + (record.house != nullptr ? " - " + record.map->name : "");
        textInRect({row.x + 7 * designScale(), row.y + 19 * designScale(), row.width - 14 * designScale(), 12 * designScale()},
            subtitle, "menu_lucida", 9, 0xffa9bbb7u);
    }
    revealFocus(viewport, m_scroll);
    setDesignClip(std::nullopt);
    if (records.empty())
        textInRect(viewport, m_guide ? "No matching records." : m_filter == WorldRecordKind::Travel
            ? "No known routes.\nVisit destinations and ask transport agents for their offers."
            : "No matching discoveries.\nVisit places and meet their residents to reveal them.",
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
        const char *labels[] = {"C", "W", "S", "O", "P", "M", "N", "I"};
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

WorldGuideSource AtlasScreen::referenceSource() const
{
    return {&m_data.itemTable(), &m_data.spellTable(), &m_data.classMultiplierTable(),
        &m_data.classSkillTable(), &m_data.mergedPotionSettingTable(), &m_data.mergedReagentSettingTable(),
        &m_data.potionMixingTable(), &m_data.potionNoteTable(), &m_data.rosterTable(),
        &m_data.houseTable(), &m_data.mapStats(), m_presentation.worldId, &m_data.monsterTable(),
        &m_data.journalQuestTable(), &m_data.npcDialogTable(), &m_world, &m_knowledge,
        &m_data.mergedCharacterSelectionTable()};
}

void AtlasScreen::rebuildReference()
{
    m_classComparison.reset();
    m_referenceEntries = buildWorldGuide(referenceSource(), m_referenceSection, m_referenceKnowledge, m_guide);
    const auto selected = std::find_if(m_referenceEntries.begin(), m_referenceEntries.end(),
        [this](const WorldGuideEntry &entry) { return entry.id == m_referenceSelected; });
    if (selected != m_referenceEntries.end())
        m_referenceScroll = std::distance(m_referenceEntries.begin(), selected) * 33;
    else
        m_referenceSelected.clear();
    m_referenceDirty = false;
}

void AtlasScreen::stepItemSourceScan()
{
    if (!m_itemSourceScanActive) return;
    const MapStatsEntry &record = *m_itemSourceMaps[m_itemSourceScanIndex++];
    try
    {
        auto map = MapAssetLoader().load(assetFileSystem(), record,
            m_data.monsterTable(), m_data.objectTable(), MapLoadPurpose::HeadlessGameplay);
        if (!map) throw std::runtime_error("Map data unavailable");
        const std::optional<MapDeltaData> &delta = map->outdoorMapData
            ? map->outdoorMapDeltaData : map->indoorMapDeltaData;
        if (!delta) throw std::runtime_error("Authored companion data unavailable");
        indexMapData(*map);
        m_indexedMaps.insert(WorldDatabase::mapId(record));
    }
    catch (const std::exception &error)
    {
        ++m_itemSourceScanFailures;
        m_locationError = "Some item sources could not be read; the source index is incomplete.";
        std::cerr << "Atlas item source scan failed for " << record.fileName << ": " << error.what() << '\n';
    }
    if (m_itemSourceScanIndex == m_itemSourceMaps.size())
    {
        m_itemSourceScanActive = false;
        m_itemSourcesComplete = true;
        m_floorPlanKey.clear();
        m_referenceDirty = true;
    }
}

void AtlasScreen::compareClass(std::optional<uint32_t> raceId)
{
    m_classComparison = buildWorldClassComparison(referenceSource(), m_referenceKnowledge, m_guide,
        m_referenceSelected, raceId);
    m_comparisonScroll = 0;
}

void AtlasScreen::drawClassComparison()
{
    const size_t raceCount = m_classComparison->races.size() + 1;
    size_t raceIndex = 0;
    std::string raceName = "Base class";
    for (size_t i = 0; i < m_classComparison->races.size(); ++i)
        if (m_classComparison->raceId == m_classComparison->races[i].first)
        {
            raceIndex = i + 1;
            raceName = m_classComparison->races[i].second;
        }
    const bool previousRace = button("atlas-compare-race-prev", canvasRect(285, 89, 25, 25), "<",
        "button", raceCount > 1);
    textInRect(canvasRect(314, 89, 151, 25), raceName, "menu_lucida", 11, 0xffc6dfeau, true);
    const bool nextRace = button("atlas-compare-race-next", canvasRect(470, 89, 25, 25), ">",
        "button", raceCount > 1);
    if (previousRace || nextRace)
    {
        raceIndex = (raceIndex + (nextRace ? 1 : raceCount - 1)) % raceCount;
        const std::optional<uint32_t> race = raceIndex == 0 ? std::nullopt
            : std::optional<uint32_t>(m_classComparison->races[raceIndex - 1].first);
        compareClass(race);
        return;
    }
    const size_t pageCount = std::max(size_t(1), (m_classComparison->classes.size() + 1) / 3);
    if (button("atlas-compare-page-prev", canvasRect(510, 89, 30, 25), "<", "button", m_comparisonPage > 0))
        --m_comparisonPage;
    textInRect(canvasRect(544, 89, 120, 25), "Page " + std::to_string(m_comparisonPage + 1)
        + " / " + std::to_string(pageCount), "menu_lucida", 11, 0xffc6dfeau, true);
    if (button("atlas-compare-page-next", canvasRect(670, 89, 30, 25), ">", "button",
        m_comparisonPage + 1 < pageCount))
        ++m_comparisonPage;
    if (button("atlas-compare-details", canvasRect(711, 89, 110, 25), "Details"))
    {
        m_classComparison.reset();
        m_search.clear();
        return;
    }
    const WorldClassComparison &comparison = *m_classComparison;
    std::vector<size_t> columns{0};
    for (size_t i = 1 + m_comparisonPage * 3; i < comparison.classes.size() && columns.size() < 4; ++i)
        columns.push_back(i);
    const float columnWidth = 596.0f / columns.size();
    textInRect(canvasRect(32, 121, 180, 32), "Skill / growth", "fondamento", 13);
    for (size_t i = 0; i < columns.size(); ++i)
    {
        const WorldClassComparisonColumn &column = comparison.classes[columns[i]];
        if (button("atlas-compare-class-" + column.id, canvasRect(215 + i * columnWidth, 121, columnWidth - 3, 32),
            column.pDefinition->displayName, "button", true, i == 0))
        {
            m_referenceSelected = column.id;
            m_referenceDirty = true;
            m_classComparison.reset();
            m_search.clear();
            m_referenceDetailScroll = 0;
            return;
        }
    }
    const Rect body = canvasRect(25, 157, 798, 234);
    std::vector<const WorldClassComparisonRow *> skills;
    for (const WorldClassComparisonRow &row : comparison.skills)
        if (m_search.empty() || toLowerCopy(displaySkillName(row.skill)).find(toLowerCopy(m_search))
            != std::string::npos) skills.push_back(&row);
    drawSolidRect(body, 0xff18160fu);
    scrollViewport(body, (skills.size() + 4) * 24, m_comparisonScroll);
    setDesignClip(body);
    const ClassMultiplierEntry &anchor = *comparison.classes.front().pDefinition;
    const std::pair<const char *, int ClassMultiplierEntry::*> growth[] = {
        {"Base HP", &ClassMultiplierEntry::baseHealth}, {"HP / level", &ClassMultiplierEntry::healthPerLevel},
        {"Base SP", &ClassMultiplierEntry::baseMana}, {"SP / level", &ClassMultiplierEntry::manaPerLevel}};
    for (size_t row = 0; row < skills.size() + 4; ++row)
    {
        const float y = 157 + row * 24 - m_comparisonScroll;
        const Rect rect = canvasRect(25, y, 786, 24);
        if (rect.y + rect.height < body.y || rect.y > body.y + body.height) continue;
        if (row % 2 == 0) drawSolidRect(rect, 0xff242019u);
        textInRect(canvasRect(32, y, 178, 24), row < 4 ? growth[row].first
            : displaySkillName(skills[row - 4]->skill), "menu_arrus", 11);
        for (size_t i = 0; i < columns.size(); ++i)
        {
            std::string value;
            int64_t difference = 0;
            if (row < 4)
            {
                const int amount = comparison.classes[columns[i]].pDefinition->*growth[row].second;
                difference = int64_t(amount) - anchor.*growth[row].second;
                value = std::to_string(amount);
                if (i > 0 && difference != 0)
                    value += " (" + std::string(difference > 0 ? "+" : "") + std::to_string(difference) + ")";
            }
            else
            {
                const WorldClassComparisonRow &skill = *skills[row - 4];
                const SkillMastery cap = skill.caps[columns[i]];
                value = cap == SkillMastery::None ? "-" : masteryDisplayName(cap);
                difference = int64_t(cap) - int64_t(skill.caps.front());
            }
            const uint32_t color = difference > 0 ? 0xff9bcea1u : difference < 0 ? 0xff83bce4u : 0xffc6dfeau;
            textInRect(canvasRect(215 + i * columnWidth, y, columnWidth - 3, 24), value,
                "menu_lucida", 11, color, true);
        }
    }
    setDesignClip(std::nullopt);
    outline(body, 0xff597284u);
    textInRect(canvasRect(34, 403, 778, 54),
        "First column stays fixed. Values follow active mods; differences use that class as the baseline. "
        "Skills unavailable to all compared classes are omitted. "
        "Race/class creation and promotion quest restrictions still apply.",
        "menu_arrus", 11, 0xffc6dfeau, false, true);
}

void AtlasScreen::drawReference()
{
    stepItemSourceScan();
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
    textInRect(canvasRect(32, 91, 229, 20), m_search.empty()
        ? (m_classComparison ? "Filter skills..." : "Search this section...") : m_search,
               "menu_lucida", 11);
    if (m_classComparison)
    {
        drawClassComparison();
        return;
    }
    const bool classes = m_referenceSection == WorldGuideSection::Classes;
    const bool scanItems = m_guide && m_referenceSection == WorldGuideSection::Items;
    textInRect(canvasRect(285, 89, classes || scanItems ? 410 : 535, 25),
        m_presentation.name + " - active game data",
        "fondamento", 14);
    if (scanItems && button("atlas-item-sources", canvasRect(711, 89, 110, 25),
        m_itemSourceScanActive ? "Scan " + std::to_string(m_itemSourceScanIndex) + "/"
            + std::to_string(m_itemSourceMaps.size()) : m_itemSourceScanFailures > 0 ? "Partial sources"
            : m_itemSourcesComplete ? "Sources ready" : "Scan sources",
        "button", !m_itemSourceScanActive && !m_itemSourcesComplete))
    {
        m_itemSourceMaps.clear();
        m_itemSourceScanFailures = 0;
        for (const WorldRecord &record : m_world.records())
            if (record.kind == WorldRecordKind::Region || record.kind == WorldRecordKind::Dungeon)
                m_itemSourceMaps.push_back(record.map);
        m_itemSourceScanIndex = 0;
        m_itemSourceScanActive = !m_itemSourceMaps.empty();
    }
    if (classes && button("atlas-class-compare", canvasRect(711, 89, 110, 25), "Compare", "button",
        std::any_of(m_referenceEntries.begin(), m_referenceEntries.end(),
            [this](const WorldGuideEntry &entry) { return entry.id == m_referenceSelected; })))
    {
        m_search.clear();
        m_comparisonPage = 0;
        compareClass();
        return;
    }
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
    scrollViewport(list, entries.size() * 33, m_referenceScroll);
    setDesignClip(list);
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const Rect row{list.x + 3 * designScale(), list.y + i * rowHeight - m_referenceScroll * designScale(),
                       list.width - 16 * designScale(), rowHeight - 2 * designScale()};
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
        const Rect text{details.x + 8 * designScale(), details.y + (8 - m_referenceDetailScroll) * designScale(),
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
                else if (link.kind == WorldGuideLink::Kind::Feature)
                {
                    const auto feature = std::find_if(m_world.features().begin(), m_world.features().end(),
                        [&](const WorldMapFeature &value) { return value.id == link.target; });
                    if (feature != m_world.features().end() && m_world.featureVisible(*feature, m_knowledge, m_guide))
                    {
                        select(*m_world.find(feature->mapId));
                        m_selectedFeature = feature->id;
                        m_siteSelection = {feature->id};
                        m_featureView = true; m_referenceOpen = false;
                    }
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
                    m_referenceSection = link.kind == WorldGuideLink::Kind::Spell ? WorldGuideSection::Spells
                        : link.kind == WorldGuideLink::Kind::Class ? WorldGuideSection::Classes
                        : link.kind == WorldGuideLink::Kind::Item ? WorldGuideSection::Items
                        : WorldGuideSection::Alchemy;
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
        m_referenceContentHeight = height / designScale() + entry.links.size() * 29 + 24;
        setDesignClip(std::nullopt);
    }
    else
        textInRect(details, entries.empty() ? (m_guide ? "No records in this section."
            : "Nothing known yet. Guide Mode opens the full reference catalog.")
            : "Select a record to inspect its details.", "menu_arrus", 13, 0xffc6dfeau, true);
    textInRect(canvasRect(34, 407, 778, 43), m_itemSourceScanActive
        ? "Reading authored item sources, one map per frame. Random loot is never generated."
        : !m_locationError.empty() ? m_locationError : m_referenceSection == WorldGuideSection::Items
        ? (m_guide ? "Sources cover authored placements, fixed chest entries and carried items. "
            "They do not predict random loot or remaining contents."
            : "Recorded sources come from actual pickups and visible chest contents. "
              "A past observation does not guarantee an item is still there.") : m_guide
        ? "Reference definitions follow the loaded game tables, including enabled mods."
        : "Discovery shows encountered items, known spell references, current classes, known recipes, "
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
            "C Chests, W Wells/Fountains, S Shrines, O Obelisks, P Passages, M Monsters, N Notes, I Items.";
        const WorldRecord *pDestination = nullptr;
        int16_t creatureId = 0;
        uint32_t itemId = 0;
        const WorldMapFeature *pNote = nullptr;
        if (selected != features.end())
        {
            const WorldMapFeature &feature = **selected;
            text = feature.title + " | " + worldMapFeatureKindName(feature.kind);
            itemId = feature.itemId;
            if (feature.kind == WorldMapFeatureKind::Item)
                text += feature.observedItem ? "\nRecorded pickup; this is not a live availability check."
                    : "\nAuthored source; scripts and the campaign state can change availability.";
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
        textInRect(canvasRect(34, 407,
            pDestination != nullptr || creatureId > 0 || itemId > 0 || pNote != nullptr ? 625 : 778, 43),
                   text, "menu_arrus", 11);
        if (itemId > 0 && button("atlas-feature-item", canvasRect(669, 416, 145, 28), "Item details"))
        {
            m_referenceOpen = true;
            m_referenceSection = WorldGuideSection::Items;
            m_referenceSelected = std::to_string(itemId);
            m_referenceDirty = true;
            m_search.clear();
            m_referenceScroll = m_referenceDetailScroll = m_referenceContentHeight = 0;
        }
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
        if (record->transport)
        {
            const WorldTransportEntry &transport = *record->transport;
            text += transport.observed ? " | Last offered" : transport.current ? " | Current timetable"
                : " | Authored timetable";
            text += transport.quote ? " | Quoted " : " | Base ";
            text += atlasTravelDays(transport.quote ? transport.quote->travelDays : transport.route.travelDays);
            if (transport.quote) text += " | " + std::to_string(transport.quote->gold) + " gold quoted";
            text += " | ";
            const char *days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
            for (size_t day = 0; day < 7; ++day)
                if (transport.route.daysAvailable[day]) text += std::string(days[day]) + " ";
            if (transport.route.requiredQBit != 0) text += "| Quest unlock required";
        }
        if (record->walkingRoute != nullptr)
        {
            text += " | Walking from " + record->map->name + " | "
                + atlasTravelDays(record->walkingRoute->travelDays);
            if (!record->walkingRoute->requiredQuestBitsAny.empty())
                text += m_pParty == nullptr ? " | Quest requirement"
                    : m_world.walkingRouteUnlocked(*record, m_knowledge) ? " | Quest requirement met"
                        : " | Locked by quest requirement";
            if (record->walkingRoute->requiredOriginSurface == MapTransitionSurfaceRequirement::Water)
                text += " | Water access required";
        }
    }
    if (!m_locationError.empty())
        text += "\n" + m_locationError;
    float textWidth = 778;
    if (record != nullptr && record->kind == WorldRecordKind::Travel && record->destination != nullptr)
    {
        const WorldRecord *pDestination = m_world.find(WorldDatabase::mapId(*record->destination));
        if (pDestination != nullptr && m_world.visible(*pDestination, m_knowledge, m_guide))
        {
            textWidth = 619;
            if (button("atlas-route-destination", canvasRect(669, 416, 145, 28), "View destination"))
                select(*pDestination);
        }
    }
    textInRect(canvasRect(34, 407, textWidth, 43), text, "menu_arrus", 12);
}
} // namespace OpenYAMM::Game
