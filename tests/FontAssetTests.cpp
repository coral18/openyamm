#include "engine/AssetFileSystem.h"
#include "engine/FontAsset.h"
#include "game/app/GameSettings.h"
#include "game/ui/UiLayoutManager.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace
{
using namespace OpenYAMM;

struct FontFixture
{
    std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("openyamm_font_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Engine::AssetFileSystem assets;

    FontFixture()
    {
        const std::filesystem::path source = std::filesystem::path(OPENYAMM_SOURCE_DIR) / "assets_dev/engine/fonts";
        std::filesystem::create_directories(root / "engine/fonts/truetype");
        std::filesystem::copy_file(source / "icons/ARRUS.FNT", root / "engine/fonts/arrus.fnt");
        std::filesystem::copy_file(source / "icons/create.fnt", root / "engine/fonts/create.fnt");
        std::filesystem::copy_file(source / "icons/Lucida.fnt", root / "engine/fonts/lucida.fnt");
        std::filesystem::copy_file(source / "icons/SMALLNUM.FNT", root / "engine/fonts/smallnum.fnt");
        std::filesystem::copy_file(source / "icons/COMIC.FNT", root / "engine/fonts/comic.fnt");
        std::filesystem::copy_file(source / "icons/Book2.FNT", root / "engine/fonts/book2.fnt");
        std::filesystem::copy_file(source / "icons/AUTONOTE.FNT", root / "engine/fonts/autonote.fnt");
        std::filesystem::copy_file(source / "icons/ENDGAME.FNT", root / "engine/fonts/endgame.fnt");
        std::filesystem::copy_file(source / "icons/SPELL.FNT", root / "engine/fonts/spell.fnt");
        for (const char *pFile : {"arrus.yml", "openyamm_arrus_faithful.ttf",
            "create.yml", "openyamm_create_faithful.ttf", "lucida.yml", "openyamm_lucida_faithful.ttf",
            "smallnum.yml", "openyamm_smallnum_faithful.ttf", "comic.yml", "openyamm_comic_faithful.ttf",
            "book2.yml", "openyamm_book2_faithful.ttf", "autonote.yml", "openyamm_autonote_faithful.ttf",
            "endgame.yml", "openyamm_endgame_faithful.ttf", "spell.yml", "openyamm_spell_faithful.ttf",
            "fondamento.yml", "fondamento_regular.ttf", "menu_arrus.yml", "menu_lucida.yml",
            "alegreya_semibold.yml", "alegreya_variable.ttf"})
        {
            std::filesystem::copy_file(source / "truetype" / pFile, root / "engine/fonts/truetype" / pFile);
        }
        REQUIRE(assets.initialize(root, root, Engine::AssetScaleTier::X1));
    }

    ~FontFixture()
    {
        assets.shutdown();
        std::filesystem::remove_all(root);
    }
};

int textWidth(const Engine::FontAtlas &font, const std::string &text)
{
    int width = 0;
    for (unsigned char character : text)
    {
        const Engine::FontGlyphMetrics &metrics = font.glyphMetrics[character];
        width += metrics.leftSpacing + metrics.width + metrics.rightSpacing;
    }
    return width;
}
}

TEST_CASE("menu outline fonts load independently of legacy metrics")
{
    FontFixture fixture;
    std::string error;
    for (const char *pName : {"fondamento", "menu_arrus", "menu_lucida", "alegreya_semibold"})
    {
        const auto image = Engine::loadTrueTypeFontAtlas(fixture.assets, pName, error);
        INFO(error);
        REQUIRE(image);
        CHECK(image->atlas.atlasScale == 1);
        CHECK(image->atlas.glyphMetrics['W'].advance() > image->atlas.glyphMetrics['i'].advance());
        CHECK(image->atlas.glyphMetrics[' '].advance() > 0);
        bool fractionalAdvance = false;
        for (const Engine::FontGlyphMetrics &metrics : image->atlas.glyphMetrics)
        {
            fractionalAdvance = fractionalAdvance || metrics.advance() != std::floor(metrics.advance());
        }
        if (std::string(pName) == "fondamento")
        {
            CHECK(fractionalAdvance);
        }
        CHECK(textWidth(image->atlas, "WWW") > textWidth(image->atlas, "iii"));
        CHECK(textWidth(image->atlas, "Begin Adventure") > 0);
        bool ink = false;
        for (size_t i = 3; i < image->atlas.mainAtlasPixels.size(); i += 4)
            ink = ink || image->atlas.mainAtlasPixels[i] != 0;
        CHECK(ink);
    }
    CHECK_FALSE(Engine::loadTrueTypeFontAtlas(fixture.assets, "../fondamento", error));
    CHECK_FALSE(Engine::loadTrueTypeFontAtlas(fixture.assets, "arrus", error));
}

TEST_CASE("menu outline font atlas rasterizes at display size with antialiased "
          "ink and one shadow mask")
{
    FontFixture fixture;
    std::string error;
    for (const char *pName : {"fondamento", "menu_arrus", "menu_lucida", "alegreya_semibold"})
    {
        for (int pixelHeight : {11, 13, 17, 21, 26, 64, 128})
        {
            CAPTURE(pName);
            CAPTURE(pixelHeight);
            const auto image = Engine::loadTrueTypeFontAtlas(fixture.assets, pName, error, pixelHeight);
            REQUIRE_MESSAGE(image, error);
            const Engine::FontAtlas &font = image->atlas;
            CHECK(font.fontHeight == pixelHeight);
            CHECK(font.atlasScale == 1);
            const int cellWidth = font.atlasCellWidth + 2 * font.atlasPadding;
            const int cellHeight = font.fontHeight + 2 * font.atlasPadding;
            for (unsigned char character : std::string("Wil\xc9\xe9"))
            {
                CAPTURE(character);
                const int cellX = character % 16 * cellWidth;
                const int cellY = character / 16 * cellHeight;
                size_t ink = 0;
                size_t antialiased = 0;
                bool sameShadow = true;
                bool whiteInk = true;
                bool contained = true;
                for (int y = 0; y < cellHeight; ++y)
                {
                    for (int x = 0; x < cellWidth; ++x)
                    {
                        const size_t offset = (size_t(cellY + y) * font.atlasWidth + cellX + x) * 4;
                        const uint8_t alpha = font.mainAtlasPixels[offset + 3];
                        ink += alpha > 0;
                        antialiased += alpha > 0 && alpha < 255;
                        sameShadow &= image->shadowPixels[offset + 3] == alpha;
                        whiteInk &= font.mainAtlasPixels[offset] == 255 && font.mainAtlasPixels[offset + 1] == 255 &&
                                    font.mainAtlasPixels[offset + 2] == 255;
                        if (alpha > 0)
                        {
                            contained &= x > 0 && x < font.glyphMetrics[character].width + 2 * font.atlasPadding - 1 &&
                                         y > 0 && y < cellHeight - 1;
                        }
                    }
                }
                CHECK(ink > 0);
                // Pixel-aligned faithful strokes can be fully opaque at their original
                // sizes.
                if (std::string(pName) == "fondamento")
                {
                    CHECK(antialiased > 0);
                }
                CHECK(sameShadow);
                CHECK(whiteInk);
                CHECK(contained);
            }
        }
    }
    CHECK_FALSE(Engine::loadTrueTypeFontAtlas(fixture.assets, "fondamento", error, -1));
    CHECK_FALSE(Engine::loadTrueTypeFontAtlas(fixture.assets, "fondamento", error, 513));
}

TEST_CASE("enemy label font loads its authored semibold weight and rejects invalid weights")
{
    FontFixture fixture;
    std::string error;
    const std::optional<Engine::FontAtlasImage> semibold =
        Engine::loadTrueTypeFontAtlas(fixture.assets, "alegreya_semibold", error);
    REQUIRE_MESSAGE(semibold, error);
    CHECK(semibold->atlas.fontHeight == 24);
    CHECK(textWidth(semibold->atlas, "Guardian of VARN 617/617") * 0.42f < 144.0f);
    CHECK(semibold->atlas.mainAtlasPixels.size() < 2 * 1024 * 1024);

    const auto writeWeight = [&](int weight)
    {
        std::ofstream descriptor(fixture.root / "engine/fonts/truetype/alegreya_semibold.yml");
        descriptor << "file: alegreya_variable.ttf\nmetrics: freetype\nlogical_height: 24\nbaseline: 18\n"
                   << "encoding: windows-1252\nweight: " << weight << '\n';
    };
    writeWeight(400);
    const std::optional<Engine::FontAtlasImage> regular =
        Engine::loadTrueTypeFontAtlas(fixture.assets, "alegreya_semibold", error);
    REQUIRE_MESSAGE(regular, error);
    const auto ink = [](const Engine::FontAtlas &font)
    {
        uint64_t sum = 0;
        for (size_t offset = 3; offset < font.mainAtlasPixels.size(); offset += 4)
        {
            sum += font.mainAtlasPixels[offset];
        }
        return sum;
    };
    CHECK(ink(semibold->atlas) > ink(regular->atlas));

    writeWeight(999);
    CHECK_FALSE(Engine::loadTrueTypeFontAtlas(fixture.assets, "alegreya_semibold", error));
    CHECK(error.find("weight") != std::string::npos);
    writeWeight(0);
    CHECK_FALSE(Engine::loadTrueTypeFontAtlas(fixture.assets, "alegreya_semibold", error));
}

TEST_CASE("menu headings fit their layout at the intended font size across display scales")
{
    FontFixture fixture;
    Game::UiLayoutManager layouts;
    for (const char *pLayout : {"settings_gameplay", "settings_video", "settings_audio", "settings_controls",
                               "settings_keyboard", "continent_selection", "character_creation", "load_game",
                               "save_game"})
    {
        const std::filesystem::path path = std::filesystem::path(OPENYAMM_SOURCE_DIR) /
                                           "assets_dev/engine/ui/gameplay" / (std::string(pLayout) + ".yml");
        std::ifstream input(path);
        REQUIRE(input.good());
        const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        REQUIRE(layouts.loadLayoutText(path.string(), text));
    }
    std::string error;
    const auto baseFont = Engine::loadTrueTypeFontAtlas(fixture.assets, "fondamento", error);
    REQUIRE_MESSAGE(baseFont, error);
    size_t checkedHeadings = 0;
    for (float scale : {0.9375f, 1.5f, 1.83f, 1.875f, 2.25f})
    {
        std::unordered_map<int, Engine::FontAtlas> sizedFonts;
        for (const auto &[key, element] : layouts.elements())
        {
            const std::string &id = element.id;
            const bool heading = id.ends_with("Title") || id.ends_with("Heading") || id.ends_with("Name") ||
                                 id.ends_with("Section0") || id.ends_with("Section1") || id.ends_with("ConfirmButton");
            if (!heading || element.fontName != "fondamento")
            {
                continue;
            }
            ++checkedHeadings;
            CAPTURE(id);
            CAPTURE(scale);
            const int pixelHeight = int(std::lround(baseFont->atlas.fontHeight * element.textScale * scale));
            if (!sizedFonts.contains(pixelHeight))
            {
                auto image = Engine::loadTrueTypeFontAtlas(fixture.assets, "fondamento", error, pixelHeight);
                REQUIRE_MESSAGE(image, error);
                sizedFonts.emplace(pixelHeight, std::move(image->atlas));
            }
            const Engine::FontAtlas &font = sizedFonts.at(pixelHeight);
            float width = 0;
            uint8_t previous = 0;
            for (unsigned char character : element.labelText)
            {
                width += font.glyphMetrics[character].advance() + font.kerning(previous, character);
                previous = character;
            }
            CHECK(width <= (element.width - 2 * element.textPadX) * scale + 0.5f);
        }
    }
    CHECK(checkedHeadings > 100);
}

TEST_CASE("font atlas TTF preserves native layout and rasterizes all CP1252 cells without clipping")
{
    const char *pName = "Arrus";
    const char *pPath = "fonts/arrus.fnt";
    int height = 19;
    SUBCASE("Arrus")
    {
    }
    SUBCASE("Create")
    {
        pName = "Create";
        pPath = "fonts/create.fnt";
        height = 18;
    }
    SUBCASE("Lucida")
    {
        pName = "Lucida";
        pPath = "fonts/lucida.fnt";
        height = 17;
    }
    SUBCASE("SMALLNUM")
    {
        pName = "SMALLNUM";
        pPath = "fonts/smallnum.fnt";
        height = 14;
    }
    SUBCASE("Comic")
    {
        pName = "Comic";
        pPath = "fonts/comic.fnt";
        height = 19;
    }
    SUBCASE("Book2")
    {
        pName = "Book2";
        pPath = "fonts/book2.fnt";
        height = 30;
    }
    SUBCASE("AUTONOTE")
    {
        pName = "AUTONOTE";
        pPath = "fonts/autonote.fnt";
        height = 18;
    }
    SUBCASE("ENDGAME")
    {
        pName = "ENDGAME";
        pPath = "fonts/endgame.fnt";
        height = 20;
    }
    SUBCASE("SPELL")
    {
        pName = "SPELL";
        pPath = "fonts/spell.fnt";
        height = 16;
    }
    FontFixture fixture;
    const std::optional<std::vector<uint8_t>> bytes = fixture.assets.readBinaryFile(pPath);
    REQUIRE(bytes);
    std::string error;
    const std::optional<Engine::FontAtlasImage> bitmap =
        Engine::loadFontAtlas(fixture.assets, *bytes, pName, {}, error);
    REQUIRE_MESSAGE(bitmap, error);
    Engine::FontSettings settings;
    settings.preferTtf = true;
    settings.ttfFonts = {"arrus", "create", "lucida", "smallnum", "comic", "book2", "autonote", "endgame", "spell"};
    const std::optional<Engine::FontAtlasImage> ttf =
        Engine::loadFontAtlas(fixture.assets, *bytes, pName, settings, error);
    REQUIRE_MESSAGE(ttf, error);
    CHECK(ttf->atlas.fontHeight == height);
    CHECK(ttf->atlas.atlasScale == 4);
    CHECK(ttf->atlas.atlasWidth == (bitmap->atlas.atlasCellWidth + 10) * 16 * 4);
    CHECK(ttf->atlas.atlasHeight == (bitmap->atlas.fontHeight + 10) * 16 * 4);
    CHECK(ttf->atlas.firstChar == bitmap->atlas.firstChar);
    CHECK(ttf->atlas.lastChar == bitmap->atlas.lastChar);
    for (int character = 0; character < 256; ++character)
    {
        const Engine::FontGlyphMetrics &original = bitmap->atlas.glyphMetrics[character];
        const Engine::FontGlyphMetrics &replacement = ttf->atlas.glyphMetrics[character];
        CHECK(original.leftSpacing == replacement.leftSpacing);
        CHECK(original.width == replacement.width);
        CHECK(original.rightSpacing == replacement.rightSpacing);
    }
    for (const std::string &text : {"Welcome to New Sorpigal, traveller!", "Illusion, magic & adventure.",
        "A very long conversation about goblins and dragons.", "\x93\xc9lise\x94 \x97 \x80"})
    {
        CHECK(textWidth(ttf->atlas, text) == textWidth(bitmap->atlas, text));
    }
    const bool noteFace = std::string(pName) == "AUTONOTE";
    size_t partialAlphaCount = 0;
    size_t mainInkCount = 0;
    size_t shadowCount = 0;
    for (size_t offset = 3; offset < ttf->atlas.mainAtlasPixels.size(); offset += 4)
    {
        const uint8_t alpha = ttf->atlas.mainAtlasPixels[offset];
        partialAlphaCount += alpha > 0 && alpha < 255;
        mainInkCount += ttf->atlas.mainAtlasPixels[offset] > 0;
        shadowCount += ttf->shadowPixels[offset] > 0;
    }
    CHECK(partialAlphaCount > 1000);
    if (std::string(pName) == "ENDGAME" || std::string(pName) == "SPELL")
    {
        CHECK(shadowCount == 0);
        CHECK(mainInkCount > 1000);
    }
    else
    {
        CHECK(shadowCount > 1000);
    }
    if (noteFace)
    {
        CHECK(mainInkCount > 1000);
        // Obsidian uses tintable ivory ink; the restored l retains its native stroke and has no shifted shadow.
        const int cellX = (108 % 16) * (ttf->atlas.atlasCellWidth + 10) * 4;
        const int cellY = (108 / 16) * (height + 10) * 4;
        bool coverageMatches = true;
        for (int y = 0; y < (height + 10) * 4; ++y)
        {
            for (int x = 0; x < 44; ++x)
            {
                const size_t offset = (size_t(cellY + y) * ttf->atlas.atlasWidth + cellX + x) * 4;
                const bool ink = x >= 20 && x < 24 && y >= 32 && y < 80;
                coverageMatches &= ttf->atlas.mainAtlasPixels[offset + 3] == (ink ? 255 : 0);
                coverageMatches &= ttf->shadowPixels[offset + 3] == 0;
                coverageMatches &= ttf->shadowPixels[offset] == 0
                    && ttf->shadowPixels[offset + 1] == 0 && ttf->shadowPixels[offset + 2] == 0;
            }
        }
        CHECK(coverageMatches);
    }

    // Compare native foreground/shadow pixels against the FNT, including reserved byte slots.
    bool nativePixelsMatch = true;
    for (int character = bitmap->atlas.firstChar; character <= bitmap->atlas.lastChar; ++character)
    {
        const size_t offsetPosition = 32 + 256 * 12 + character * 4;
        const uint32_t glyphOffset = uint32_t((*bytes)[offsetPosition])
            | (uint32_t((*bytes)[offsetPosition + 1]) << 8)
            | (uint32_t((*bytes)[offsetPosition + 2]) << 16)
            | (uint32_t((*bytes)[offsetPosition + 3]) << 24);
        const int width = bitmap->atlas.glyphMetrics[character].width;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const uint8_t value = (*bytes)[32 + 4096 + glyphOffset + y * width + x];
                const size_t pixel = (size_t((character / 16) * height + y) * bitmap->atlas.atlasWidth
                    + (character % 16) * bitmap->atlas.atlasCellWidth + x) * 4;
                for (int channel = 0; channel < 4; ++channel)
                {
                    nativePixelsMatch &= bitmap->atlas.mainAtlasPixels[pixel + channel] == (value > 1 ? 255 : 0);
                }
                nativePixelsMatch &= bitmap->shadowPixels[pixel + 3] == (value == 1 ? 255 : 0);
            }
        }
    }
    CHECK(nativePixelsMatch);
}

