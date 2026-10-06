#pragma once

#include "game/ui/MenuDesignScreen.h"
#include "game/world/WorldDatabase.h"

namespace OpenYAMM::Game
{
class GameDataRepository;

class AtlasScreen : public MenuDesignScreen
{
public:
    AtlasScreen(const Engine::AssetFileSystem &assets, const GameDataRepository &data,
                WorldKnowledge knowledge, std::function<void()> close, bool paused,
                GameAudioSystem *audio = nullptr);
    ~AtlasScreen() override;
    AppMode mode() const override;
    void handleSdlEvent(const SDL_Event &event) override;
    void onExit() override;

private:
    void drawScreen(float deltaSeconds) override;
    void select(const WorldRecord &record);
    void drawMap();
    void drawPartyArrow(const WorldRecord &map, const Rect &image, const WorldFloorPlanRaster *floorPlan);
    void drawList();
    void drawDetails();

    WorldKnowledge m_knowledge;
    WorldDatabase m_world;
    std::function<void()> m_close;
    bool m_paused = false;
    bool m_guide = false;
    bool m_searchEditing = false;
    bool m_suppressEscape = false;
    std::string m_search, m_selected, m_map;
    std::optional<WorldRecordKind> m_filter;
    float m_scroll = 0;
    float m_zoom = 1;
    float m_panX = 0, m_panY = 0;
    std::string m_floorPlanKey;
    WorldFloorPlanRaster m_floorPlan;
    bgfx::TextureHandle m_floorPlanTexture = BGFX_INVALID_HANDLE;
    std::string m_fogMap;
    bgfx::TextureHandle m_fogTexture = BGFX_INVALID_HANDLE;
    bool m_dragging = false;
    float m_dragX = 0, m_dragY = 0;
};
} // namespace OpenYAMM::Game
