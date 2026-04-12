#include "eyelib/rendering/2d/Font.h"
#include "eyelib/rendering/resources/Texture2D.h"
#include "eyelib/rendering/resources/TextureTypes.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace eyelib {
namespace rendering {

struct Font::Impl {
    std::vector<unsigned char> fontData;
    stbtt_fontinfo info;
    stbtt_pack_context packCtx;
    std::vector<stbtt_packedchar> chardata;
    int firstCodepoint = 0;
    int numCodepoints = 0;
    std::vector<unsigned char> atlasBitmap;
};

Font::Font() : m_impl(std::make_unique<Impl>()) {}

Font::~Font() {
    Shutdown();
}

void Font::Shutdown() {
    m_atlas.reset();
    m_impl->chardata.clear();
    m_impl->atlasBitmap.clear();
    m_impl->fontData.clear();
    m_impl->firstCodepoint = 0;
    m_impl->numCodepoints = 0;
    m_ascender = m_descender = m_lineGap = m_scale = m_pixelHeight = 0.0f;
    m_atlasW = m_atlasH = 0;
}

bool Font::LoadFromFile(const std::string& path) {
    Shutdown();
    m_impl->fontData = platform::filesystem::ReadFile(path);
    if (m_impl->fontData.empty()) {
        core::Log::ErrorF("Font: failed to read %s", path.c_str());
        return false;
    }
    if (!stbtt_InitFont(&m_impl->info, m_impl->fontData.data(), stbtt_GetFontOffsetForIndex(m_impl->fontData.data(), 0))) {
        core::Log::ErrorF("Font: stbtt_InitFont failed for %s", path.c_str());
        return false;
    }
    return true;
}

bool Font::BuildAtlas(float pixelHeight, int atlasWidth, int atlasHeight, int firstCodepoint, int numCodepoints) {
    if (m_impl->fontData.empty()) return false;
    m_impl->firstCodepoint = firstCodepoint;
    m_impl->numCodepoints = numCodepoints;
    m_impl->chardata.resize(static_cast<size_t>(numCodepoints));
    m_impl->atlasBitmap.assign(static_cast<size_t>(atlasWidth * atlasHeight), 0);

    if (!stbtt_PackBegin(&m_impl->packCtx, m_impl->atlasBitmap.data(), atlasWidth, atlasHeight, 0, 1, nullptr)) {
        core::Log::ErrorF("Font: PackBegin failed");
        return false;
    }
    stbtt_PackSetOversampling(&m_impl->packCtx, 2, 2);
    if (!stbtt_PackFontRange(&m_impl->packCtx, m_impl->fontData.data(), 0, pixelHeight,
                             firstCodepoint, numCodepoints, m_impl->chardata.data())) {
        stbtt_PackEnd(&m_impl->packCtx);
        core::Log::ErrorF("Font: PackFontRange failed");
        return false;
    }
    stbtt_PackEnd(&m_impl->packCtx);

    m_scale = stbtt_ScaleForPixelHeight(&m_impl->info, pixelHeight);
    int a = 0, d = 0, l = 0;
    stbtt_GetFontVMetrics(&m_impl->info, &a, &d, &l);
    m_ascender = static_cast<float>(a) * m_scale;
    m_descender = static_cast<float>(d) * m_scale;
    m_lineGap = static_cast<float>(l) * m_scale;
    m_pixelHeight = pixelHeight;
    m_atlasW = atlasWidth;
    m_atlasH = atlasHeight;

    m_atlas = std::make_shared<Texture2D>();
    std::vector<unsigned char> rgba(static_cast<size_t>(atlasWidth * atlasHeight * 4));
    for (size_t i = 0; i < m_impl->atlasBitmap.size(); ++i) {
        unsigned char v = m_impl->atlasBitmap[i];
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = v;
    }
    if (!m_atlas->Create(atlasWidth, atlasHeight, TextureFormat::RGBA8, rgba.data())) {
        core::Log::ErrorF("Font: atlas texture create failed");
        return false;
    }
    m_atlas->SetFilter(TextureFilter::Linear, TextureFilter::Linear);
    m_atlas->SetWrap(TextureWrap::ClampToEdge, TextureWrap::ClampToEdge);
    return true;
}

bool Font::GetGlyphQuad(int codepoint, float x, float y, GlyphQuad& out) const {
    int idx = codepoint - m_impl->firstCodepoint;
    if (idx < 0 || idx >= m_impl->numCodepoints) return false;
    float xpos = x, ypos = y;
    stbtt_aligned_quad q;
    stbtt_GetPackedQuad(m_impl->chardata.data(), m_atlasW, m_atlasH, idx, &xpos, &ypos, &q, 1);
    out.x0 = q.x0; out.y0 = q.y0; out.x1 = q.x1; out.y1 = q.y1;
    out.s0 = q.s0; out.t0 = q.t0; out.s1 = q.s1; out.t1 = q.t1;
    out.advance = xpos - x;
    return true;
}

float Font::GetAdvance(int codepoint) const {
    int idx = codepoint - m_impl->firstCodepoint;
    if (idx < 0 || idx >= m_impl->numCodepoints) return 0.0f;
    const stbtt_packedchar& pc = m_impl->chardata[static_cast<size_t>(idx)];
    return pc.xadvance;
}

} // namespace rendering
} // namespace eyelib
