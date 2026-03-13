#include "eyelib/render/passes/LightingPass.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/core/Log.h"
#include <algorithm>

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
    if (!m_lightingShader->Load(device, "passes/deferred/lighting", fullscreenLayout)) {
        core::Log::Warn("LightingPass: deferred lighting shader load failed");
        return;
    }
}

void LightingPass::SetLightingShader(std::shared_ptr<Shader> shader) {
    m_lightingShader = std::move(shader);
}

void LightingPass::ReloadShaders() {
    if (m_lightingShader) m_lightingShader->Reload();
}

static const int kMaxPointLights = 8;
static const int kMaxSpotLights = 4;

void LightingPass::Render(GBuffer& gbuffer,
                          const RenderContext& ctx,
                          const LightParams& light,
                          const math::Mat4& view,
                          const math::Mat4& projection,
                          const math::Vec3& viewPos,
                          CascadeShadowMap* cascadeMap,
                          const std::vector<PointLight>* pointLights,
                          const std::vector<SpotLight>* spotLights,
                          rhi::IBuffer* sceneLightingBuffer) {
    Render(gbuffer, ctx.device, ctx.cmd, light, view, projection, viewPos, cascadeMap, pointLights, spotLights, sceneLightingBuffer);
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
                          rhi::IBuffer* sceneLightingBuffer) {
    (void)view;
    (void)projection;
    (void)light;
    (void)viewPos;
    (void)pointLights;
    (void)spotLights;
    EnsureDefaultShader(device);
    if (!m_lightingShader || !m_lightingShader->IsValid()) return;
    rhi::IPipeline* pipe = m_lightingShader->GetPipelineRHI();
    if (pipe && !pipe->HasValidProgram()) return;

    gbuffer.BindForLightingPass(cmd);
    m_lightingShader->Bind(device);
    m_lightingShader->SetInt("u_Position", 0);
    m_lightingShader->SetInt("u_Normal", 1);
    m_lightingShader->SetInt("u_Albedo", 2);
    m_lightingShader->SetInt("u_PBR", 3);
    if (cascadeMap && cmd) {
        rhi::ITexture* shadowTex = cascadeMap->GetDepthTextureRHI();
        if (shadowTex) cmd->BindTexture(shadowTex, 6);
        rhi::IBuffer* lightSpaceBuf = cascadeMap->GetLightSpaceBufferRHI();
        if (lightSpaceBuf) cmd->BindStorageBuffer(lightSpaceBuf, 7, 0, 0);
        math::Mat4 viewProj = projection * view;
        m_lightingShader->SetMat4("u_ViewProj", viewProj);
    }
    m_lightingShader->SetInt("u_ShadowMap", 6);

    if (sceneLightingBuffer && cmd) {
        cmd->BindStorageBuffer(sceneLightingBuffer, 5, 0, 0);
    } else {
        if (cascadeMap && cascadeMap->GetSplitCount() > 0) {
            m_lightingShader->SetMat4("u_LightSpaceMatrix", cascadeMap->GetLightSpaceMatrix(0));
            m_lightingShader->SetFloat("u_ShadowBias", 0.005f);
        }
        m_lightingShader->SetInt("u_UseShadows", cascadeMap ? 1 : 0);
        m_lightingShader->SetVec3("u_ViewPos", viewPos);
        m_lightingShader->SetVec3("u_LightDir", light.direction);
        m_lightingShader->SetVec3("u_LightColor", light.color);
        m_lightingShader->SetFloat("u_LightIntensity", light.intensity);
        m_lightingShader->SetVec3("u_Ambient", light.ambient);
        m_lightingShader->SetFloat("u_Exposure", light.exposure);
        const int numPoint = pointLights ? static_cast<int>(std::min(pointLights->size(), size_t(kMaxPointLights))) : 0;
        m_lightingShader->SetInt("u_PointLightCount", numPoint);
        for (int i = 0; i < numPoint; ++i) {
            const auto& p = (*pointLights)[static_cast<size_t>(i)];
            m_lightingShader->SetVec3("u_PointLightPos[" + std::to_string(i) + "]", p.position);
            m_lightingShader->SetVec3("u_PointLightColor[" + std::to_string(i) + "]", p.color);
            m_lightingShader->SetFloat("u_PointLightIntensity[" + std::to_string(i) + "]", p.intensity);
            m_lightingShader->SetFloat("u_PointLightRadius[" + std::to_string(i) + "]", p.radius);
        }
        const int numSpot = spotLights ? static_cast<int>(std::min(spotLights->size(), size_t(kMaxSpotLights))) : 0;
        m_lightingShader->SetInt("u_SpotLightCount", numSpot);
        for (int i = 0; i < numSpot; ++i) {
            const auto& s = (*spotLights)[static_cast<size_t>(i)];
            m_lightingShader->SetVec3("u_SpotLightPos[" + std::to_string(i) + "]", s.position);
            m_lightingShader->SetVec3("u_SpotLightDir[" + std::to_string(i) + "]", s.direction);
            m_lightingShader->SetVec3("u_SpotLightColor[" + std::to_string(i) + "]", s.color);
            m_lightingShader->SetFloat("u_SpotLightIntensity[" + std::to_string(i) + "]", s.intensity);
            m_lightingShader->SetFloat("u_SpotLightRange[" + std::to_string(i) + "]", s.range);
            m_lightingShader->SetFloat("u_SpotLightInnerCos[" + std::to_string(i) + "]", s.innerConeCos);
            m_lightingShader->SetFloat("u_SpotLightOuterCos[" + std::to_string(i) + "]", s.outerConeCos);
        }
    }

    if (cmd) {
        cmd->SetDepthTest(false);
        cmd->SetBlend(false);
    }
    if (cmd && m_lightingShader->GetPipelineRHI())
        cmd->DrawFullscreenQuad(m_lightingShader->GetPipelineRHI());
    if (cmd) cmd->SetDepthTest(true);

    if (cascadeMap && cmd) cmd->UnbindTexture(6);
    m_lightingShader->Unbind();
}

} // namespace render
} // namespace eyelib
