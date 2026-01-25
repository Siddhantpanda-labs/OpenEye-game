#include "api/render/Texture3D.h"
#include "eyelib/render/resources/Texture3D.h"
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

class Texture3D::Impl {
public:
    std::shared_ptr<render::Texture3D> texture = std::make_shared<render::Texture3D>();
};

Texture3D::Texture3D() : m_impl(std::make_unique<Impl>()) {}

Texture3D::~Texture3D() = default;

Texture3D::Texture3D(Texture3D&& other) noexcept = default;

Texture3D& Texture3D::operator=(Texture3D&& other) noexcept = default;

bool Texture3D::Create(int width, int height, int depth, TextureFormat format, const void* data) {
    return m_impl->texture->Create(width, height, depth, ToInternal(format), data);
}

void Texture3D::SetData(const void* data, int x, int y, int z, int width, int height, int depth) {
    m_impl->texture->SetData(data, x, y, z, width, height, depth);
}

std::vector<uint8_t> Texture3D::GetData() const {
    return m_impl->texture->GetData();
}

void Texture3D::GenerateMipmaps() {
    m_impl->texture->GenerateMipmaps();
}

void Texture3D::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    m_impl->texture->SetFilter(ToInternal(minFilter), ToInternal(magFilter));
}

void Texture3D::SetWrap(TextureWrap s, TextureWrap t, TextureWrap r) {
    m_impl->texture->SetWrap(ToInternal(s), ToInternal(t), ToInternal(r));
}

int Texture3D::GetWidth() const {
    return m_impl->texture->GetWidth();
}

int Texture3D::GetHeight() const {
    return m_impl->texture->GetHeight();
}

int Texture3D::GetDepth() const {
    return m_impl->texture->GetDepth();
}

TextureFormat Texture3D::GetFormat() const {
    return FromInternal(m_impl->texture->GetFormat());
}

bool Texture3D::IsValid() const {
    return m_impl->texture->IsValid();
}

std::shared_ptr<render::Texture3D> Texture3D::GetInternal() const {
    return m_impl->texture;
}

std::shared_ptr<Texture3D> Texture3D::WrapInternal(const std::shared_ptr<render::Texture3D>& texture) {
    auto apiTexture = std::make_shared<Texture3D>();
    if (texture) apiTexture->m_impl->texture = texture;
    return apiTexture;
}

} // namespace api
} // namespace eyelib
