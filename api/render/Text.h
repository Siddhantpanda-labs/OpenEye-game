#pragma once

#include "eyelib/math/Types.h"
#include <memory>
#include <string>

namespace eyelib {
namespace api {

class Font;

/**
 * @brief Text renderer API (opaque). Render text using a font.
 */
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
    TextRenderer(TextRenderer&& other) noexcept;
    TextRenderer& operator=(TextRenderer&& other) noexcept;

    bool Initialize();
    void Shutdown();

    /** Draw text. Position = bottom-left in 2D space. */
    void RenderText(const std::string& text, Font& font, const math::Vec2& position,
                    const math::Vec4& color, float size,
                    const math::Mat4& view, const math::Mat4& projection);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

/**
 * @brief Font API (opaque). Load TTF/OTF, build atlas, get metrics.
 */
class Font {
public:
    Font();
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;

    /** Load from file (TTF/OTF). */
    bool LoadFromFile(const std::string& path);

    /** Build atlas for codepoint range. */
    bool BuildAtlas(float pixelHeight, int atlasWidth = 1024, int atlasHeight = 1024,
                    int firstCodepoint = 32, int numCodepoints = 95);

    void Shutdown();

    bool IsValid() const;
    float GetAscender() const;
    float GetDescender() const;
    float GetLineGap() const;
    float GetPixelHeight() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    friend class TextRenderer;
};

} // namespace api
} // namespace eyelib
