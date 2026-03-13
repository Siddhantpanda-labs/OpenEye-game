#include "eyelib/render/shadows/ShadowPass.h"
#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/ShaderKey.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <map>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;
}

ShadowPass::ShadowPass() = default;

void ShadowPass::Render(CascadeShadowMap& cascadeMap,
                        rhi::IDevice* device,
                        rhi::ICommandList* cmd,
                        DrawableFeed feedDrawables,
                        const math::Vec3& lightDir,
                        const math::Mat4& view,
                        const math::Mat4& projection,
                        const math::Vec3& cameraPos,
                        float nearPlane,
                        float farPlane,
                        float aspectRatio) {
    if (!m_shaderCache || !device || !cmd) return;

    float aspect = aspectRatio;
    if (aspect <= 0.0f && projection[0][0] != 0.0f)
        aspect = projection[1][1] / projection[0][0];

    cascadeMap.Update(lightDir, view, projection, cameraPos, nearPlane, farPlane, aspect);
    cascadeMap.MapLightSpaceToBuffer();

    const uint32_t splitCount = cascadeMap.GetSplitCount();

    for (uint32_t i = 0; i < splitCount; ++i) {
        cascadeMap.BindForShadowPass(cmd, i);
        cmd->SetDepthTest(true);
        cmd->SetDepthFuncLess(true);
        cmd->SetCullFace(true);
        cmd->SetCullFrontFace(true);

        const math::Mat4 lightMatrix = cascadeMap.GetLightSpaceMatrix(i);

        auto drawOne = [&](Shader* shader, const Drawable& d) {
            const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
            if (!d.mesh || !material) return;
            if (cmd) cmd->SetCullFace(!material->GetTwoSided());
            const math::Mat4* bonePtr = nullptr;
            size_t boneCount = 0;
            if (d.bones && d.boneCount > 0) {
                bonePtr = d.bones;
                boneCount = d.boneCount;
            } else if (d.materialInstance) {
                d.materialInstance->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                    if (name == "u_Bones" && count > 0) { bonePtr = values; boneCount = count; }
                });
            } else {
                material->ForEachMat4Array([&](const std::string& name, const math::Mat4* values, size_t count) {
                    if (name == "u_Bones" && count > 0) { bonePtr = values; boneCount = count; }
                });
            }
            shader->SetMat4("u_LightSpaceMatrix", lightMatrix);
            shader->SetMat4("u_Model", d.transform);
            if (bonePtr && boneCount > 0) {
                boneCount = std::min(boneCount, kMaxBones);
                shader->SetMat4Array("u_Bones", bonePtr, boneCount);
            }
            d.mesh->Bind(device);
            { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(device); }
            d.mesh->Unbind();
        };

        std::map<ShaderKey, std::vector<Drawable>> buckets;
        feedDrawables([&](const Drawable& d) {
            const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
            if (!d.mesh || !material) return;
            InstanceType it = (d.bones && d.boneCount > 0) ? InstanceType::Skinned : InstanceType::Static;
            ShaderKey key{ PipelineStage::Depth, it, material->GetShaderIndex() };
            buckets[key].push_back(d);
        });
        for (auto& [key, drawables] : buckets) {
            const Material* matForCompile = (key.materialTypeId != kDefaultMaterialTypeId && !drawables.empty())
                ? (drawables[0].materialInstance ? drawables[0].materialInstance->GetMaterial().get() : drawables[0].material.get())
                : nullptr;
            auto shader = matForCompile
                ? m_shaderCache->getOrCompile(device, key, matForCompile)
                : m_shaderCache->getOrCompile(device, key);
            if (!shader || !shader->IsValid()) continue;
            rhi::IPipeline* shaderPipe = shader->GetPipelineRHI();
            if (shaderPipe && !shaderPipe->HasValidProgram()) continue;
            shader->Bind(device);
            for (const auto& d : drawables)
                drawOne(shader.get(), d);
            shader->Unbind();
        }

        cmd->SetCullFrontFace(false);
    }

    cascadeMap.Unbind(cmd);
}

void ShadowPass::RenderInstanced(CascadeShadowMap& cascadeMap,
                                 rhi::IDevice* device,
                                 rhi::ICommandList* cmd,
                                 const Mesh* mesh,
                                 const Material* material,
                                 rhi::IBuffer* instanceMatrixBuffer,
                                 uint32_t instanceCount,
                                 const math::Vec3& lightDir,
                                 const math::Mat4& view,
                                 const math::Mat4& projection,
                                 const math::Vec3& cameraPos,
                                 float nearPlane,
                                 float farPlane,
                                 float aspectRatio) {
    if (!m_shaderCache || !device || !cmd || !mesh || !material || !instanceMatrixBuffer || instanceCount == 0) return;

    float aspect = aspectRatio;
    if (aspect <= 0.0f && projection[0][0] != 0.0f)
        aspect = projection[1][1] / projection[0][0];

    cascadeMap.Update(lightDir, view, projection, cameraPos, nearPlane, farPlane, aspect);
    cascadeMap.MapLightSpaceToBuffer();

    ShaderKey key{ PipelineStage::Depth, InstanceType::Instanced, material->GetShaderIndex() };
    auto shader = m_shaderCache->getOrCompile(device, key, material);
    if (!shader || !shader->IsValid()) return;
    rhi::IPipeline* shaderPipe = shader->GetPipelineRHI();
    if (shaderPipe && !shaderPipe->HasValidProgram()) return;

    const uint32_t splitCount = cascadeMap.GetSplitCount();
    cmd->BindStorageBuffer(instanceMatrixBuffer, rhi::StorageBinding::InstanceMatrix, 0, instanceCount * 64u);

    for (uint32_t i = 0; i < splitCount; ++i) {
        cascadeMap.BindForShadowPass(cmd, i);
        cmd->SetDepthTest(true);
        cmd->SetDepthFuncLess(true);
        cmd->SetCullFace(!material->GetTwoSided());
        cmd->SetCullFrontFace(true);

        shader->SetMat4("u_LightSpaceMatrix", cascadeMap.GetLightSpaceMatrix(i));
        shader->Bind(device);
        mesh->Bind(device);
        MeshHandle h = mesh->GetHandle();
        if (h.IsValid()) h.DrawInstanced(device, -1, instanceCount);
        mesh->Unbind();
        shader->Unbind();

        cmd->SetCullFrontFace(false);
    }

    cascadeMap.Unbind(cmd);
}

} // namespace render
} // namespace eyelib
