"""Run the real bgfx D3D11 upload function without requiring Windows or a GPU."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile


REPO = Path(__file__).resolve().parents[1]
BGFX = Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / 'build/_deps/bgfx-src'

PREAMBLE = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>

struct D3D11_BOX { uint32_t left, top, front, right, bottom, back; };
struct ID3D11DeviceContext
{
    uint32_t subresource = 0, pitch = 0, slicePitch = 0;
    bool whole = false;
    D3D11_BOX box = {};
    void UpdateSubresource(void *, uint32_t subres, const D3D11_BOX *region, const void *,
        uint32_t row, uint32_t slice)
    {
        subresource = subres; pitch = row; slicePitch = slice; whole = region == nullptr;
        if (region) box = *region;
    }
};
namespace bimg
{
    namespace TextureFormat { enum Enum { BC4, BC5, BC7, BGRA8, Depth }; }
    struct ImageBlockInfo { uint16_t bitsPerPixel, blockWidth, blockHeight, blockSize; };
    bool isCompressed(TextureFormat::Enum format) { return format <= TextureFormat::BC7; }
    bool isDepth(TextureFormat::Enum format) { return format == TextureFormat::Depth; }
    const ImageBlockInfo &getBlockInfo(TextureFormat::Enum format)
    {
        static const ImageBlockInfo bc4 = {4, 4, 4, 8}, bc7 = {8, 4, 4, 16}, rgba = {32, 1, 1, 4};
        return format == TextureFormat::BC4 ? bc4 : isCompressed(format) ? bc7 : rgba;
    }
    void imageDecodeToBgra8(void *, void *, void *, uint32_t, uint32_t, uint32_t, TextureFormat::Enum)
    { std::abort(); } // These checks exercise native uploads, without format conversion.
}
namespace bx
{
    template<class T> T max(T a, T b) { return std::max(a, b); }
    void *alloc(void *, size_t bytes) { return std::malloc(bytes); }
    void free(void *, void *data) { std::free(data); }
}
namespace bgfx
{
    struct Rect { uint16_t m_x, m_y, m_width, m_height; };
    struct Memory { uint8_t *data; };
    struct Renderer { ID3D11DeviceContext *m_deviceCtx; };
    Renderer *s_renderD3D11;
    void *g_allocator = nullptr;
    struct TextureD3D11
    {
        enum { Texture2D, TextureCube, Texture3D };
        int m_type = Texture2D;
        uint32_t m_width = 0, m_height = 0, m_depth = 1, m_numMips = 13;
        uint8_t m_textureFormat = bimg::TextureFormat::BC7, m_requestedFormat = m_textureFormat;
        void *m_ptr = nullptr;
        void update(uint8_t, uint8_t, const Rect &, uint16_t, uint16_t, uint16_t, const Memory *);
    };
}
'''

CHECKS = r'''
int main()
{
    using namespace bgfx;
    ID3D11DeviceContext context;
    Renderer renderer = {&context};
    s_renderD3D11 = &renderer;
    Memory memory = {nullptr};
    TextureD3D11 texture;
    for (const auto format : {bimg::TextureFormat::BC4, bimg::TextureFormat::BC5, bimg::TextureFormat::BC7})
    {
        texture.m_textureFormat = texture.m_requestedFormat = format;
        for (const Rect rect : {Rect{0, 0, 246, 280}, Rect{0, 0, 123, 140}, Rect{0, 0, 242, 296},
            Rect{0, 0, 121, 148}, Rect{0, 0, 124, 133}, Rect{0, 0, 102, 151},
            Rect{0, 0, 1, 1}, Rect{0, 0, 2, 2}, Rect{0, 0, 4, 4}})
        {
            texture.m_width = rect.m_width * 16;
            texture.m_height = rect.m_height * 16;
            const uint32_t pitch = ((rect.m_width + 3) / 4) * (format == bimg::TextureFormat::BC4 ? 8 : 16);
            texture.update(0, 4, rect, 2, 1, UINT16_MAX, &memory);
            assert(context.pitch == pitch && context.whole && context.subresource == 2 * 13 + 4);
            texture.update(0, 4, rect, 0, 1, pitch + 32, &memory);
            assert(context.pitch == pitch + 32 && context.whole);
        }
    }
    texture.m_width = texture.m_height = 512;
    texture.update(0, 0, Rect{4, 8, 8, 12}, 0, 1, UINT16_MAX, &memory);
    assert(context.pitch == 32 && !context.whole && context.box.left == 4 && context.box.bottom == 20);
    texture.m_type = TextureD3D11::TextureCube;
    texture.update(5, 0, Rect{4, 8, 8, 12}, 2, 1, 64, &memory);
    assert(context.subresource == (2 * 6 + 5) * 13 && context.pitch == 64 && !context.whole);

    texture.m_type = TextureD3D11::Texture3D;
    texture.m_width = 96; texture.m_height = 112; texture.m_depth = 32;
    texture.update(0, 4, Rect{0, 0, 6, 7}, 0, 2, UINT16_MAX, &memory);
    assert(context.pitch == 32 && context.slicePitch == 64 && context.whole);
    texture.update(0, 4, Rect{0, 0, 6, 7}, 1, 1, 48, &memory);
    assert(context.pitch == 48 && context.slicePitch == 96 && !context.whole);
    assert(context.box.front == 1 && context.box.back == 2);

    texture.m_type = TextureD3D11::Texture2D;
    texture.m_textureFormat = texture.m_requestedFormat = bimg::TextureFormat::BGRA8;
    texture.update(0, 0, Rect{0, 0, 7, 5}, 0, 1, UINT16_MAX, &memory);
    assert(context.pitch == 28 && context.slicePitch == 0 && !context.whole);
    texture.m_type = TextureD3D11::Texture3D;
    texture.update(0, 0, Rect{0, 0, 7, 5}, 0, 1, 40, &memory);
    assert(context.pitch == 40 && context.slicePitch == 200 && !context.whole);
    texture.m_textureFormat = texture.m_requestedFormat = bimg::TextureFormat::Depth;
    texture.update(0, 0, Rect{0, 0, 7, 5}, 0, 1, UINT16_MAX, &memory);
    assert(context.pitch == 28 && context.whole);
}
'''

source = (BGFX / 'src/renderer_d3d11.cpp').read_text()
match = re.search(r'void TextureD3D11::update\(.*?\n\t\}', source, re.DOTALL)
assert match, 'Cannot find the pinned bgfx upload function'
with tempfile.TemporaryDirectory(prefix='openyamm-d3d11-upload-check-') as temporary:
    root = Path(temporary)
    cpp = root / 'check.cpp'
    cpp.write_text(PREAMBLE + '\nnamespace bgfx {\n' + match[0] + '\n}\n' + CHECKS)
    subprocess.run(['c++', '-std=c++20', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(root / 'check')], check=True)
    subprocess.run([str(root / 'check')], check=True)
print('Direct3D 11 texture upload checks passed')
