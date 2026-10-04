#include "game/render/NativeSpriteTextureCache.h"

#include <doctest/doctest.h>

TEST_CASE("native sprite residency pins active resources and isolates map overrides and refreshes")
{
    using namespace OpenYAMM;
    bgfx::Init init;
    init.type = bgfx::RendererType::Noop;
    init.resolution.width = 16;
    init.resolution.height = 16;
    REQUIRE(bgfx::init(init));
    struct Shutdown
    {
        ~Shutdown() { bgfx::shutdown(); }
    } shutdown;
    Engine::AssetFileSystem assets;
    Game::NativeSpriteTextureCache cache(100);
    cache.beginLevel(assets);
    const auto create = [&cache](const std::string &identity)
    {
        Game::SpriteBillboardTexture texture;
        texture.textureName = "same";
        texture.width = texture.physicalWidth = 4;
        texture.height = texture.physicalHeight = 4;
        texture.textureHandle = bgfx::createTexture2D(4, 4, true, 1, bgfx::TextureFormat::BGRA8);
        REQUIRE(bgfx::isValid(texture.textureHandle));
        cache.retain(texture, identity);
        CHECK(texture.retained);
        return texture.textureHandle;
    };
    const bgfx::TextureHandle shared = create({});
    const bgfx::TextureHandle mapOne = create("worlds/mm6/rendering/map-one/same.png");
    CHECK(cache.residentBytes() > 100); // Both are required by the active map.
    REQUIRE(cache.find("SAME", 0));
    CHECK(cache.find("same", 0)->textureHandle.idx == shared.idx);
    REQUIRE(cache.find("same", 0, "worlds/mm6/rendering/map-one/same.png"));
    CHECK(cache.find("same", 0, "worlds/mm6/rendering/map-one/same.png")->textureHandle.idx == mapOne.idx);
    CHECK_FALSE(cache.find("same", 1));
    cache.beginLevel(assets);
    CHECK(cache.find("same", 0)); // Pin the shared object in the next map.
    create("worlds/mm6/rendering/map-two/same.png");
    CHECK_FALSE(cache.find("same", 0, "worlds/mm6/rendering/map-one/same.png"));
    CHECK(cache.find("same", 0));
    CHECK(cache.uploadedTextures() == 1);
    assets.refreshLookupCache();
    cache.beginLevel(assets);
    CHECK_FALSE(cache.find("same", 0));
    CHECK(cache.residentBytes() == 0);
    cache.clear(true);
    bgfx::frame();
}
