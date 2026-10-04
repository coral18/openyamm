#include "game/ui/GameplayUiSkin.h"
#include "game/ui/GameplayHudGeometry.h"
#include "game/gameplay/GameplayInputFrame.h"
#include "game/gameplay/GameplayScreenRuntime.h"

#include <algorithm>
#include <array>

namespace OpenYAMM::Game::GameplayUiSkin
{
namespace
{
constexpr std::array<std::array<uint32_t, 3>, 3> HealthColors = {{
    {{0xacd69b, 0x80ad77, 0x315834}},
    {{0xead28c, 0xc7a557, 0x766031}},
    {{0xe99682, 0xc9685e, 0x76362d}}
}};
}

std::optional<GameplayHudTextureHandle> healthMeterTexture(const GameplayScreenRuntime &context, float fraction, bool vertical)
{
    // A shaded native gradient stays crisp at any HUD scale and is uploaded only once per colour band.
    constexpr std::array<const char *, 3> Names = {{
        "__obsidian_hp_green__", "__obsidian_hp_yellow__", "__obsidian_hp_red__"
    }};
    const size_t band = size_t(healthBand(fraction));
    const std::string name = std::string(Names[band]) + (vertical ? "vertical" : "");
    const char *pSignature = "shaded-hp-2129";
    GameplayUiRuntime &runtime = context.gameplayUiRuntime();
    const std::optional<GameplayHudTextureHandle> cached = runtime.findCachedDynamicHudTexture(name, pSignature);
    if (cached)
    {
        return cached;
    }
    constexpr int Height = 32;
    std::vector<uint8_t> pixels(Height * 4);
    for (int y = 0; y < Height; ++y)
    {
        const float phase = y / float(Height - 1);
        const size_t stop = phase < 0.3f ? 0 : 1;
        const float t = stop == 0 ? phase / 0.3f : (phase - 0.3f) / 0.7f;
        for (int channel = 0; channel < 3; ++channel)
        {
            const uint8_t from = (HealthColors[band][stop] >> (channel * 8)) & 0xff;
            const uint8_t to = (HealthColors[band][stop + 1] >> (channel * 8)) & 0xff;
            pixels[y * 4 + channel] = static_cast<uint8_t>(from + (to - from) * t);
        }
        pixels[y * 4 + 3] = 255;
    }
    const std::optional<GameplayHudTextureHandle> texture =
        runtime.ensureDynamicHudTexture(name, vertical ? Height : 1, vertical ? 1 : Height, pixels);
    if (texture)
    {
        runtime.setDynamicHudTextureContentSignature(name, pSignature);
    }
    return texture;
}

void renderArc(GameplayScreenRuntime &context, const GameplayResolvedHudLayoutElement &rect,
    float strokeWidth, float startDegrees, float sweepDegrees, ArcMaterial material, float begin, float end)
{
    constexpr std::array<std::array<uint32_t, 3>, 9> Colors = {{
        HealthColors[0], HealthColors[1], HealthColors[2],
        {{0xabcfd8, 0x73a4b7, 0x2d596a}},
        {{0xb6a47b, 0x938666, 0x544d3d}},
        {{0x15201d, 0x0a1312, 0x080e0c}},
        {{0xc2ad80, 0x938666, 0x665b44}},
        {{0xffedbc, 0xf0dca3, 0xc6af77}},
        {{0xf2d8ad, 0xe4c793, 0xa88651}}
    }};
    GameplayUiRuntime &runtime = context.gameplayUiRuntime();
    const std::string name = "__obsidian_arc_" + std::to_string(int(material));
    constexpr const char *pSignature = "split-arc-shading-v1";
    std::optional<GameplayHudTextureHandle> texture = runtime.findCachedDynamicHudTexture(name, pSignature);
    if (!texture)
    {
        // Shading and narrow edge/end fades are sampled with the normal HUD shader, once per material.
        constexpr int Width = 256;
        constexpr int Height = HudArcTextureHeight;
        std::vector<uint8_t> pixels(Width * Height * 4);
        const std::array<uint32_t, 3> &colors = Colors[size_t(material)];
        for (int y = 0; y < Height; ++y)
        {
            const float phase = y / float(Height - 1);
            const size_t stop = phase < 0.3f ? 0 : 1;
            const float t = stop == 0 ? phase / 0.3f : (phase - 0.3f) / 0.7f;
            const float opacity = std::min(1.0f, std::min(y, Height - 1 - y) / HudArcEdgeTexels);
            for (int x = 0; x < Width; ++x)
            {
                const size_t pixel = size_t(y * Width + x) * 4;
                for (int channel = 0; channel < 3; ++channel)
                {
                    const uint8_t from = (colors[stop] >> (channel * 8)) & 0xff;
                    const uint8_t to = (colors[stop + 1] >> (channel * 8)) & 0xff;
                    pixels[pixel + channel] = static_cast<uint8_t>(from + (to - from) * t);
                }
                pixels[pixel + 3] = x == 0 || x == Width - 1 ? 0 : static_cast<uint8_t>(255 * opacity);
            }
        }
        texture = runtime.ensureDynamicHudTexture(name, Width, Height, pixels);
        if (texture)
        {
            runtime.setDynamicHudTextureContentSignature(name, pSignature);
        }
    }
    if (texture)
    {
        runtime.submitHudTexturedArc(texture->textureHandle, rect, strokeWidth * rect.scale,
            startDegrees, sweepDegrees, begin, end);
    }
}

GameplayResolvedHudLayoutElement scrollbarThumb(const GameplayResolvedHudLayoutElement &viewport,
    float fraction, float visibleFraction)
{
    const float height = std::min(viewport.height,
        std::max(10.666667f * viewport.scale, viewport.height * visibleFraction));
    return {viewport.x + viewport.width - 4.8f * viewport.scale,
        viewport.y + (viewport.height - height) * std::clamp(fraction, 0.0f, 1.0f),
        3.2f * viewport.scale, height, viewport.scale};
}

void renderScrollbar(GameplayScreenRuntime &context, const GameplayResolvedHudLayoutElement &viewport,
    float fraction, float visibleFraction, bool active)
{
    if (visibleFraction >= 1)
    {
        return;
    }
    const std::optional<GameplayHudTextureHandle> track =
        context.gameplayUiRuntime().ensureSolidHudTextureLoaded("__obsidian_scroll_track__", 0xff29382au);
    const std::optional<GameplayHudTextureHandle> thumb = context.gameplayUiRuntime().ensureSolidHudTextureLoaded(
        active ? "__obsidian_scroll_active__" : "__obsidian_scroll_thumb__", active ? 0xff90bedau : 0xff6a8dacu);
    if (track && thumb)
    {
        const GameplayResolvedHudLayoutElement rect = scrollbarThumb(viewport, fraction, visibleFraction);
        context.submitHudTexturedQuad(*track, viewport.x + viewport.width - 4 * viewport.scale, viewport.y,
            1.6f * viewport.scale, viewport.height);
        context.submitHudTexturedQuad(*thumb, rect.x, rect.y, rect.width, rect.height);
    }
}

void renderTexture(GameplayScreenRuntime &context, const std::string &asset,
    const GameplayResolvedHudLayoutElement &rect)
{
    const std::optional<GameplayHudTextureHandle> texture = context.gameplayUiRuntime().ensureHudTextureLoaded(asset);
    if (texture)
    {
        context.submitHudTexturedQuad(*texture, rect.x, rect.y, rect.width, rect.height);
    }
}

void renderIconButton(GameplayScreenRuntime &context, const GameplayResolvedHudLayoutElement &rect,
    const std::string &icon, bool hovered, bool pressed, bool selected)
{
    const float size = std::min(rect.width, rect.height);
    const GameplayResolvedHudLayoutElement plate = {
        rect.x + (rect.width - size) * 0.5f, rect.y + (rect.height - size) * 0.5f, size, size, rect.scale};
    renderTexture(context, pressed ? "obsidian_touch_pressed" : selected ? "obsidian_touch_active"
        : hovered ? "obsidian_touch_hover" : "obsidian_touch_normal", plate);
    const float inset = size * 0.15f;
    renderTexture(context, icon, {plate.x + inset, plate.y + inset + (pressed ? rect.scale : 0),
        size - 2 * inset, size - 2 * inset, rect.scale});
}

void renderButton(GameplayScreenRuntime &context, const std::string &id, int width, int height,
    bool selected, bool enabled)
{
    const UiLayoutManager::LayoutElement *pLayout = context.findHudLayoutElement(id);
    const std::optional<GameplayResolvedHudLayoutElement> rect =
        context.resolveHudLayoutElement(id, width, height, 0, 0);
    if (pLayout == nullptr || !rect)
    {
        return;
    }
    const GameplayInputFrame *pInput = context.currentGameplayInputFrame();
    const bool hover = enabled && pInput != nullptr
        && GameplayHudCommon::isPointerInsideResolvedElement(*rect, pInput->pointerX, pInput->pointerY);
    const bool pressed = hover && pInput->leftMouseButton.held;
    const std::string &asset = !enabled && !pLayout->disabledAsset.empty() ? pLayout->disabledAsset
        : pressed && !pLayout->pressedAsset.empty() ? pLayout->pressedAsset
        : hover && !pLayout->hoverAsset.empty() ? pLayout->hoverAsset
        : selected && !pLayout->selectedAsset.empty() ? pLayout->selectedAsset : pLayout->primaryAsset;
    if (!asset.empty())
    {
        renderTexture(context, asset, *rect);
    }
    const auto drawLabel = [&](const UiLayoutManager::LayoutElement &source, GameplayResolvedHudLayoutElement labelRect)
    {
        UiLayoutManager::LayoutElement label = source;
        label.textColorAbgr = !enabled ? 0xff7e8b96u : hover ? 0xffd4efffu : selected ? Gold : source.textColorAbgr;
        labelRect.y += pressed ? rect->scale : 0.0f;
        context.renderLayoutLabel(label, labelRect, label.labelText, false);
    };
    if (!pLayout->labelText.empty())
    {
        drawLabel(*pLayout, *rect);
    }
    for (const std::string &childId : context.sortedHudLayoutIdsForScreen(pLayout->screen))
    {
        const UiLayoutManager::LayoutElement *pChild = context.findHudLayoutElement(childId);
        if (pChild == nullptr || pChild->parentId != id || !pChild->visible)
        {
            continue;
        }
        const std::optional<GameplayResolvedHudLayoutElement> childRect =
            context.resolveHudLayoutElement(childId, width, height, 0, 0);
        if (!childRect)
        {
            continue;
        }
        if (!pChild->primaryAsset.empty())
        {
            renderTexture(context, pChild->primaryAsset, *childRect);
        }
        if (!pChild->labelText.empty())
        {
            drawLabel(*pChild, *childRect);
        }
    }
}

void renderSurface(GameplayScreenRuntime &context, const GameplayResolvedHudLayoutElement &rect, uint8_t alpha)
{
    const std::optional<GameplayHudTextureHandle> texture =
        context.gameplayUiRuntime().ensureHudTextureLoaded("obsidian_reading_surface");
    if (!texture || rect.width <= 0 || rect.height <= 0)
    {
        return;
    }
    const bgfx::TextureHandle surfaceTexture = alpha == 255 ? texture->textureHandle
        : context.gameplayUiRuntime().ensureHudTextureColorModulated(*texture, (uint32_t(alpha) << 24) | 0x00ffffffu);
    const float tileWidth = 640.0f * rect.scale;
    const float tileHeight = 480.0f * rect.scale;
    for (float y = 0; y < rect.height; y += tileHeight)
    {
        for (float x = 0; x < rect.width; x += tileWidth)
        {
            const float w = std::min(tileWidth, rect.width - x);
            const float h = std::min(tileHeight, rect.height - y);
            context.gameplayUiRuntime().submitHudTexturedQuad(surfaceTexture,
                rect.x + x, rect.y + y, w, h, 0, 0, w / tileWidth, h / tileHeight);
        }
    }
}

void renderPanel(GameplayScreenRuntime &context, const GameplayResolvedHudLayoutElement &rect,
    bool openBottom, float borderScale, uint8_t surfaceAlpha)
{
    const float corner = 30.933333f * rect.scale * borderScale;
    const float tile = 48.286179f * rect.scale * borderScale;
    const float inset = 8.533333f * rect.scale * borderScale;
    renderSurface(context, {rect.x + inset, rect.y + inset,
        rect.width - 2 * inset, rect.height - inset - (openBottom ? 1.6f * rect.scale : inset), rect.scale}, surfaceAlpha);
    const auto edge = [&](const char *pAsset, float x, float y, float length, bool vertical)
    {
        const std::optional<GameplayHudTextureHandle> texture =
            context.gameplayUiRuntime().ensureHudTextureLoaded(pAsset);
        if (!texture)
        {
            return;
        }
        for (float offset = 0; offset < length; offset += tile)
        {
            const float segment = std::min(tile, length - offset);
            context.gameplayUiRuntime().submitHudTexturedQuad(texture->textureHandle,
                x + (vertical ? 0 : offset), y + (vertical ? offset : 0),
                vertical ? corner : segment, vertical ? segment : corner,
                0, 0, vertical ? 1 : segment / tile, vertical ? segment / tile : 1);
        }
    };
    edge("obsidian_frame_top", rect.x + corner, rect.y, rect.width - 2 * corner, false);
    edge("obsidian_frame_left", rect.x, rect.y + corner,
        rect.height - (openBottom ? corner : 2 * corner), true);
    edge("obsidian_frame_right", rect.x + rect.width - corner, rect.y + corner,
        rect.height - (openBottom ? corner : 2 * corner), true);
    renderTexture(context, "obsidian_frame_tl", {rect.x, rect.y, corner, corner, rect.scale});
    renderTexture(context, "obsidian_frame_tr",
        {rect.x + rect.width - corner, rect.y, corner, corner, rect.scale});
    if (!openBottom)
    {
        edge("obsidian_frame_bottom", rect.x + corner, rect.y + rect.height - corner,
            rect.width - 2 * corner, false);
        renderTexture(context, "obsidian_frame_bl",
            {rect.x, rect.y + rect.height - corner, corner, corner, rect.scale});
        renderTexture(context, "obsidian_frame_br",
            {rect.x + rect.width - corner, rect.y + rect.height - corner, corner, corner, rect.scale});
    }
}

void renderPartyBasebar(GameplayScreenRuntime &context, const GameplayResolvedHudLayoutElement &rect)
{
    renderPanel(context, rect, false, 0.75f);
    const std::optional<GameplayHudTextureHandle> ornament =
        context.gameplayUiRuntime().ensureHudTextureLoaded("obsidian_party_endcap_compact");
    if (!ornament)
    {
        return;
    }
    const float height = rect.height;
    const float width = height * 0.293f;
    const float inset = 7 * rect.scale;
    const float y = rect.y;
    // Registered foreground bounds of the unchanged generated RGBA sprite; mirror only the UVs.
    constexpr float U0 = 330.0f / 992;
    constexpr float U1 = 722.0f / 992;
    constexpr float V0 = 103.0f / 1585;
    constexpr float V1 = 1429.0f / 1585;
    context.gameplayUiRuntime().submitHudTexturedQuad(ornament->textureHandle,
        rect.x + inset - width, y, width, height, U0, V0, U1, V1);
    context.gameplayUiRuntime().submitHudTexturedQuad(ornament->textureHandle,
        rect.x + rect.width - inset, y, width, height, U1, V0, U0, V1);
}
}
