#include "api/render/RenderTarget.h"
#include "eyelib/render/RenderTarget.h"
#include "eyelib/render/resources/Texture2D.h"

namespace eyelib {
namespace api {

namespace {
::eyelib::render::TextureFormat ToInternal(TextureFormat format) {
    return static_cast<::eyelib::render::TextureFormat>(format);
}
} // namespace

class RenderTarget::Impl {
public:
    std::shared_ptr<::eyelib::render::RenderTarget> target = std::make_shared<::eyelib::render::RenderTarget>();
};

RenderTarget::RenderTarget() : m_impl(std::make_unique<Impl>()) {}

RenderTarget::~RenderTarget() = default;

RenderTarget::RenderTarget(RenderTarget&& other) noexcept = default;

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept = default;

bool RenderTarget::Create(uint32_t width, uint32_t height,
                          const std::vector<TextureFormat>& colorFormats,
                          TextureFormat depthFormat, int samples) {
    std::vector<::eyelib::render::TextureFormat> internalFormats;
    internalFormats.reserve(colorFormats.size());
    for (auto format : colorFormats) {
        internalFormats.push_back(ToInternal(format));
    }
    return m_impl->target->Create(width, height, internalFormats, ToInternal(depthFormat), samples);
}

void RenderTarget::Resize(uint32_t width, uint32_t height) {
    m_impl->target->Resize(width, height);
}

void RenderTarget::Destroy() {
    m_impl->target->Destroy();
}

void RenderTarget::Bind() const {
    m_impl->target->Bind();
}

void RenderTarget::Unbind() const {
    m_impl->target->Unbind();
}

void RenderTarget::Resolve() {
    m_impl->target->Resolve();
}

uint32_t RenderTarget::GetWidth() const {
    return m_impl->target->GetWidth();
}

uint32_t RenderTarget::GetHeight() const {
    return m_impl->target->GetHeight();
}

int RenderTarget::GetSamples() const {
    return m_impl->target->GetSamples();
}

size_t RenderTarget::GetColorAttachmentCount() const {
    return m_impl->target->GetColorAttachmentCount();
}

std::shared_ptr<Texture> RenderTarget::GetColorTexture(size_t slot) const {
    auto tex = m_impl->target->GetColorTexture(slot);
    return tex ? Texture::WrapInternal(tex) : nullptr;
}

std::shared_ptr<Texture> RenderTarget::GetDepthTexture() const {
    auto tex = m_impl->target->GetDepthTexture();
    return tex ? Texture::WrapInternal(tex) : nullptr;
}

void RenderTarget::Blit(const RenderTarget& source, const RenderTarget& dest, bool colorOnly) {
    ::eyelib::render::RenderTarget::Blit(*source.m_impl->target, *dest.m_impl->target, colorOnly);
}

int RenderTarget::GetMaxSamples(int samples) {
    return ::eyelib::render::RenderTarget::GetMaxSamples(samples);
}

RenderTarget RenderTarget::CreateColorDepth(uint32_t width, uint32_t height,
                                            TextureFormat colorFormat,
                                            TextureFormat depthFormat,
                                            int samples) {
    RenderTarget target;
    target.Create(width, height, std::vector<TextureFormat>{colorFormat}, depthFormat, samples);
    return target;
}

RenderTarget RenderTarget::CreateColorOnly(uint32_t width, uint32_t height,
                                           TextureFormat colorFormat,
                                           int samples) {
    RenderTarget target;
    target.Create(width, height, std::vector<TextureFormat>{colorFormat}, TextureFormat::None, samples);
    return target;
}

RenderTarget RenderTarget::CreateDepthOnly(uint32_t width, uint32_t height,
                                           TextureFormat depthFormat,
                                           int samples) {
    RenderTarget target;
    target.Create(width, height, std::vector<TextureFormat>{}, depthFormat, samples);
    return target;
}

std::shared_ptr<::eyelib::render::RenderTarget> RenderTarget::GetInternal() const {
    return m_impl->target;
}

std::shared_ptr<RenderTarget> RenderTarget::WrapInternal(const std::shared_ptr<::eyelib::render::RenderTarget>& target) {
    auto apiTarget = std::make_shared<RenderTarget>();
    if (target) apiTarget->m_impl->target = target;
    return apiTarget;
}

} // namespace api
} // namespace eyelib
