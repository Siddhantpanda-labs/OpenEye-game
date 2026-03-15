#include "eyelib/render/shadows/ShadowPass.h"
#include "eyelib/render/ShaderCache.h"
#include "eyelib/render/ShaderKey.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/core/Log.h"
#include "eyelib/math/Frustum.h"
#include <algorithm>
#include <map>

namespace eyelib {
namespace render {

namespace {
constexpr size_t kMaxBones = 128;

bool IsVisibleInCascade(const Drawable& d, const math::Frustum& cascadeFrustum) {
    if (!d.mesh) return false;
    // Skinned meshes use animated bounds; avoid false culling artifacts.
    if (d.bones && d.boneCount > 0) return true;
    const math::AABB worldAabb = d.mesh->GetAABB().Transform(d.transform);
    return cascadeFrustum.Contains(worldAabb) != 0;
}
}

ShadowPass::ShadowPass() = default;

void ShadowPass::EnsureResourceSpaceSets(rhi::IDevice* device) {
    if (!device || m_frameSet) return;
    rhi::IPipelineLayout* frameLayout = device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::Frame);
    rhi::IPipelineLayout* shadowPassLayout = device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::ShadowPass);
    rhi::IPipelineLayout* sceneLayout = device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::SceneBuffers);
    if (!frameLayout || !shadowPassLayout || !sceneLayout) return;
    m_frameSet = device->CreateDescriptorSet(frameLayout);
    m_shadowPassSet = device->CreateDescriptorSet(shadowPassLayout);
    m_sceneBuffersSet = device->CreateDescriptorSet(sceneLayout);
    if (!m_frameSet || !m_shadowPassSet || !m_sceneBuffersSet) {
        if (m_frameSet) { device->DestroyDescriptorSet(m_frameSet); m_frameSet = nullptr; }
        if (m_shadowPassSet) { device->DestroyDescriptorSet(m_shadowPassSet); m_shadowPassSet = nullptr; }
        if (m_sceneBuffersSet) { device->DestroyDescriptorSet(m_sceneBuffersSet); m_sceneBuffersSet = nullptr; }
        return;
    }
    rhi::BufferDesc uboDesc;
    uboDesc.sizeInBytes = 256;
    uboDesc.usage = rhi::BufferUsage::Uniform;
    uboDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;
    m_frameUBO = device->CreateBuffer(uboDesc, nullptr);
    rhi::BufferDesc bonesDesc;
    bonesDesc.sizeInBytes = kMaxBones * 64u;
    bonesDesc.usage = rhi::BufferUsage::Storage;
    bonesDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;
    m_bonesBuffer = device->CreateBuffer(bonesDesc, nullptr);
    if (m_frameUBO) {
        m_frameSet->UpdateUniformBuffer(0, m_frameUBO, 0, 128);
        m_frameSet->UpdateUniformBuffer(1, m_frameUBO, 128, 128);
        m_shadowPassSet->UpdateUniformBuffer(0, m_frameUBO, 0, 128);
    }
    if (m_bonesBuffer)
        m_sceneBuffersSet->UpdateStorageBuffer(1, m_bonesBuffer, 0, static_cast<uint32_t>(bonesDesc.sizeInBytes));
    m_device = device;
}

