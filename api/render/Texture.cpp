#include "api/render/Texture.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/TextureTypes.h"

namespace eyelib {
namespace api {

namespace {
render::TextureFormat ToInternal(TextureFormat format) {
    return static_cast<render::TextureFormat>(format);
}

render::TextureFilter ToInternal(TextureFilter filter) {
    return static_cast<render::TextureFilter>(filter);
}

render::TextureWrap ToInternal(TextureWrap wrap) {
    return static_cast<render::TextureWrap>(wrap);
}

TextureFormat FromInternal(render::TextureFormat format) {
    return static_cast<TextureFormat>(format);
}
} // namespace

class Texture::Impl {
public:
    std::shared_ptr<render::Texture2D> texture = std::make_shared<render::Texture2D>();
};

Texture::Texture() : m_impl(std::make_unique<Impl>()) {}

Texture::~Texture() = default;

Texture::Texture(Texture&& other) noexcept = default;

Texture& Texture::operator=(Texture&& other) noexcept = default;

bool Texture::LoadFromFile(const std::string& path, bool isSRGB, bool generateMipmaps) {
    return m_impl->texture->LoadFromFile(path, isSRGB, generateMipmaps);
}

bool Texture::LoadFromMemory(const uint8_t* data, size_t size, bool isSRGB, bool generateMipmaps) {
    return m_impl->texture->LoadFromMemory(data, size, isSRGB, generateMipmaps);
}

bool Texture::Create(int width, int height, int format, const void* data) {
    render::TextureFormat fmt = static_cast<render::TextureFormat>(format);
    return m_impl->texture->Create(width, height, fmt, data);
}

bool Texture::Create(int width, int height, TextureFormat format, const void* data) {
    return m_impl->texture->Create(width, height, ToInternal(format), data);
}

void Texture::SetData(const void* data, int x, int y, int width, int height) {
    m_impl->texture->SetData(data, x, y, width, height);
}

std::vector<uint8_t> Texture::GetData() const {
    return m_impl->texture->GetData();
}

void Texture::GenerateMipmaps() {
    m_impl->texture->GenerateMipmaps();
}

void Texture::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    m_impl->texture->SetFilter(ToInternal(minFilter), ToInternal(magFilter));
}

void Texture::SetWrap(TextureWrap s, TextureWrap t) {
    m_impl->texture->SetWrap(ToInternal(s), ToInternal(t));
}

int Texture::GetWidth() const {
    return m_impl->texture->GetWidth();
}

int Texture::GetHeight() const {
    return m_impl->texture->GetHeight();
}

TextureFormat Texture::GetFormat() const {
    return FromInternal(m_impl->texture->GetFormat());
}

bool Texture::IsValid() const {
    return m_impl->texture->IsValid();
}

std::shared_ptr<render::Texture2D> Texture::GetInternal() const {
    return m_impl->texture;
}

std::shared_ptr<Texture> Texture::WrapInternal(const std::shared_ptr<render::Texture2D>& texture) {
    auto apiTexture = std::make_shared<Texture>();
    if (texture) apiTexture->m_impl->texture = texture;
    return apiTexture;
}

} // namespace api
} // namespace eyelib
