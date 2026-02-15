#include "eyelib/render/passes/LightingPass.h"
#include "eyelib/render/ShadowMap.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/rhi/RHIContext.h"
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

LightingPass::~LightingPass() {
    if (m_device) {
        if (m_fullscreenVBO) { m_device->DestroyBuffer(m_fullscreenVBO); m_fullscreenVBO = nullptr; }
        if (m_fullscreenIBO) { m_device->DestroyBuffer(m_fullscreenIBO); m_fullscreenIBO = nullptr; }
    }
}

void LightingPass::EnsureFullscreenQuad() {
    if (!m_device || m_fullscreenVBO) return;
    rhi::IDevice* device = m_device;
    const float vertices[] = {
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f,
    };
    const uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
    rhi::BufferDesc vboDesc;
    vboDesc.sizeInBytes = sizeof(vertices);
    vboDesc.usage = rhi::BufferUsage::Vertex;
    m_fullscreenVBO = device->CreateBuffer(vboDesc, vertices);
    rhi::BufferDesc iboDesc;
    iboDesc.sizeInBytes = sizeof(indices);
    iboDesc.usage = rhi::BufferUsage::Index;
    m_fullscreenIBO = device->CreateBuffer(iboDesc, indices);
}

void LightingPass::DrawFullscreenQuad(rhi::ICommandList* cmd) {
    if (!cmd || !m_fullscreenVBO || !m_fullscreenIBO) return;
    cmd->BindVertexBuffer(m_fullscreenVBO, 0);
    cmd->BindIndexBuffer(m_fullscreenIBO, rhi::IndexType::UInt16);
    cmd->DrawIndexed(6, 0, 0);
}

void LightingPass::EnsureDefaultShader() {
    core::Log::Info("LightingPass::EnsureDefaultShader: entry");
    if (m_lightingShader && m_lightingShader->IsValid()) {
        core::Log::Info("LightingPass::EnsureDefaultShader: already valid, return");
        return;
    }
    if (!m_device) return;
    rhi::IDevice* device = m_device;
    core::Log::InfoF("LightingPass::EnsureDefaultShader: device=%p", static_cast<void*>(device));
    if (!device) return;
    m_lightingShader = std::make_shared<Shader>();
    const rhi::VertexLayout fullscreenLayout = GetFullscreenVertexLayout();
    core::Log::Info("LightingPass::EnsureDefaultShader: before Load(passes/deferred/lighting)");
    if (!m_lightingShader->Load(device, "passes/deferred/lighting", fullscreenLayout)) {
        core::Log::Warn("LightingPass::EnsureDefaultShader: Load failed");
        return;
    }
    core::Log::Info("LightingPass::EnsureDefaultShader: after Load, before EnsureFullscreenQuad");
    EnsureFullscreenQuad();
    core::Log::Info("LightingPass::EnsureDefaultShader: done");
}

void LightingPass::SetLightingShader(std::shared_ptr<Shader> shader) {
    m_lightingShader = std::move(shader);
}

static const int kMaxPointLights = 8;
static const int kMaxSpotLights = 4;

void LightingPass::Render(GBuffer& gbuffer,
                          rhi::ICommandList* cmd,
                          const LightParams& light,
                          const math::Mat4& view,
                          const math::Mat4& projection,
                          const math::Vec3& viewPos,
                          ShadowMap* shadowMap,
                          const std::vector<PointLight>* pointLights,
                          const std::vector<SpotLight>* spotLights) {
    (void)view;
    (void)projection;
    EnsureDefaultShader();
    bool shaderValid = m_lightingShader && m_lightingShader->IsValid();
    rhi::IPipeline* pipe = m_lightingShader ? m_lightingShader->GetPipelineRHI() : nullptr;
    bool programValid = (!pipe || pipe->HasValidProgram());
    core::Log::InfoF("LightingPass: lighting shader valid=%d pipeline program valid=%d", shaderValid ? 1 : 0, programValid ? 1 : 0);
    if (!shaderValid) {
        core::Log::Info("LightingPass: skipped (lighting shader invalid or missing)");
        return;
    }
    if (pipe && !pipe->HasValidProgram()) {
        core::Log::Info("LightingPass: skipped (no valid backend program)");
        return;
    }
    core::Log::Info("LightingPass: executing");

    core::Log::Info("LightingPass: before BindForLightingPass");
    gbuffer.BindForLightingPass(cmd);
    core::Log::Info("LightingPass: after BindForLightingPass, before Bind");
    m_lightingShader->Bind();
    core::Log::Info("LightingPass: after Bind, before SetInt 0-3");
    m_lightingShader->SetInt("u_Position", 0);
    m_lightingShader->SetInt("u_Normal", 1);
    m_lightingShader->SetInt("u_Albedo", 2);
    m_lightingShader->SetInt("u_PBR", 3);
    core::Log::Info("LightingPass: after SetInt 0-3, before shadow");
    if (shadowMap) {
        if (cmd) {
            rhi::ITexture* shadowTex = shadowMap->GetDepthTextureRHI();
            if (shadowTex) cmd->BindTexture(shadowTex, 5);
        }
        m_lightingShader->SetMat4("u_LightSpaceMatrix", shadowMap->GetShadowMatrix());
        m_lightingShader->SetFloat("u_ShadowBias", 0.005f);
    }
    m_lightingShader->SetInt("u_ShadowMap", 5);
    m_lightingShader->SetInt("u_UseShadows", shadowMap ? 1 : 0);
    core::Log::Info("LightingPass: after shadow, before view/light params");
    m_lightingShader->SetVec3("u_ViewPos", viewPos);
    m_lightingShader->SetVec3("u_LightDir", light.direction);
    m_lightingShader->SetVec3("u_LightColor", light.color);
    m_lightingShader->SetFloat("u_LightIntensity", light.intensity);
    m_lightingShader->SetVec3("u_Ambient", light.ambient);
    m_lightingShader->SetFloat("u_Exposure", light.exposure);

    core::Log::Info("LightingPass: before point lights");
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

    core::Log::Info("LightingPass: before DrawFullscreenQuad");
    if (cmd) {
        cmd->SetDepthTest(false);
        cmd->SetBlend(false);
    }
    DrawFullscreenQuad(cmd);
    core::Log::Info("LightingPass: after DrawFullscreenQuad");
    if (cmd) cmd->SetDepthTest(true);

    if (shadowMap && cmd) cmd->UnbindTexture(5);
    m_lightingShader->Unbind();
    core::Log::Info("LightingPass: done");
}

} // namespace render
} // namespace eyelib
