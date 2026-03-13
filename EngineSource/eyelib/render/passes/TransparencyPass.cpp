#include "eyelib/render/passes/TransparencyPass.h"
#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/ShaderKey.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/math/TransformUtils.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace eyelib {
namespace render {

namespace {

constexpr size_t kMaxBones = 128;

void applyMaterialBlendAndCull(rhi::ICommandList* cmd, const Material* material) {
    if (!cmd || !material) return;
    cmd->SetCullFace(!material->GetTwoSided());
    switch (material->GetBlending()) {
    case Blending::Opaque:
        cmd->SetBlend(false);
        cmd->SetDepthWrite(true);
        break;
    case Blending::Translucent:
        cmd->SetBlend(true);
        cmd->SetBlendFunc(rhi::BlendFactor::SrcAlpha, rhi::BlendFactor::OneMinusSrcAlpha);
        cmd->SetDepthWrite(false);
        break;
    case Blending::Additive:
        cmd->SetBlend(true);
        cmd->SetBlendFunc(rhi::BlendFactor::One, rhi::BlendFactor::One);
        cmd->SetDepthWrite(false);
        break;
    case Blending::Modulate:
        cmd->SetBlend(true);
        cmd->SetBlendFunc(rhi::BlendFactor::DstColor, rhi::BlendFactor::Zero);
        cmd->SetDepthWrite(false);
        break;
    }
}

float distanceToCamera(const math::Mat4& transform, const math::Vec3& viewPos) {
    math::Vec3 p(transform[3][0], transform[3][1], transform[3][2]);
    math::Vec3 d(p.x - viewPos.x, p.y - viewPos.y, p.z - viewPos.z);
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

} // namespace

void TransparencyPass::Render(const RenderContext& ctx,
                              const std::vector<Drawable>& transparent,
                              const math::Mat4& view,
                              const math::Mat4& projection,
                              const math::Vec3& viewPos,
                              ShaderCache* shaderCache) {
    Render(ctx.cmd, ctx.device, transparent, view, projection, viewPos, shaderCache);
}

void TransparencyPass::Render(rhi::ICommandList* cmd,
                              rhi::IDevice* device,
                              const std::vector<Drawable>& transparent,
                              const math::Mat4& view,
                              const math::Mat4& projection,
                              const math::Vec3& viewPos,
                              ShaderCache* shaderCache) {
    if (transparent.empty() || !device || !shaderCache) return;

    struct Entry { size_t idx; float dist; };
    std::vector<Entry> sorted;
    sorted.reserve(transparent.size());
    for (size_t i = 0; i < transparent.size(); ++i) {
        const auto material = transparent[i].materialInstance ? transparent[i].materialInstance->GetMaterial() : transparent[i].material;
        if (!transparent[i].mesh || !material) continue;
        const float alpha = transparent[i].materialInstance ? transparent[i].materialInstance->GetAlpha() : material->GetAlpha();
        if (alpha >= 0.99f) continue;
        float d = distanceToCamera(transparent[i].transform, viewPos);
        sorted.push_back({i, d});
    }
    std::sort(sorted.begin(), sorted.end(), [](const Entry& a, const Entry& b) {
        return a.dist > b.dist;
    });

    if (cmd) cmd->SetDepthTest(true);

    for (const auto& e : sorted) {
        const auto& d = transparent[e.idx];
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!material) continue;
        if (cmd) applyMaterialBlendAndCull(cmd, material.get());
        const Material* baseMat = material.get();
        InstanceType it = (d.bones && d.boneCount > 0) ? InstanceType::Skinned : InstanceType::Static;
        ShaderKey key{ PipelineStage::Forward, it, baseMat->GetMaterialShaderId() };
        auto shader = shaderCache->getOrCompile(device, key, baseMat);
        if (!shader || !shader->IsValid()) continue;
        Shader* sh = shader.get();
        sh->Bind(device);
        if (d.materialInstance)
            d.materialInstance->ApplyToShader(device, sh, cmd);
        else
            material->ApplyToShader(device, sh, cmd);
        sh->SetMat4("u_Model", d.transform);
        sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
        sh->SetMat4("u_View", view);
        sh->SetMat4("u_Projection", projection);
        sh->SetVec3("u_ViewPos", viewPos);
        if (d.bones && d.boneCount > 0) {
            const size_t boneCount = std::min(d.boneCount, kMaxBones);
            sh->SetMat4Array("u_Bones", d.bones, boneCount);
        }
        d.mesh->Bind(device);
        { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(device, d.submeshIndex); }
        d.mesh->Unbind();
        sh->Unbind();
    }

    if (cmd) {
        cmd->SetDepthWrite(true);
        cmd->SetBlend(false);
        cmd->SetCullFace(true);
    }
}

} // namespace render
} // namespace eyelib
