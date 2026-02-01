#include "eyelib/render/passes/TransparencyPass.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/MaterialInstance.h"
#include "eyelib/render/resources/Mesh.h"
#include <glad/glad.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace eyelib {
namespace render {

namespace {

float distanceToCamera(const math::Mat4& transform, const math::Vec3& viewPos) {
    math::Vec3 p(transform[3][0], transform[3][1], transform[3][2]);
    math::Vec3 d(p.x - viewPos.x, p.y - viewPos.y, p.z - viewPos.z);
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

} // namespace

void TransparencyPass::Render(const std::vector<Drawable>& transparent,
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);

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
        sh->SetMat4("u_View", view);
        sh->SetMat4("u_Projection", projection);
        sh->SetVec3("u_ViewPos", viewPos);
        const float alpha = d.materialInstance ? d.materialInstance->GetAlpha() : material->GetAlpha();
        sh->SetFloat("u_Alpha", alpha);
        d.mesh->Bind();
        d.mesh->Draw();
        d.mesh->Unbind();
        sh->Unbind();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

} // namespace render
} // namespace eyelib
