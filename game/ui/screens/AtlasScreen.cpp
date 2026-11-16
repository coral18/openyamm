#include "engine/Localization.h"
#include "engine/Utf8Text.h"
#include "game/ui/screens/AtlasScreen.h"
#include "game/data/GameDataRepository.h"
#include "game/data/GameDataLoader.h"
#include "game/gameplay/MasteryTeacherDialog.h"
#include "game/StringUtils.h"
#include "game/maps/MapAssetLoader.h"
#include "game/ui/GameplayMinimapTransform.h"
#include "game/events/EvtEnums.h"
#include "game/world/WorldGuildStockKnowledge.h"

#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <charconv>

namespace OpenYAMM::Game
{
namespace
{
std::optional<uint32_t> atlasItemId(const std::string &value)
{
    uint32_t id = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), id);
    return parsed.ec == std::errc() && parsed.ptr == value.data() + value.size() && id != 0
               ? std::optional<uint32_t>{id}
               : std::nullopt;
}

std::string initialAtlasWorld(const Engine::AssetFileSystem &assets, const MapAssetInfo *pCurrentMap)
{
    const std::string world = pCurrentMap != nullptr ? pCurrentMap->map.worldId : assets.getActiveWorldId();
    return assets.exists("engine/world/atlas/" + world + ".yml") ? world : "mm7";
}
WorldNpcState atlasNpcState(const GameDataRepository &data, const WorldKnowledge &knowledge, bool guide, bool hasParty)
{
    WorldNpcState result = guide || !hasParty ? knowledge.currentNpcs
                                              : worldObservedNpcs(data.npcDialogTable(), data.mergedTeacherTopicTable(),
                                                                  knowledge.variables);
    result.generatedNpcs.clear();
    result.mercenaries.clear();
    for (const auto &[map, observations] : knowledge.generatedNpcs)
        for (const auto &[key, npc] : observations) result.generatedNpcs.push_back(&npc);
    for (const auto &[map, observations] : knowledge.mercenaries)
        for (const auto &[id, npc] : observations) result.mercenaries.push_back(&npc);
    return result;
}
std::string atlasTravelDays(int64_t days)
{
    return Engine::trn("{count} day", "{count} days", uint64_t(std::max(int64_t(0), days)),
                       {{"count", std::to_string(days)}});
}
WorldTransportState atlasTransportState(const GameDataRepository &data, const WorldKnowledge &knowledge, bool guide,
                                        bool hasParty)
{
    return guide || !hasParty ? knowledge.currentTransport
                              : worldObservedTransport(data.houseTable(), data.mapStats(), knowledge.variables);
}
} // namespace

AtlasScreen::AtlasScreen(const Engine::AssetFileSystem &assets, const GameDataRepository &data,
                         WorldKnowledge knowledge, std::function<void()> close, bool paused, GameAudioSystem *audio,
                         const MapAssetInfo *pCurrentMap, const Party *pParty, NoteWriter writeNote,
                         std::string rulesetRoot)
    : MenuDesignScreen(assets, paused, audio), m_knowledge(std::move(knowledge)),
      m_presentation(loadWorldAtlasPresentation(assets, initialAtlasWorld(assets, pCurrentMap))),
      m_world(data.mapStats(), data.houseTable(), data.npcDialogTable(), data.mergedTeacherTopicTable(),
              m_presentation.worldId, m_presentation.maps, atlasNpcState(data, m_knowledge, false, pParty != nullptr),
              atlasTransportState(data, m_knowledge, false, pParty != nullptr)),
      m_data(data), m_rulesetRoot(std::move(rulesetRoot)), m_pCurrentMap(pCurrentMap), m_pParty(pParty),
      m_pGlobalEvents(pCurrentMap != nullptr && pCurrentMap->globalEventProgram ? &*pCurrentMap->globalEventProgram
                                                                                : nullptr),
      m_pLocalEvents(pCurrentMap != nullptr && pCurrentMap->localEventProgram ? &*pCurrentMap->localEventProgram
                                                                              : nullptr),
      m_writeNote(std::move(writeNote)), m_close(std::move(close)), m_paused(paused)
{
    loadDesign("menu/main_menu");
    m_decorationTable = MapAssetLoader::loadDecorationTable(assets);
    if (!m_decorationTable) throw std::runtime_error("Missing or invalid decoration registry");
    if (!m_world.transportIssues().empty())
    {
        m_locationError = Engine::tr("Some saved transport offers are unavailable.");
        for (const std::string &issue : m_world.transportIssues()) std::cerr << issue << '\n';
    }
    m_world.indexMapNotes(m_knowledge);
    m_world.indexMapArrivals(m_knowledge);
    m_world.indexObservedMonsters(m_knowledge, m_data.monsterTable());
    m_world.indexObservedItemSources(m_knowledge, m_data.itemTable());
    for (const std::string &file : assets.enumerate("engine/world/atlas"))
        if (std::filesystem::path(file).extension() == ".yml")
            m_availableWorlds.push_back(std::filesystem::path(file).stem().string());
    m_referenceKnowledge.recipes = m_knowledge.autonoteIds;
    for (const auto &[file, observations] : m_knowledge.itemObservations)
        for (const WorldItemSource &observation : observations)
        {
            m_referenceKnowledge.items.insert(observation.itemId);
            if ((observation.kind == WorldItemSourceKind::Carried ||
                 observation.kind == WorldItemSourceKind::DeathDrop) &&
                observation.monsterId > 0)
                m_referenceKnowledge.monsters.insert(observation.monsterId);
        }
    for (const auto &[file, observations] : m_knowledge.monsterObservations)
        for (const WorldMonsterObservation &observation : observations)
            m_referenceKnowledge.monsters.insert(observation.monsterId);
    if (m_pParty != nullptr)
    {
        const MapBoundaryEdge edges[] = {MapBoundaryEdge::North, MapBoundaryEdge::South, MapBoundaryEdge::East,
                                         MapBoundaryEdge::West};
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
        {
            if (item.itemId != 0 && worldGuildBookKnown(m_knowledge.variables, item.itemId))
            {
                m_referenceKnowledge.items.insert(item.itemId);
                m_referenceKnowledge.identifiedItems.insert(item.itemId);
                m_knowledge.identifiedItems.insert(item.itemId);
            }
            if (m_pParty->hasEverOwnedItem(item.itemId) || m_pParty->hasItemAnywhere(item.itemId) ||
                m_pParty->hasIdentifiedItem(item.itemId))
                m_referenceKnowledge.items.insert(item.itemId);
            if (m_pParty->hasIdentifiedItem(item.itemId))
            {
                m_referenceKnowledge.identifiedItems.insert(item.itemId);
                m_knowledge.identifiedItems.insert(item.itemId);
            }
        }
        for (size_t i = 0; i < m_pParty->memberCount(); ++i)
            if (const Character *pMember = m_pParty->member(i))
            {
                m_referenceKnowledge.classes.insert(canonicalClassName(pMember->className));
                m_referenceKnowledge.races.insert(pMember->raceId);
                for (const SpellEntry &spell : m_data.spellTable().entries())
                    if (pMember->knowsSpell(uint32_t(spell.id))) m_referenceKnowledge.spells.insert(uint32_t(spell.id));
            }
        for (const RosterEntry *pMember : m_data.rosterTable().getEntriesSortedById())
            if (m_pParty->hasRosterMember(pMember->id)) m_referenceKnowledge.roster.insert(pMember->id);
    }
    if (m_pGlobalEvents != nullptr) m_world.indexNpcLocations(*m_pGlobalEvents);
    if (m_pLocalEvents != nullptr) m_world.indexNpcLocations(*m_pLocalEvents);
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
    if (pCurrentMap != nullptr && pCurrentMap->localEventProgram)
    {
        m_world.indexMapMonsterSpawns(pCurrentMap->map, *pCurrentMap->localEventProgram, m_data.monsterTable());
        m_world.indexMapNpcTopics(pCurrentMap->map, *pCurrentMap->localEventProgram);
        m_world.indexMapNpcEncounters(pCurrentMap->map, *pCurrentMap->localEventProgram);
        m_world.indexMapInteractionReferences(pCurrentMap->map, m_data.itemTable(), m_data.spellTable(),
                                              m_data.journalQuestTable());
    }
    if (pCurrentMap != nullptr && m_pGlobalEvents != nullptr &&
        (pCurrentMap->outdoorMapData || pCurrentMap->indoorMapData))
    {
        const auto sites = pCurrentMap->outdoorMapData
                               ? worldInheritedDecorationSites(
                                     *pCurrentMap->outdoorMapData, *m_decorationTable,
                                     pCurrentMap->outdoorMapDeltaData ? &*pCurrentMap->outdoorMapDeltaData : nullptr)
                               : worldInheritedDecorationSites(
                                     *pCurrentMap->indoorMapData, *m_decorationTable,
                                     pCurrentMap->indoorMapDeltaData ? &*pCurrentMap->indoorMapDeltaData : nullptr);
        m_world.indexInheritedDecorations(pCurrentMap->map, sites, *m_pGlobalEvents);
        m_world.indexDecorationItemRewards(pCurrentMap->map, sites, *m_pGlobalEvents, m_data.itemTable());
    }
    if (m_knowledge.partyPosition)
        for (const auto &record : m_world.records())
            if ((record.kind == WorldRecordKind::Region || record.kind == WorldRecordKind::Dungeon) &&
                toLowerCopy(record.map->fileName) == m_knowledge.partyPosition->mapFileName)
            {
                select(record);
                break;
            }
}

AtlasScreen::~AtlasScreen()
{
    if (bgfx::isValid(m_floorPlanTexture)) bgfx::destroy(m_floorPlanTexture);
    if (bgfx::isValid(m_fogTexture)) bgfx::destroy(m_fogTexture);
}

AppMode AtlasScreen::mode() const
{
    return m_paused ? AppMode::PauseMenu : AppMode::MainMenu;
}
void AtlasScreen::onExit()
{
    setTextEditing(false);
}

void AtlasScreen::handleSdlEvent(const SDL_Event &event)
{
    if (m_placingNote && event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)
    {
        m_placingNote = false;
        m_suppressEscape = true;
        return;
    }
    if (!m_searchEditing && !m_noteEditorOpen) return;
    std::string &text = m_noteEditorOpen ? m_noteDraft.text : m_search;
    if (event.type == SDL_EVENT_TEXT_INPUT)
    {
        const std::string input = event.text.text;
        if (text.size() + input.size() <= (m_noteEditorOpen ? 512u : 128u)) text += input;
        m_scroll = 0;
    }
    if (event.type == SDL_EVENT_KEY_DOWN)
    {
        if (event.key.scancode == SDL_SCANCODE_BACKSPACE && !text.empty())
        {
            size_t start = text.size() - 1;
            while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xc0) == 0x80) --start;
            text.erase(start);
            m_scroll = 0;
        }
        if (event.key.scancode == SDL_SCANCODE_ESCAPE)
        {
            if (m_noteEditorOpen) closeNoteEditor();
            m_searchEditing = false;
            setTextEditing(false);
            m_suppressEscape = true;
        }
    }
}

