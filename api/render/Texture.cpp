#include "api/render/Texture.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/TextureTypes.h"

namespace eyelib {
namespace api {

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

bool Texture::Create(int width, int height, int format, const void* data) {
    render::TextureFormat fmt = static_cast<render::TextureFormat>(format);
    return m_impl->texture->Create(width, height, fmt, data);
}

int Texture::GetWidth() const {
    return m_impl->texture->GetWidth();
}

int Texture::GetHeight() const {
    return m_impl->texture->GetHeight();
}

bool Texture::IsValid() const {
    return m_impl->texture->IsValid();
}

std::shared_ptr<render::Texture2D> Texture::GetInternal() const {
    return m_impl->texture;
}

} // namespace api
} // namespace eyelib
