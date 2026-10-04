#include "game/render/NativeSpriteTextureCache.h"

namespace OpenYAMM::Game
{
NativeSpriteTextureCache::Key NativeSpriteTextureCache::key(
    std::string name, int16_t palette, const std::string &resourceIdentity)
{
    for (char &letter : name)
    {
        if (letter >= 'A' && letter <= 'Z')
        {
            letter += 'a' - 'A';
        }
    }
    return {resourceIdentity.empty() ? "sprite:" + name : "asset:" + resourceIdentity, palette};
}

void NativeSpriteTextureCache::beginLevel(const Engine::AssetFileSystem &assets)
{
    if (m_generation != assets.contentGeneration())
    {
        clear(true);
        m_generation = assets.contentGeneration();
    }
    ++m_level;
    m_uploadedTextures = 0;
}

const SpriteBillboardTexture *NativeSpriteTextureCache::find(
    const std::string &name, int16_t palette, const std::string &resourceIdentity)
{
    const auto entry = m_entries.find(key(name, palette, resourceIdentity));
    if (entry == m_entries.end())
    {
        return nullptr;
    }
    entry->second.lastUse = m_level;
    return &entry->second.texture;
}

void NativeSpriteTextureCache::retain(SpriteBillboardTexture &texture, const std::string &resourceIdentity)
{
    if (texture.atlas || !bgfx::isValid(texture.textureHandle))
    {
        return;
    }
    const Key textureKey = key(texture.textureName, texture.paletteId, resourceIdentity);
    const auto existing = m_entries.find(textureKey);
    if (existing != m_entries.end())
    {
        if (existing->second.texture.textureHandle.idx != texture.textureHandle.idx)
        {
            bgfx::destroy(texture.textureHandle);
        }
        existing->second.lastUse = m_level;
        texture = existing->second.texture;
        return;
    }
    bgfx::TextureInfo info;
    bgfx::calcTextureSize(info, uint16_t(texture.physicalWidth), uint16_t(texture.physicalHeight),
        1, false, true, 1, bgfx::TextureFormat::BGRA8);
    while (m_residentBytes + info.storageSize > m_budget)
    {
        auto oldest = m_entries.end();
        for (auto entry = m_entries.begin(); entry != m_entries.end(); ++entry)
        {
            if (entry->second.lastUse != m_level
                && (oldest == m_entries.end() || entry->second.lastUse < oldest->second.lastUse))
            {
                oldest = entry;
            }
        }
        if (oldest == m_entries.end())
        {
            break; // Required active textures stay pinned.
        }
        bgfx::destroy(oldest->second.texture.textureHandle);
        m_residentBytes -= oldest->second.bytes;
        m_entries.erase(oldest);
    }
    texture.retained = true;
    m_entries.emplace(textureKey, Entry{texture, m_level, info.storageSize});
    m_residentBytes += info.storageSize;
    ++m_uploadedTextures;
}

void NativeSpriteTextureCache::clear(bool destroyGpu)
{
    if (destroyGpu)
    {
        for (const auto &[key, entry] : m_entries)
        {
            bgfx::destroy(entry.texture.textureHandle);
        }
    }
    m_entries.clear();
    m_residentBytes = 0;
    m_uploadedTextures = 0;
}
}