void AtlasScreen::drawSearchField(const std::string &id, const Rect &rect)
{
    const bool submit = m_searchEditing && keyPressed(SDL_SCANCODE_RETURN);
    if (m_searchEditing && (keyPressed(SDL_SCANCODE_TAB) || (leftMouseJustPressed() && !pointerInside(rect))))
    {
        m_searchEditing = false;
        setTextEditing(false);
    }
    if (button(id, rect, "", "text_field"))
    {
        m_searchEditing = true;
        setTextEditing(true);
    }
    // Commit after activation; the same Enter must not focus the field again.
    if (submit)
    {
        m_searchEditing = false;
        setTextEditing(false);
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
    if (!m_writeNote || !m_writeNote(m_noteDraft, m_noteError)) return;
    auto &notes = m_knowledge.notes[toLowerCopy(m_noteDraft.mapFileName)];
    std::erase_if(notes, [this](const WorldMapNote &note) { return note.value.id == m_noteDraft.id; });
    if (!remove)
        notes.push_back(
            {{m_noteDraft.id, m_noteDraft.x, m_noteDraft.y, m_noteDraft.mapFileName, m_noteDraft.text, false, true},
             true,
             m_noteDraft.elevation});
    m_world.indexMapNotes(m_knowledge);
    m_world.indexMapArrivals(m_knowledge);
    m_world.indexObservedMonsters(m_knowledge, m_data.monsterTable());
    m_world.indexObservedItemSources(m_knowledge, m_data.itemTable());
    if (!remove && m_noteDraft.elevation) selectFloorLayer(m_floorPlan.layerAtHeight(float(*m_noteDraft.elevation)));
    m_selectedFeature = remove ? "" : m_map + "/note/" + std::to_string(m_noteDraft.id);
    m_floorPlanKey.clear();
    m_featureView = true;
    m_filter.reset();
    m_search.clear();
    m_siteSelection.clear();
    m_scroll = 0;
    closeNoteEditor();
}

void AtlasScreen::drawNoteEditor()
{
    textInRect(canvasRect(80, 87, 690, 35), Engine::tr("Map note"), "fondamento", 23, 0xffbcddeeu);
    const WorldRecord *pMap = m_world.find(m_map);
    const bool dungeon = pMap != nullptr && pMap->kind == WorldRecordKind::Dungeon;
    textInRect(canvasRect(80, 125, dungeon ? 315 : 690, 25),
               Engine::localizeKnown((pMap != nullptr ? pMap->name() : Engine::tr("Map")) + " | " +
                                         std::to_string(m_noteDraft.x) + ", " + std::to_string(m_noteDraft.y),
                                     "world:" + m_presentation.worldId),
               "menu_lucida", 11);
    if (dungeon) drawNoteElevation();
    const Rect field = canvasRect(80, 160, 690, 125);
    drawSolidRect(field, 0xff18160fu);
    outline(field, 0xff8dc5e1u);
    if (button("atlas-note-text", field, "", "text_field")) setTextEditing(true);
    setDesignClip(field);
    textInRect(canvasRect(88, 165, 674, 115), m_noteDraft.text + "_", "menu_arrus", 12, 0xffc6dfeau, false, true);
    setDesignClip(std::nullopt);
    textInRect(canvasRect(80, 287, 690, 32),
               m_noteError.empty() ? Engine::tr("Your notes are shared with the Map Book. Escape cancels changes.")
                                   : Engine::tr(m_noteError),
               "menu_lucida", 11);
    if (button("atlas-note-save", canvasRect(80, 330, 155, 30), Engine::tr("Save note"))) saveNote(false);
    if (button("atlas-note-cancel", canvasRect(260, 330, 155, 30), Engine::tr("Cancel"))) closeNoteEditor();
    if (m_noteDraft.id != 0 && button("atlas-note-delete", canvasRect(615, 330, 155, 30), Engine::tr("Delete note")))
        saveNote(true);
}

void AtlasScreen::drawNoteElevation()
{
    const auto current =
        m_noteDraft.elevation ? m_floorPlan.layerAtHeight(float(*m_noteDraft.elevation)) : std::nullopt;
    textInRect(canvasRect(400, 125, 100, 25),
               current ? Engine::tr("Layer {name}", {{"name", Engine::localizeKnown(current->name())}})
                       : Engine::tr("All heights"),
               "menu_lucida", 11, 0xffc6dfeau, true);
    const auto lower =
        current ? std::lower_bound(m_floorLayers.begin(), m_floorLayers.end(), *current) : m_floorLayers.end();
    const auto higher =
        current ? std::upper_bound(m_floorLayers.begin(), m_floorLayers.end(), *current) : m_floorLayers.begin();
    if (button("atlas-note-lower", canvasRect(504, 125, 30, 25), Engine::tr("<"), "button",
               lower != m_floorLayers.begin()))
        m_noteDraft.elevation = worldFloorPlanLayerElevation(*std::prev(lower));
    if (button("atlas-note-higher", canvasRect(538, 125, 30, 25), Engine::tr(">"), "button",
               higher != m_floorLayers.end()))
        m_noteDraft.elevation = worldFloorPlanLayerElevation(*higher);
    if (button("atlas-note-all", canvasRect(572, 125, 65, 25), Engine::tr("All"), "button", current.has_value()))
        m_noteDraft.elevation.reset();
    std::optional<int32_t> partyHeight;
    if (m_knowledge.partyPosition && m_knowledge.partyPosition->z &&
        m_knowledge.partyPosition->mapFileName == toLowerCopy(m_noteDraft.mapFileName))
    {
        const double height = std::round(double(*m_knowledge.partyPosition->z));
        if (std::isfinite(height) && height >= INT32_MIN && height <= INT32_MAX) partyHeight = int32_t(height);
    }
    if (button("atlas-note-party", canvasRect(642, 125, 128, 25), Engine::tr("Party height"), "button",
               partyHeight.has_value()))
        m_noteDraft.elevation = partyHeight;
}

void AtlasScreen::select(const WorldRecord &record)
{
    m_placingNote = false;
    m_selectedFeature.clear();
    m_selected = record.id;
    if (record.kind == WorldRecordKind::Travel && m_map.empty()) return;
    const MapStatsEntry *pDestination = m_world.recordMap(record, m_knowledge, m_guide, m_map);
    if (pDestination == nullptr) return;
    const std::string destination = WorldDatabase::mapId(*pDestination);
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
    else
    {
        m_floorLayer.reset();
        m_floorPlanKey.clear();
    }
}

std::string AtlasScreen::regionTexture(const WorldRecord &record) const
{
    const std::string base =
        "worlds/" + record.map->worldId + "/icons/" + std::filesystem::path(record.map->fileName).stem().string();
    for (const char *pExtension : {".png", ".bmp", ".pcx"})
        if (assetFileSystem().exists(base + pExtension)) return base + pExtension;
    return base;
}

void AtlasScreen::rebuildWorld()
{
    m_world = WorldDatabase(m_data.mapStats(), m_data.houseTable(), m_data.npcDialogTable(),
                            m_data.mergedTeacherTopicTable(), m_presentation.worldId, m_presentation.maps,
                            atlasNpcState(m_data, m_knowledge, m_guide, m_pParty != nullptr),
                            atlasTransportState(m_data, m_knowledge, m_guide, m_pParty != nullptr));
    m_world.indexMapNotes(m_knowledge);
    m_world.indexMapArrivals(m_knowledge);
    m_world.indexObservedMonsters(m_knowledge, m_data.monsterTable());
    m_world.indexObservedItemSources(m_knowledge, m_data.itemTable());
    if (m_pGlobalEvents != nullptr) m_world.indexNpcLocations(*m_pGlobalEvents);
    if (m_pLocalEvents != nullptr) m_world.indexNpcLocations(*m_pLocalEvents);
    indexQuestLocations();
    m_sourceMaps.clear();
    m_sourceScanIndex = 0;
    m_sourceScanActive = m_sourcesComplete = false;
    m_sourceScanFailures = 0;
    m_connectionScanMaps.clear();
    m_connectionScanIndex = m_connectionScanFailures = 0;
    m_connectionCatalogComplete = false;
    m_graphNode.clear();
    m_graphEdge.clear();
    m_graphPage = m_graphEdgePage = 0;
    m_indexedMaps.clear();
    m_floorPlanKey.clear();
}

void AtlasScreen::selectWorld(const std::string &worldId)
{
    m_placingNote = false;
    if (worldId == m_presentation.worldId) return;
    m_siteReading.reset();
    try
    {
        WorldAtlasPresentation presentation = loadWorldAtlasPresentation(assetFileSystem(), worldId);
        WorldDatabase world(m_data.mapStats(), m_data.houseTable(), m_data.npcDialogTable(),
                            m_data.mergedTeacherTopicTable(), worldId, presentation.maps,
                            atlasNpcState(m_data, m_knowledge, m_guide, m_pParty != nullptr),
                            atlasTransportState(m_data, m_knowledge, m_guide, m_pParty != nullptr));
        m_world = std::move(world);
        m_world.indexMapNotes(m_knowledge);
        m_world.indexMapArrivals(m_knowledge);
        m_world.indexObservedMonsters(m_knowledge, m_data.monsterTable());
        m_world.indexObservedItemSources(m_knowledge, m_data.itemTable());
        m_selectedFeature.clear();
        m_referenceDirty = true;
        m_referenceSelected.clear();
        m_pinnedItem.reset();
        m_itemComparison.reset();
        m_referenceScroll = m_referenceDetailScroll = 0;
        m_sourceMaps.clear();
        m_sourceScanIndex = 0;
        m_sourceScanActive = m_sourcesComplete = false;
        m_connectionView = m_connectionCatalogComplete = false;
        m_connectionScanMaps.clear();
        m_connectionScanIndex = m_connectionScanFailures = 0;
        m_graphNode.clear();
        m_graphEdge.clear();
        m_graphPage = m_graphEdgePage = 0;
        m_sourceScanFailures = 0;
        m_presentation = std::move(presentation);
        if (m_pGlobalEvents != nullptr) m_world.indexNpcLocations(*m_pGlobalEvents);
        if (m_pLocalEvents != nullptr) m_world.indexNpcLocations(*m_pLocalEvents);
        indexQuestLocations();
        m_trainerMastery = 0;
        m_indexedMaps.clear();
        m_siteSelection.clear();
        m_map.clear();
        m_selected.clear();
        m_search.clear();
        m_filter.reset();
        m_floorPlanKey.clear();
        m_fogMap.clear();
        m_locationError.clear();
        m_zoom = 1;
        m_panX = m_panY = m_scroll = 0;
        m_dragging = false;
    }
    catch (const std::exception &error)
    {
        m_locationError = Engine::tr("Could not open the selected world's atlas.");
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
    textInRect(canvasRect(25, 15, 245, 30), Engine::tr("Interactive Atlas"), "fondamento", 20, 0xffbcddeeu);
    const float worldCount = float(std::max(size_t(1), m_availableWorlds.size()));
    const float worldButtonWidth = std::min(105.0f, (230.0f - 10.0f * (worldCount - 1.0f)) / worldCount);
    for (size_t i = 0; i < m_availableWorlds.size(); ++i)
        if (button("atlas-catalog-" + m_availableWorlds[i],
                   canvasRect(285 + i * (worldButtonWidth + 10), 17, worldButtonWidth, 27),
                   m_availableWorlds[i], "button", true, m_presentation.worldId == m_availableWorlds[i]))
        {
            selectWorld(m_availableWorlds[i]);
            return;
        }
    if (button("atlas-reference", canvasRect(205, 17, 70, 27),
               m_referenceOpen || m_siteReading ? Engine::tr("Map") : Engine::tr("Guidebook")))
    {
        m_placingNote = false;
        m_referenceOpen = m_siteReading ? false : !m_referenceOpen;
        m_siteReading.reset();
        m_referenceDirty = true;
        m_search.clear();
        m_referenceScroll = m_referenceDetailScroll = 0;
        m_searchEditing = false;
        setTextEditing(false);
    }
    if (button("atlas-mode", canvasRect(526, 17, 177, 27),
               m_guide ? Engine::tr("Guide Mode: ON") : Engine::tr("Discovery Mode")))
    {
        m_guide = !m_guide;
        m_pinnedItem.reset();
        m_itemComparison.reset();
        rebuildWorld();
        if (const WorldRecord *pSelected = m_world.find(m_selected);
            pSelected != nullptr && pSelected->npc != nullptr && m_world.visible(*pSelected, m_knowledge, m_guide))
            select(*pSelected);
        m_referenceDirty = true;
        m_referenceScroll = m_referenceDetailScroll = 0;
        m_siteJournalScroll = 0;
        if (const auto *selected = m_world.find(m_selected);
            selected == nullptr || !m_world.visible(*selected, m_knowledge, m_guide))
            m_selected.clear();
        if (const auto *map = m_world.find(m_map); map && !m_world.visible(*map, m_knowledge, m_guide)) m_map.clear();
        m_scroll = 0;
        m_zoom = 1;
        m_panX = m_panY = 0;
        m_siteSelection.clear();
    }
    if (m_siteReading && !m_world.featureVisible(*m_siteReading, m_knowledge, m_guide)) m_siteReading.reset();
    if (button("atlas-close", canvasRect(716, 17, 107, 27), Engine::tr("Back")) ||
        (!m_suppressEscape && !m_searchEditing && keyPressed(SDL_SCANCODE_ESCAPE)))
    {
        if (m_siteReading)
            m_siteReading.reset();
        else if (m_connectionView)
            m_connectionView = false;
        else
            m_close();
        return;
    }
    m_suppressEscape = false;
    if ((m_siteReading || m_referenceOpen) && !m_sourceScanActive) indexKnownRegions();
    if (m_siteReading)
    {
        drawSiteJournal();
        return;
    }
    if (m_referenceOpen)
    {
        drawReference();
        return;
    }
    const Rect search = canvasRect(25, 53, 274, 28);
    drawSearchField("atlas-search", search);
    textInRect(canvasRect(35, 57, 253, 20),
               m_search.empty() ? (m_connectionView ? Engine::tr("Search these connections...")
                                   : m_guide        ? Engine::tr("Search the world...")
                                                    : Engine::tr("Search discovered places..."))
                                : m_search + (m_searchEditing ? "_" : ""),
               "menu_lucida", 11);
    const std::pair<const char *, std::optional<WorldRecordKind>> filters[] = {
        {Engine::trNoop("All").data(), {}},
        {Engine::trNoop("Regions").data(), WorldRecordKind::Region},
        {Engine::trNoop("Dungeons").data(), WorldRecordKind::Dungeon},
        {Engine::trNoop("Buildings").data(), WorldRecordKind::House},
        {Engine::trNoop("NPCs").data(), WorldRecordKind::Npc},
        {Engine::trNoop("Trainers").data(), WorldRecordKind::Trainer},
        {Engine::trNoop("Travel").data(), WorldRecordKind::Travel}};
    for (int i = 0; i < 8; ++i)
    {
        const Rect rect = canvasRect(312 + i * 64, 53, 61, 28);
        if (button("atlas-filter-" + std::to_string(i), rect,
                   i == 7 ? Engine::tr("Sites") : Engine::tr(filters[i].first), "button", true,
                   i == 7 ? m_featureView : !m_featureView && m_filter == filters[i].second))
        {
            m_connectionView = false;
            m_featureView = i == 7;
            m_filter = i == 7 ? std::nullopt : filters[i].second;
            m_selectedFeature.clear();
            m_trainerMastery = 0;
            m_scroll = 0;
            m_siteSelection.clear();
        }
        if (i == 7 ? m_featureView : !m_featureView && m_filter == filters[i].second) outline(rect, 0xff8dc5e1u);
    }
    if (button("atlas-world", canvasRect(25, 89, 132, 25), Engine::localize(m_presentation.name), "button", true,
               m_map.empty()))
    {
        m_connectionView = false;
        m_map.clear();
        m_selected.clear();
        m_selectedFeature.clear();
        m_siteSelection.clear();
        m_zoom = 1;
        m_panX = m_panY = m_scroll = 0;
    }
    if (const auto *map = m_world.find(m_map))
    {
        textInRect(canvasRect(168, 89, 265, 25), Engine::localizeKnown(map->name(), "world:" + m_presentation.worldId),
                   "fondamento", 14);
        if (button("atlas-connections", canvasRect(445, 89, 125, 25),
                   m_connectionView ? Engine::tr("Map plan") : Engine::tr("Connections")))
        {
            m_connectionView = !m_connectionView;
            m_placingNote = false;
            m_graphNode = m_map;
            m_graphEdge.clear();
            m_graphPage = m_graphEdgePage = 0;
        }
    }
    else
        m_connectionView = false;
    if (m_map.empty() && m_filter == WorldRecordKind::Travel)
        textInRect(canvasRect(168, 89, 403, 25),
                   Engine::tr("Walk: solid   Coach: dashed   Boat: dotted   |   Arrows show direction"), "menu_lucida",
                   9);
    indexKnownRegions();
    if (m_connectionJumpEvent != 0)
    {
        bool matched = false;
        if (m_map == m_connectionJumpMap)
        {
            m_siteSelection.clear();
            for (const WorldMapFeature &feature : m_world.features())
                if (feature.mapId == m_map && feature.site.eventId == m_connectionJumpEvent &&
                    feature.kind == WorldMapFeatureKind::Connection &&
                    !feature.site.id.starts_with("inherited-decoration/") &&
                    m_world.featureVisible(feature, m_knowledge, m_guide))
                {
                    matched = true;
                    m_siteSelection.insert(feature.id);
                    if (m_selectedFeature.empty()) m_selectedFeature = feature.id;
                }
        }
        if (!matched) m_connectionJumpDestination.clear();
        m_connectionJumpEvent = 0;
        m_connectionJumpMap.clear();
    }
    if (m_connectionView)
    {
        drawConnectionGraph();
        return;
    }
    const std::string listHeading =
        !m_siteSelection.empty()  ? Engine::tr("Clear selection")
        : m_pendingMapIndexes > 0 ? Engine::tr("Reading maps: {m_pendingMapIndexes}",
                                               {{"m_pendingMapIndexes", std::to_string(m_pendingMapIndexes)}})
        : m_filter == WorldRecordKind::Travel
            ? (m_guide ? Engine::tr("All recorded routes") : Engine::tr("Known routes"))
        : m_map.empty() ? (m_guide ? Engine::tr("All recorded locations") : Engine::tr("Your party's discoveries"))
                        : (m_guide ? Engine::tr("Locations and residents here") : Engine::tr("Discovered here"));
    const WorldRecord *pNoteMap = m_world.find(m_map);
    const bool canAddNote =
        m_writeNote && pNoteMap != nullptr && m_knowledge.visitedMaps.contains(toLowerCopy(pNoteMap->map->fileName));
    if (m_siteSelection.empty())
        textInRect(canvasRect(589, 89, canAddNote ? 115 : 233, 25), listHeading, "menu_lucida", 11);
    else if (button("atlas-clear-sites", canvasRect(589, 89, canAddNote ? 115 : 233, 25), listHeading))
    {
        m_siteSelection.clear();
        m_scroll = 0;
    }
    if (canAddNote && button("atlas-new-note", canvasRect(711, 89, 110, 25),
                             m_placingNote ? Engine::tr("Cancel pin") : Engine::tr("Add note")))
    {
        m_placingNote = !m_placingNote;
        m_searchEditing = false;
        setTextEditing(false);
    }
    if (m_placingNote)
        textInRect(canvasRect(34, 407, 778, 43), Engine::tr("Click or tap the map to place your note. Escape cancels."),
                   "menu_arrus", 12);
    drawMap();
    if (m_featureView)
        drawFeatureList();
    else
        drawList();
    if (!m_placingNote) drawDetails();
}

void AtlasScreen::indexMapData(MapAssetInfo &map)
{
    if (updateWorldMapLayoutKnowledge(m_knowledge, map.map, map.layoutIdentity))
    {
        rebuildWorld();
        m_indexedMaps.insert(WorldDatabase::mapId(map.map));
        m_floorGeometryKey.clear();
        m_floorPlanKey.clear();
        m_fogMap.clear();
    }

    const std::optional<MapDeltaData> &delta = map.outdoorMapData ? map.outdoorMapDeltaData : map.indoorMapDeltaData;
    if (delta)
        m_world.indexMapItemSources(map.map, buildWorldItemSources(*delta, m_data.itemTable(), m_data.objectTable()),
                                    m_data.itemTable());
    std::string error;
    if (!GameDataLoader::loadEventPrograms(assetFileSystem(), map, error, m_rulesetRoot) || !map.localEventProgram)
        throw std::runtime_error(error.empty() ? "Missing event metadata" : error);
    m_world.indexMapConnections(map.map, *map.localEventProgram);
    m_world.indexMapNpcTopics(map.map, *map.localEventProgram);
    m_world.indexMapNpcEncounters(map.map, *map.localEventProgram);
    const auto &observations = m_knowledge.monsterObservations[toLowerCopy(map.map.fileName)];
    if (map.outdoorMapData)
    {
        m_world.indexMapLocations(*map.outdoorMapData, *map.localEventProgram);
        m_world.indexMapMonsters(map.map, *map.outdoorMapData, m_data.monsterTable(), observations,
                                 delta ? &*delta : nullptr);
        m_world.indexMapItemRewards(map.map, *map.localEventProgram, worldInteractionSites(*map.outdoorMapData),
                                    m_data.itemTable());
    }
    else if (map.indoorMapData)
    {
        m_world.indexMapLocations(map.map, *map.indoorMapData, *map.localEventProgram);
        m_world.indexMapMonsters(map.map, *map.indoorMapData, m_data.monsterTable(), observations,
                                 delta ? &*delta : nullptr);
        m_world.indexMapItemRewards(map.map, *map.localEventProgram, worldInteractionSites(*map.indoorMapData),
                                    m_data.itemTable());
    }
    else
        throw std::runtime_error("Missing map geometry");
    if (delta) m_world.indexMapNpcActors(map.map, *delta);
    m_world.indexMapInteractionReferences(map.map, m_data.itemTable(), m_data.spellTable(), m_data.journalQuestTable());
    m_world.indexMapMonsterSpawns(map.map, *map.localEventProgram, m_data.monsterTable());
    if (map.localEventProgram) m_world.indexNpcLocations(*map.localEventProgram);
    if (map.globalEventProgram)
    {
        if (m_decorationTable)
        {
            const auto sites =
                map.outdoorMapData
                    ? worldInheritedDecorationSites(*map.outdoorMapData, *m_decorationTable, delta ? &*delta : nullptr)
                    : worldInheritedDecorationSites(*map.indoorMapData, *m_decorationTable, delta ? &*delta : nullptr);
            m_world.indexInheritedDecorations(map.map, sites, *map.globalEventProgram);
            m_world.indexDecorationItemRewards(map.map, sites, *map.globalEventProgram, m_data.itemTable());
        }
        m_world.indexSharedItemRewards(*map.globalEventProgram, m_data.itemTable());
        m_world.indexNpcLocations(*map.globalEventProgram);
    }
}

void AtlasScreen::indexKnownRegions()
{
    // Indexing event metadata can append possible residents; retain stable table pointers across it.
    const bool searchDungeons =
        m_guide && (!m_search.empty() || m_filter == WorldRecordKind::Npc || m_filter == WorldRecordKind::Trainer);
    std::vector<const MapStatsEntry *> maps;
    for (const WorldRecord &record : m_world.records())
        if ((record.kind == WorldRecordKind::Region || (record.kind == WorldRecordKind::Dungeon && !m_connectionView &&
                                                        (record.id == m_map || searchDungeons))) &&
            m_world.visible(record, m_knowledge, m_guide) && !m_indexedMaps.contains(record.id))
            maps.push_back(record.map);
    m_pendingMapIndexes = maps.size();
    if (maps.empty()) return;
    const auto selected = std::find_if(maps.begin(), maps.end(), [this](const MapStatsEntry *pMap)
                                       { return WorldDatabase::mapId(*pMap) == m_map; });
    const MapStatsEntry *pMap = selected != maps.end() ? *selected : maps.front();
    // Keep the selected map first and bound synchronous indexing to one map per frame.
    m_indexedMaps.insert(WorldDatabase::mapId(*pMap));
    --m_pendingMapIndexes;
    try
    {
        auto map = MapAssetLoader().load(assetFileSystem(), *pMap, m_data.monsterTable(), m_data.objectTable(),
                                         MapLoadPurpose::HeadlessGameplay,
                                         MapCompanionLoadOptions{.rulesetRoot = m_rulesetRoot});
        if (!map) throw std::runtime_error("Missing map data");
        indexMapData(*map);
    }
    catch (const std::exception &error)
    {
        m_locationError = Engine::tr("Some map markers are unavailable in this content package.");
        std::cerr << "World locations could not be indexed for " << pMap->fileName << ": " << error.what() << '\n';
    }
    if (m_pendingMapIndexes == 0 && !m_classComparison) m_referenceDirty = true;
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
    for (const WorldMapLocation &location :
         m_world.queryLocations(m_knowledge, m_guide, map.id, pFloorPlan != nullptr ? m_floorLayer : std::nullopt))
    {
        auto records = m_world.recordsAtLocation(location, m_knowledge, m_guide, m_filter == WorldRecordKind::Travel);
        std::erase_if(records,
                      [this](const WorldRecord *pRecord)
                      {
                          return (m_filter && pRecord->kind != *m_filter) ||
                                 (m_trainerMastery != 0 && pRecord->teacher != nullptr &&
                                  pRecord->teacher->mastery != m_trainerMastery) ||
                                 (!m_search.empty() &&
                                  Engine::Utf8::lower(m_world.recordSearchText(*pRecord, m_knowledge, m_guide, m_map))
                                          .find(Engine::Utf8::lower(m_search)) == std::string::npos);
                      });
        if (records.empty()) continue;
        const auto point = pFloorPlan != nullptr ? pFloorPlan->worldToUv(location.x, location.y) : [&]()
        {
            const auto uv = gameplayMinimapWorldToUv({}, location.x, location.y, false);
            return std::pair{uv.x, uv.y};
        }();
        if (point.first < 0 || point.first > 1 || point.second < 0 || point.second > 1) continue;
        const float x = image.x + point.first * image.width, y = image.y + point.second * image.height;
        if (x < viewport.x || x > viewport.x + viewport.width || y < viewport.y || y > viewport.y + viewport.height)
            continue;
        auto pin = std::find_if(pins.begin(), pins.end(),
                                [&](const Pin &other) { return std::hypot(x - other.x, y - other.y) < spacing; });
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
                                      : house != pin.records.end() ? *house
                                                                   : pin.records.front();
        const bool dungeon = pPrimary->kind == WorldRecordKind::Dungeon;
        const bool transport = pPrimary->kind == WorldRecordKind::Travel;
        const char *label = transport                      ? (toLowerCopy(pPrimary->house->type) == "boats" ? "B" : "C")
                            : dungeon                      ? "D"
                            : trainer != pin.records.end() ? "T"
                            : pPrimary->npc != nullptr     ? "N"
                                                           : "B";
        const std::string caption =
            pin.sites > 1 ? Engine::tr("{sites} locations - select to inspect", {{"sites", std::to_string(pin.sites)}})
                          : pPrimary->name();
        const Rect marker{pin.x - 6 * designScale(), pin.y - 6 * designScale(), 12 * designScale(), 12 * designScale()};
        const Rect hitArea{pin.x - 12 * designScale(), pin.y - 12 * designScale(), 24 * designScale(),
                           24 * designScale()};
        const std::string buttonId = "atlas-site-" + pin.id;
        hovered = hovered || pointerInside(hitArea);
        if (button(buttonId, hitArea, "", "button_square", true, false, "fondamento", &marker))
        {
            if (dungeon && pin.records.size() == 1)
            {
                select(*pPrimary);
                return true;
            }
            m_siteSelection.clear();
            for (const WorldRecord *pRecord : pin.records) m_siteSelection.insert(pRecord->id);
            m_selected = pPrimary->id;
            m_scroll = 0;
        }
        textInRect(marker, pin.sites > 1 ? std::to_string(pin.sites) : label, "menu_lucida", 8, 0xffc6dfeau, true);
        outline(marker, trainer != pin.records.end() ? 0xff96c798u : dungeon ? 0xff8dc5e1u : 0xff8dc8dfu);
        if (pointerInside(hitArea) || focused(buttonId))
        {
            Rect text{pin.x - 105 * designScale(), pin.y - 35 * designScale(), 210 * designScale(), 20 * designScale()};
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
                if ((links[j].sourceId == link.sourceId && links[j].destinationId == link.destinationId) ||
                    (links[j].sourceId == link.destinationId && links[j].destinationId == link.sourceId))
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
            x1 -= dy * lane;
            x2 -= dy * lane;
            y1 += dx * lane;
            y2 += dx * lane;
            const bool selected = m_selected == link.recordId;
            const uint32_t baseColor = selected                              ? 0xff8dc5e1u
                                       : link.locked                         ? 0xff53698bu
                                       : link.kind == WorldTravelKind::Boat  ? 0xffcfb487u
                                       : link.kind == WorldTravelKind::Coach ? 0xff8ebed6u
                                                                             : 0xff91a990u;
            const uint32_t alpha = selected ? 0xff000000u : selectedRoute ? 0x55000000u : 0x99000000u;
            const uint32_t color = (baseColor & 0x00ffffffu) | alpha;
            const float thickness = (selected ? 2.5f : 1.0f) * designScale();
            const auto segment = [&](float startX, float startY, float endX, float endY, float stroke)
            {
                // Keep routes readable over snow, terrain and the schematic's region thumbnails.
                if (selected) drawLine(startX, startY, endX, endY, stroke + 1.4f * designScale(), 0xe0161712u);
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
                    segment(x1 + dx * offset, y1 + dy * offset, x1 + dx * end, y1 + dy * end, thickness);
                }
            }
            const float tipX = x1 + (x2 - x1) * 0.62f, tipY = y1 + (y2 - y1) * 0.62f;
            const float arrow = (selected ? 7.0f : 5.0f) * designScale();
            segment(tipX, tipY, tipX - dx * arrow - dy * arrow / 2, tipY - dy * arrow + dx * arrow / 2, designScale());
            segment(tipX, tipY, tipX - dx * arrow + dy * arrow / 2, tipY - dy * arrow - dx * arrow / 2, designScale());
        }
    }
}

void AtlasScreen::drawMap()
{
    const WorldRecord *map = m_world.find(m_map);
    const bool dungeon = map != nullptr && map->kind == WorldRecordKind::Dungeon;
    const Rect frame = canvasRect(25, 121, 544, 270);
    const Rect viewport = canvasRect(25, 121, 544, dungeon ? 234 : 270);
    drawSolidRect(frame, 0xff18160fu);
    outline(frame, 0xff597284u);
    if (dungeon) drawSolidRect(canvasRect(25, 354, 544, 1), 0xff597284u);
    const bool schematic = map == nullptr && m_presentation.overviewTexture.empty();
    const std::string texture = map == nullptr ? m_presentation.overviewTexture : regionTexture(*map);
    const WorldFloorPlanRaster *floorPlan = nullptr;
    std::optional<TextureSize> size;
    std::string floorPlanKey;
    if (dungeon)
    {
        const std::string context = map->id + (m_guide ? ":guide" : ":discovery");
        if (m_floorLayerContext != context)
        {
            m_floorLayerContext = context;
            m_floorLayer.reset();
            for (const WorldMapFeature &feature : m_world.features())
                if (feature.id == m_selectedFeature &&
                    (feature.kind != WorldMapFeatureKind::Note || feature.noteElevation) &&
                    m_world.featureVisible(feature, m_knowledge, m_guide))
                {
                    const auto sites = m_world.featureSites(feature, m_knowledge, m_guide);
                    if (!sites.empty()) m_floorLayer = worldFloorPlanLayerForHeight(sites.front()->z, true);
                }
        }
        floorPlanKey = context + (m_floorLayer ? ":" + m_floorLayer->name() : ":all");
        if (m_floorPlanKey != floorPlanKey)
        {
            m_floorPlanKey = floorPlanKey;
            m_floorPlan = {};
            m_floorLayers.clear();
            if (bgfx::isValid(m_floorPlanTexture)) bgfx::destroy(m_floorPlanTexture);
            m_floorPlanTexture = BGFX_INVALID_HANDLE;
            if (m_floorGeometryKey != map->id)
            {
                m_floorGeometryKey = map->id;
                m_floorGeometry.reset();
                if (m_pCurrentMap != nullptr && WorldDatabase::mapId(m_pCurrentMap->map) == map->id)
                    m_floorGeometry = m_pCurrentMap->indoorMapData;
                else
                {
                    // Use the active scene/overlays, rather than bypassing 2.5's map loader with raw BLV geometry.
                    std::optional<MapAssetInfo> loaded = MapAssetLoader().load(
                        assetFileSystem(), *map->map, m_data.monsterTable(), m_data.objectTable(),
                        MapLoadPurpose::HeadlessGameplay, MapCompanionLoadOptions{.rulesetRoot = m_rulesetRoot});
                    if (loaded) m_floorGeometry = std::move(loaded->indoorMapData);
                }
            }
            const std::optional<IndoorMapData> &geometry = m_floorGeometry;
            if (geometry)
            {
                const auto knowledge = m_knowledge.floorPlans.find(toLowerCopy(map->map->fileName));
                std::vector<std::pair<float, float>> positions;
                for (const WorldMapFeature *pFeature : m_world.queryFeatures(m_knowledge, m_guide, "", map->id,
                                                                             &m_data.spellTable(), &m_data.itemTable()))
                    for (const WorldInteractionSite *pSite : m_world.featureSites(*pFeature, m_knowledge, m_guide))
                        positions.emplace_back(pSite->x, pSite->y);
                if (m_knowledge.partyPosition &&
                    m_knowledge.partyPosition->mapFileName == toLowerCopy(map->map->fileName))
                    positions.emplace_back(m_knowledge.partyPosition->x, m_knowledge.partyPosition->y);
                m_floorPlan = buildWorldFloorPlan(*geometry,
                                                  knowledge == m_knowledge.floorPlans.end() ? WorldFloorPlanKnowledge{}
                                                                                            : knowledge->second,
                                                  m_guide, positions, m_floorLayer);
                m_floorLayer = m_floorPlan.selectedLayer;
                m_floorPlanKey = context + (m_floorLayer ? ":" + m_floorLayer->name() : ":all");
                m_floorLayers = m_floorPlan.layers;
                for (const WorldMapFeature *pFeature : m_world.queryFeatures(m_knowledge, m_guide, "", map->id,
                                                                             &m_data.spellTable(), &m_data.itemTable()))
                    if (pFeature->kind != WorldMapFeatureKind::Note || pFeature->noteElevation)
                        for (const WorldInteractionSite *pSite : m_world.featureSites(*pFeature, m_knowledge, m_guide))
                            if (const auto layer = m_floorPlan.layerAtHeight(pSite->z)) m_floorLayers.push_back(*layer);
                if (m_knowledge.partyPosition && m_knowledge.partyPosition->z &&
                    m_knowledge.partyPosition->mapFileName == toLowerCopy(map->map->fileName))
                    if (const auto layer = m_floorPlan.layerAtHeight(*m_knowledge.partyPosition->z))
                        m_floorLayers.push_back(*layer);
                std::sort(m_floorLayers.begin(), m_floorLayers.end());
                m_floorLayers.erase(std::unique(m_floorLayers.begin(), m_floorLayers.end()), m_floorLayers.end());
            }
            if (!m_floorPlan.pixelsBgra.empty())
                m_floorPlanTexture = bgfx::createTexture2D(
                    uint16_t(m_floorPlan.width), uint16_t(m_floorPlan.height), false, 1, bgfx::TextureFormat::BGRA8,
                    BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
                    bgfx::copy(m_floorPlan.pixelsBgra.data(), uint32_t(m_floorPlan.pixelsBgra.size())));
        }
        floorPlan = &m_floorPlan;
        if (floorPlan->pixelsBgra.empty())
        {
            textInRect(canvasRect(66, 185, 466, 115),
                       m_guide ? Engine::tr("No dungeon geometry is available in this content package.")
                               : Engine::tr("No explored floor plan yet. Explore this dungeon to reveal its map."),
                       "menu_arrus", 14, 0xffc6dfeau, true);
            return;
        }
        size = TextureSize{float(floorPlan->width), float(floorPlan->height)};
    }
    else
    {
        m_floorLayer.reset();
        m_floorLayerContext.clear();
        size = schematic ? std::optional<TextureSize>(TextureSize{1000, 600}) : textureSize(texture);
    }
    if (!size)
    {
        textInRect(canvasRect(66, 185, 466, 115),
                   Engine::tr("The region's map image is unavailable in this content package."), "menu_arrus", 14,
                   0xffc6dfeau, true);
        return;
    }
    if (pointerInside(viewport) && mouseWheelDelta() != 0)
        m_zoom = std::clamp(m_zoom + mouseWheelDelta() * 0.2f, 1.0f, 3.0f);
    const float fit = std::min(viewport.width / size->width, viewport.height / size->height);
    const float width = size->width * fit * m_zoom, height = size->height * fit * m_zoom;
    m_panX =
        std::clamp(m_panX, -std::max(0.0f, (width - viewport.width) / 2), std::max(0.0f, (width - viewport.width) / 2));
    m_panY = std::clamp(m_panY, -std::max(0.0f, (height - viewport.height) / 2),
                        std::max(0.0f, (height - viewport.height) / 2));
    const Rect image{viewport.x + (viewport.width - width) / 2 + m_panX,
                     viewport.y + (viewport.height - height) / 2 + m_panY, width, height};
    setDesignClip(viewport);
    if (floorPlan != nullptr)
        drawTextureHandle(m_floorPlanTexture, image);
    else if (!schematic)
        drawTexture(texture, image);
    if (floorPlan != nullptr && floorPlan->outlineCount == 0)
        textInRect(canvasRect(34, 126, 200, 34),
                   m_floorLayer ? Engine::tr("No mapped outlines at this height.")
                                : Engine::tr("Recorded positions only.\nExplore to reveal dungeon outlines."),
                   "menu_lucida", 10);
    if (map != nullptr && !dungeon && !m_guide)
    {
        if (m_fogMap != map->id)
        {
            m_fogMap = map->id;
            if (bgfx::isValid(m_fogTexture)) bgfx::destroy(m_fogTexture);
            const auto knowledge = m_knowledge.regionMaps.find(toLowerCopy(map->map->fileName));
            const auto fog = buildWorldRegionFog(knowledge == m_knowledge.regionMaps.end() ? WorldRegionMapKnowledge{}
                                                                                           : knowledge->second,
                                                 GameplayMinimapState{});
            m_fogTexture =
                bgfx::createTexture2D(WorldRegionFog::Size, WorldRegionFog::Size, false, 1, bgfx::TextureFormat::BGRA8,
                                      BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT |
                                          BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT,
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
        if (m_filter == WorldRecordKind::Travel) drawTravelLinks(image);
        for (const auto &region : m_world.records())
        {
            if (!region.position || !m_world.visible(region, m_knowledge, m_guide)) continue;
            const float x = image.x + region.position->first * image.width;
            const float y = image.y + region.position->second * image.height;
            float tileHeight = 50;
            if (schematic)
            {
                for (const WorldRecord &other : m_world.records())
                    if (other.position && other.id != region.id && m_world.visible(other, m_knowledge, m_guide) &&
                        std::abs(other.position->first - region.position->first) < 0.08f)
                    {
                        const float distance =
                            std::abs(other.position->second - region.position->second) * image.height / designScale();
                        tileHeight = std::min(tileHeight, std::max(16.0f, distance - 18));
                    }
                const Rect tile{x - 33 * designScale(), y - tileHeight / 2 * designScale(), 66 * designScale(),
                                tileHeight * designScale()};
                if (m_guide && region.kind == WorldRecordKind::Region)
                {
                    const std::string regionalTexture = regionTexture(region);
                    if (textureSize(regionalTexture)) drawTexture(regionalTexture, tile);
                }
                outline(tile, 0xff597284u);
                textInRect({tile.x - 15 * designScale(), tile.y + tile.height, tile.width + 30 * designScale(),
                            16 * designScale()},
                           Engine::localizeKnown(region.name(), "world:" + m_presentation.worldId), "menu_lucida", 8,
                           0xffc6dfeau, true);
            }
            const Rect marker{x - 5 * designScale(), y - 5 * designScale(), 10 * designScale(), 10 * designScale()};
            const Rect hitArea{x - (schematic ? 33 : 14) * designScale(),
                               y - (schematic ? tileHeight / 2 : 14) * designScale(),
                               (schematic ? 66 : 28) * designScale(), (schematic ? tileHeight : 28) * designScale()};
            markerHovered = markerHovered || pointerInside(hitArea);
            if (button("atlas-pin-" + region.id, hitArea, "", "button_square", true, false, "fondamento", &marker))
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
                textInRect(caption, Engine::localizeKnown(region.name(), "world:" + m_presentation.worldId),
                           "menu_arrus", 12, 0xffc6dfeau, true);
            }
        }
    }
    if (map != nullptr) drawPartyArrow(*map, image, floorPlan);
    const float zoomX = schematic ? 34 : 490;
    const Rect zoomControls = canvasRect(zoomX, 357, 67, 27);
    const Rect layerControls = canvasRect(34, 357, 356, 27);
    const bool overLayerControls = dungeon && pointerInside(layerControls);
    const GameplayInputFrame &input = inputFrame();
    const bool touchDragging = input.mobileTouchDragActive || input.mobileTouchDragReleased;
    if (m_placingNote && map != nullptr && leftMouseJustPressed() && pointerInside(viewport) && pointerInside(image) &&
        !pointerInside(canvasRect(490, 357, 67, 27)) && !overLayerControls)
    {
        const float u = (mouseX() - image.x) / image.width, v = (mouseY() - image.y) / image.height;
        const auto point = floorPlan != nullptr ? floorPlan->uvToWorld(u, v) : [&]()
        {
            const GameplayMinimapPoint world = gameplayMinimapUvToWorld({}, u, v);
            return std::pair{world.x, world.y};
        }();
        m_noteDraft = {
            map->map->fileName, 0, int32_t(std::lround(point.first)), int32_t(std::lround(point.second)), {}, false};
        if (floorPlan != nullptr && m_floorLayer) m_noteDraft.elevation = worldFloorPlanLayerElevation(*m_floorLayer);
        m_noteError.clear();
        m_placingNote = false;
        m_noteEditorOpen = true;
        m_dragging = false;
        setTextEditing(true);
    }
    else if (touchDragging && input.mobileTouchDragStarted)
    {
        const auto startsInside = [&input](const Rect &rect)
        {
            return input.mobileTouchDragStartX >= rect.x && input.mobileTouchDragStartX < rect.x + rect.width &&
                   input.mobileTouchDragStartY >= rect.y && input.mobileTouchDragStartY < rect.y + rect.height;
        };
        // A recognized drag pans even when the finger originally pressed a pin. Buttons cancel that tap.
        m_dragging = !m_noteEditorOpen && !m_placingNote && startsInside(viewport) && !startsInside(zoomControls) &&
                     !(dungeon && startsInside(layerControls));
        m_dragX = input.mobileTouchDragStartX;
        m_dragY = input.mobileTouchDragStartY;
    }
    else if (!touchDragging && !m_noteEditorOpen && leftMouseJustPressed() && pointerInside(viewport) &&
             !markerHovered && !overLayerControls && !pointerInside(zoomControls))
    {
        m_dragging = true;
        m_dragX = mouseX();
        m_dragY = mouseY();
    }
    if (m_dragging && (leftMouseDown() || input.mobileTouchDragReleased))
    {
        m_panX += mouseX() - m_dragX;
        m_panY += mouseY() - m_dragY;
        m_dragX = mouseX();
        m_dragY = mouseY();
    }
    if (!leftMouseDown()) m_dragging = false;
    setDesignClip(std::nullopt);
    if (button("atlas-zoom-out", canvasRect(zoomX, 357, 30, 27), Engine::tr("-")))
        m_zoom = std::max(1.0f, m_zoom - 0.25f);
    if (button("atlas-zoom-in", canvasRect(zoomX + 37, 357, 30, 27), Engine::tr("+")))
        m_zoom = std::min(3.0f, m_zoom + 0.25f);
    if (dungeon) drawFloorLayers(*map);
}

void AtlasScreen::selectFloorLayer(std::optional<WorldFloorPlanLayer> layer)
{
    if (m_floorLayer == layer) return;
    m_floorLayer = layer;
    m_floorPlanKey.clear();
    m_siteSelection.clear();
    m_selectedFeature.clear();
    m_selected.clear();
    m_scroll = 0;
    m_dragging = false;
}

void AtlasScreen::drawFloorLayers(const WorldRecord &map)
{
    const auto lower = m_floorLayer ? std::lower_bound(m_floorLayers.begin(), m_floorLayers.end(), *m_floorLayer)
                                    : m_floorLayers.end();
    const auto higher = m_floorLayer ? std::upper_bound(m_floorLayers.begin(), m_floorLayers.end(), *m_floorLayer)
                                     : m_floorLayers.begin();
    if (button("atlas-layer-lower", canvasRect(34, 357, 43, 27), Engine::tr("Lower"), "button",
               lower != m_floorLayers.begin()))
        selectFloorLayer(*std::prev(lower));
    textInRect(canvasRect(81, 357, 110, 27),
               m_floorLayer ? Engine::tr("Layer {name}", {{"name", Engine::localizeKnown(m_floorLayer->name())}})
                            : Engine::tr("All heights"),
               "menu_lucida", 11, 0xffc6dfeau, true);
    if (button("atlas-layer-higher", canvasRect(195, 357, 43, 27), Engine::tr("Higher"), "button",
               higher != m_floorLayers.end()))
        selectFloorLayer(*higher);
    std::optional<WorldFloorPlanLayer> partyLayer;
    if (m_knowledge.partyPosition && m_knowledge.partyPosition->z &&
        m_knowledge.partyPosition->mapFileName == toLowerCopy(map.map->fileName))
        partyLayer = m_floorPlan.layerAtHeight(*m_knowledge.partyPosition->z);
    if (button("atlas-layer-all", canvasRect(242, 357, 48, 27), Engine::tr("All"), "button", m_floorLayer.has_value()))
        selectFloorLayer(std::nullopt);
    if (button("atlas-layer-party", canvasRect(294, 357, 92, 27), Engine::tr("Party height"), "button",
               partyLayer.has_value()))
        selectFloorLayer(partyLayer);
}

void AtlasScreen::drawPartyArrow(const WorldRecord &map, const Rect &image, const WorldFloorPlanRaster *floorPlan)
{
    if (!m_knowledge.partyPosition || toLowerCopy(map.map->fileName) != m_knowledge.partyPosition->mapFileName) return;
    const auto &party = *m_knowledge.partyPosition;
    if (floorPlan != nullptr && m_floorLayer && (!party.z || !worldFloorPlanLayerMatches(m_floorLayer, *party.z)))
        return;
    std::pair<float, float> uv;
    if (floorPlan != nullptr)
        uv = floorPlan->worldToUv(party.x, party.y);
    else
    {
        const auto point = gameplayMinimapWorldToUv(GameplayMinimapState{}, party.x, party.y, false);
        uv = {point.x, point.y};
    }
    if (uv.first < 0 || uv.first > 1 || uv.second < 0 || uv.second > 1) return;
    const std::string arrow = "MAPDIR" + std::to_string(gameplayMinimapArrowIndex(party.yawRadians) + 1);
    if (const auto size = textureSize(arrow))
    {
        const float width = 14 * designScale(), height = width * size->height / std::max(1.0f, size->width);
        drawTexture(arrow, {image.x + uv.first * image.width - width / 2,
                            image.y + uv.second * image.height - height / 2, width, height});
    }
}

void AtlasScreen::drawList()
{
    const Rect viewport = canvasRect(588, 121, 235, 270);
    auto records = m_world.query(m_knowledge, m_guide, m_search, m_filter, m_map, m_filter == WorldRecordKind::Travel);
    if (m_trainerMastery != 0)
        std::erase_if(records, [this](const WorldRecord *pRecord)
                      { return pRecord->teacher != nullptr && pRecord->teacher->mastery != m_trainerMastery; });
    if (m_floorLayer)
    {
        std::unordered_set<std::string> located, onLayer;
        for (const WorldMapLocation &location : m_world.queryLocations(m_knowledge, m_guide, m_map))
            for (const WorldRecord *pRecord :
                 m_world.recordsAtLocation(location, m_knowledge, m_guide, m_filter == WorldRecordKind::Travel))
                located.insert(pRecord->id);
        for (const WorldMapLocation &location : m_world.queryLocations(m_knowledge, m_guide, m_map, m_floorLayer))
            for (const WorldRecord *pRecord :
                 m_world.recordsAtLocation(location, m_knowledge, m_guide, m_filter == WorldRecordKind::Travel))
                onLayer.insert(pRecord->id);
        std::erase_if(records, [&](const WorldRecord *pRecord)
                      { return located.contains(pRecord->id) && !onLayer.contains(pRecord->id); });
    }
    if (!m_siteSelection.empty())
        std::erase_if(records, [this](const WorldRecord *pRecord) { return !m_siteSelection.contains(pRecord->id); });
    if (m_map.empty() && m_search.empty() && !m_filter)
        std::erase_if(records,
                      [](const WorldRecord *record)
                      {
                          return !record->position ||
                                 (record->kind != WorldRecordKind::Region && record->kind != WorldRecordKind::Dungeon);
                      });
    scrollViewport(viewport, records.size() * 38, m_scroll, 38);
    setDesignClip(viewport);
    for (size_t i = 0; i < records.size(); ++i)
    {
        const Rect row{viewport.x, viewport.y + (i * 38 - m_scroll) * designScale(),
                       viewport.width - 12 * designScale(), 34 * designScale()};
        const auto &record = *records[i];
        const MapStatsEntry *pRecordMap = m_world.recordMap(record, m_knowledge, m_guide, m_map);
        if (pRecordMap == nullptr) continue;
        if (button("atlas-record-" + record.id, row, "", "button", true, m_selected == record.id))
        {
            select(record);
            break;
        }
        if (m_selected == record.id) outline(row, 0xff8dc5e1u);
        textInRect(
            {row.x + 7 * designScale(), row.y + 2 * designScale(), row.width - 14 * designScale(), 18 * designScale()},
            m_world.recordDisplayName(record, m_knowledge, m_guide), "menu_arrus", 12);
        const std::string subtitle =
            record.kind == WorldRecordKind::Travel
                ? Engine::tr("From {name} - {base}{travelDays}",
                             {{"name", Engine::localizeKnown(pRecordMap->name)},
                              {"base", (record.walkingRoute != nullptr ? ""
                                        : record.transport->quote      ? Engine::tr("quoted ")
                                                                       : Engine::tr("base "))},
                              {"travelDays", atlasTravelDays(record.walkingRoute ? record.walkingRoute->travelDays
                                                             : record.transport->quote
                                                                 ? record.transport->quote->travelDays
                                                                 : int64_t(record.transport->route.travelDays))}})
                : Engine::tr(WorldDatabase::kindName(record.kind)) +
                      (record.house != nullptr ? " - " + pRecordMap->name : "");
        textInRect(
            {row.x + 7 * designScale(), row.y + 19 * designScale(), row.width - 14 * designScale(), 12 * designScale()},
            subtitle, "menu_lucida", 9, 0xffa9bbb7u);
    }
    revealFocus(viewport, m_scroll);
    setDesignClip(std::nullopt);
    if (records.empty())
        textInRect(viewport,
                   m_guide ? Engine::tr("No matching records.")
                   : m_filter == WorldRecordKind::Travel
                       ? Engine::tr("No known routes.\nVisit destinations and ask transport agents for their offers.")
                       : Engine::tr("No matching discoveries.\nVisit places and meet their residents to reveal them."),
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
    for (const WorldMapFeature *pFeature :
         m_world.queryFeatures(m_knowledge, m_guide, m_search, map.id, &m_data.spellTable(), &m_data.itemTable()))
    {
        const auto sites =
            m_world.featureSites(*pFeature, m_knowledge, m_guide, pFloorPlan != nullptr ? m_floorLayer : std::nullopt);
        if (sites.empty()) continue;
        const WorldInteractionSite &site = *sites.front();
        const auto point = pFloorPlan != nullptr ? pFloorPlan->worldToUv(site.x, site.y) : [&]()
        {
            const auto uv = gameplayMinimapWorldToUv({}, site.x, site.y, false);
            return std::pair{uv.x, uv.y};
        }();
        if (point.first < 0 || point.first > 1 || point.second < 0 || point.second > 1) continue;
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
        const Rect marker{pin.x - 5 * designScale(), pin.y - 5 * designScale(), 10 * designScale(), 10 * designScale()};
        const Rect hit{pin.x - 12 * designScale(), pin.y - 12 * designScale(), 24 * designScale(), 24 * designScale()};
        const std::string id = "atlas-feature-pin-" + feature.id;
        const bool selected =
            std::any_of(pin.features.begin(), pin.features.end(),
                        [this](const WorldMapFeature *pFeature) { return pFeature->id == m_selectedFeature; });
        hovered = hovered || pointerInside(hit);
        if (button(id, hit, "", "button_square", true, selected, "fondamento", &marker))
        {
            m_siteSelection.clear();
            for (const WorldMapFeature *pFeature : pin.features) m_siteSelection.insert(pFeature->id);
            m_selectedFeature = feature.id;
            m_scroll = 0;
        }
        const char *labels[] = {"C", "W", "S", "O", "P", "M", "N", "I", "D"};
        textInRect(marker,
                   pin.features.size() > 1 ? std::to_string(pin.features.size())
                   : feature.arrival       ? Engine::tr("A")
                                           : labels[size_t(feature.kind)],
                   "menu_lucida", 7, 0xffc6dfeau, true);
        outline(marker, selected ? 0xff99d3eeu : 0xff8dc5e1u);
        if (pointerInside(hit) || focused(id))
        {
            Rect caption{pin.x - 100 * designScale(), pin.y - 28 * designScale(), 200 * designScale(),
                         19 * designScale()};
            caption.x = std::clamp(caption.x, viewport.x, viewport.x + viewport.width - caption.width);
            caption.y = std::max(caption.y, viewport.y);
            drawSolidRect(caption, 0xed100e09u);
            textInRect(caption,
                       pin.features.size() > 1
                           ? Engine::tr("{size} sites", {{"size", std::to_string(pin.features.size())}})
                           : m_world.featureTitle(feature, m_knowledge, m_guide),
                       "menu_arrus", 11, 0xffc6dfeau, true);
        }
    }
    return hovered;
}

void AtlasScreen::drawFeatureList()
{
    const Rect viewport = canvasRect(588, 121, 235, 270);
    auto features =
        m_world.queryFeatures(m_knowledge, m_guide, m_search, m_map, &m_data.spellTable(), &m_data.itemTable());
    std::erase_if(features, [this](const WorldMapFeature *pFeature)
                  { return m_world.featureSites(*pFeature, m_knowledge, m_guide, m_floorLayer).empty(); });
    if (!m_siteSelection.empty())
        std::erase_if(features,
                      [this](const WorldMapFeature *pFeature) { return !m_siteSelection.contains(pFeature->id); });
    scrollViewport(viewport, features.size() * 38, m_scroll, 38);
    setDesignClip(viewport);
    for (size_t i = 0; i < features.size(); ++i)
    {
        const Rect row{viewport.x, viewport.y + (i * 38 - m_scroll) * designScale(),
                       viewport.width - 12 * designScale(), 34 * designScale()};
        if (row.y + row.height < viewport.y || row.y > viewport.y + viewport.height) continue;
        const WorldMapFeature &feature = *features[i];
        if (button("atlas-feature-" + feature.id, row, "", "button", true, m_selectedFeature == feature.id))
        {
            m_selectedFeature = feature.id;
            m_map = feature.mapId;
            m_panX = m_panY = 0;
            m_zoom = 1;
        }
        textInRect({row.x + 7 * designScale(), row.y, row.width - 14 * designScale(), 18 * designScale()},
                   m_world.featureTitle(feature, m_knowledge, m_guide), "menu_arrus", 12);
        const WorldRecord *pMap = m_world.find(feature.mapId);
        std::string subtitle = Engine::tr(worldMapFeatureKindName(feature.kind));
        if (feature.kind == WorldMapFeatureKind::Note && pMap->kind == WorldRecordKind::Dungeon)
        {
            const auto layer =
                feature.noteElevation
                    ? (feature.mapId == m_map ? m_floorPlan.layerAtHeight(float(*feature.noteElevation))
                                              : worldFloorPlanLayerForHeight(float(*feature.noteElevation)))
                    : std::nullopt;
            subtitle += layer ? Engine::tr(" | Layer {name}", {{"name", Engine::localizeKnown(layer->name())}})
                              : Engine::tr(" | All heights");
        }
        textInRect(
            {row.x + 7 * designScale(), row.y + 19 * designScale(), row.width - 14 * designScale(), 12 * designScale()},
            Engine::localizeKnown(subtitle + " - " + pMap->name(), "world:" + m_presentation.worldId), "menu_lucida", 9,
            0xffa9bbb7u);
    }
    revealFocus(viewport, m_scroll);
    setDesignClip(std::nullopt);
    if (features.empty())
        textInRect(
            viewport,
            m_floorLayer ? Engine::tr("No matching sites at this height.")
            : m_guide
                ? Engine::tr("No matching sites found.")
                : Engine::tr("No discovered sites here. Explore the region or interact with objects in the dungeon."),
            "menu_arrus", 13, 0xffc6dfeau, true);
}

void AtlasScreen::indexQuestLocations()
{
    WorldGuideSource source;
    source.pQuests = &m_data.journalQuestTable();
    source.pMaps = &m_data.mapStats();
    source.pHouses = &m_data.houseTable();
    source.worldId = m_presentation.worldId;
    for (const WorldGuideEntry &entry : buildWorldGuide(source, WorldGuideSection::Quests, m_referenceKnowledge, false))
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
    return {&m_data.itemTable(),
            &m_data.spellTable(),
            &m_data.classMultiplierTable(),
            &m_data.classSkillTable(),
            &m_data.mergedPotionSettingTable(),
            &m_data.mergedReagentSettingTable(),
            &m_data.potionMixingTable(),
            &m_data.potionNoteTable(),
            &m_data.rosterTable(),
            &m_data.houseTable(),
            &m_data.mapStats(),
            m_presentation.worldId,
            &m_data.monsterTable(),
            &m_data.journalQuestTable(),
            &m_data.npcDialogTable(),
            &m_world,
            &m_knowledge,
            &m_data.mergedCharacterSelectionTable(),
            m_pParty,
            m_pGlobalEvents,
            m_pLocalEvents};
}

void AtlasScreen::rebuildReference()
{
    m_classComparison.reset();
    m_referenceEntries = buildWorldGuide(referenceSource(), m_referenceSection, m_referenceKnowledge, m_guide);
    const auto selected =
        std::find_if(m_referenceEntries.begin(), m_referenceEntries.end(),
                     [this](const WorldGuideEntry &entry) { return entry.id == m_referenceSelected; });
    if (selected != m_referenceEntries.end())
        m_referenceScroll = std::distance(m_referenceEntries.begin(), selected) * 33;
    else
        m_referenceSelected.clear();
    m_referenceDirty = false;
}

void AtlasScreen::stepMapSourceScan()
{
    if (!m_sourceScanActive) return;
    const MapStatsEntry &record = *m_sourceMaps[m_sourceScanIndex++];
    try
    {
        auto map = MapAssetLoader().load(assetFileSystem(), record, m_data.monsterTable(), m_data.objectTable(),
                                         MapLoadPurpose::HeadlessGameplay,
                                         MapCompanionLoadOptions{.rulesetRoot = m_rulesetRoot});
        if (!map) throw std::runtime_error("Map data unavailable");
        const std::optional<MapDeltaData> &delta =
            map->outdoorMapData ? map->outdoorMapDeltaData : map->indoorMapDeltaData;
        if (!delta) throw std::runtime_error("Authored companion data unavailable");
        indexMapData(*map);
        m_indexedMaps.insert(WorldDatabase::mapId(record));
    }
    catch (const std::exception &error)
    {
        ++m_sourceScanFailures;
        m_locationError = Engine::tr("Some map sources could not be read; the source index is incomplete.");
        std::cerr << "Atlas map source scan failed for " << record.fileName << ": " << error.what() << '\n';
    }
    if (m_sourceScanIndex == m_sourceMaps.size())
    {
        m_sourceScanActive = false;
        m_sourcesComplete = true;
        m_floorPlanKey.clear();
        m_referenceDirty = true;
    }
}

void AtlasScreen::compareClass(std::optional<uint32_t> raceId)
{
    m_classComparison =
        buildWorldClassComparison(referenceSource(), m_referenceKnowledge, m_guide, m_referenceSelected, raceId);
    m_comparisonScroll = 0;
}

void AtlasScreen::drawClassComparison()
{
    const size_t raceCount = m_classComparison->races.size() + 1;
    size_t raceIndex = 0;
    std::string raceName = Engine::tr("Base class");
    for (size_t i = 0; i < m_classComparison->races.size(); ++i)
        if (m_classComparison->raceId == m_classComparison->races[i].first)
        {
            raceIndex = i + 1;
            raceName = m_classComparison->races[i].second;
        }
    const bool previousRace =
        button("atlas-compare-race-prev", canvasRect(285, 89, 25, 25), Engine::tr("<"), "button", raceCount > 1);
    textInRect(canvasRect(314, 89, 151, 25),
               Engine::localizeKnown(raceName == "DarkElf" ? "Dark Elf" : raceName, "race"), "menu_lucida", 11,
               0xffc6dfeau, true);
    const bool nextRace =
        button("atlas-compare-race-next", canvasRect(470, 89, 25, 25), Engine::tr(">"), "button", raceCount > 1);
    if (previousRace || nextRace)
    {
        raceIndex = (raceIndex + (nextRace ? 1 : raceCount - 1)) % raceCount;
        const std::optional<uint32_t> race =
            raceIndex == 0 ? std::nullopt : std::optional<uint32_t>(m_classComparison->races[raceIndex - 1].first);
        compareClass(race);
        return;
    }
    const size_t pageCount = std::max(size_t(1), (m_classComparison->classes.size() + 1) / 3);
    if (button("atlas-compare-page-prev", canvasRect(510, 89, 30, 25), Engine::tr("<"), "button", m_comparisonPage > 0))
        --m_comparisonPage;
    textInRect(
        canvasRect(544, 89, 120, 25),
        Engine::tr("Page {m_comparisonPage} / {pageCount}", {{"m_comparisonPage", std::to_string(m_comparisonPage + 1)},
                                                             {"pageCount", std::to_string(pageCount)}}),
        "menu_lucida", 11, 0xffc6dfeau, true);
    if (button("atlas-compare-page-next", canvasRect(670, 89, 30, 25), Engine::tr(">"), "button",
               m_comparisonPage + 1 < pageCount))
        ++m_comparisonPage;
    if (button("atlas-compare-details", canvasRect(711, 89, 110, 25), Engine::tr("Details")))
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
    textInRect(canvasRect(32, 121, 180, 32), Engine::tr("Skill / growth"), "fondamento", 13);
    for (size_t i = 0; i < columns.size(); ++i)
    {
        const WorldClassComparisonColumn &column = comparison.classes[columns[i]];
        if (button("atlas-compare-class-" + column.id, canvasRect(215 + i * columnWidth, 121, columnWidth - 3, 32),
                   Engine::localize(column.pDefinition->displayName, "class"), "button", true, i == 0))
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
        if (m_search.empty() || Engine::Utf8::lower(Engine::localize(displaySkillName(row.skill), "skill"))
                                        .find(Engine::Utf8::lower(m_search)) != std::string::npos)
            skills.push_back(&row);
    drawSolidRect(body, 0xff18160fu);
    scrollViewport(body, (skills.size() + 4) * 24, m_comparisonScroll);
    setDesignClip(body);
    const ClassMultiplierEntry &anchor = *comparison.classes.front().pDefinition;
    const std::pair<const char *, int ClassMultiplierEntry::*> growth[] = {
        {Engine::trNoop("Base HP").data(), &ClassMultiplierEntry::baseHealth},
        {Engine::trNoop("HP / level").data(), &ClassMultiplierEntry::healthPerLevel},
        {Engine::trNoop("Base SP").data(), &ClassMultiplierEntry::baseMana},
        {Engine::trNoop("SP / level").data(), &ClassMultiplierEntry::manaPerLevel}};
    for (size_t row = 0; row < skills.size() + 4; ++row)
    {
        const float y = 157 + row * 24 - m_comparisonScroll;
        const Rect rect = canvasRect(25, y, 786, 24);
        if (rect.y + rect.height < body.y || rect.y > body.y + body.height) continue;
        if (row % 2 == 0) drawSolidRect(rect, 0xff242019u);
        textInRect(canvasRect(32, y, 178, 24),
                   Engine::localizeKnown(row < 4 ? growth[row].first : displaySkillName(skills[row - 4]->skill),
                                         "world:" + m_presentation.worldId),
                   "menu_arrus", 11);
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
                value = cap == SkillMastery::None ? "-" : Engine::localize(masteryDisplayName(cap), "mastery");
                difference = int64_t(cap) - int64_t(skill.caps.front());
            }
            const uint32_t color = difference > 0 ? 0xff9bcea1u : difference < 0 ? 0xff83bce4u : 0xffc6dfeau;
            textInRect(canvasRect(215 + i * columnWidth, y, columnWidth - 3, 24), value, "menu_lucida", 11, color,
                       true);
        }
    }
    setDesignClip(std::nullopt);
    outline(body, 0xff597284u);
    textInRect(
        canvasRect(34, 403, 778, 54),
        Engine::tr(
            "First column stays fixed. Values follow the active ruleset; differences use that class as the baseline. "
            "Skills unavailable to all compared classes are omitted. "
            "Race/class creation and promotion quest restrictions still apply."),
        "menu_arrus", 11, 0xffc6dfeau, false, true);
}

bool AtlasScreen::drawReferenceItemIcon(const ItemDefinition &item, const Rect &box)
{
    if (item.iconVirtualPath.empty()) return false;
    const auto size = textureSize(item.iconVirtualPath);
    if (!size || size->width <= 0 || size->height <= 0) return false;
    const float fit = std::min(box.width / size->width, box.height / size->height);
    const float width = size->width * fit, height = size->height * fit;
    drawTexture(item.iconVirtualPath,
                {box.x + (box.width - width) / 2, box.y + (box.height - height) / 2, width, height});
    return true;
}

void AtlasScreen::drawItemComparison()
{
    textInRect(canvasRect(25, 89, 670, 25), Engine::tr("Base item comparison"), "fondamento", 14);
    if (button("atlas-item-compare-details", canvasRect(711, 89, 110, 25), Engine::tr("Details")))
    {
        m_itemComparison.reset();
        return;
    }
    for (size_t i = 0; i < 2; ++i)
    {
        const ItemDefinition &item = *m_itemComparison->items[i];
        const float x = 25 + i * 405;
        if (button("atlas-item-compare-column-" + std::to_string(i), canvasRect(x, 121, 393, 28), item.name))
        {
            m_referenceSelected = std::to_string(item.itemId);
            m_referenceDirty = true;
            m_search.clear();
            m_referenceDetailScroll = 0;
            m_itemComparison.reset();
            return;
        }
        const Rect viewport = canvasRect(x, 157, 393, 234);
        drawSolidRect(viewport, 0xff18160fu);
        scrollViewport(viewport, m_itemComparisonHeight[i], m_itemComparisonScroll[i]);
        setDesignClip(viewport);
        const float y = viewport.y + (8 - m_itemComparisonScroll[i]) * designScale();
        const bool icon = drawReferenceItemIcon(
            item, {viewport.x + 8 * designScale(), y, viewport.width - 24 * designScale(), 72 * designScale()});
        float height = 0;
        textInRect({viewport.x + 8 * designScale(), y + (icon ? 80 : 0) * designScale(),
                    viewport.width - 24 * designScale(), 50000 * designScale()},
                   m_itemComparison->details[i], "menu_arrus", 12, 0xffc6dfeau, false, true, &height);
        m_itemComparisonHeight[i] = (icon ? 80 : 0) + height / designScale() + 20;
        setDesignClip(std::nullopt);
        outline(viewport, 0xff597284u);
    }
    textInRect(
        canvasRect(34, 407, 778, 43),
        Engine::tr("Base definitions from the active game data. Character skills, enchantments and equipment condition "
                   "affect actual results. Scroll each column to read its full description."),
        "menu_arrus", 11);
}

void AtlasScreen::drawReference()
{
    stepMapSourceScan();
    const std::pair<const char *, WorldGuideSection> sections[] = {
        {Engine::trNoop("Items").data(), WorldGuideSection::Items},
        {Engine::trNoop("Spells").data(), WorldGuideSection::Spells},
        {Engine::trNoop("Classes").data(), WorldGuideSection::Classes},
        {Engine::trNoop("Alchemy").data(), WorldGuideSection::Alchemy},
        {Engine::trNoop("Companions").data(), WorldGuideSection::Companions},
        {Engine::trNoop("Progress").data(), WorldGuideSection::Progress},
        {Engine::trNoop("Bestiary").data(), WorldGuideSection::Monsters},
        {Engine::trNoop("Quests").data(), WorldGuideSection::Quests}};
    for (size_t i = 0; i < std::size(sections); ++i)
        if (button("atlas-ref-section-" + std::to_string(i), canvasRect(25 + i * 100, 53, 94, 28),
                   Engine::tr(sections[i].first), "button", true, m_referenceSection == sections[i].second))
        {
            m_referenceSection = sections[i].second;
            m_referenceSelected.clear();
            m_search.clear();
            m_pinnedItem.reset();
            m_itemComparison.reset();
            m_referenceDirty = true;
            m_referenceScroll = m_referenceDetailScroll = 0;
        }
    if (m_referenceDirty) rebuildReference();
    if (m_itemComparison)
    {
        drawItemComparison();
        return;
    }
    const Rect search = canvasRect(25, 89, 245, 25);
    drawSearchField("atlas-ref-search", search);
    textInRect(canvasRect(32, 91, 229, 20),
               m_search.empty()
                   ? (m_classComparison ? Engine::tr("Filter skills...") : Engine::tr("Search this section..."))
                   : m_search,
               "menu_lucida", 11);
    if (m_classComparison)
    {
        drawClassComparison();
        return;
    }
    const std::string query = Engine::Utf8::lower(m_search);
    const auto matches = [&query](const WorldGuideEntry &entry)
    {
        return query.empty() ||
               Engine::Utf8::lower(entry.title + " " + entry.details).find(query) != std::string::npos ||
               std::any_of(entry.links.begin(), entry.links.end(), [&](const WorldGuideLink &link)
                           { return Engine::Utf8::lower(link.label).find(query) != std::string::npos; });
    };
    const bool classes = m_referenceSection == WorldGuideSection::Classes;
    const bool items = m_referenceSection == WorldGuideSection::Items;
    const bool visibleItem =
        items && std::any_of(m_referenceEntries.begin(), m_referenceEntries.end(), [&](const WorldGuideEntry &entry)
                             { return entry.id == m_referenceSelected && matches(entry); });
    const std::optional<uint32_t> itemId = visibleItem ? atlasItemId(m_referenceSelected) : std::nullopt;
    const ItemDefinition *pSelectedItem = itemId ? m_data.itemTable().get(*itemId) : nullptr;
    if (items)
    {
        if (button("atlas-pin-item", canvasRect(475, 89, 105, 25),
                   itemId && m_pinnedItem == itemId ? Engine::tr("Unpin") : Engine::tr("Pin item"), "button",
                   pSelectedItem != nullptr && worldItemPropertiesKnown(*pSelectedItem, m_referenceKnowledge, m_guide)))
        {
            if (m_pinnedItem == itemId)
                m_pinnedItem.reset();
            else
                m_pinnedItem = itemId;
        }
        const auto comparison =
            m_pinnedItem && itemId
                ? buildWorldItemComparison(referenceSource(), m_referenceKnowledge, m_guide, *m_pinnedItem, *itemId)
                : std::nullopt;
        if (button("atlas-compare-items", canvasRect(589, 89, 110, 25), Engine::tr("Compare"), "button",
                   comparison.has_value()))
        {
            m_itemComparison = comparison;
            m_itemComparisonScroll = m_itemComparisonHeight = {};
            return;
        }
    }
    const bool scanSources =
        m_guide &&
        (m_referenceSection == WorldGuideSection::Items || m_referenceSection == WorldGuideSection::Monsters ||
         m_referenceSection == WorldGuideSection::Quests || m_referenceSection == WorldGuideSection::Companions);
    textInRect(canvasRect(285, 89,
                          items                    ? 180
                          : classes || scanSources ? 410
                                                   : 535,
                          25),
               m_pendingMapIndexes > 0 ? Engine::tr("Reading maps: {m_pendingMapIndexes}",
                                                    {{"m_pendingMapIndexes", std::to_string(m_pendingMapIndexes)}})
               : items
                   ? m_presentation.name
                   : Engine::tr("{name} - active game data", {{"name", Engine::localizeKnown(m_presentation.name)}}),
               "fondamento", 14);
    if (scanSources && button("atlas-map-sources", canvasRect(711, 89, 110, 25),
                              m_sourceScanActive ? Engine::tr("Scan {m_sourceScanIndex}/{size}",
                                                              {{"m_sourceScanIndex", std::to_string(m_sourceScanIndex)},
                                                               {"size", std::to_string(m_sourceMaps.size())}})
                              : m_sourceScanFailures > 0 ? Engine::tr("Partial sources")
                              : m_sourcesComplete        ? Engine::tr("Sources ready")
                                                         : Engine::tr("Scan sources"),
                              "button", !m_sourceScanActive && !m_sourcesComplete))
    {
        m_sourceMaps.clear();
        m_sourceScanFailures = 0;
        for (const WorldRecord &record : m_world.records())
            if (record.kind == WorldRecordKind::Region || record.kind == WorldRecordKind::Dungeon)
                m_sourceMaps.push_back(record.map);
        m_sourceScanIndex = 0;
        m_sourceScanActive = !m_sourceMaps.empty();
    }
    if (classes &&
        button("atlas-class-compare", canvasRect(711, 89, 110, 25), Engine::tr("Compare"), "button",
               std::any_of(m_referenceEntries.begin(), m_referenceEntries.end(),
                           [this](const WorldGuideEntry &entry) { return entry.id == m_referenceSelected; })))
    {
        m_search.clear();
        m_comparisonPage = 0;
        compareClass();
        return;
    }
    const Rect list = canvasRect(25, 121, 245, 270), details = canvasRect(285, 121, 535, 270);
    drawSolidRect(list, 0xff18160fu);
    outline(list, 0xff597284u);
    drawSolidRect(details, 0xff18160fu);
    outline(details, 0xff597284u);
    std::vector<const WorldGuideEntry *> entries;
    for (const WorldGuideEntry &entry : m_referenceEntries)
        if (matches(entry)) entries.push_back(&entry);
    if (!m_search.empty())
        std::stable_sort(entries.begin(), entries.end(),
                         [&query](const WorldGuideEntry *a, const WorldGuideEntry *b)
                         {
                             return (Engine::Utf8::lower(a->title).find(query) != std::string::npos) >
                                    (Engine::Utf8::lower(b->title).find(query) != std::string::npos);
                         });
    const float rowHeight = 33 * designScale();
    scrollViewport(list, entries.size() * 33, m_referenceScroll);
    setDesignClip(list);
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const Rect row{list.x + 3 * designScale(), list.y + i * rowHeight - m_referenceScroll * designScale(),
                       list.width - 16 * designScale(), rowHeight - 2 * designScale()};
        if (row.y + row.height < list.y || row.y > list.y + list.height) continue;
        const WorldGuideEntry &entry = *entries[i];
        if (button("atlas-ref-entry-" + entry.id, row, "", "button", true, m_referenceSelected == entry.id))
        {
            m_referenceSelected = entry.id;
            m_referenceDetailScroll = m_referenceContentHeight = 0;
        }
        textInRect({row.x + 6 * designScale(), row.y, row.width - 12 * designScale(), row.height}, entry.title,
                   "menu_arrus", 12);
        if (m_referenceSelected == entry.id) outline(row, 0xff8dc5e1u);
    }
    setDesignClip(std::nullopt);
    const auto selected = std::find_if(entries.begin(), entries.end(), [this](const WorldGuideEntry *pEntry)
                                       { return pEntry->id == m_referenceSelected; });
    if (selected != entries.end())
    {
        const WorldGuideEntry &entry = **selected;
        scrollViewport(details, m_referenceContentHeight, m_referenceDetailScroll);
        setDesignClip(details);
        Rect text{details.x + 8 * designScale(), details.y + (8 - m_referenceDetailScroll) * designScale(),
                  details.width - 24 * designScale(), 50000 * designScale()};
        float iconHeight = 0;
        if (pSelectedItem != nullptr &&
            drawReferenceItemIcon(*pSelectedItem, {text.x, text.y, text.width, 72 * designScale()}))
        {
            iconHeight = 80;
            text.y += iconHeight * designScale();
        }
        float height = 0;
        textInRect(text, entry.details, "menu_arrus", 12, 0xffc6dfeau, false, true, &height);
        float y = text.y + height + 12 * designScale();
        for (size_t i = 0; i < entry.links.size(); ++i)
        {
            const WorldGuideLink &link = entry.links[i];
            const Rect row{text.x, y, text.width, 26 * designScale()};
            if (row.y + row.height >= details.y && row.y <= details.y + details.height &&
                button("atlas-ref-link-" + std::to_string(i), row, link.label))
            {
                if (link.kind == WorldGuideLink::Kind::Trainer)
                {
                    m_referenceOpen = false;
                    m_featureView = false;
                    m_map.clear();
                    m_siteSelection.clear();
                    m_selected.clear();
                    m_filter = WorldRecordKind::Trainer;
                    m_trainerMastery = link.mastery;
                    m_search = link.target;
                    m_scroll = 0;
                }
                else if (link.kind == WorldGuideLink::Kind::Feature)
                {
                    const auto feature =
                        std::find_if(m_world.features().begin(), m_world.features().end(),
                                     [&](const WorldMapFeature &value) { return value.id == link.target; });
                    if (feature != m_world.features().end() && m_world.featureVisible(*feature, m_knowledge, m_guide))
                    {
                        select(*m_world.find(feature->mapId));
                        m_selectedFeature = feature->id;
                        m_floorLayerContext.clear();
                        m_siteSelection = {feature->id};
                        m_featureView = true;
                        m_referenceOpen = false;
                    }
                }
                else if (link.kind == WorldGuideLink::Kind::Location)
                {
                    const WorldRecord *pLocation = m_world.find(link.target);
                    if (pLocation != nullptr && m_world.visible(*pLocation, m_knowledge, m_guide))
                    {
                        m_referenceOpen = false;
                        m_featureView = false;
                        m_search.clear();
                        m_filter.reset();
                        m_trainerMastery = 0;
                        m_siteSelection.clear();
                        select(*pLocation);
                    }
                }
                else
                {
                    m_referenceSection = link.kind == WorldGuideLink::Kind::Monster ? WorldGuideSection::Monsters
                                         : link.kind == WorldGuideLink::Kind::Spell ? WorldGuideSection::Spells
                                         : link.kind == WorldGuideLink::Kind::Class ? WorldGuideSection::Classes
                                         : link.kind == WorldGuideLink::Kind::Item  ? WorldGuideSection::Items
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
        m_referenceContentHeight = iconHeight + height / designScale() + entry.links.size() * 29 + 24;
        setDesignClip(std::nullopt);
    }
    else
        textInRect(details,
                   entries.empty()
                       ? (!m_search.empty() ? (m_guide ? Engine::tr("No records match this search.")
                                                       : Engine::tr("No known records match this search."))
                          : m_guide ? Engine::tr("No records in this section.")
                                    : Engine::tr("Nothing known yet. Guide Mode opens the full reference catalog."))
                       : Engine::tr("Select a record to inspect its details."),
                   "menu_arrus", 13, 0xffc6dfeau, true);
    if (items && m_pinnedItem)
    {
        const ItemDefinition *pPinned = m_data.itemTable().get(*m_pinnedItem);
        textInRect(canvasRect(34, 407, 778, 43),
                   Engine::tr("Pinned: {name}. Select another item and Compare. Character skills and enchantments "
                              "affect actual results.",
                              {{"name", Engine::localizeKnown(pPinned->name)}}),
                   "menu_arrus", 11);
        return;
    }
    textInRect(
        canvasRect(34, 407, 778, 43),
        m_sourceScanActive ? Engine::tr("Reading authored map sources. Random loot and encounters are never generated.")
        : !m_locationError.empty() ? m_locationError
        : m_referenceSection == WorldGuideSection::Items
            ? (m_guide ? Engine::tr("Sources include fixed loot and conditional scripted rewards. "
                                    "They do not guarantee quest eligibility, random loot or remaining contents.")
                       : Engine::tr(
                             "Recorded sources include pickups, creature loot, received rewards and seen chest items. "
                             "Past observations do not guarantee that loot remains."))
        : m_guide
            ? Engine::tr("Reference definitions follow the loaded game tables, including enabled mods.")
            : Engine::tr("Discovery shows encountered items, known spell references, current classes, known recipes, "
                         "party and Inn companions, observed monsters and recorded quests."),
        "menu_arrus", 11);
}

void AtlasScreen::drawSiteJournal()
{
    const WorldMapFeature &feature = *m_siteReading;
    const WorldRecord *pMap = m_world.find(feature.mapId);
    textInRect(canvasRect(25, 60, 798, 28),
               Engine::localizeKnown(m_world.featureTitle(feature, m_knowledge, m_guide) + " - " + pMap->map->name,
                                     "world:" + m_presentation.worldId),
               "fondamento", 15);
    textInRect(canvasRect(25, 89, 798, 25),
               m_guide ? Engine::tr("Site reference") : Engine::tr("Party's recorded findings"), "menu_lucida", 11,
               0xffc6dfeau);
    const Rect viewport = canvasRect(25, 121, 798, 270);
    outline(viewport, 0xff597284u);
    std::string content = Engine::localizeKnown(m_world.siteReference(feature, m_knowledge, m_guide));
    try
    {
        const std::string observed =
            m_world.siteObservationText(feature, m_knowledge, &m_data.spellTable(), &m_data.itemTable());
        if (!observed.empty())
        {
            if (!content.empty()) content += "\n\n";
            content += observed;
        }
        for (const JournalAutonoteEntry *pNote :
             m_world.siteJournalEntries(feature, m_data.journalAutonoteTable(), m_knowledge, m_guide))
        {
            if (!content.empty()) content += "\n\n";
            content += pNote->text;
        }
    }
    catch (const std::invalid_argument &error)
    {
        content = error.what();
    }
    if (content.empty()) content = Engine::tr("No findings recorded about this site yet.");
    scrollViewport(viewport, m_siteJournalHeight, m_siteJournalScroll);
    setDesignClip(viewport);
    float height = 0;
    textInRect({viewport.x + 10 * designScale(), viewport.y + (10 - m_siteJournalScroll) * designScale(),
                viewport.width - 26 * designScale(), 50000 * designScale()},
               content, "menu_arrus", 13, 0xffc6dfeau, false, true, &height);
    m_siteJournalHeight = height / designScale() + 20;
    setDesignClip(std::nullopt);
    textInRect(
        canvasRect(34, 407, 778, 43),
        m_guide ? feature.kind == WorldMapFeatureKind::Connection
                      ? Engine::tr("Passage references describe possible interactions; current availability and "
                                   "destination conditions "
                                   "may differ.")
                      : Engine::tr("Effects depend on the selected character, available resources and previous visits.")
                : Engine::tr(
                      "Recorded findings describe past visits; results can depend on the character and previous use."),
        "menu_arrus", 11);
}

void AtlasScreen::drawDetails()
{
    const Rect details = canvasRect(25, 403, 798, 52);
    outline(details, 0xff597284u);
    if (m_featureView)
    {
        const auto features =
            m_world.queryFeatures(m_knowledge, m_guide, m_search, m_map, &m_data.spellTable(), &m_data.itemTable());
        const auto selected =
            std::find_if(features.begin(), features.end(),
                         [this](const WorldMapFeature *pFeature)
                         {
                             return pFeature->id == m_selectedFeature &&
                                    (m_siteSelection.empty() || m_siteSelection.contains(pFeature->id));
                         });
        std::string text = Engine::tr("Select a site to inspect it. "
                                      "C Chests, W Wells/Fountains, S Shrines, O Obelisks, P Passages, A Arrivals, "
                                      "M Monsters, N Notes, I Items, D Objects.");
        const WorldRecord *pDestination = nullptr;
        const MapStatsEntry *pConnectedMap = nullptr;
        const WorldMapFeature *pArrivalDestination = nullptr;
        size_t arrivalCount = 0;
        if (m_connectionContext != m_selectedFeature)
        {
            m_connectionContext = m_selectedFeature;
            m_connectionChoice = 0;
        }
        int16_t creatureId = 0;
        uint32_t itemId = 0;
        const WorldMapFeature *pNote = nullptr;
        bool hasJournal = false;
        if (selected != features.end())
        {
            const WorldMapFeature &feature = **selected;
            text = m_world.featureTitle(feature, m_knowledge, m_guide) + " | " +
                   Engine::tr(worldMapFeatureKindName(feature.kind));
            hasJournal = !m_world.siteReference(feature, m_knowledge, m_guide).empty();
            const auto observations = m_world.siteObservations(feature, m_knowledge);
            if (!observations.empty())
            {
                hasJournal = true;
                text += Engine::tr("\nRecorded outcome available.");
            }
            if (!m_world.siteReference(feature, m_knowledge, m_guide).empty())
                text += Engine::tr("\nEffects and requirements available.");
            try
            {
                const auto notes =
                    m_world.siteJournalEntries(feature, m_data.journalAutonoteTable(), m_knowledge, m_guide);
                if (!notes.empty())
                {
                    hasJournal = true;
                    text += "\n" + notes.front()->text;
                }
            }
            catch (const std::invalid_argument &error)
            {
                text += "\n" + std::string(error.what());
            }
            itemId = feature.itemId;
            if (feature.kind == WorldMapFeatureKind::Item)
                text += feature.observedItem
                            ? Engine::tr("\nRecorded pickup; this is not a live availability check.")
                            : Engine::tr("\nAuthored source; scripts and the campaign state can change availability.");
            if (feature.kind == WorldMapFeatureKind::Connection)
            {
                const auto arrivals = m_world.connectionArrivals(feature, m_knowledge);
                const auto declared = m_world.connectionTargets(feature, m_knowledge, m_guide);
                std::vector<const WorldMapFeature *> additionalArrivals = arrivals;
                std::erase_if(additionalArrivals,
                              [&](const WorldMapFeature *pArrival)
                              {
                                  return std::any_of(declared.begin(), declared.end(), [&](const MapStatsEntry *pMap)
                                                     { return WorldDatabase::mapId(*pMap) == pArrival->mapId; });
                              });
                arrivalCount = declared.size() + additionalArrivals.size();
                if (!m_connectionJumpDestination.empty())
                {
                    size_t index = 0;
                    const auto choose = [&](const std::string &mapId)
                    {
                        if (mapId == m_connectionJumpDestination) m_connectionChoice = index;
                        ++index;
                    };
                    for (const MapStatsEntry *pMap : declared) choose(WorldDatabase::mapId(*pMap));
                    for (const WorldMapFeature *pArrival : additionalArrivals) choose(pArrival->mapId);
                    m_connectionJumpDestination.clear();
                }
                if (arrivalCount != 0) m_connectionChoice %= arrivalCount;
                if (m_connectionChoice < declared.size())
                {
                    pConnectedMap = declared[m_connectionChoice];
                    text += Engine::tr("\nPossible destination: {name}. Conditions may apply.",
                                       {{"name", Engine::localizeKnown(pConnectedMap->name)}});
                    const auto arrival =
                        std::find_if(arrivals.begin(), arrivals.end(), [&](const WorldMapFeature *pValue)
                                     { return pValue->mapId == WorldDatabase::mapId(*pConnectedMap); });
                    if (arrival != arrivals.end()) pArrivalDestination = *arrival;
                }
                else if (arrivalCount != 0)
                {
                    pArrivalDestination = additionalArrivals[m_connectionChoice - declared.size()];
                    pConnectedMap = m_world.mapDefinition(pArrivalDestination->mapId);
                }
                else if (feature.arrival && !feature.targetMapId.empty())
                    pConnectedMap = m_world.mapDefinition(feature.targetMapId);
                else
                    for (const WorldRecord &record : m_world.records())
                        if ((record.kind == WorldRecordKind::Region || record.kind == WorldRecordKind::Dungeon) &&
                            toLowerCopy(record.map->fileName) == toLowerCopy(feature.targetMap) &&
                            m_world.visible(record, m_knowledge, m_guide))
                        {
                            pDestination = &record;
                            pConnectedMap = record.map;
                        }
                if (pArrivalDestination != nullptr)
                    text += Engine::tr(
                        "\nTravelled to: {name} | Day {gameMinutes}",
                        {{"name", Engine::localizeKnown(pConnectedMap->name)},
                         {"gameMinutes", std::to_string(pArrivalDestination->arrival->gameMinutes / (24 * 60) + 1)}});
                if (arrivalCount > 1)
                    text += " | " + std::to_string(m_connectionChoice + 1) + "/" + std::to_string(arrivalCount);
                if (feature.arrival)
                    text +=
                        Engine::tr("\nRecorded arrival | Day {gameMinutes}. This does not establish a return passage.",
                                   {{"gameMinutes", std::to_string(feature.arrival->gameMinutes / (24 * 60) + 1)}});
                else if (arrivalCount == 0)
                    text += feature.interactionReference && feature.interactionReference->kind == "dimension_door"
                                ? Engine::tr("\nUse this gateway to learn its destinations.")
                            : !feature.targetMaps.empty()
                                ? Engine::tr("\nTravel through this passage to learn its destinations.")
                            : feature.targetMap.empty() ? Engine::tr("\nDestination not mapped")
                            : pDestination != nullptr
                                ? Engine::tr("\nDeclared connection: {name}",
                                             {{"name", Engine::localizeKnown(pDestination->name())}})
                            : m_guide ? Engine::tr("\nDestination outside this catalog")
                                      : Engine::tr("\nDestination undiscovered");
            }
            else if (feature.kind == WorldMapFeatureKind::Chest)
            {
                const auto observations = m_world.observedChests(feature, m_knowledge);
                text += Engine::tr("\nContainers: {size}", {{"size", std::to_string(feature.chestIds.size())}});
                if (observations.empty())
                    text += Engine::tr(" | No recorded inspection");
                else if (feature.chestIds.size() == 1)
                {
                    const WorldChestObservation &chest = observations.front();
                    text +=
                        Engine::tr(" | Last inspected: {unconfirmed} | Day {gameMinutes}",
                                   {{"unconfirmed", (chest.empty              ? "empty"
                                                     : chest.visibleSlots > 0 ? std::to_string(chest.visibleSlots) +
                                                                                    Engine::tr(" visible entries")
                                                                              : Engine::tr("contents unconfirmed"))},
                                    {"gameMinutes", std::to_string(chest.gameMinutes / (24 * 60) + 1)}});
                }
                else
                    text += Engine::tr(" | Inspected: {size} | Last seen empty: {chest}",
                                       {{"size", std::to_string(observations.size())},
                                        {"chest", std::to_string(std::count_if(observations.begin(), observations.end(),
                                                                               [](const WorldChestObservation &chest)
                                                                               { return chest.empty; }))}});
            }
            else if (feature.kind == WorldMapFeatureKind::Note)
            {
                text += feature.userNote ? Engine::tr("\nYour note") : Engine::tr("\nRecorded by the party");
                if (feature.userNote && m_writeNote) pNote = &feature;
            }
            else if (feature.monster)
            {
                const WorldMonsterFeature &monster = *feature.monster;
                if (monster.observed)
                    text += Engine::tr("\nLast observed: {alive} | Day {gameMinutes}",
                                       {{"alive", (monster.dead ? "defeated" : "alive")},
                                        {"gameMinutes", std::to_string(monster.gameMinutes / (24 * 60) + 1)}});
                else if (monster.placed)
                    text += Engine::tr(
                        "\nAuthored placement. Scripts and the current world state can change its presence.");
                else if (monster.scripted)
                    text += Engine::tr("\nScripted group size per call: {minCount}. Conditions may apply; not a live "
                                       "population count.",
                                       {{"minCount", std::to_string(monster.minCount)}});
                else
                    text +=
                        Engine::tr("\nAuthored group: {minCount} - {maxCount} creatures. Not a live population count.",
                                   {{"minCount", std::to_string(monster.minCount)},
                                    {"maxCount", std::to_string(monster.maxCount)}});
                if (!monster.definitions.empty()) creatureId = monster.definitions.front();
            }
        }
        if (!m_locationError.empty()) text += "\n" + m_locationError;
        const bool otherAction = pConnectedMap != nullptr || creatureId > 0 || itemId > 0 || pNote != nullptr;
        const float journalX = otherAction ? (arrivalCount > 1 ? 454 : 514) : 669;
        const float textWidth = hasJournal && otherAction   ? journalX - 44
                                : arrivalCount > 1          ? 565
                                : otherAction || hasJournal ? 625
                                                            : 778;
        textInRect(canvasRect(34, 407, textWidth, 43), text, "menu_arrus", 11);
        if (hasJournal && button("atlas-site-journal", canvasRect(journalX, 416, 145, 28),
                                 (m_guide && !(**selected).guideReference.empty()) ||
                                         !m_world.siteObservations(**selected, m_knowledge).empty()
                                     ? Engine::tr("Read details")
                                     : Engine::tr("Read notes")))
        {
            m_siteReading = **selected;
            m_siteJournalScroll = m_siteJournalHeight = 0;
            m_searchEditing = false;
            setTextEditing(false);
        }
        if (arrivalCount > 1 && button("atlas-next-arrival", canvasRect(609, 416, 50, 28), Engine::tr(">")))
            m_connectionChoice = (m_connectionChoice + 1) % arrivalCount;
        if (itemId > 0 && button("atlas-feature-item", canvasRect(669, 416, 145, 28), Engine::tr("Item details")))
        {
            m_referenceOpen = true;
            m_referenceSection = WorldGuideSection::Items;
            m_referenceSelected = std::to_string(itemId);
            m_referenceDirty = true;
            m_search.clear();
            m_referenceScroll = m_referenceDetailScroll = m_referenceContentHeight = 0;
        }
        if (pNote != nullptr && button("atlas-edit-note", canvasRect(669, 416, 145, 28), Engine::tr("Edit note")))
        {
            const WorldRecord *pMap = m_world.find(pNote->mapId);
            m_noteDraft = {pMap->map->fileName, pNote->noteId, int32_t(pNote->site.x), int32_t(pNote->site.y),
                           pNote->title,        false,         pNote->noteElevation};
            m_noteError.clear();
            m_noteEditorOpen = true;
            m_searchEditing = false;
            setTextEditing(true);
        }
        if (creatureId > 0 &&
            button("atlas-feature-creature", canvasRect(669, 416, 145, 28), Engine::tr("Creature details")))
        {
            m_referenceOpen = true;
            m_referenceSection = WorldGuideSection::Monsters;
            m_referenceSelected = std::to_string(creatureId);
            m_referenceDirty = true;
            m_search.clear();
            m_referenceScroll = m_referenceDetailScroll = m_referenceContentHeight = 0;
        }
        const bool hasWorldAtlas =
            pConnectedMap != nullptr && std::find(m_availableWorlds.begin(), m_availableWorlds.end(),
                                                  pConnectedMap->worldId) != m_availableWorlds.end();
        if (pConnectedMap != nullptr &&
            button("atlas-feature-destination", canvasRect(669, 416, 145, 28),
                   !hasWorldAtlas                                       ? Engine::tr("Atlas unavailable")
                   : pArrivalDestination != nullptr                     ? Engine::tr("View arrival")
                   : selected != features.end() && (*selected)->arrival ? Engine::tr("View origin")
                                                                        : Engine::tr("Open connected map"),
                   "button", hasWorldAtlas))
        {
            const std::string arrivalId = pArrivalDestination != nullptr ? pArrivalDestination->id : "";
            const std::string mapId = WorldDatabase::mapId(*pConnectedMap);
            const std::string worldId = pConnectedMap->worldId;
            selectWorld(worldId);
            if (const WorldRecord *pMap = m_world.find(mapId)) select(*pMap);
            m_siteSelection.clear();
            m_floorLayerContext.clear();
            if (!arrivalId.empty())
            {
                m_featureView = true;
                m_selectedFeature = arrivalId;
                m_search.clear();
            }
        }
        return;
    }
    const auto *record = m_world.find(m_selected);
    std::string text = m_guide ? Engine::tr("Guide Mode reveals the database without changing your discoveries.")
                               : Engine::tr("Known places and encountered residents are shown. Quest information does "
                                            "not reveal unexplored terrain.");
    if (record != nullptr)
    {
        text = record->name();
        if (record->lastObservedNpc)
            text += record->scriptLocation ? Engine::tr(" | Last encountered in this map")
                                           : Engine::tr(" | Last encountered here");
        if (record->possibleNpcLocation)
            text += Engine::tr(" | Authored possibility; quest conditions may apply. Current presence is not implied.");
        if (record->actorLocation || record->scriptLocation)
            text += " | " + Engine::localize(record->map->name, "world:" + record->map->worldId);
        if (record->scriptLocation) text += Engine::tr(" | No exact position is recorded");
        if (record->house != nullptr)
        {
            const MapStatsEntry *pRecordMap = m_world.recordMap(*record, m_knowledge, m_guide, m_map);
            if (pRecordMap != nullptr)
                text += " | " + Engine::localize(pRecordMap->name, "world:" + pRecordMap->worldId)
                    + " | " + Engine::localize(record->house->type);
            if (record->kind == WorldRecordKind::House)
                text += Engine::tr(" | Hours {openHour}:00 - {closeHour}:00",
                                   {{"openHour", std::to_string(record->house->openHour)},
                                    {"closeHour", std::to_string(record->house->closeHour)}});
        }
        if (record->teacher != nullptr)
        {
            if (m_pParty != nullptr)
            {
                const auto evaluation =
                    evaluateMasteryTeacherTopic(record->teacher->topicId, *m_pParty, m_data.classSkillTable(),
                                                m_data.npcDialogTable(), &m_data.mergedTeacherTopicTable());
                if (evaluation)
                    text += Engine::tr(
                        " | {cost} gold\nSelected character: {displayText}",
                        {{"cost", std::to_string(evaluation->cost)}, {"displayText", evaluation->displayText}});
            }
            else
            {
                if (record->teacher->requiredSkill != 0)
                    text += Engine::tr(" | Skill {requiredSkill}",
                                       {{"requiredSkill", std::to_string(record->teacher->requiredSkill)}});
                if (record->teacher->requiredGold != 0)
                    text += Engine::tr(" | {requiredGold} gold",
                                       {{"requiredGold", std::to_string(record->teacher->requiredGold)}});
                text += Engine::tr("\nSelect a party in-game to check its training requirements.");
            }
        }
        if (record->transport)
        {
            const WorldTransportEntry &transport = *record->transport;
            text += transport.observed  ? Engine::tr(" | Last offered")
                    : transport.current ? Engine::tr(" | Current timetable")
                                        : Engine::tr(" | Authored timetable");
            text += transport.quote ? Engine::tr(" | Quoted ") : Engine::tr(" | Base ");
            text += atlasTravelDays(transport.quote ? transport.quote->travelDays : transport.route.travelDays);
            if (transport.quote)
                text += Engine::tr(" | {gold} gold quoted", {{"gold", std::to_string(transport.quote->gold)}});
            text += " | ";
            const char *days[] = {Engine::trNoop("Mon").data(), Engine::trNoop("Tue").data(),
                                  Engine::trNoop("Wed").data(), Engine::trNoop("Thu").data(),
                                  Engine::trNoop("Fri").data(), Engine::trNoop("Sat").data(),
                                  Engine::trNoop("Sun").data()};
            for (size_t day = 0; day < 7; ++day)
                if (transport.route.daysAvailable[day]) text += Engine::tr(days[day]) + " ";
            if (transport.route.requiredQBit != 0) text += Engine::tr("| Quest unlock required");
        }
        if (record->walkingRoute != nullptr)
        {
            text += Engine::tr(" | Walking from {name} | {travelDays}",
                               {{"name", Engine::localizeKnown(record->map->name)},
                                {"travelDays", atlasTravelDays(record->walkingRoute->travelDays)}});
            if (!record->walkingRoute->requiredQuestBitsAny.empty())
                text += m_pParty == nullptr ? Engine::tr(" | Quest requirement")
                        : m_world.walkingRouteUnlocked(*record, m_knowledge)
                            ? Engine::tr(" | Quest requirement met")
                            : Engine::tr(" | Locked by quest requirement");
            if (record->walkingRoute->requiredOriginSurface == MapTransitionSurfaceRequirement::Water)
                text += Engine::tr(" | Water access required");
        }
    }
    if (!m_locationError.empty()) text += "\n" + m_locationError;
    float textWidth = 778;
    if (record != nullptr && record->kind == WorldRecordKind::Travel && record->destination != nullptr)
    {
        const WorldRecord *pDestination = m_world.find(WorldDatabase::mapId(*record->destination));
        if (pDestination != nullptr && m_world.visible(*pDestination, m_knowledge, m_guide))
        {
            textWidth = 619;
            if (button("atlas-route-destination", canvasRect(669, 416, 145, 28), Engine::tr("View destination")))
                select(*pDestination);
        }
    }
    textInRect(canvasRect(34, 407, textWidth, 43), text, "menu_arrus", 12);
}
} // namespace OpenYAMM::Game
