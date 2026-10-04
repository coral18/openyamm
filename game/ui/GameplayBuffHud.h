#pragma once

#include "game/party/Party.h"
#include "game/ui/GameplayOverlayTypes.h"
#include <algorithm>
#include <array>

namespace OpenYAMM::Game
{
class GameplayScreenRuntime;

struct GameplayBuffHudEntry
{
    PartyBuffId id;
    GameplayResolvedHudLayoutElement rect;
};

enum class GameplayBuffPresentation
{
    Hero,
    Skull,
    Movement,
    Special
};

enum class GameplayBuffHudLayerPass
{
    Underlay,
    Base,
    Overlay
};

struct GameplayBuffHudRegion
{
    float x;
    float y;
    float width;
    float height;
};

struct GameplayBuffHudLayer
{
    const char *pAsset;
    GameplayBuffHudRegion uv;
    GameplayBuffHudRegion placement;
    GameplayBuffHudLayerPass pass = GameplayBuffHudLayerPass::Overlay;
};

// Source UVs select individual foreground sprites from unchanged RGBA atlases.
// Placement uses the old 128-square panel coordinates. Soft effects draw behind the base.
// Resistance gems share a 24-square size budget; teardrops keep their narrower shape.
inline constexpr GameplayBuffHudLayer GameplayBuffHudFireResistance = {
    "obsidian_buff_resistances", {82 / 1536.0f, 78 / 1024.0f, 374 / 1536.0f, 369 / 1024.0f},
    {5.5f, 69, 24, 24}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudWaterResistance = {
    "obsidian_buff_resistances", {616 / 1536.0f, 50 / 1024.0f, 305 / 1536.0f, 415 / 1024.0f},
    {3, 37, 18, 24}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudAirResistance = {
    "obsidian_buff_resistances", {1085 / 1536.0f, 83 / 1024.0f, 358 / 1536.0f, 358 / 1024.0f},
    {23, 6.5f, 24, 24}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudEarthResistance = {
    "obsidian_buff_resistances", {105 / 1536.0f, 556 / 1024.0f, 299 / 1536.0f, 400 / 1024.0f},
    {82, 4.5f, 18, 24}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudMindResistance = {
    "obsidian_buff_resistances", {587 / 1536.0f, 575 / 1024.0f, 361 / 1536.0f, 360 / 1024.0f},
    {99.5f, 29, 24, 24}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudBodyResistance = {
    "obsidian_buff_resistances", {1105 / 1536.0f, 568 / 1024.0f, 358 / 1536.0f, 370 / 1024.0f},
    {97, 61.5f, 24, 24}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudSword = {
    "obsidian_buff_combat", {75 / 1254.0f, 51 / 1254.0f, 505 / 1254.0f, 565 / 1254.0f},
    {11, 6, 45, 80}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudShield = {
    "obsidian_buff_combat", {764 / 1254.0f, 130 / 1254.0f, 374 / 1254.0f, 449 / 1254.0f},
    {57, 24, 52, 62}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudWings = {
    "obsidian_buff_combat", {51 / 1254.0f, 775 / 1254.0f, 525 / 1254.0f, 373 / 1254.0f},
    {48, 81, 40, 35}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudFlames = {
    "obsidian_buff_combat", {644 / 1254.0f, 717 / 1254.0f, 599 / 1254.0f, 482 / 1254.0f},
    {9, 77, 114, 51}, GameplayBuffHudLayerPass::Underlay};
inline constexpr GameplayBuffHudLayer GameplayBuffHudNearEye = {
    "obsidian_buff_skull_effects", {74 / 1536.0f, 89 / 1024.0f, 366 / 1536.0f, 335 / 1024.0f},
    {44, 55, 43, 35}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudFarEye = {
    "obsidian_buff_skull_effects", {661 / 1536.0f, 151 / 1024.0f, 214 / 1536.0f, 214 / 1024.0f},
    {23, 57, 23, 26}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudGarland = {
    "obsidian_buff_skull_effects", {1025 / 1536.0f, 103 / 1024.0f, 495 / 1536.0f, 303 / 1024.0f},
    {24, 30, 81, 37}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudHalo = {
    "obsidian_buff_skull_effects", {29 / 1536.0f, 603 / 1024.0f, 472 / 1536.0f, 295 / 1024.0f},
    {10, 0, 118, 60}};
inline constexpr GameplayBuffHudLayer GameplayBuffHudTorch = {
    "obsidian_buff_skull_effects", {512 / 1536.0f, 517 / 1024.0f, 496 / 1536.0f, 461 / 1024.0f},
    {0, 23, 128, 72}, GameplayBuffHudLayerPass::Underlay};
inline constexpr GameplayBuffHudLayer GameplayBuffHudClouds = {
    "obsidian_buff_skull_effects", {1025 / 1536.0f, 657 / 1024.0f, 505 / 1536.0f, 288 / 1024.0f},
    {0, 79, 128, 49}, GameplayBuffHudLayerPass::Underlay};
inline constexpr GameplayBuffHudLayer GameplayBuffHudStone = {
    "obsidian_buff_skull_stone", {0 / 1254.0f, 0 / 1254.0f, 1254 / 1254.0f, 1254 / 1254.0f},
    {0, 0, 128, 128}, GameplayBuffHudLayerPass::Base};

struct GameplayBuffHudDefinition
{
    const char *pName;
    const char *pIcon;
    GameplayBuffPresentation presentation;
    const GameplayBuffHudLayer *pLayer = nullptr;
    float badgeX = 0;
    float badgeY = 0;
};

// Preserve the desktop skull mapping; the old Android YAML incorrectly assigned these layers to other spells.
// The far eye extends that mapping for Detect Life; badges cover effects without a legacy desktop layer.
// Flight and Water Walk retain separate animated indicators. Special is reserved for future modifiers.
inline constexpr std::array<GameplayBuffHudDefinition, PartyBuffCount> GameplayBuffHudDefinitions = {{
    {"Torch Light", "obsidian_buff_sbf01a", GameplayBuffPresentation::Skull, &GameplayBuffHudTorch},
    {"Wizard Eye", "obsidian_buff_sba01a", GameplayBuffPresentation::Skull, &GameplayBuffHudNearEye},
    {"Feather Fall", "obsidian_buff_sba02a", GameplayBuffPresentation::Skull, &GameplayBuffHudClouds},
    {"Detect Life", "obsidian_buff_sbs01a", GameplayBuffPresentation::Skull, &GameplayBuffHudFarEye},
    {"Water Walk", "obsidian_buff_sbw05a", GameplayBuffPresentation::Movement},
    {"Fly", "obsidian_buff_sba10a", GameplayBuffPresentation::Movement},
    {"Invisibility", "obsidian_buff_sba08a", GameplayBuffPresentation::Skull, nullptr, 46, 2},
    {"Stoneskin", "obsidian_buff_sbe05a", GameplayBuffPresentation::Skull, &GameplayBuffHudStone},
    {"Day of the Gods", "obsidian_buff_sbl06a", GameplayBuffPresentation::Skull, &GameplayBuffHudHalo},
    {"Protection from Magic", "obsidian_buff_sbb09a", GameplayBuffPresentation::Skull, &GameplayBuffHudGarland},
    {"Fire Resistance", "obsidian_buff_sbf03a", GameplayBuffPresentation::Hero, &GameplayBuffHudFireResistance},
    {"Water Resistance", "obsidian_buff_sbw03a", GameplayBuffPresentation::Hero, &GameplayBuffHudWaterResistance},
    {"Air Resistance", "obsidian_buff_sba03a", GameplayBuffPresentation::Hero, &GameplayBuffHudAirResistance},
    {"Earth Resistance", "obsidian_buff_sbe03a", GameplayBuffPresentation::Hero, &GameplayBuffHudEarthResistance},
    {"Mind Resistance", "obsidian_buff_sbm03a", GameplayBuffPresentation::Hero, &GameplayBuffHudMindResistance},
    {"Body Resistance", "obsidian_buff_sbb03a", GameplayBuffPresentation::Hero, &GameplayBuffHudBodyResistance},
    {"Shield", "obsidian_buff_sba06a", GameplayBuffPresentation::Hero, &GameplayBuffHudShield},
    {"Heroism", "obsidian_buff_sbs07a", GameplayBuffPresentation::Hero, &GameplayBuffHudSword},
    {"Haste", "obsidian_buff_sbf05a", GameplayBuffPresentation::Hero, &GameplayBuffHudWings},
    {"Immolation", "obsidian_buff_sbf08a", GameplayBuffPresentation::Hero, &GameplayBuffHudFlames},
    {"Glamour", "obsidian_buff_sbde01a", GameplayBuffPresentation::Hero, nullptr, 46, 46},
    {"Levitate", "obsidian_buff_sbv02a", GameplayBuffPresentation::Movement}
}};

inline const char *gameplayBuffPanelLayoutId(GameplayBuffPresentation presentation)
{
    switch (presentation)
    {
        case GameplayBuffPresentation::Hero: return "ObsidianHeroBuffPanel";
        case GameplayBuffPresentation::Skull: return "ObsidianSkullBuffPanel";
        case GameplayBuffPresentation::Movement: return nullptr;
        case GameplayBuffPresentation::Special: return nullptr;
    }
    return nullptr;
}

inline std::optional<PartyBuffId> gameplayFlightBuff(const Party &party)
{
    if (party.hasPartyBuff(PartyBuffId::Fly))
    {
        return PartyBuffId::Fly;
    }
    if (party.hasPartyBuff(PartyBuffId::Levitate))
    {
        return PartyBuffId::Levitate;
    }
    return std::nullopt;
}

extern const std::array<const char *, CharacterBuffCount> GameplayPersonalBuffNames;
inline std::vector<PartyBuffId> sortedGameplayBuffs(const Party &party)
{
    std::vector<PartyBuffId> result;
    for (size_t i = 0; i < PartyBuffCount; ++i)
    {
        const PartyBuffId id = static_cast<PartyBuffId>(i);
        if (party.partyBuff(id) != nullptr)
        {
            result.push_back(id);
        }
    }
    std::stable_sort(result.begin(), result.end(), [&party](PartyBuffId left, PartyBuffId right)
    {
        return party.partyBuff(left)->remainingSeconds < party.partyBuff(right)->remainingSeconds;
    });
    return result;
}
inline std::vector<PartyBuffId> sortedGameplaySpecialBuffs(const Party &party)
{
    std::vector<PartyBuffId> result = sortedGameplayBuffs(party);
    std::erase_if(result, [](PartyBuffId id)
    {
        return GameplayBuffHudDefinitions[size_t(id)].presentation != GameplayBuffPresentation::Special;
    });
    return result;
}
std::vector<GameplayBuffHudEntry> gameplayBuffHudEntries(GameplayScreenRuntime &context, int width, int height);
std::optional<GameplayBuffHudEntry> gameplayMovementBuffAtPoint(GameplayScreenRuntime &context,
    int width, int height, float x, float y);
void renderGameplayBuffHud(GameplayScreenRuntime &context, int width, int height);
GameplayHudPointerTarget gameplayBuffHudTarget(GameplayScreenRuntime &context,
    int width, int height, float x, float y);
}
