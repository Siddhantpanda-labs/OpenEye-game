#include "api/render/TextureArray.h"
#include "eyelib/render/resources/TextureArray.h"
#include "eyelib/render/resources/TextureTypes.h"

namespace eyelib {
namespace api {

namespace {
::eyelib::render::TextureFormat ToInternal(TextureFormat format) {
    return static_cast<::eyelib::render::TextureFormat>(format);
}

::eyelib::render::TextureFilter ToInternal(TextureFilter filter) {
    return static_cast<::eyelib::render::TextureFilter>(filter);
}

::eyelib::render::TextureWrap ToInternal(TextureWrap wrap) {
    return static_cast<::eyelib::render::TextureWrap>(wrap);
}

TextureFormat FromInternal(::eyelib::render::TextureFormat format) {
    return static_cast<TextureFormat>(format);
}
} // namespace

class TextureArray::Impl {
public:
    std::shared_ptr<::eyelib::render::TextureArray> texture = std::make_shared<::eyelib::render::TextureArray>();
};

TextureArray::TextureArray() : m_impl(std::make_unique<Impl>()) {}

TextureArray::~TextureArray() = default;

TextureArray::TextureArray(TextureArray&& other) noexcept = default;

TextureArray& TextureArray::operator=(TextureArray&& other) noexcept = default;

bool TextureArray::Create(int width, int height, int layers, TextureFormat format, const void* data) {
    return m_impl->texture->Create(width, height, layers, ToInternal(format), data);
}

void TextureArray::SetData(const void* data, int layer, int x, int y, int width, int height) {
    m_impl->texture->SetData(data, layer, x, y, width, height);
}

void TextureArray::GenerateMipmaps() {
    m_impl->texture->GenerateMipmaps();
}

void TextureArray::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    m_impl->texture->SetFilter(ToInternal(minFilter), ToInternal(magFilter));
}

void TextureArray::SetWrap(TextureWrap s, TextureWrap t) {
    m_impl->texture->SetWrap(ToInternal(s), ToInternal(t));
}

int TextureArray::GetWidth() const {
    return m_impl->texture->GetWidth();
}

int TextureArray::GetHeight() const {
    return m_impl->texture->GetHeight();
}

int TextureArray::GetLayers() const {
    return m_impl->texture->GetLayers();
}

TextureFormat TextureArray::GetFormat() const {
    return FromInternal(m_impl->texture->GetFormat());
}

bool TextureArray::IsValid() const {
    return m_impl->texture->IsValid();
}

std::shared_ptr<::eyelib::render::TextureArray> TextureArray::GetInternal() const {
    return m_impl->texture;
}

std::shared_ptr<TextureArray> TextureArray::WrapInternal(const std::shared_ptr<::eyelib::render::TextureArray>& texture) {
    auto apiTexture = std::make_shared<TextureArray>();
    if (texture) apiTexture->m_impl->texture = texture;
    return apiTexture;
}

} // namespace api
} // namespace eyelib
