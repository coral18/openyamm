#include "game/render/SpriteAtlasCook.h"
#include "tools/SpriteAtlasEncode.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace
{
using namespace OpenYAMM;

std::vector<uint8_t> read(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() < 0 || uint64_t(file.tellg()) > Game::SpriteAtlasCookByteLimit)
    {
        throw std::runtime_error("Missing/oversized atlas file: " + path.string());
    }
    std::vector<uint8_t> bytes(size_t(file.tellg()));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size())))
    {
        throw std::runtime_error("Incomplete read: " + path.string());
    }
    return bytes;
}

Engine::SpriteAtlas parse(const std::vector<uint8_t> &bytes)
{
    std::string error;
    const std::optional<Engine::SpriteAtlas> atlas = Engine::SpriteAtlas::parse(
        std::string(bytes.begin(), bytes.end()), error);
    if (!atlas)
    {
        throw std::runtime_error(error);
    }
    return *atlas;
}

void verify(const std::filesystem::path &root, const std::string &profile)
{
    const std::vector<uint8_t> manifest = read(root / "manifest.json");
    const Engine::SpriteAtlas atlas = parse(manifest);
    if (atlas.schemaVersion != 2 || atlas.textureProfile != profile)
    {
        throw std::runtime_error("Wrong runtime schema/profile in " + root.string());
    }
    std::set<std::filesystem::path> expected = {"manifest.json"};
    for (size_t i = 0; i < atlas.pages.size(); ++i)
    {
        expected.insert(atlas.pages[i].texture);
        Game::decodeCookedSpriteAtlasPage(read(root / atlas.pages[i].texture), atlas, int(i),
            Game::spriteAtlasContentHash(manifest), Game::SpriteAtlasCookTextureLimit);
    }
    for (const auto &[id, variant] : atlas.variants)
    {
        if (!variant.lookup.empty())
        {
            expected.insert(variant.lookup);
            if (read(root / variant.lookup).size() != size_t(variant.lookupSize[0]) * variant.lookupSize[1] * 16)
            {
                throw std::runtime_error("Invalid atlas palette lookup size");
            }
        }
    }
    for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (entry.is_symlink()
            || (entry.is_regular_file() && !expected.contains(entry.path().lexically_relative(root))))
        {
            throw std::runtime_error("Unexpected file in runtime atlas package: " + entry.path().string());
        }
    }
}
}

int main(int argc, char **argv)
{
    using namespace OpenYAMM;
    try
    {
        if (argc == 4 && std::string(argv[1]) == "--verify")
        {
            size_t count = 0;
            for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(argv[2]))
            {
                if (!entry.is_directory() || entry.is_symlink())
                {
                    throw std::runtime_error("Unexpected entry in runtime sprite root: " + entry.path().string());
                }
                verify(entry.path(), argv[3]);
                ++count;
            }
            if (count == 0)
            {
                throw std::runtime_error("Empty runtime sprite root");
            }
            std::cout << "Verified " << count << " source-free " << argv[3] << " sprite packages\n";
            return 0;
        }
        const bool reuseColour = argc == 5 && std::string(argv[3]) == "--reuse-colour";
        if (argc != 3 && !reuseColour)
        {
            std::cerr << "Usage: openyamm_sprite_atlas_cook SOURCE_PACKAGE STAGED_RUNTIME_PACKAGE\n"
                << "       openyamm_sprite_atlas_cook --verify RUNTIME_SPRITE_ROOT desktop|android\n"
                << "Deploy with tools/cook_sprite_atlases.py, which creates manifests and publishes packages.\n";
            return 1;
        }
        const std::filesystem::path sourceRoot = argv[1];
        const std::filesystem::path outputRoot = argv[2];
        const Engine::SpriteAtlas source = parse(read(sourceRoot / "manifest.json"));
        const std::vector<uint8_t> manifest = read(outputRoot / "manifest.json");
        const Engine::SpriteAtlas atlas = parse(manifest);
        if (reuseColour && read(std::filesystem::path(argv[4]) / "manifest.json") != manifest)
        {
            throw std::runtime_error("Cannot reuse colour textures with a changed manifest");
        }
        if (source.schemaVersion != 1 || atlas.schemaVersion != 2 || source.pages.size() != atlas.pages.size()
            || source.frames.size() != atlas.frames.size() || source.maskChannels != atlas.maskChannels
            || source.brightnessMultiplier != atlas.brightnessMultiplier)
        {
            throw std::runtime_error("Invalid source/runtime atlas manifest pair");
        }
        for (const auto &[name, frame] : source.frames)
        {
            const Engine::SpriteAtlasFrame &runtime = atlas.frames.at(name);
            if (runtime.page != frame.page || runtime.rectangle != frame.rectangle
                || runtime.cropOrigin != frame.cropOrigin || runtime.drawSize != frame.drawSize
                || runtime.paletteOverrides != frame.paletteOverrides)
            {
                throw std::runtime_error("Source/runtime sprite placement mismatch");
            }
        }
        size_t diskBytes = 0;
        size_t gpuBytes = 0;
        for (size_t i = 0; i < source.pages.size(); ++i)
        {
            const Engine::SpriteAtlasPage &page = source.pages[i];
            const std::optional<Engine::ImagePixelsBgra> base =
                Engine::decodeImagePixelsBgra(read(sourceRoot / page.base), page.base);
            const std::optional<Engine::ImagePixelsBgra> mask =
                Engine::decodeImagePixelsBgra(read(sourceRoot / page.mask), page.mask);
            if (!base || !mask || page.size != atlas.pages[i].size)
            {
                throw std::runtime_error("Invalid source atlas image/dimensions");
            }
            Game::PreparedSpriteAtlasPage prepared;
            std::vector<uint8_t> bytes;
            if (reuseColour)
            {
                bytes = read(std::filesystem::path(argv[4]) / atlas.pages[i].texture);
                prepared = Game::decodeCookedSpriteAtlasPage(bytes, atlas, int(i),
                    Game::spriteAtlasContentHash(manifest), Game::SpriteAtlasCookTextureLimit);
            }
            else
            {
                prepared = Game::compressSpriteAtlasPage(
                    Game::prepareSpriteAtlasPage(source, int(i), *base, *mask, Game::SpriteAtlasCookTextureLimit),
                    source.maskChannels, atlas.textureProfile);
                bytes = Game::encodeCookedSpriteAtlasPage(atlas, int(i),
                    Game::spriteAtlasContentHash(manifest), prepared);
            }
            const std::filesystem::path output = outputRoot / atlas.pages[i].texture;
            std::filesystem::create_directories(output.parent_path());
            std::ofstream file(output, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
            file.close();
            if (!file)
            {
                throw std::runtime_error("Cannot write " + output.string());
            }
            diskBytes += bytes.size();
            for (const Game::SpriteAtlasTextureLevel &level : prepared.levels)
            {
                gpuBytes += level.baseBlocks.size() + level.maskBlocks.size();
            }
        }
        verify(outputRoot, atlas.textureProfile);
        std::cout << sourceRoot.filename().string() << ": " << source.pages.size() << " pages, "
            << diskBytes << " disk bytes, " << gpuBytes << " GPU block bytes\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Sprite atlas cook failed: " << error.what() << '\n';
        return 1;
    }
}
