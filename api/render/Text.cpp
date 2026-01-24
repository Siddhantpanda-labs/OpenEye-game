#include "api/render/Text.h"
#include "eyelib/render/2d/Font.h"
#include "eyelib/render/2d/TextRenderer.h"

namespace eyelib {
namespace api {

class Font::Impl {
public:
    std::unique_ptr<render::Font> font = std::make_unique<render::Font>();
};

Font::Font() : m_impl(std::make_unique<Impl>()) {}

Font::~Font() = default;

Font::Font(Font&& other) noexcept = default;

Font& Font::operator=(Font&& other) noexcept = default;

bool Font::LoadFromFile(const std::string& path) {
    return m_impl->font->LoadFromFile(path);
}

bool Font::BuildAtlas(float pixelHeight, int atlasWidth, int atlasHeight,
                      int firstCodepoint, int numCodepoints) {
    return m_impl->font->BuildAtlas(pixelHeight, atlasWidth, atlasHeight, firstCodepoint, numCodepoints);
}

void Font::Shutdown() {
    m_impl->font->Shutdown();
}

bool Font::IsValid() const {
    return m_impl->font->IsValid();
}

float Font::GetAscender() const {
    return m_impl->font->GetAscender();
}

float Font::GetDescender() const {
    return m_impl->font->GetDescender();
}

float Font::GetLineGap() const {
    return m_impl->font->GetLineGap();
}

float Font::GetPixelHeight() const {
    return m_impl->font->GetPixelHeight();
}

class TextRenderer::Impl {
public:
    std::unique_ptr<render::TextRenderer> renderer = std::make_unique<render::TextRenderer>();
};

TextRenderer::TextRenderer() : m_impl(std::make_unique<Impl>()) {}

TextRenderer::~TextRenderer() = default;

TextRenderer::TextRenderer(TextRenderer&& other) noexcept = default;

TextRenderer& TextRenderer::operator=(TextRenderer&& other) noexcept = default;

bool TextRenderer::Initialize() {
    return m_impl->renderer->Initialize();
}

void TextRenderer::Shutdown() {
    m_impl->renderer->Shutdown();
}

void TextRenderer::RenderText(const std::string& text, Font& font, const math::Vec2& position,
                              const math::Vec4& color, float size,
                              const math::Mat4& view, const math::Mat4& projection) {
    m_impl->renderer->RenderText(text, *font.m_impl->font, position, color, size, view, projection);
}

} // namespace api
} // namespace eyelib