TEST_CASE("font atlas selection is explicit and broken selected replacements report an error")
{
    FontFixture fixture;
    const std::optional<std::vector<uint8_t>> bytes = fixture.assets.readBinaryFile("fonts/arrus.fnt");
    REQUIRE(bytes);
    Engine::FontSettings settings;
    CHECK_FALSE(settings.usesTrueType("Arrus"));
    settings.preferTtf = true;
    CHECK(settings.usesTrueType("ARRUS"));
    CHECK_FALSE(settings.usesTrueType("Create"));
    std::string error;
    const std::optional<Engine::FontAtlasImage> unlisted =
        Engine::loadFontAtlas(fixture.assets, *bytes, "Create", settings, error);
    REQUIRE_MESSAGE(unlisted, error);
    CHECK(unlisted->atlas.atlasScale == 1);

    std::ofstream(fixture.root / "engine/fonts/truetype/arrus.yml")
        << "file: openyamm_arrus_faithful.ttf\nlogical_height: 19\nbaseline: 14\n"
        << "encoding: windows-1252\npresentation: unknown\n";
    CHECK_FALSE(Engine::loadFontAtlas(fixture.assets, *bytes, "Arrus", settings, error));
    CHECK(error.find("unsupported TTF presentation") != std::string::npos);
    std::ofstream(fixture.root / "engine/fonts/truetype/arrus.yml") << "file: bad.ttf\n";
    CHECK_FALSE(Engine::loadFontAtlas(fixture.assets, *bytes, "Arrus", settings, error));
    CHECK(error.find("invalid TTF descriptor") != std::string::npos);
    settings.preferTtf = false;
    CHECK(Engine::loadFontAtlas(fixture.assets, *bytes, "Arrus", settings, error));
    CHECK_FALSE(Engine::loadFontAtlas(fixture.assets, {0, 1, 2}, "Arrus", settings, error));
    CHECK(error == "invalid legacy FNT");
}

