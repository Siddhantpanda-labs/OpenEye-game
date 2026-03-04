#include "eyelib/render/passes/ShadowPass.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/MaterialInstance.h"
#include "eyelib/core/Log.h"
#include <algorithm>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;
}

ShadowPass::ShadowPass() = default;

void ShadowPass::EnsureDefaultShader(rhi::IDevice* device) {
    if (m_depthShader && m_depthShader->IsValid()) return;
    if (!device) return;
    m_depthShader = std::make_shared<Shader>();
    m_depthShader->Load(device, "passes/shadow/shadow", Mesh::GetVertexLayoutRHI());
}

void ShadowPass::EnsureSkinnedShader(rhi::IDevice* device) {
    if (m_depthSkinnedShader && m_depthSkinnedShader->IsValid()) return;
    if (!device) return;
    m_depthSkinnedShader = std::make_shared<Shader>();
    m_depthSkinnedShader->Load(device, "passes/shadow/shadow_skinned", Mesh::GetVertexLayoutRHI());
}

void ShadowPass::SetDepthShader(std::shared_ptr<Shader> shader) {
    m_depthShader = std::move(shader);
}

void ShadowPass::SetDepthSkinnedShader(std::shared_ptr<Shader> shader) {
    m_depthSkinnedShader = std::move(shader);
}

void ShadowPass::ReloadShaders() {
    if (m_depthShader) m_depthShader->Reload();
    if (m_depthSkinnedShader) m_depthSkinnedShader->Reload();
}

void ShadowPass::Render(ShadowMap& shadowMap,
                        const RenderContext& ctx,
                        const std::vector<Drawable>& drawables,
                        const math::Vec3& lightDir,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Vec3& cameraPos,
                        float nearPlane,
                        float farPlane) {
    Render(shadowMap, ctx.device, ctx.cmd, drawables, lightDir, view, projection, cameraPos, nearPlane, farPlane);
}

void ShadowPass::Render(ShadowMap& shadowMap,
                        rhi::IDevice* device,
                        rhi::ICommandList* cmd,
                        const std::vector<Drawable>& drawables,
                        const math::Vec3& lightDir,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Vec3& cameraPos,
                        float nearPlane,
                        float farPlane) {
    RenderDrawables(shadowMap, device, cmd,
                   [&drawables](std::function<void(const Drawable&)> c) {
                       for (const auto& d : drawables) c(d);
                   },
                   lightDir, view, projection, cameraPos, nearPlane, farPlane);
}

void ShadowPass::Render(ShadowMap& shadowMap,
                        const RenderContext& ctx,
                        DrawableFeed feedDrawables,
                        const math::Vec3& lightDir,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Vec3& cameraPos,
                        float nearPlane,
                        float farPlane) {
    Render(shadowMap, ctx.device, ctx.cmd, std::move(feedDrawables), lightDir, view, projection, cameraPos, nearPlane, farPlane);
}

void ShadowPass::Render(ShadowMap& shadowMap,
                        rhi::IDevice* device,
                        rhi::ICommandList* cmd,
                        DrawableFeed feedDrawables,
                        const math::Vec3& lightDir,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Vec3& cameraPos,
                        float nearPlane,
                        float farPlane) {
    RenderDrawables(shadowMap, device, cmd, std::move(feedDrawables), lightDir, view, projection, cameraPos, nearPlane, farPlane);
}

void ShadowPass::RenderDrawables(ShadowMap& shadowMap,
                                rhi::IDevice* device,
                                rhi::ICommandList* cmd,
                                DrawableFeed feedDrawables,
                                const math::Vec3& lightDir,
                                const math::Mat4& view,
                                const math::Mat4& projection,
                                const math::Vec3& cameraPos,
                                float nearPlane,
                                float farPlane) {
    EnsureDefaultShader(device);
    EnsureSkinnedShader(device);
    bool depthValid = m_depthShader && m_depthShader->IsValid();
    bool skinnedValid = m_depthSkinnedShader && m_depthSkinnedShader->IsValid();
    rhi::IPipeline* pipe = m_depthShader ? m_depthShader->GetPipelineRHI() : nullptr;
    bool programValid = (!pipe || pipe->HasValidProgram());
#if 0
    core::Log::InfoF("ShadowPass: depth shader valid=%d skinned shader valid=%d pipeline program valid=%d", depthValid ? 1 : 0, skinnedValid ? 1 : 0, programValid ? 1 : 0);
#endif
    if (!depthValid) {
        core::Log::Info("ShadowPass: skipped (depth shader invalid or missing)");
        return;
    }
    if (pipe && !pipe->HasValidProgram()) {
        core::Log::Info("ShadowPass: skipped (no valid backend program)");
        return;
    }
#if 0
    core::Log::Info("ShadowPass: executing");
#endif

    shadowMap.UpdateMatrices(lightDir, view, projection, cameraPos, nearPlane, farPlane);
    shadowMap.BindForShadowPass(cmd);
    m_currentShadowMap = &shadowMap;

    if (cmd) {
        cmd->SetDepthTest(true);
        cmd->SetDepthFuncLess(true);
        cmd->SetCullFace(true);
        cmd->SetCullFrontFace(true);
    }

    const math::Mat4 lightMatrix = shadowMap.GetShadowMatrix();

    feedDrawables([this, &shadowMap, device, cmd, lightMatrix](const Drawable& d) {
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!d.mesh || !material) return;

        const math::Mat4* bonePtr = nullptr;
        size_t boneCount = 0;
        if (d.bones && d.boneCount > 0) {
            bonePtr = d.bones;
            boneCount = d.boneCount;
        } else if (d.materialInstance) {
            d.materialInstance->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                if (name == "u_Bones" && count > 0) {
                    bonePtr = values;
                    boneCount = count;
                }
            });
        } else {
            material->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                if (name == "u_Bones" && count > 0) {
                    bonePtr = values;
                    boneCount = count;
                }
            });
        }

        if (bonePtr && boneCount > 0 && m_depthSkinnedShader->IsValid()) {
            boneCount = std::min(boneCount, kMaxBones);
            m_depthSkinnedShader->Bind(device);
            m_depthSkinnedShader->SetMat4("u_LightSpaceMatrix", lightMatrix);
            m_depthSkinnedShader->SetMat4("u_Model", d.transform);
            m_depthSkinnedShader->SetMat4Array("u_Bones", bonePtr, boneCount);
        } else {
            m_depthShader->Bind(device);
            m_depthShader->SetMat4("u_LightSpaceMatrix", lightMatrix);
            m_depthShader->SetMat4("u_Model", d.transform);
        }
        d.mesh->Bind(device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(device); }
        d.mesh->Unbind();
    });

    if (cmd) {
        cmd->SetCullFrontFace(false);
    }
    m_depthShader->Unbind();
    if (m_depthSkinnedShader && m_depthSkinnedShader->IsValid())
        m_depthSkinnedShader->Unbind();
    shadowMap.Unbind(cmd);
    m_currentShadowMap = nullptr;
}

} // namespace render
} // namespace eyelib
