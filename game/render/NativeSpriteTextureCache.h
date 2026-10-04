#pragma once

#include "game/render/SpriteAtlasCache.h"

namespace OpenYAMM::Game
{
// Owns standalone billboard textures shared by indoor and outdoor renderers.
// Mounted world, scale policy and development refresh are part of the content generation.
class NativeSpriteTextureCache
{
public:
#if defined(__ANDROID__)
    static constexpr size_t DefaultBudget = 32 * 1024 * 1024;
#else
    static constexpr size_t DefaultBudget = 128 * 1024 * 1024;
#endif
    explicit NativeSpriteTextureCache(size_t budget = DefaultBudget) : m_budget(budget) {}
    void beginLevel(const Engine::AssetFileSystem &assets);
    const SpriteBillboardTexture *find(const std::string &name, int16_t palette,
        const std::string &resourceIdentity = {});
    void retain(SpriteBillboardTexture &texture, const std::string &resourceIdentity = {});
    void clear(bool destroyGpu);
    size_t uploadedTextures() const { return m_uploadedTextures; }
    size_t residentBytes() const { return m_residentBytes; }

private:
    using Key = std::pair<std::string, int16_t>;
    static Key key(std::string name, int16_t palette, const std::string &resourceIdentity);
    struct Entry
    {
        SpriteBillboardTexture texture;
        uint64_t lastUse = 0;
        size_t bytes = 0;
    };
    std::map<Key, Entry> m_entries;
    uint64_t m_generation = 0;
    uint64_t m_level = 0;
    size_t m_residentBytes = 0;
    size_t m_uploadedTextures = 0;
    size_t m_budget;
};
}
