#include "engine/AssetScaleTier.h"

#include <algorithm>
#include <cctype>

namespace OpenYAMM::Engine
{
const char *assetScaleTierToString(AssetScaleTier assetScaleTier)
{
    switch (assetScaleTier)
    {
        case AssetScaleTier::X1:
            return "x1";

        case AssetScaleTier::X2:
            return "x2";

        case AssetScaleTier::X4:
            return "x4";
    }

    return "unknown";
}

std::optional<AssetScaleTier> parseAssetScaleTier(const std::string &value)
{
    std::string normalizedValue = value;
    std::transform(
        normalizedValue.begin(),
        normalizedValue.end(),
        normalizedValue.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });

    if (normalizedValue == "1" || normalizedValue == "x1")
    {
        return AssetScaleTier::X1;
    }

    if (normalizedValue == "2" || normalizedValue == "x2")
    {
        return AssetScaleTier::X2;
    }

    if (normalizedValue == "4" || normalizedValue == "x4")
    {
        return AssetScaleTier::X4;
    }

    return std::nullopt;
}

int assetScaleTierFactor(AssetScaleTier assetScaleTier)
{
    switch (assetScaleTier)
    {
        case AssetScaleTier::X1:
            return 1;

        case AssetScaleTier::X2:
            return 2;

        case AssetScaleTier::X4:
            return 4;
    }

    return 1;
}

std::string assetScaleTierDirectorySuffix(AssetScaleTier assetScaleTier)
{
    switch (assetScaleTier)
    {
        case AssetScaleTier::X1:
            return "";

        case AssetScaleTier::X2:
            return "_x2";

        case AssetScaleTier::X4:
            return "_x4";
    }

    return "";
}

int scalePhysicalPixelsToLogical(int physicalPixels, AssetScaleTier assetScaleTier)
{
    if (physicalPixels <= 0)
    {
        return 0;
    }

    const int scaleFactor = assetScaleTierFactor(assetScaleTier);
    return std::max(1, (physicalPixels + scaleFactor - 1) / scaleFactor);
}

AssetScaleTier assetScaleTierFromResolvedPath(const std::string &path)
{
    // Only asset directory components carry scale; names and world ids do not.
    size_t start = path.starts_with("assets/") ? 7 : 0;
    if (path.compare(start, 7, "engine/") == 0)
    {
        start += 7;
    }
    else if (path.compare(start, 7, "worlds/") == 0)
    {
        const size_t separator = path.find('/', start + 7);
        start = separator == std::string::npos ? path.size() : separator + 1;
    }
    while (start < path.size())
    {
        const size_t end = path.find('/', start);
        if (end == std::string::npos)
        {
            break;
        }
        std::string component = path.substr(start, end - start);
        std::transform(component.begin(), component.end(), component.begin(), [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
        for (const char *pDirectory : {"icons", "ui", "hud", "textures", "bitmaps", "terrain", "sky",
                                      "sprites", "decorations", "effects", "fonts"})
        {
            if (component == std::string(pDirectory) + "_x2")
            {
                return AssetScaleTier::X2;
            }
            if (component == std::string(pDirectory) + "_x4")
            {
                return AssetScaleTier::X4;
            }
        }
        start = end + 1;
    }
    return AssetScaleTier::X1;
}

AssetScaleProfile createUniformAssetScaleProfile(AssetScaleTier assetScaleTier)
{
    AssetScaleProfile assetScaleProfile;
    assetScaleProfile.textures = assetScaleTier;
    assetScaleProfile.terrain = assetScaleTier;
    assetScaleProfile.sky = assetScaleTier;
    assetScaleProfile.sprites = assetScaleTier;
    assetScaleProfile.decorations = assetScaleTier;
    assetScaleProfile.icons = assetScaleTier;
    assetScaleProfile.ui = assetScaleTier;
    assetScaleProfile.effects = assetScaleTier;
    assetScaleProfile.fonts = assetScaleTier;
    return assetScaleProfile;
}

AssetScaleTier assetScaleTierForCategory(
    const AssetScaleProfile &assetScaleProfile,
    AssetScaleCategory assetScaleCategory)
{
    switch (assetScaleCategory)
    {
        case AssetScaleCategory::Textures:
            return assetScaleProfile.textures;

        case AssetScaleCategory::Terrain:
            return assetScaleProfile.terrain;

        case AssetScaleCategory::Sky:
            return assetScaleProfile.sky;

        case AssetScaleCategory::Sprites:
            return assetScaleProfile.sprites;

        case AssetScaleCategory::Decorations:
            return assetScaleProfile.decorations;

        case AssetScaleCategory::Icons:
            return assetScaleProfile.preferRestoredIcons ? AssetScaleTier::X2 : assetScaleProfile.icons;

        case AssetScaleCategory::Ui:
            return assetScaleProfile.ui;

        case AssetScaleCategory::Effects:
            return assetScaleProfile.effects;

        case AssetScaleCategory::Fonts:
            return assetScaleProfile.fonts;
    }

    return AssetScaleTier::X1;
}

void setAssetScaleTierForCategory(
    AssetScaleProfile &assetScaleProfile,
    AssetScaleCategory assetScaleCategory,
    AssetScaleTier assetScaleTier)
{
    switch (assetScaleCategory)
    {
        case AssetScaleCategory::Textures:
            assetScaleProfile.textures = assetScaleTier;
            return;

        case AssetScaleCategory::Terrain:
            assetScaleProfile.terrain = assetScaleTier;
            return;

        case AssetScaleCategory::Sky:
            assetScaleProfile.sky = assetScaleTier;
            return;

        case AssetScaleCategory::Sprites:
            assetScaleProfile.sprites = assetScaleTier;
            return;

        case AssetScaleCategory::Decorations:
            assetScaleProfile.decorations = assetScaleTier;
            return;

        case AssetScaleCategory::Icons:
            assetScaleProfile.icons = assetScaleTier;
            return;

        case AssetScaleCategory::Ui:
            assetScaleProfile.ui = assetScaleTier;
            return;

        case AssetScaleCategory::Effects:
            assetScaleProfile.effects = assetScaleTier;
            return;

        case AssetScaleCategory::Fonts:
            assetScaleProfile.fonts = assetScaleTier;
            return;
    }
}
}
