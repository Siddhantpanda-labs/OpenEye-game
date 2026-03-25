#include "eyelib/render/passes/LightingPass.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/ibl/IBL.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/PassPushConstants.h"
#include "eyelib/render/RenderBackend.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/render/rhi/PipelineAttachmentFormatSets.h"
#include "eyelib/core/Log.h"
#include <optional>

namespace eyelib {
namespace render {

namespace {
rhi::VertexLayout GetFullscreenVertexLayout() {
    rhi::VertexLayout layout;
    layout.strideInBytes = 4 * sizeof(float);
    layout.attributes = {
        {0, rhi::VertexAttributeType::Float2, 0},
        {1, rhi::VertexAttributeType::Float2, 2 * sizeof(float)},
    };
    return layout;
}
} // namespace

LightingPass::LightingPass() = default;

LightingPass::~LightingPass() = default;

void LightingPass::EnsureDefaultShader(rhi::IDevice* device) {
    if (m_lightingShader && m_lightingShader->IsValid()) return;
    if (!device) return;
    m_lightingShader = std::make_shared<Shader>();
    const rhi::VertexLayout fullscreenLayout = GetFullscreenVertexLayout();
    rhi::PipelineLayoutKind layoutKind = rhi::PipelineLayoutKind::PushOnly;
    if (device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::DeferredLightingPass))
        layoutKind = rhi::PipelineLayoutKind::DeferredLighting;
    const std::optional<rhi::PipelineAttachmentFormats> lightingFormats = std::optional{rhi::StandardAttachmentFormats::DeferredLightingAccumulation()};
    if (!m_lightingShader->Load(device, "passes/deferred/lighting", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, layoutKind, lightingFormats)) {
        if (layoutKind != rhi::PipelineLayoutKind::PushOnly &&
            !m_lightingShader->Load(device, "passes/deferred/lighting", fullscreenLayout, rhi::PrimitiveTopology::TriangleList, rhi::PipelineLayoutKind::PushOnly, lightingFormats)) {
            core::Log::Warn("LightingPass: deferred lighting shader load failed");
            return;
        }
    }
}

void LightingPass::SetLightingShader(std::shared_ptr<Shader> shader) {
    m_lightingShader = std::move(shader);
}

void LightingPass::ReloadShaders() {
    if (m_lightingShader) m_lightingShader->Reload();
}

void LightingPass::Render(GBuffer& gbuffer,
                          const RenderContext& ctx,
                          const LightParams& light,
                          const math::Mat4& view,
                          const math::Mat4& projection,
                          const math::Vec3& viewPos,
                          CascadeShadowMap* cascadeMap,
                          const std::vector<PointLight>* pointLights,
                          const std::vector<SpotLight>* spotLights,
                          rhi::IBuffer* sceneLightingBuffer,
                          IBL* ibl) {
    Render(gbuffer, ctx.device, ctx.cmd, light, view, projection, viewPos, cascadeMap, pointLights, spotLights, sceneLightingBuffer, ibl);
}

void LightingPass::Render(GBuffer& gbuffer,
                          rhi::IDevice* device,
                          rhi::ICommandList* cmd,
                          const LightParams& light,
                          const math::Mat4& view,
                          const math::Mat4& projection,
                          const math::Vec3& viewPos,
                          CascadeShadowMap* cascadeMap,
                          const std::vector<PointLight>* pointLights,
                          const std::vector<SpotLight>* spotLights,
                          rhi::IBuffer* sceneLightingBuffer,
                          IBL* ibl) {
    (void)gbuffer;
    (void)light;
    (void)view;
    (void)projection;
    (void)viewPos;
    (void)cascadeMap;
    (void)pointLights;
    (void)spotLights;
    (void)sceneLightingBuffer;
    EnsureDefaultShader(device);
    if (!m_lightingShader || !m_lightingShader->IsValid()) return;
    rhi::IPipeline* pipe = m_lightingShader->GetPipelineRHI();
    if (pipe && !pipe->HasValidProgram()) return;

    if (!m_lightingFrameSet || !m_lightingDeferredPassSet || !cmd) return;

    m_lightingShader->Bind(device);
    const int useIBL = (ibl && ibl->IsValid()) ? 1 : 0;
    if (device && device->GetRenderBackend() == RenderBackend::Vulkan) {
        if (cmd) abi::WriteDeferredLightingUseIBL(cmd, useIBL);
    } else {
        m_lightingShader->SetInt("u_UseIBL", useIBL);
    }

    rhi::IDescriptorSet* sets[2] = { m_lightingFrameSet, m_lightingDeferredPassSet };
    cmd->BindDescriptorSets(0, 2, sets);

    if (cmd) {
        cmd->SetDepthTest(false);
        cmd->SetBlend(false);
    }
    if (cmd && m_lightingShader->GetPipelineRHI())
        cmd->DrawFullscreenQuad(m_lightingShader->GetPipelineRHI());
    if (cmd) cmd->SetDepthTest(true);

    m_lightingShader->Unbind();
}

} // namespace render
} // namespace eyelib
