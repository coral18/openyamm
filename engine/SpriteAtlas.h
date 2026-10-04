#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenYAMM::Engine
{
struct SpriteAtlasPage
{
    std::string base;
    std::string mask;
    std::array<int, 2> size = {};
    std::string texture;
};

struct SpriteAtlasFrame
{
    int page = 0;
    std::array<int, 4> rectangle = {};
    std::array<float, 2> cropOrigin = {};
    std::array<float, 2> drawSize = {};
    std::unordered_map<int, int> paletteOverrides;
};

struct SpriteAtlasVariant
{
    // w: 0 bypass, 1 chroma, 2 luminance, 3/4 region vectors, 5/6 luminance LUT, 7 RGB displacement LUT.
    std::array<float, 4> chroma = {};
    std::array<float, 4> secondChroma = {};
    std::array<float, 4> thirdChroma = {};
    std::array<float, 4> fourthChroma = {};
    std::string lookup;
    std::array<int, 2> lookupSize = {};
};

struct SpriteAtlas
{
    int schemaVersion = 1;
    std::string textureProfile;
    float pixelsPerLogicalPixel = 1.0f;
    float brightnessMultiplier = 1.0f;
    int maskChannels = 1;
    std::array<int, 2> logicalCanvas = {};
    std::array<float, 2> logicalPivot = {};
    std::vector<SpriteAtlasPage> pages;
    std::unordered_map<std::string, SpriteAtlasFrame> frames;
    std::unordered_map<int, SpriteAtlasVariant> variants;

    static std::optional<SpriteAtlas> parse(const std::string &text, std::string &error);
};

struct SpriteAtlasReference
{
    std::string package;
    std::string frame;
};

// Explicit opt-in; native texture names never probe the enhanced asset directory.
std::optional<SpriteAtlasReference> parseSpriteAtlasReference(const std::string &name);
}
