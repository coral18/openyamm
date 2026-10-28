#pragma once

#include "game/ui/MenuDesignScreen.h"
#include "game/world/WorldDatabase.h"
#include "game/world/WorldAtlasPresentation.h"
#include "game/world/WorldGuide.h"
#include "game/tables/SpriteTables.h"

namespace OpenYAMM::Game
{
class GameDataRepository;
struct MapAssetInfo;
class Party;

class AtlasScreen : public MenuDesignScreen
{
public:
    using NoteWriter = std::function<bool(WorldMapNoteEdit &, std::string &)>;
    AtlasScreen(const Engine::AssetFileSystem &assets, const GameDataRepository &data,
                WorldKnowledge knowledge, std::function<void()> close, bool paused,
                GameAudioSystem *audio = nullptr, const MapAssetInfo *pCurrentMap = nullptr,
                const Party *pParty = nullptr, NoteWriter writeNote = {});
    ~AtlasScreen() override;
    AppMode mode() const override;
    void handleSdlEvent(const SDL_Event &event) override;
    void onExit() override;

private:
    void drawScreen(float deltaSeconds) override;
    void select(const WorldRecord &record);
    void selectWorld(const std::string &worldId);
    std::string regionTexture(const WorldRecord &record) const;
    void drawMap();
    void drawConnectionGraph();
    void stepConnectionScan();
    void drawTravelLinks(const Rect &image);
    void indexKnownRegions();
    void indexMapData(MapAssetInfo &map);
    bool drawLocationMarkers(const WorldRecord &map, const Rect &image, const Rect &viewport,
        const WorldFloorPlanRaster *pFloorPlan);
    bool drawFeatureMarkers(const WorldRecord &map, const Rect &image, const Rect &viewport,
                            const WorldFloorPlanRaster *pFloorPlan);
    void drawFeatureList();
    void drawPartyArrow(const WorldRecord &map, const Rect &image, const WorldFloorPlanRaster *floorPlan);
    void drawFloorLayers(const WorldRecord &map);
    void selectFloorLayer(std::optional<WorldFloorPlanLayer> layer);
    void drawList();
    void drawDetails();
    void drawSiteJournal();
    void drawReference();
    void rebuildReference();
    WorldGuideSource referenceSource() const;
    void drawClassComparison();
    void drawItemComparison();
    bool drawReferenceItemIcon(const ItemDefinition &item, const Rect &box);
    void compareClass(std::optional<uint32_t> raceId = std::nullopt);
    void stepMapSourceScan();
    void indexQuestLocations();
    void rebuildWorld();
    void drawNoteEditor();
    void drawSearchField(const std::string &id, const Rect &rect);
    void drawNoteElevation();
    void saveNote(bool remove);
    void closeNoteEditor();

    WorldKnowledge m_knowledge;
    WorldAtlasPresentation m_presentation;
    WorldDatabase m_world;
    std::vector<std::string> m_availableWorlds;
    const GameDataRepository &m_data;
    const Party *m_pParty = nullptr;
    // The paused gameplay map/program outlives this screen, like the supplied party and game registries.
    const ScriptedEventProgram *m_pGlobalEvents = nullptr;
    const ScriptedEventProgram *m_pLocalEvents = nullptr;
    WorldGuideKnowledge m_referenceKnowledge;
    std::optional<DecorationTable> m_decorationTable;
    std::vector<WorldGuideEntry> m_referenceEntries;
    WorldGuideSection m_referenceSection = WorldGuideSection::Items;
    bool m_referenceOpen = false, m_referenceDirty = true;
    std::optional<WorldMapFeature> m_siteReading;
    float m_siteJournalScroll = 0, m_siteJournalHeight = 0;
    std::string m_referenceSelected;
    float m_referenceScroll = 0, m_referenceDetailScroll = 0, m_referenceContentHeight = 0;
    std::optional<WorldClassComparison> m_classComparison;
    std::optional<uint32_t> m_pinnedItem;
    std::optional<WorldItemComparison> m_itemComparison;
    std::array<float, 2> m_itemComparisonScroll{}, m_itemComparisonHeight{};
    size_t m_comparisonPage = 0;
    float m_comparisonScroll = 0;
    std::vector<const MapStatsEntry *> m_sourceMaps;
    size_t m_sourceScanIndex = 0;
    size_t m_sourceScanFailures = 0;
    bool m_sourceScanActive = false, m_sourcesComplete = false;
    std::unordered_set<std::string> m_indexedMaps, m_siteSelection;
    size_t m_pendingMapIndexes = 0;
    std::string m_locationError;
    NoteWriter m_writeNote;
    WorldMapNoteEdit m_noteDraft;
    std::string m_noteError;
    bool m_noteEditorOpen = false, m_placingNote = false;
    bool m_featureView = false;
    bool m_connectionView = false, m_connectionCatalogComplete = false;
    std::vector<const MapStatsEntry *> m_connectionScanMaps;
    size_t m_connectionScanIndex = 0, m_connectionScanFailures = 0;
    size_t m_graphPage = 0, m_graphEdgePage = 0;
    std::string m_graphNode, m_graphEdge;
    uint16_t m_connectionJumpEvent = 0;
    std::string m_connectionJumpMap;
    std::string m_connectionJumpDestination;
    std::string m_selectedFeature;
    std::string m_connectionContext;
    size_t m_connectionChoice = 0;
    std::function<void()> m_close;
    bool m_paused = false;
    bool m_guide = false;
    bool m_searchEditing = false;
    bool m_suppressEscape = false;
    std::string m_search, m_selected, m_map;
    std::optional<WorldRecordKind> m_filter;
    uint32_t m_trainerMastery = 0;
    float m_scroll = 0;
    float m_zoom = 1;
    float m_panX = 0, m_panY = 0;
    std::string m_floorPlanKey;
    std::string m_floorLayerContext;
    std::optional<WorldFloorPlanLayer> m_floorLayer;
    std::vector<WorldFloorPlanLayer> m_floorLayers;
    WorldFloorPlanRaster m_floorPlan;
    bgfx::TextureHandle m_floorPlanTexture = BGFX_INVALID_HANDLE;
    std::string m_fogMap;
    bgfx::TextureHandle m_fogTexture = BGFX_INVALID_HANDLE;
    bool m_dragging = false;
    float m_dragX = 0, m_dragY = 0;
};
} // namespace OpenYAMM::Game
