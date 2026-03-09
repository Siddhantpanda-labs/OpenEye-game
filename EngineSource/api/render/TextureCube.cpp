#include "api/render/TextureCube.h"
#include "api/render/RenderDevice.h"
#include "eyelib/render/resources/TextureCube.h"
#include "eyelib/render/resources/TextureTypes.h"

namespace eyelib {
namespace api {

namespace {
::eyelib::render::TextureFilter ToInternal(TextureFilter filter) {
    return static_cast<::eyelib::render::TextureFilter>(filter);
}

::eyelib::render::TextureWrap ToInternal(TextureWrap wrap) {
    return static_cast<::eyelib::render::TextureWrap>(wrap);
}
} // namespace

class TextureCube::Impl {
public:
    std::shared_ptr<::eyelib::render::TextureCube> texture = std::make_shared<::eyelib::render::TextureCube>();
};

TextureCube::TextureCube() : m_impl(std::make_unique<Impl>()) {}

TextureCube::~TextureCube() = default;

TextureCube::TextureCube(TextureCube&& other) noexcept = default;

TextureCube& TextureCube::operator=(TextureCube&& other) noexcept = default;

bool TextureCube::LoadFromFiles(RenderDevice device, const std::string& px, const std::string& nx,
                                const std::string& py, const std::string& ny,
                                const std::string& pz, const std::string& nz,
                                bool isSRGB, bool generateMipmaps) {
    auto* dev = static_cast<::eyelib::render::rhi::IDevice*>(device.Get());
    return dev && m_impl->texture->LoadFromFiles(dev, px, nx, py, ny, pz, nz, isSRGB, generateMipmaps);
}

bool TextureCube::LoadFromFiles(RenderDevice device, const std::vector<std::string>& facePaths, bool isSRGB, bool generateMipmaps) {
    auto* dev = static_cast<::eyelib::render::rhi::IDevice*>(device.Get());
    return dev && m_impl->texture->LoadFromFiles(dev, facePaths, isSRGB, generateMipmaps);
}

void TextureCube::GenerateMipmaps() {
    m_impl->texture->GenerateMipmaps();
}

void TextureCube::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    m_impl->texture->SetFilter(ToInternal(minFilter), ToInternal(magFilter));
}

void TextureCube::SetWrap(TextureWrap s, TextureWrap t, TextureWrap r) {
    m_impl->texture->SetWrap(ToInternal(s), ToInternal(t), ToInternal(r));
}

int TextureCube::GetSize() const {
    return m_impl->texture->GetSize();
}

bool TextureCube::IsValid() const {
    return m_impl->texture->IsValid();
}

std::shared_ptr<::eyelib::render::TextureCube> TextureCube::GetInternal() const {
    return m_impl->texture;
}

std::shared_ptr<TextureCube> TextureCube::WrapInternal(const std::shared_ptr<::eyelib::render::TextureCube>& texture) {
    auto apiTexture = std::make_shared<TextureCube>();
    if (texture) apiTexture->m_impl->texture = texture;
    return apiTexture;
}

} // namespace api
} // namespace eyelib
