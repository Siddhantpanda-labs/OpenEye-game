#include "eyelib/render/passes/DeferredLightingPassSystem.h"
#include "eyelib/render/RenderContext.h"
#include "eyelib/debug/Profiler.h"
#include "eyelib/render/passes/LightingPass.h"
#include "eyelib/render/renderer/GBuffer.h"
#include "eyelib/render/rhi/RHIUtils.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/ibl/IBL.h"
#include "eyelib/core/Log.h"
#include <cstring>

namespace eyelib {
namespace render {

DeferredLightingPassSystem::~DeferredLightingPassSystem() {
    Shutdown(m_fallbackDevice);
}

void DeferredLightingPassSystem::Shutdown(rhi::IDevice* device) {
    rhi::IDevice* d = device ? device : m_fallbackDevice;
    if (!d)
        return;
    if (m_fallback2DTexture) {
        d->DestroyTexture(m_fallback2DTexture);
        m_fallback2DTexture = nullptr;
    }
    if (m_fallbackCubeTexture) {
        d->DestroyTexture(m_fallbackCubeTexture);
        m_fallbackCubeTexture = nullptr;
    }
    if (m_fallbackStorageBuffer) {
        d->DestroyBuffer(m_fallbackStorageBuffer);
        m_fallbackStorageBuffer = nullptr;
    }
    if (m_fallbackUniformBuffer) {
        d->DestroyBuffer(m_fallbackUniformBuffer);
        m_fallbackUniformBuffer = nullptr;
    }
    m_fallbackDevice = nullptr;
}

void DeferredLightingPassSystem::EnsureFallbackResources(rhi::IDevice* device) const {
    if (!device)
        return;
    if (m_fallbackDevice && m_fallbackDevice != device) {
        // Device swap safety for shared/editor reload flows.
        const_cast<DeferredLightingPassSystem*>(this)->Shutdown(m_fallbackDevice);
    }
    m_fallbackDevice = device;

    if (!m_fallback2DTexture) {
        rhi::TextureDesc td{};
        td.type = rhi::TextureType::Texture2D;
        td.width = 1;
        td.height = 1;
        td.format = rhi::RHITextureFormat::RGBA8;
        td.usage = rhi::TextureUsage::ShaderResource;
        td.mipLevels = 1;
        static const uint8_t black[4] = { 0, 0, 0, 255 };
        m_fallback2DTexture = device->CreateTexture(td, black);
    }
    if (!m_fallbackCubeTexture) {
        rhi::TextureDesc td{};
        td.type = rhi::TextureType::TextureCube;
        td.width = 1;
        td.height = 1;
        td.format = rhi::RHITextureFormat::RGBA8;
        td.usage = rhi::TextureUsage::ShaderResource;
        td.mipLevels = 1;
        m_fallbackCubeTexture = device->CreateTexture(td, nullptr);
    }
    if (!m_fallbackStorageBuffer) {
        rhi::BufferDesc bd{};
        bd.sizeInBytes = 64u;
        bd.usage = rhi::BufferUsage::Storage;
        bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_fallbackStorageBuffer = device->CreateBuffer(bd, nullptr);
    }
    if (!m_fallbackUniformBuffer) {
        rhi::BufferDesc bd{};
        bd.sizeInBytes = 256u;
        bd.usage = rhi::BufferUsage::Uniform;
        bd.memoryHint = rhi::BufferMemoryHint::HostVisible;
        m_fallbackUniformBuffer = device->CreateBuffer(bd, nullptr);
    }
}

DeferredLightingPassExecuteParams DeferredLightingPassSystem::BuildParams(const DeferredLightingPassBuildParams& params) const {
    DeferredLightingPassExecuteParams out{};
    out.build = params;

    if (params.frameUbo) {
        const math::Mat4 viewProj = params.camView.projection * params.camView.view;
        float frameUboData[5 * 16];
        std::memcpy(frameUboData, &params.camView.view[0][0], 16 * sizeof(float));
        std::memcpy(frameUboData + 16, &params.camView.projection[0][0], 16 * sizeof(float));
        std::memcpy(frameUboData + 32, &viewProj[0][0], 16 * sizeof(float));
        std::memcpy(frameUboData + 48, &params.prevView[0][0], 16 * sizeof(float));
        std::memcpy(frameUboData + 64, &params.prevProjection[0][0], 16 * sizeof(float));
        params.frameUbo->Update(frameUboData, 0, 320);
    }

    rhi::IDescriptorSet* const passSet = params.deferredLightingSet;
    GBuffer* const gbuffer = params.gbuffer;
    LightingPass* const lightingPass = params.lightingPass;
    if (!passSet || !gbuffer || !lightingPass) {
        static bool s_loggedMissingBuildInputs = false;
        if (!s_loggedMissingBuildInputs) {
            core::Log::WarnF(
                "DeferredLightingPassSystem::BuildParams skipped: passSet=%p gbuffer=%p lightingPass=%p",
                (void*)passSet, (void*)gbuffer, (void*)lightingPass);
            s_loggedMissingBuildInputs = true;
        }
        return out;
    }

    EnsureFallbackResources(params.device);
    if (!m_fallback2DTexture || !m_fallbackCubeTexture || !m_fallbackStorageBuffer || !m_fallbackUniformBuffer) {
        static bool s_loggedFallbackFailure = false;
        if (!s_loggedFallbackFailure) {
            core::Log::Warn("DeferredLightingPassSystem: fallback resource init failed; deferred set may be incomplete.");
            s_loggedFallbackFailure = true;
        }
    }

    // Vulkan contract: DeferredLighting set (bindings 0..9) must be fully populated before bind.
    // Fill everything with typed fallbacks first, then override with real resources if present.
    rhi::ITexture* gPos = gbuffer->GetPositionTextureRHI();
    rhi::ITexture* gNorm = gbuffer->GetNormalTextureRHI();
    rhi::ITexture* gAlb = gbuffer->GetAlbedoTextureRHI();
    rhi::ITexture* gPbr = gbuffer->GetPBRParamsTextureRHI();
    rhi::ITexture* gDepth = gbuffer->GetDepthTextureRHI();
    rhi::ITexture* shadowDepth = params.cascadeForLighting ? params.cascadeForLighting->GetDepthTextureRHI() : nullptr;
    rhi::IBuffer* sceneLighting = params.sceneLightingBuffer ? params.sceneLightingBuffer : m_fallbackStorageBuffer;
    rhi::IBuffer* lightSpaceBuf = params.cascadeForLighting ? params.cascadeForLighting->GetLightSpaceBufferRHI() : nullptr;
    rhi::ITexture* ir = (params.ibl && params.ibl->IsValid()) ? params.ibl->GetIrradianceTextureRHI() : nullptr;
    rhi::ITexture* pf = (params.ibl && params.ibl->IsValid()) ? params.ibl->GetPrefilterTextureRHI() : nullptr;
    rhi::ITexture* brdf = (params.ibl && params.ibl->IsValid()) ? params.ibl->GetBRDFLUTTextureRHI() : nullptr;

    passSet->UpdateTexture(0, gPos ? gPos : m_fallback2DTexture);
    passSet->UpdateTexture(1, gNorm ? gNorm : m_fallback2DTexture);
    passSet->UpdateTexture(2, gAlb ? gAlb : m_fallback2DTexture);
    passSet->UpdateTexture(3, gPbr ? gPbr : m_fallback2DTexture);
    passSet->UpdateTexture(4, shadowDepth ? shadowDepth : (gDepth ? gDepth : m_fallback2DTexture));
    passSet->UpdateStorageBuffer(5, sceneLighting, 0, 0);
    passSet->UpdateUniformBuffer(6, lightSpaceBuf ? lightSpaceBuf : m_fallbackUniformBuffer, 0, 0);
    passSet->UpdateTexture(7, ir ? ir : m_fallbackCubeTexture);
    passSet->UpdateTexture(8, pf ? pf : m_fallbackCubeTexture);
    passSet->UpdateTexture(9, brdf ? brdf : m_fallback2DTexture);

    lightingPass->SetResourceSpaceSets(params.frameSet, passSet);
    return out;
}

bool DeferredLightingPassSystem::Execute(const DeferredLightingPassExecuteParams& params) const {
    rhi::IDevice* const device = params.build.device;
    rhi::ICommandList* const cmd = params.build.cmd;
    LightingPass* const lightingPass = params.build.lightingPass;
    GBuffer* const gbuffer = params.build.gbuffer;
    if (!params.build.frameData || !lightingPass || !gbuffer) {
        static bool s_loggedMissingExecuteInputs = false;
        if (!s_loggedMissingExecuteInputs) {
            core::Log::WarnF(
                "DeferredLightingPassSystem::Execute skipped: frameData=%p lightingPass=%p gbuffer=%p",
                (void*)params.build.frameData, (void*)lightingPass, (void*)gbuffer);
            s_loggedMissingExecuteInputs = true;
        }
        return false;
    }
    if (cmd && params.build.hdrRenderTarget)
        rhi::ClearAndSetRenderTargetWithViewport(cmd, params.build.hdrRenderTarget, 0, 0, params.build.width, params.build.height,
                                                 params.build.clearColor);
    RenderContext c{};
    c.device = device;
    c.cmd = cmd;
    lightingPass->Render(*gbuffer, c, params.build.lightParams, params.build.camView.view, params.build.camView.projection,
                         params.build.camView.viewPos, params.build.cascadeForLighting,
                         &params.build.frameData->pointLights, &params.build.frameData->spotLights,
                         params.build.sceneLightingBuffer, params.build.ibl);
    debug::Profiler::AddFrameCounter("drawCalls", 1);
    debug::Profiler::AddFrameCounter("triangles", 2);
    debug::Profiler::AddFrameCounter("stateChanges", 1);
    if (cmd) cmd->SetRenderTarget(nullptr);
    return true;
}

}  // namespace render
}  // namespace eyelib

