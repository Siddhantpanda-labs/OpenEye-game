#include "eyelib/render/renderer/GBuffer.h"
#include "eyelib/render/rhi/RHIUtils.h"

namespace eyelib {
namespace render {

GBuffer::GBuffer() = default;

GBuffer::~GBuffer() { Destroy(); }

GBuffer::GBuffer(GBuffer&& other) noexcept
    : m_width(other.m_width), m_height(other.m_height),
      m_deviceRHI(other.m_deviceRHI),
      m_renderTargetRHI(other.m_renderTargetRHI) {
    other.m_deviceRHI = nullptr;
    other.m_renderTargetRHI = nullptr;
}

GBuffer& GBuffer::operator=(GBuffer&& other) noexcept {
    if (this != &other) {
        Destroy();
        m_width = other.m_width; m_height = other.m_height;
        m_deviceRHI = other.m_deviceRHI;
        m_renderTargetRHI = other.m_renderTargetRHI;
        other.m_deviceRHI = nullptr;
        other.m_renderTargetRHI = nullptr;
    }
    return *this;
}

bool GBuffer::Initialize(uint32_t width, uint32_t height) {
    // Device must be passed explicitly; use Initialize(device, width, height).
    return false;
}

bool GBuffer::Initialize(rhi::IDevice* device, uint32_t width, uint32_t height) {
    if (!device || !width || !height) return false;
    Destroy();
    m_width = width;
    m_height = height;
    m_deviceRHI = device;

    std::vector<rhi::RHITextureFormat> colors = {
        rhi::RHITextureFormat::RGBA16F,  // position (alpha unused)
        rhi::RHITextureFormat::RGBA16F,  // normal (alpha unused)
        rhi::RHITextureFormat::RGBA8,    // albedo
        rhi::RHITextureFormat::RGBA8,    // PBR params (metallic, roughness, reflective, unused)
        rhi::RHITextureFormat::RGBA16F   // velocity (RG16F not in RHI)
    };
    m_renderTargetRHI = device->CreateRenderTarget(width, height, colors, rhi::RHITextureFormat::Depth24, 1);
    if (!m_renderTargetRHI) {
        Destroy();
        return false;
    }
    return true;
}

void GBuffer::Resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    if (m_deviceRHI) Initialize(m_deviceRHI, width, height);
}

void GBuffer::BindForGeometryPass(rhi::ICommandList* cmd) {
    if (!cmd || !m_renderTargetRHI) return;
    rhi::ClearValue clear = {0.f, 0.f, 0.f, 1.f};
    rhi::ClearAndSetRenderTargetWithViewport(cmd, m_renderTargetRHI, 0, 0, m_width, m_height, clear);
}

void GBuffer::BindForGeometryPassNoClear(rhi::ICommandList* cmd) {
    if (!cmd || !m_renderTargetRHI) return;
    cmd->SetRenderTarget(m_renderTargetRHI);
    cmd->SetViewport(0, 0, m_width, m_height);
}

void GBuffer::BindForLightingPass(rhi::ICommandList* cmd) {
    (void)cmd;
    // Deferred lighting uses Frame + DeferredLightingPass descriptor sets; no manual G-buffer texture binds.
}

void GBuffer::BindDepthTexture(unsigned int textureUnit) {
    if (!m_deviceRHI) return;
    rhi::IDevice* device = m_deviceRHI;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    rhi::ITexture* depth = GetDepthTextureRHI();
    if (cmd && depth) cmd->BindTexture(depth, textureUnit);
}

void GBuffer::Unbind(rhi::ICommandList* cmd) {
    if (cmd) cmd->SetRenderTarget(nullptr);
}

void GBuffer::Destroy() {
    if (m_deviceRHI && m_renderTargetRHI) {
        m_deviceRHI->DestroyRenderTarget(m_renderTargetRHI);
        m_renderTargetRHI = nullptr;
    }
    m_deviceRHI = nullptr;
}

unsigned int GBuffer::GetFBO() const {
    if (!m_renderTargetRHI) return 0;
    void* native = m_renderTargetRHI->GetNativeFramebuffer();
    return native ? static_cast<unsigned int>(reinterpret_cast<uintptr_t>(native)) : 0u;
}

rhi::ITexture* GBuffer::GetColorTextureRHI(uint32_t index) const {
    return m_renderTargetRHI ? m_renderTargetRHI->GetColorTexture(index) : nullptr;
}

rhi::ITexture* GBuffer::GetPositionTextureRHI() const { return GetColorTextureRHI(0); }
rhi::ITexture* GBuffer::GetNormalTextureRHI() const { return GetColorTextureRHI(1); }
rhi::ITexture* GBuffer::GetAlbedoTextureRHI() const { return GetColorTextureRHI(2); }
rhi::ITexture* GBuffer::GetPBRParamsTextureRHI() const { return GetColorTextureRHI(3); }
rhi::ITexture* GBuffer::GetVelocityTextureRHI() const { return GetColorTextureRHI(4); }
rhi::ITexture* GBuffer::GetDepthTextureRHI() const { return m_renderTargetRHI ? m_renderTargetRHI->GetDepthTexture() : nullptr; }

static unsigned int NativeIdFromTexture(rhi::ITexture* tex) {
    if (!tex) return 0;
    void* native = tex->GetNativeTexture();
    return native ? static_cast<unsigned int>(reinterpret_cast<uintptr_t>(native)) : 0u;
}

unsigned int GBuffer::GetPositionTexture() const { return NativeIdFromTexture(GetPositionTextureRHI()); }
unsigned int GBuffer::GetNormalTexture() const { return NativeIdFromTexture(GetNormalTextureRHI()); }
unsigned int GBuffer::GetAlbedoTexture() const { return NativeIdFromTexture(GetAlbedoTextureRHI()); }
unsigned int GBuffer::GetPBRParamsTexture() const { return NativeIdFromTexture(GetPBRParamsTextureRHI()); }
unsigned int GBuffer::GetVelocityTexture() const { return NativeIdFromTexture(GetVelocityTextureRHI()); }
unsigned int GBuffer::GetDepthTexture() const { return NativeIdFromTexture(GetDepthTextureRHI()); }

} // namespace render
} // namespace eyelib