TEST_CASE("font atlas retains compact MMX bitmap parsing")
{
    Engine::AssetFileSystem assets;
    std::vector<uint8_t> bytes(32 + 1280 + 4, 0);
    bytes[0] = 65;
    bytes[1] = 65;
    bytes[2] = 8;
    bytes[5] = 2;
    bytes[32 + 65] = 2;
    bytes[32 + 1280] = 2;
    bytes[32 + 1280 + 3] = 1;
    std::string error;
    const std::optional<Engine::FontAtlasImage> font = Engine::loadFontAtlas(assets, bytes, "test", {}, error);
    REQUIRE_MESSAGE(font, error);
    CHECK(font->atlas.glyphMetrics[65].width == 2);
    const size_t firstPixel = (size_t(4 * 2) * font->atlas.atlasWidth + 2) * 4;
    CHECK(font->atlas.mainAtlasPixels[firstPixel + 3] == 255);
    CHECK(font->shadowPixels[firstPixel + (font->atlas.atlasWidth + 1) * 4 + 3] == 255);
    bytes.pop_back();
    CHECK_FALSE(Engine::loadFontAtlas(assets, bytes, "test", {}, error));
}

TEST_CASE("font settings round trip normalizes the selected font list and preserves bitmap mode")
{
    FontFixture fixture;
    const std::filesystem::path path = fixture.root / "settings.ini";
    std::ofstream(path) << "[fonts]\nprefer_ttf=true\nttf_fonts=Arrus, CREATE, arrus, ,\n";
    std::string error;
    std::optional<Game::GameSettings> settings = Game::loadGameSettings(path, error);
    REQUIRE_MESSAGE(settings, error);
    CHECK(settings->fonts.preferTtf);
    CHECK(settings->fonts.ttfFonts == std::vector<std::string>{"arrus", "create"});
    REQUIRE(Game::saveGameSettings(path, *settings, error));
    std::optional<Game::GameSettings> reloaded = Game::loadGameSettings(path, error);
    REQUIRE(reloaded);
    CHECK(reloaded->fonts == settings->fonts);
    settings->fonts.preferTtf = false;
    settings->fonts.ttfFonts.clear();
    REQUIRE(Game::saveGameSettings(path, *settings, error));
    reloaded = Game::loadGameSettings(path, error);
    REQUIRE(reloaded);
    CHECK_FALSE(reloaded->fonts.preferTtf);
    CHECK(reloaded->fonts.ttfFonts.empty());
}
