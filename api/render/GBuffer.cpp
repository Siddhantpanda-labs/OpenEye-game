#include "api/render/GBuffer.h"
#include "eyelib/render/GBuffer.h"
#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace api {

class GBuffer::Impl {
public:
    std::shared_ptr<::eyelib::render::GBuffer> gbuffer = std::make_shared<::eyelib::render::GBuffer>();
    ::eyelib::render::rhi::IDevice* device = nullptr;
};

GBuffer::GBuffer() : m_impl(std::make_unique<Impl>()) {}

GBuffer::~GBuffer() = default;

GBuffer::GBuffer(GBuffer&& other) noexcept = default;

GBuffer& GBuffer::operator=(GBuffer&& other) noexcept = default;

bool GBuffer::Initialize(uint32_t width, uint32_t height) {
    return m_impl->gbuffer->Initialize(width, height);
}

void GBuffer::Resize(uint32_t width, uint32_t height) {
    m_impl->gbuffer->Resize(width, height);
}

void GBuffer::SetDevice(RenderDevice device) {
    m_impl->device = static_cast<::eyelib::render::rhi::IDevice*>(device.Get());
}

void GBuffer::BindForGeometryPass() {
    auto* cmd = m_impl->device ? m_impl->device->GetImmediateCommandList() : nullptr;
    m_impl->gbuffer->BindForGeometryPass(cmd);
}

void GBuffer::BindForLightingPass() {
    auto* cmd = m_impl->device ? m_impl->device->GetImmediateCommandList() : nullptr;
    m_impl->gbuffer->BindForLightingPass(cmd);
}

void GBuffer::BindDepthTexture(uint32_t textureUnit) {
    m_impl->gbuffer->BindDepthTexture(textureUnit);
}

void GBuffer::Unbind() {
    auto* cmd = m_impl->device ? m_impl->device->GetImmediateCommandList() : nullptr;
    m_impl->gbuffer->Unbind(cmd);
}

uint32_t GBuffer::GetWidth() const {
    return m_impl->gbuffer->GetWidth();
}

uint32_t GBuffer::GetHeight() const {
    return m_impl->gbuffer->GetHeight();
}

uint32_t GBuffer::GetPositionTexture() const {
    return static_cast<uint32_t>(m_impl->gbuffer->GetPositionTexture());
}

uint32_t GBuffer::GetNormalTexture() const {
    return static_cast<uint32_t>(m_impl->gbuffer->GetNormalTexture());
}

uint32_t GBuffer::GetAlbedoTexture() const {
    return static_cast<uint32_t>(m_impl->gbuffer->GetAlbedoTexture());
}

uint32_t GBuffer::GetPBRParamsTexture() const {
    return static_cast<uint32_t>(m_impl->gbuffer->GetPBRParamsTexture());
}

uint32_t GBuffer::GetVelocityTexture() const {
    return static_cast<uint32_t>(m_impl->gbuffer->GetVelocityTexture());
}

uint32_t GBuffer::GetDepthTexture() const {
    return static_cast<uint32_t>(m_impl->gbuffer->GetDepthTexture());
}

std::shared_ptr<::eyelib::render::GBuffer> GBuffer::GetInternal() const {
    return m_impl->gbuffer;
}

std::shared_ptr<GBuffer> GBuffer::WrapInternal(const std::shared_ptr<::eyelib::render::GBuffer>& buffer) {
    auto apiBuffer = std::make_shared<GBuffer>();
    if (buffer) apiBuffer->m_impl->gbuffer = buffer;
    return apiBuffer;
}

} // namespace api
} // namespace eyelib
