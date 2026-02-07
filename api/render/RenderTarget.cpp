#include "api/render/RenderTarget.h"
#include "api/render/RenderPipeline.h"
#include "eyelib/render/RenderTarget.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/rhi/RHI.h"

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

bool RenderTarget::Create(RenderPipeline* pipeline, uint32_t width, uint32_t height,
                          const std::vector<TextureFormat>& colorFormats,
                          TextureFormat depthFormat, int samples) {
    if (!pipeline) return false;
    void* dev = pipeline->GetDevice();
    if (!dev) return false;
    auto* device = static_cast<::eyelib::render::rhi::IDevice*>(dev);
    std::vector<::eyelib::render::TextureFormat> internalFormats;
    internalFormats.reserve(colorFormats.size());
    for (auto format : colorFormats) {
        internalFormats.push_back(ToInternal(format));
    }
    return m_impl->target->Create(device, width, height, internalFormats, ToInternal(depthFormat), static_cast<uint32_t>(samples));
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

int RenderTarget::GetMaxSamples(RenderPipeline* pipeline, int samples) {
    if (!pipeline) return 0;
    void* dev = pipeline->GetDevice();
    if (!dev) return 0;
    return ::eyelib::render::RenderTarget::GetMaxSamples(static_cast<::eyelib::render::rhi::IDevice*>(dev), samples);
}

RenderTarget RenderTarget::CreateColorDepth(RenderPipeline* pipeline, uint32_t width, uint32_t height,
                                            TextureFormat colorFormat,
                                            TextureFormat depthFormat,
                                            int samples) {
    RenderTarget target;
    target.Create(pipeline, width, height, std::vector<TextureFormat>{colorFormat}, depthFormat, samples);
    return target;
}

RenderTarget RenderTarget::CreateColorOnly(RenderPipeline* pipeline, uint32_t width, uint32_t height,
                                           TextureFormat colorFormat,
                                           int samples) {
    RenderTarget target;
    target.Create(pipeline, width, height, std::vector<TextureFormat>{colorFormat}, TextureFormat::None, samples);
    return target;
}

RenderTarget RenderTarget::CreateDepthOnly(RenderPipeline* pipeline, uint32_t width, uint32_t height,
                                           TextureFormat depthFormat,
                                           int samples) {
    RenderTarget target;
    target.Create(pipeline, width, height, std::vector<TextureFormat>{}, depthFormat, samples);
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
