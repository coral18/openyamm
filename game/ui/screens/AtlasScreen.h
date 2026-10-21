#pragma once

#include "game/ui/MenuDesignScreen.h"
#include "game/world/WorldDatabase.h"
#include "game/world/WorldAtlasPresentation.h"
#include "game/world/WorldGuide.h"

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
    void selectFloorLayer(std::optional<int32_t> layer);
    void drawList();
    void drawDetails();
    void drawReference();
    void rebuildReference();
    WorldGuideSource referenceSource() const;
    void drawClassComparison();
    void compareClass(std::optional<uint32_t> raceId = std::nullopt);
    void stepItemSourceScan();
    void indexQuestLocations();
    void rebuildWorld();
    void drawNoteEditor();
    void drawNoteElevation();
    void saveNote(bool remove);
    void closeNoteEditor();

    WorldKnowledge m_knowledge;
    WorldAtlasPresentation m_presentation;
    WorldDatabase m_world;
    std::vector<std::string> m_availableWorlds;
    const GameDataRepository &m_data;
    const Party *m_pParty = nullptr;
    WorldGuideKnowledge m_referenceKnowledge;
    std::vector<WorldGuideEntry> m_referenceEntries;
    WorldGuideSection m_referenceSection = WorldGuideSection::Items;
    bool m_referenceOpen = false, m_referenceDirty = true;
    std::string m_referenceSelected;
    float m_referenceScroll = 0, m_referenceDetailScroll = 0, m_referenceContentHeight = 0;
    std::optional<WorldClassComparison> m_classComparison;
    size_t m_comparisonPage = 0;
    float m_comparisonScroll = 0;
    std::vector<const MapStatsEntry *> m_itemSourceMaps;
    size_t m_itemSourceScanIndex = 0;
    size_t m_itemSourceScanFailures = 0;
    bool m_itemSourceScanActive = false, m_itemSourcesComplete = false;
    std::unordered_set<std::string> m_indexedMaps, m_siteSelection;
    std::string m_locationError;
    NoteWriter m_writeNote;
    WorldMapNoteEdit m_noteDraft;
    std::string m_noteError;
    bool m_noteEditorOpen = false, m_placingNote = false;
    bool m_featureView = false;
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
    std::optional<int32_t> m_floorLayer;
    std::vector<int32_t> m_floorLayers;
    WorldFloorPlanRaster m_floorPlan;
    bgfx::TextureHandle m_floorPlanTexture = BGFX_INVALID_HANDLE;
    std::string m_fogMap;
    bgfx::TextureHandle m_fogTexture = BGFX_INVALID_HANDLE;
    bool m_dragging = false;
    float m_dragX = 0, m_dragY = 0;
};
} // namespace OpenYAMM::Game