void ShadowPass::Shutdown() {
    if (!m_device) return;
    if (m_frameSet) { m_device->DestroyDescriptorSet(m_frameSet); m_frameSet = nullptr; }
    if (m_shadowPassSet) { m_device->DestroyDescriptorSet(m_shadowPassSet); m_shadowPassSet = nullptr; }
    if (m_sceneBuffersSet) { m_device->DestroyDescriptorSet(m_sceneBuffersSet); m_sceneBuffersSet = nullptr; }
    if (m_frameUBO) { m_device->DestroyBuffer(m_frameUBO); m_frameUBO = nullptr; }
    if (m_bonesBuffer) { m_device->DestroyBuffer(m_bonesBuffer); m_bonesBuffer = nullptr; }
    m_device = nullptr;
}

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
    EnsureResourceSpaceSets(device);

    float aspect = aspectRatio;
    if (aspect <= 0.0f && projection[0][0] != 0.0f)
        aspect = projection[1][1] / projection[0][0];

    cascadeMap.Update(lightDir, view, projection, cameraPos, nearPlane, farPlane, aspect);
    cascadeMap.MapLightSpaceToBuffer();

    std::map<ShaderKey, std::vector<Drawable>> buckets;
    feedDrawables([&](const Drawable& d) {
        const auto material = d.materialInstance ? d.materialInstance->GetMaterial() : d.material;
        if (!d.mesh || !material) return;
        InstanceType it = (d.bones && d.boneCount > 0) ? InstanceType::Skinned : InstanceType::Static;
        ShaderKey key{ PipelineStage::Depth, it, material->GetShaderIndex() };
        buckets[key].push_back(d);
    });

    std::map<ShaderKey, std::shared_ptr<Shader>> shaders;
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
        shaders.emplace(key, std::move(shader));
    }

    const uint32_t splitCount = cascadeMap.GetSplitCount();

    for (uint32_t i = 0; i < splitCount; ++i) {
        cascadeMap.BindForShadowPass(cmd, i);
        cmd->SetDepthTest(true);
        cmd->SetDepthFuncLess(true);
        cmd->SetCullFace(true);
        cmd->SetCullFrontFace(true);

        const math::Mat4 lightMatrix = cascadeMap.GetLightSpaceMatrix(i);
        math::Frustum cascadeFrustum;
        cascadeFrustum.FromMatrix(lightMatrix);

        const bool useResourceSpace = (m_frameSet && m_shadowPassSet && m_sceneBuffersSet);
        if (useResourceSpace && m_frameUBO)
            m_frameUBO->Update(&lightMatrix, 0, sizeof(math::Mat4));

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
            shader->SetMat4("u_Model", d.transform);
            if (bonePtr && boneCount > 0) {
                boneCount = std::min(boneCount, kMaxBones);
                if (useResourceSpace && m_bonesBuffer)
                    m_bonesBuffer->Update(bonePtr, 0, boneCount * sizeof(math::Mat4));
                else
                    shader->SetMat4Array("u_Bones", bonePtr, boneCount);
            }
            if (!useResourceSpace)
                shader->SetMat4("u_LightSpaceMatrix", lightMatrix);
            d.mesh->Bind(device);
            { MeshHandle h = d.mesh->GetHandle(); if (h.IsValid()) h.Draw(device, d.submeshIndex); }
            d.mesh->Unbind();
        };

        for (auto& [key, drawables] : buckets) {
            auto it = shaders.find(key);
            if (it == shaders.end()) continue;
            auto& shader = it->second;
            shader->Bind(device);
            if (m_frameSet && m_shadowPassSet && m_sceneBuffersSet) {
                rhi::IDescriptorSet* sets[3] = { m_frameSet, m_shadowPassSet, m_sceneBuffersSet };
                cmd->BindDescriptorSets(0, 3, sets);
            }
            for (const auto& d : drawables)
                if (IsVisibleInCascade(d, cascadeFrustum))
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
                                 float aspectRatio,
                                 int32_t submeshIndex) {
    if (!m_shaderCache || !device || !cmd || !mesh || !material || !instanceMatrixBuffer || instanceCount == 0) return;
    EnsureResourceSpaceSets(device);

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
    const bool useResourceSpace = (m_frameSet && m_shadowPassSet && m_sceneBuffersSet);
    if (useResourceSpace)
        m_sceneBuffersSet->UpdateStorageBuffer(1, instanceMatrixBuffer, 0, instanceCount * 64u);
    else
        cmd->BindStorageBuffer(instanceMatrixBuffer, rhi::StorageBinding::InstanceMatrix, 0, instanceCount * 64u);

    for (uint32_t i = 0; i < splitCount; ++i) {
        cascadeMap.BindForShadowPass(cmd, i);
        cmd->SetDepthTest(true);
        cmd->SetDepthFuncLess(true);
        cmd->SetCullFace(!material->GetTwoSided());
        cmd->SetCullFrontFace(true);

        const math::Mat4 lightMatrix = cascadeMap.GetLightSpaceMatrix(i);
        if (useResourceSpace && m_frameUBO)
            m_frameUBO->Update(&lightMatrix, 0, sizeof(math::Mat4));
        else
            shader->SetMat4("u_LightSpaceMatrix", lightMatrix);
        shader->Bind(device);
        if (useResourceSpace) {
            rhi::IDescriptorSet* sets[3] = { m_frameSet, m_shadowPassSet, m_sceneBuffersSet };
            cmd->BindDescriptorSets(0, 3, sets);
        }
        mesh->Bind(device);
        MeshHandle h = mesh->GetHandle();
        if (h.IsValid()) h.DrawInstanced(device, submeshIndex, instanceCount);
        mesh->Unbind();
        shader->Unbind();

        cmd->SetCullFrontFace(false);
    }

    cascadeMap.Unbind(cmd);
}

} // namespace render
} // namespace eyelib
