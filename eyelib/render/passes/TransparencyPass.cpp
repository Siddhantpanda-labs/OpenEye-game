#include "eyelib/render/passes/TransparencyPass.h"
#include "api/render/RenderScene.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/MaterialInstance.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/math/TransformUtils.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace eyelib {
namespace render {

namespace {

constexpr size_t kMaxBones = 128;

float distanceToCamera(const math::Mat4& transform, const math::Vec3& viewPos) {
    math::Vec3 p(transform[3][0], transform[3][1], transform[3][2]);
    math::Vec3 d(p.x - viewPos.x, p.y - viewPos.y, p.z - viewPos.z);
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

} // namespace

void TransparencyPass::Render(rhi::ICommandList* cmd,
                              const std::vector<Drawable>& transparent,
                              const math::Mat4& view,
                              const math::Mat4& projection,
                              const math::Vec3& viewPos) {
    if (transparent.empty()) return;

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

    if (cmd) {
        cmd->SetBlend(true);
        cmd->SetDepthWrite(false);
        cmd->SetDepthTest(true);
    }

    for (const auto& e : sorted) {
        const auto& d = transparent[e.idx];
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (d.materialInstance) {
            d.materialInstance->Bind();
        } else if (material) {
            material->Bind();
        }
        Shader* sh = material ? material->GetShader().get() : nullptr;
        if (!sh) continue;
        sh->SetMat4("u_Model", d.transform);
        sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(d.transform)));
        sh->SetMat4("u_View", view);
        sh->SetMat4("u_Projection", projection);
        sh->SetVec3("u_ViewPos", viewPos);
        if (d.bones && d.boneCount > 0) {
            const size_t boneCount = std::min(d.boneCount, kMaxBones);
            sh->SetMat4Array("u_Bones", d.bones, boneCount);
        }
        const float alpha = d.materialInstance ? d.materialInstance->GetAlpha() : material->GetAlpha();
        sh->SetFloat("u_Alpha", alpha);
        d.mesh->Bind();
        d.mesh->Draw();
        d.mesh->Unbind();
        sh->Unbind();
    }

    if (cmd) {
        cmd->SetDepthWrite(true);
        cmd->SetBlend(false);
    }
}

void TransparencyPass::Render(rhi::ICommandList* cmd,
                              std::span<const api::Drawable> transparent,
                              const math::Mat4& view,
                              const math::Mat4& projection,
                              const math::Vec3& viewPos) {
    if (transparent.empty()) return;

    struct Entry { size_t idx; float dist; };
    std::vector<Entry> sorted;
    sorted.reserve(transparent.size());
    for (size_t i = 0; i < transparent.size(); ++i) {
        const auto& apiD = transparent[i];
        if (!apiD.mesh || !apiD.material) continue;
        if (apiD.params.alpha >= 0.99f) continue;
        float d = distanceToCamera(apiD.transform, viewPos);
        sorted.push_back({i, d});
    }
    std::sort(sorted.begin(), sorted.end(), [](const Entry& a, const Entry& b) {
        return a.dist > b.dist;
    });

    if (cmd) {
        cmd->SetBlend(true);
        cmd->SetDepthWrite(false);
        cmd->SetDepthTest(true);
    }

    for (const auto& e : sorted) {
        const auto& apiD = transparent[e.idx];
        Mesh* mesh = apiD.mesh ? apiD.mesh->GetInternal().get() : nullptr;
        Material* material = apiD.material ? apiD.material->GetInternal().get() : nullptr;
        if (!mesh || !material) continue;
        Shader* sh = material->GetShader().get();
        if (!sh) continue;
        sh->Bind();
        sh->SetMat4("u_Model", apiD.transform);
        sh->SetMat4("u_NormalMatrix", math::Mat4FromMat3(math::NormalMatrix(apiD.transform)));
        sh->SetMat4("u_View", view);
        sh->SetMat4("u_Projection", projection);
        sh->SetVec3("u_ViewPos", viewPos);
        if (apiD.bones && apiD.boneCount > 0) {
            const size_t boneCount = std::min(apiD.boneCount, kMaxBones);
            sh->SetMat4Array("u_Bones", apiD.bones, boneCount);
        }
        sh->SetFloat("u_Alpha", apiD.params.alpha);
        mesh->Bind();
        mesh->Draw();
        mesh->Unbind();
        sh->Unbind();
    }

    if (cmd) {
        cmd->SetDepthWrite(true);
        cmd->SetBlend(false);
    }
}

} // namespace render
} // namespace eyelib
