#include "tools/SpriteAtlasEncode.h"

#include <ProcessDxtc.hpp>
#include <ProcessRGB.hpp>
#include <bc7enc.h>
#include <EtcImage.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace OpenYAMM::Game
{
namespace
{
std::vector<uint8_t> compress(const SpriteAtlasMipLevel &level, SpriteAtlasCodec codec, int channels, bool mask)
{
    const int width = (level.width + 3) / 4 * 4;
    const int height = (level.height + 3) / 4 * 4;
    const bool etc = codec == SpriteAtlasCodec::EacR || codec == SpriteAtlasCodec::EacRg;
    // etcpak's EAC input is BGRA; BC/Etc2Comp input is RGBA. Pad partial blocks by edge replication.
    std::vector<uint32_t> pixels(size_t(width) * height);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const size_t source = size_t(std::min(y, level.height - 1)) * level.width
                + std::min(x, level.width - 1);
            std::array<uint8_t, 4> rgba;
            if (mask)
            {
                rgba = {level.mask[source * channels],
                    channels >= 2 ? level.mask[source * channels + 1] : uint8_t(0),
                    channels == 4 ? level.mask[source * channels + 2] : uint8_t(0),
                    channels == 4 ? level.mask[source * channels + 3] : uint8_t(255)};
            }
            else
            {
                rgba = {level.baseBgra[source * 4 + 2], level.baseBgra[source * 4 + 1],
                    level.baseBgra[source * 4], level.baseBgra[source * 4 + 3]};
            }
            if (etc)
            {
                std::swap(rgba[0], rgba[2]);
            }
            std::memcpy(&pixels[size_t(y) * width + x], rgba.data(), 4);
        }
    }
    const size_t bytes = spriteAtlasBlockBytes(codec, width, height);
    if (codec == SpriteAtlasCodec::Etc2Rgba)
    {
        std::vector<float> rgba(pixels.size() * 4);
        const uint8_t *pBytes = reinterpret_cast<const uint8_t *>(pixels.data());
        for (size_t i = 0; i < rgba.size(); ++i)
        {
            rgba[i] = pBytes[i] / 255.0f;
        }
        // Mask alpha is an independent material weight, never coverage for its other channels.
        const Etc::ErrorMetric metric = mask ? Etc::ErrorMetric::RGBX : Etc::ErrorMetric::RGBA;
        Etc::Image image(rgba.data(), width, height, metric);
        const unsigned int workers = std::clamp(std::thread::hardware_concurrency(), 1u, 8u);
        const Etc::Image::EncodingStatus status = image.Encode(Etc::Image::Format::RGBA8, metric, 80, workers, workers);
        if (status >= Etc::Image::ERROR_THRESHOLD || image.GetEncodingBitsBytes() != bytes)
        {
            throw std::runtime_error("ETC2 sprite encoding failed");
        }
        return {image.GetEncodingBits(), image.GetEncodingBits() + bytes};
    }
    std::vector<uint64_t> blocks(bytes / 8);
    bc7enc_compress_block_params parameters;
    bc7enc_compress_block_params_init(&parameters);
    parameters.m_uber_level = 2;
    if (mask)
    {
        bc7enc_compress_block_params_init_linear_weights(&parameters);
    }
    else
    {
        // Preserve silhouette/soft coverage as carefully as luminance.
        parameters.m_weights[3] = 128;
    }
    const int rows = height / 4;
    std::atomic<int> next = 0;
    const size_t workers = std::min(size_t(rows), std::clamp(size_t(std::thread::hardware_concurrency()),
        size_t(1), size_t(8)));
    std::vector<std::jthread> threads;
    for (size_t i = 0; i < workers; ++i)
    {
        threads.emplace_back([&]()
        {
            for (int row = next.fetch_add(1); row < rows; row = next.fetch_add(1))
            {
                const uint32_t *pSource = pixels.data() + size_t(row) * width * 4;
                uint64_t *pTarget = blocks.data() + size_t(row) * (blocks.size() / rows);
                switch (codec)
                {
                case SpriteAtlasCodec::Bc7: CompressBc7(pSource, pTarget, width / 4, width, &parameters); break;
                case SpriteAtlasCodec::Bc4: CompressBc4(pSource, pTarget, width / 4, width); break;
                case SpriteAtlasCodec::Bc5: CompressBc5(pSource, pTarget, width / 4, width); break;
                case SpriteAtlasCodec::Etc2Rgba: break; // Encoded above with the offline quality encoder.
                case SpriteAtlasCodec::EacR: CompressEacR(pSource, pTarget, width / 4, width); break;
                case SpriteAtlasCodec::EacRg: CompressEacRg(pSource, pTarget, width / 4, width); break;
                }
            }
        });
    }
    threads.clear();
    std::vector<uint8_t> result(bytes);
    std::memcpy(result.data(), blocks.data(), bytes);
    return result;
}
}

PreparedSpriteAtlasPage compressSpriteAtlasPage(SpriteAtlasSourcePage source, int maskChannels,
    const std::string &profile)
{
    if ((profile != "desktop" && profile != "android") || (maskChannels != 1 && maskChannels != 2 && maskChannels != 4))
    {
        throw std::invalid_argument("Invalid sprite atlas texture profile or channels");
    }
    static std::once_flag initialize;
    std::call_once(initialize, bc7enc_compress_block_init);
    PreparedSpriteAtlasPage result;
    const bool desktop = profile == "desktop";
    result.baseCodec = desktop ? SpriteAtlasCodec::Bc7 : SpriteAtlasCodec::Etc2Rgba;
    result.maskCodec = maskChannels == 4 ? result.baseCodec
        : maskChannels == 2 ? (desktop ? SpriteAtlasCodec::Bc5 : SpriteAtlasCodec::EacRg)
        : (desktop ? SpriteAtlasCodec::Bc4 : SpriteAtlasCodec::EacR);
    result.rectangles = std::move(source.mips.rectangles);
    result.opacity = std::move(source.opacity);
    for (const SpriteAtlasMipLevel &level : source.mips.levels)
    {
        result.levels.push_back({level.width, level.height, compress(level, result.baseCodec, maskChannels, false),
            compress(level, result.maskCodec, maskChannels, true)});
    }
    return result;
}
}
