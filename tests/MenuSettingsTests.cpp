#include "game/ui/MenuSettingsModel.h"
#include <doctest/doctest.h>
#include <fstream>

using namespace OpenYAMM::Game;

TEST_CASE("menu settings validate input and preserve unrelated game configuration")
{
    GameSettings settings = GameSettings::createDefault();
    settings.assetRoot = "/custom/content";
    settings.immortal = true;
    settings.mouseSensitivity = 42;
    CHECK(setMenuSettingValue(settings, "music_volume", "0"));
    CHECK(settings.musicVolume == 0);
    CHECK_FALSE(setMenuSettingValue(settings, "music_volume", "10"));
    CHECK_FALSE(setMenuSettingValue(settings, "music_volume", "2noise"));
    CHECK(settings.musicVolume == 0);
    CHECK_FALSE(setMenuSettingValue(settings, "mouse_sensitivity", "-1"));
    CHECK(settings.mouseSensitivity == 42);
    CHECK(setMenuSettingValue(settings, "resolution", "1920x1080"));
    CHECK(menuSettingValue(settings, "resolution") == "1920x1080");
    CHECK_FALSE(setMenuSettingValue(settings, "resolution", "1920x1080junk"));
    CHECK_FALSE(setMenuSettingValue(settings, "resolution", "32x0"));
    CHECK(menuSettingValue(settings, "resolution") == "1920x1080");
    CHECK(setMenuSettingValue(settings, "window_mode", "windowed_fullscreen"));
    CHECK(settings.windowMode == WindowMode::WindowedFullscreen);
    CHECK_FALSE(setMenuSettingValue(settings, "window_mode", "fake"));
    CHECK(setMenuSettingValue(settings, "terrain_anisotropy", "16x"));
    CHECK(settings.terrainFiltering == "anisotropic");
    CHECK(settings.terrainAnisotropy == "16x");
    CHECK(setMenuSettingValue(settings, "bolster_monsters", "true"));
    CHECK(settings.bolsterMonsters);
    CHECK_FALSE(setMenuSettingValue(settings, "bolster_monsters", "yes"));
    CHECK_FALSE(setMenuSettingValue(settings, "immortal", "false"));
    CHECK(settings.immortal);
    CHECK(settings.assetRoot == "/custom/content");
}

TEST_CASE("menu binding reservation and equality follow native single input bindings")
{
    CHECK(reservedMenuBinding(keyboardInputBinding(SDL_SCANCODE_ESCAPE)));
    CHECK(reservedMenuBinding(keyboardInputBinding(SDL_SCANCODE_F9)));
    CHECK(reservedMenuBinding(keyboardInputBinding(SDL_SCANCODE_F10)));
    CHECK(reservedMenuBinding(keyboardInputBinding(SDL_SCANCODE_5)));
    CHECK_FALSE(reservedMenuBinding(keyboardInputBinding(SDL_SCANCODE_F2)));
    CHECK_FALSE(reservedMenuBinding(mouseButtonInputBinding(SDL_BUTTON_LEFT)));
    CHECK(sameMenuBinding(mouseButtonInputBinding(SDL_BUTTON_LEFT), mouseButtonInputBinding(SDL_BUTTON_LEFT)));
    CHECK_FALSE(sameMenuBinding(mouseButtonInputBinding(SDL_BUTTON_LEFT), keyboardInputBinding(SDL_SCANCODE_A)));
    CHECK_FALSE(sameMenuBinding(mouseButtonInputBinding(SDL_BUTTON_LEFT), mouseButtonInputBinding(SDL_BUTTON_RIGHT)));
}

TEST_CASE("enemy health menu settings validate modes and preserve other options")
{
    GameSettings settings = GameSettings::createDefault();
    CHECK(menuSettingValue(settings, "enemy_health_bars") == "combat");
    CHECK(setMenuSettingValue(settings, "enemy_health_bars", "target"));
    CHECK_FALSE(setMenuSettingValue(settings, "enemy_health_bars", "true"));
    CHECK(menuSettingValue(settings, "enemy_health_bars") == "target");
    CHECK(setMenuSettingValue(settings, "enemy_health_bar_values", "all"));
    CHECK_FALSE(setMenuSettingValue(settings, "enemy_health_bar_values", "unknown"));
    CHECK(menuSettingValue(settings, "enemy_health_bar_values") == "all");
    CHECK(setMenuSettingValue(settings, "enemy_health_bar_damage_trail", "false"));
    CHECK_FALSE(settings.enemyHealthBarDamageTrail);
    CHECK(settings.combatText);
    CHECK_FALSE(setMenuSettingValue(settings, "combat_target_panel", "true"));
}

TEST_CASE("enemy health settings migrate legacy toggles and roundtrip the replacement options")
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "openyamm-enemy-bars-settings.ini";
    std::string error;
    for (const char *panel : {"true", "false"})
    {
        {
            std::ofstream file(path);
            file << "[gameplay]\ncombat_actor_hp_bars=false\ncombat_target_panel=" << panel << '\n';
        }
        const std::optional<GameSettings> loaded = loadGameSettings(path, error);
        REQUIRE(loaded);
        CHECK(loaded->enemyHealthBarMode == (std::string_view(panel) == "true" ? "target" : "off"));
    }
    GameSettings settings = GameSettings::createDefault();
    settings.enemyHealthBarMode = "nearby";
    settings.enemyHealthBarValues = "all";
    settings.enemyHealthBarDamageTrail = false;
    REQUIRE(saveGameSettings(path, settings, error));
    const std::optional<GameSettings> loaded = loadGameSettings(path, error);
    REQUIRE(loaded);
    CHECK(loaded->enemyHealthBarMode == "nearby");
    CHECK(loaded->enemyHealthBarValues == "all");
    CHECK_FALSE(loaded->enemyHealthBarDamageTrail);
    std::filesystem::remove(path);
}
