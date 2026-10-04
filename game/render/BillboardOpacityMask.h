#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace OpenYAMM::Game
{
class BillboardOpacityMask
{
public:
    void assignFromBgra(const std::vector<uint8_t> &pixels, int width, int height)
    {
        assignFromBgraRegion(pixels, width, height, 0, 0, width, height);
    }

    void assignFromBgraRegion(const std::vector<uint8_t> &pixels, int sourceWidth, int sourceHeight,
        int originX, int originY, int width, int height)
    {
        m_width = 0;
        m_height = 0;
        m_opaqueTop = 0;
        m_hasOpaquePixel = false;
        m_bits.clear();

        if (width <= 0 || height <= 0 || sourceWidth < width || sourceHeight < height
            || originX < 0 || originY < 0 || originX > sourceWidth - width || originY > sourceHeight - height)
        {
            return;
        }

        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);

        if (pixels.size() / 4 < size_t(sourceWidth) * sourceHeight)
        {
            return;
        }

        m_width = width;
        m_height = height;
        m_bits.assign((pixelCount + 7) / 8, 0);

        for (int y = 0; y < height; ++y)
        {
            const uint8_t *pRow = pixels.data() + (size_t(originY + y) * sourceWidth + originX) * 4;
            for (int x = 0; x < width; ++x)
            {
                if (pRow[size_t(x) * 4 + 3] != 0)
                {
                    const size_t pixelIndex = size_t(y) * width + x;
                    m_bits[pixelIndex / 8] |= uint8_t(1u << (pixelIndex % 8));
                    if (!m_hasOpaquePixel)
                    {
                        m_opaqueTop = y;
                        m_hasOpaquePixel = true;
                    }
                }
            }
        }
    }

    bool empty() const
    {
        return m_bits.empty();
    }

    size_t byteSize() const
    {
        return m_bits.size();
    }

    const std::vector<uint8_t> &bits() const { return m_bits; }

    bool assignBits(int width, int height, std::vector<uint8_t> bits)
    {
        if (width <= 0 || height <= 0 || bits.size() != (size_t(width) * height + 7) / 8)
        {
            return false;
        }
        const size_t usedBits = size_t(width) * height % 8;
        if (usedBits != 0 && (bits.back() >> usedBits) != 0)
        {
            return false;
        }
        m_width = width;
        m_height = height;
        m_bits = std::move(bits);
        m_hasOpaquePixel = false;
        m_opaqueTop = 0;
        for (size_t i = 0; i < m_bits.size(); ++i)
        {
            if (m_bits[i] != 0)
            {
                m_hasOpaquePixel = true;
                m_opaqueTop = int((i * 8 + std::countr_zero(unsigned(m_bits[i]))) / size_t(width));
                break;
            }
        }
        return true;
    }

    bool isOpaque(int x, int y) const
    {
        if (m_bits.empty() || m_width <= 0 || m_height <= 0)
        {
            return true;
        }

        if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        {
            return false;
        }

        const size_t pixelIndex = static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x);
        return (m_bits[pixelIndex / 8] & static_cast<uint8_t>(1u << (pixelIndex % 8))) != 0;
    }

    bool isOpaqueNormalized(float normalizedU, float normalizedV) const
    {
        if (m_bits.empty() || m_width <= 0 || m_height <= 0)
        {
            return true;
        }

        const int x = std::clamp(
            static_cast<int>(std::floor(normalizedU * static_cast<float>(m_width))),
            0,
            m_width - 1);
        const int y = std::clamp(
            static_cast<int>(std::floor(normalizedV * static_cast<float>(m_height))),
            0,
            m_height - 1);
        return isOpaque(x, y);
    }

    std::optional<std::array<float, 2>> nearestOpaqueNormalized(
        float normalizedU,
        float normalizedV) const
    {
        if (!std::isfinite(normalizedU) || !std::isfinite(normalizedV))
        {
            return std::nullopt;
        }

        if (m_bits.empty() || m_width <= 0 || m_height <= 0)
        {
            return std::array<float, 2>{
                std::clamp(normalizedU, 0.0f, 1.0f),
                std::clamp(normalizedV, 0.0f, 1.0f),
            };
        }

        if (!m_hasOpaquePixel)
        {
            return std::nullopt;
        }

        const float targetX = normalizedU * static_cast<float>(m_width);
        const float targetY = normalizedV * static_cast<float>(m_height);
        float bestDistanceSquared = std::numeric_limits<float>::max();
        float bestX = 0.0f;
        float bestY = 0.0f;
        const size_t pixelCount = static_cast<size_t>(m_width) * static_cast<size_t>(m_height);

        for (size_t byteIndex = 0; byteIndex < m_bits.size(); ++byteIndex)
        {
            uint8_t remainingBits = m_bits[byteIndex];
            while (remainingBits != 0)
            {
                const unsigned int bitIndex = std::countr_zero(static_cast<unsigned int>(remainingBits));
                const size_t pixelIndex = byteIndex * 8 + bitIndex;
                remainingBits &= static_cast<uint8_t>(remainingBits - 1);

                if (pixelIndex >= pixelCount)
                {
                    continue;
                }

                const int x = static_cast<int>(pixelIndex % static_cast<size_t>(m_width));
                const int y = static_cast<int>(pixelIndex / static_cast<size_t>(m_width));
                const float candidateX = std::clamp(targetX, static_cast<float>(x), static_cast<float>(x + 1));
                const float candidateY = std::clamp(targetY, static_cast<float>(y), static_cast<float>(y + 1));
                const float deltaX = targetX - candidateX;
                const float deltaY = targetY - candidateY;
                const float distanceSquared = deltaX * deltaX + deltaY * deltaY;

                if (distanceSquared < bestDistanceSquared)
                {
                    bestDistanceSquared = distanceSquared;
                    bestX = candidateX;
                    bestY = candidateY;

                    if (distanceSquared == 0.0f)
                    {
                        return std::array<float, 2>{normalizedU, normalizedV};
                    }
                }
            }
        }

        if (bestDistanceSquared == std::numeric_limits<float>::max())
        {
            return std::nullopt;
        }

        return std::array<float, 2>{
            bestX / static_cast<float>(m_width),
            bestY / static_cast<float>(m_height),
        };
    }

    float opaqueTopNormalized() const
    {
        if (!m_hasOpaquePixel || m_height <= 0)
        {
            return 0.0f;
        }
        return static_cast<float>(m_opaqueTop) / static_cast<float>(m_height);
    }

private:
    int m_width = 0;
    int m_height = 0;
    int m_opaqueTop = 0;
    bool m_hasOpaquePixel = false;
    std::vector<uint8_t> m_bits;
};
}
