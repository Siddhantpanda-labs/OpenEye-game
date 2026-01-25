#include "api/render/TextureCube.h"
#include "eyelib/render/resources/TextureCube.h"
#include "eyelib/render/resources/TextureTypes.h"

namespace eyelib {
namespace api {

namespace {
render::TextureFilter ToInternal(TextureFilter filter) {
    return static_cast<render::TextureFilter>(filter);
}

render::TextureWrap ToInternal(TextureWrap wrap) {
    return static_cast<render::TextureWrap>(wrap);
}
} // namespace

class TextureCube::Impl {
public:
    std::shared_ptr<render::TextureCube> texture = std::make_shared<render::TextureCube>();
};

TextureCube::TextureCube() : m_impl(std::make_unique<Impl>()) {}

TextureCube::~TextureCube() = default;

TextureCube::TextureCube(TextureCube&& other) noexcept = default;

TextureCube& TextureCube::operator=(TextureCube&& other) noexcept = default;

bool TextureCube::LoadFromFiles(const std::string& px, const std::string& nx,
                                const std::string& py, const std::string& ny,
                                const std::string& pz, const std::string& nz,
                                bool isSRGB, bool generateMipmaps) {
    return m_impl->texture->LoadFromFiles(px, nx, py, ny, pz, nz, isSRGB, generateMipmaps);
}

bool TextureCube::LoadFromFiles(const std::vector<std::string>& facePaths, bool isSRGB, bool generateMipmaps) {
    return m_impl->texture->LoadFromFiles(facePaths, isSRGB, generateMipmaps);
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

std::shared_ptr<render::TextureCube> TextureCube::GetInternal() const {
    return m_impl->texture;
}

std::shared_ptr<TextureCube> TextureCube::WrapInternal(const std::shared_ptr<render::TextureCube>& texture) {
    auto apiTexture = std::make_shared<TextureCube>();
    if (texture) apiTexture->m_impl->texture = texture;
    return apiTexture;
}

} // namespace api
} // namespace eyelib
