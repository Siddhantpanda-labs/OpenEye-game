#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/rendering/resources/Texture2D.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace eyelib {
namespace rendering {

/** Glyph quad in pixel space and UV [0,1]. */
struct GlyphQuad {
    float x0, y0, x1, y1;  // Screen quad
    float s0, t0, s1, t1;  // Atlas UV
    float advance;         // Advance to next glyph
};

/**
 * @brief Font from TTF/OTF. Builds glyph atlas, provides quad info per codepoint.
 */
class Font {
public:
    Font();
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    /** Load from file (TTF/OTF). Returns false on failure. */
    bool LoadFromFile(const std::string& path);
    /** Build atlas for codepoint range. Default 32-126 (ASCII). */
    bool BuildAtlas(float pixelHeight, int atlasWidth = 1024, int atlasHeight = 1024,
                    int firstCodepoint = 32, int numCodepoints = 95);
    void Shutdown();

    std::shared_ptr<Texture2D> GetAtlasTexture() const { return m_atlas; }
    bool IsValid() const { return m_atlas && m_atlas->IsValid(); }

    /** Get quad for codepoint. (x,y) is current pen position; quad is filled, advance stored. */
    bool GetGlyphQuad(int codepoint, float x, float y, GlyphQuad& out) const;
    /** Get horizontal advance for codepoint. */
    float GetAdvance(int codepoint) const;
    /** Get font ascender/descender in pixels (for current atlas height). */
    float GetAscender() const { return m_ascender; }
    float GetDescender() const { return m_descender; }
    float GetLineGap() const { return m_lineGap; }
    float GetPixelHeight() const { return m_pixelHeight; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::shared_ptr<Texture2D> m_atlas;
    float m_ascender = 0.0f;
    float m_descender = 0.0f;
    float m_lineGap = 0.0f;
    float m_scale = 0.0f;
    float m_pixelHeight = 0.0f;
    int m_atlasW = 0;
    int m_atlasH = 0;
};

} // namespace rendering
} // namespace eyelib
