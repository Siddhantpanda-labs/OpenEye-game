#include "eyelib/render/shadows/ShadowPass.h"
#include "eyelib/render/backend/RenderBackendPolicy.h"
#include "eyelib/render/PassPushConstants.h"
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
#include <cmath>
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
    const uint32_t bonesStrideBytes = static_cast<uint32_t>(kMaxBones * sizeof(math::Mat4));
    bonesDesc.sizeInBytes = bonesStrideBytes * kMaxSkinnedDrawsPerFrame * kMaxFramesInFlight;
    bonesDesc.usage = rhi::BufferUsage::Storage;
    bonesDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;
    m_bonesBuffer = device->CreateBuffer(bonesDesc, nullptr);
    rhi::BufferDesc fallbackDesc;
    fallbackDesc.sizeInBytes = 64u;
    fallbackDesc.usage = rhi::BufferUsage::Storage;
    fallbackDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;
    m_sceneFallbackBuffer = device->CreateBuffer(fallbackDesc, nullptr);
    if (m_frameUBO) {
        m_frameSet->UpdateUniformBuffer(0, m_frameUBO, 0, 128);
        m_frameSet->UpdateUniformBuffer(1, m_frameUBO, 128, 128);
        m_shadowPassSet->UpdateUniformBuffer(0, m_frameUBO, 0, 128);
    }
    rhi::IBuffer* sceneFallback = m_sceneFallbackBuffer ? m_sceneFallbackBuffer : m_bonesBuffer;
    if (sceneFallback) {
        m_sceneBuffersSet->UpdateStorageBuffer(0, sceneFallback, 0, 0);
        m_sceneBuffersSet->UpdateStorageBuffer(1, sceneFallback, 0, 0);
        m_sceneBuffersSet->UpdateStorageBuffer(2, sceneFallback, 0, 0);
        m_sceneBuffersSet->UpdateStorageBuffer(3, sceneFallback, 0, 0);
    }
    if (m_bonesBuffer)
        m_sceneBuffersSet->UpdateStorageBuffer(1, m_bonesBuffer, 0, 0);
    m_device = device;
}

void ShadowPass::ResetTransientFrameStateIfNeeded(rhi::IDevice* device) {
    if (!device) return;
    const uint32_t frameNumber = device->GetFrameNumber();
    if (frameNumber == m_lastTransientFrameNumber) return;
    m_lastTransientFrameNumber = frameNumber;
    const uint32_t frameIndex = device->GetCurrentFrameIndex() % kMaxFramesInFlight;
    m_perDrawSceneSetCursor[frameIndex] = 0;
    m_bonesWriteCursorBytes[frameIndex] = 0;
}

rhi::IDescriptorSet* ShadowPass::AcquirePerDrawSceneSet(rhi::IDevice* device, uint32_t frameIndex) {
    if (!device || frameIndex >= kMaxFramesInFlight) return nullptr;
    auto& sets = m_perDrawSceneSets[frameIndex];
    uint32_t& cursor = m_perDrawSceneSetCursor[frameIndex];
    if (cursor >= sets.size()) {
        rhi::IPipelineLayout* sceneLayout = device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::SceneBuffers);
        if (!sceneLayout) return nullptr;
        rhi::IDescriptorSet* set = device->CreateDescriptorSet(sceneLayout);
        if (!set) return nullptr;
        sets.push_back(set);
    }
    return sets[cursor++];
}

rhi::IDescriptorSet* ShadowPass::BuildSkinnedSceneSet(rhi::IDevice* device, uint32_t frameIndex, const math::Mat4* bonePtr, size_t boneCount) {
    if (!device || !bonePtr || boneCount == 0 || !m_bonesBuffer || !m_sceneFallbackBuffer || frameIndex >= kMaxFramesInFlight)
        return m_sceneBuffersSet;
    const size_t strideBytes = kMaxBones * sizeof(math::Mat4);
    const size_t perFrameCapacity = strideBytes * kMaxSkinnedDrawsPerFrame;
    size_t& cursorBytes = m_bonesWriteCursorBytes[frameIndex];
    if (cursorBytes + strideBytes > perFrameCapacity)
        return m_sceneBuffersSet;
    const size_t baseOffset = static_cast<size_t>(frameIndex) * perFrameCapacity;
    const size_t bufferOffset = baseOffset + cursorBytes;
    const size_t boneBytes = boneCount * sizeof(math::Mat4);
    m_bonesBuffer->Update(bonePtr, bufferOffset, boneBytes);
    cursorBytes += strideBytes;

    rhi::IDescriptorSet* set = AcquirePerDrawSceneSet(device, frameIndex);
    if (!set) return m_sceneBuffersSet;
    set->UpdateStorageBuffer(0, m_sceneFallbackBuffer, 0, 0);
    set->UpdateStorageBuffer(1, m_bonesBuffer, static_cast<uint32_t>(bufferOffset), static_cast<uint32_t>(boneBytes));
    set->UpdateStorageBuffer(2, m_sceneFallbackBuffer, 0, 0);
    set->UpdateStorageBuffer(3, m_sceneFallbackBuffer, 0, 0);
    return set;
}

rhi::IDescriptorSet* ShadowPass::BuildInstancedSceneSet(rhi::IDevice* device, uint32_t frameIndex, rhi::IBuffer* instanceMatrixBuffer, uint32_t instanceCount) {
    if (!device || !instanceMatrixBuffer || instanceCount == 0 || !m_sceneFallbackBuffer || frameIndex >= kMaxFramesInFlight)
        return m_sceneBuffersSet;
    rhi::IDescriptorSet* set = AcquirePerDrawSceneSet(device, frameIndex);
    if (!set) return m_sceneBuffersSet;
    set->UpdateStorageBuffer(0, m_sceneFallbackBuffer, 0, 0);
    set->UpdateStorageBuffer(1, instanceMatrixBuffer, 0, instanceCount * 64u);
    set->UpdateStorageBuffer(2, m_sceneFallbackBuffer, 0, 0);
    set->UpdateStorageBuffer(3, m_sceneFallbackBuffer, 0, 0);
    return set;
}

void ShadowPass::Shutdown() {
    if (!m_device) return;
    for (auto& perFrameSets : m_perDrawSceneSets) {
        for (rhi::IDescriptorSet*& set : perFrameSets) {
            if (set) { m_device->DestroyDescriptorSet(set); set = nullptr; }
        }
        perFrameSets.clear();
    }
    m_perDrawSceneSetCursor = {};
    m_bonesWriteCursorBytes = {};
    m_lastTransientFrameNumber = 0xffffffffu;
    if (m_frameSet) { m_device->DestroyDescriptorSet(m_frameSet); m_frameSet = nullptr; }
    if (m_shadowPassSet) { m_device->DestroyDescriptorSet(m_shadowPassSet); m_shadowPassSet = nullptr; }
    if (m_sceneBuffersSet) { m_device->DestroyDescriptorSet(m_sceneBuffersSet); m_sceneBuffersSet = nullptr; }
    if (m_frameUBO) { m_device->DestroyBuffer(m_frameUBO); m_frameUBO = nullptr; }
    if (m_bonesBuffer) { m_device->DestroyBuffer(m_bonesBuffer); m_bonesBuffer = nullptr; }
    if (m_sceneFallbackBuffer) { m_device->DestroyBuffer(m_sceneFallbackBuffer); m_sceneFallbackBuffer = nullptr; }
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
    ResetTransientFrameStateIfNeeded(device);
    const uint32_t frameIndex = device->GetCurrentFrameIndex() % kMaxFramesInFlight;

    float aspect = aspectRatio;
    if (aspect <= 0.0f && projection[0][0] != 0.0f)
        aspect = std::abs(projection[1][1] / projection[0][0]);

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

    const bool useTypedShadowPush = backend::UseTypedPushConstants(device);
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
        if (backend::IsVulkan(device) && !useResourceSpace)
            continue;
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
            if (useTypedShadowPush)
                abi::WriteShadowPushConstants(cmd, d.transform);
            else
                shader->SetMat4("u_Model", d.transform);
            if (bonePtr && boneCount > 0) {
                boneCount = std::min(boneCount, kMaxBones);
                if (!useResourceSpace)
                    shader->SetMat4Array("u_Bones", bonePtr, boneCount);
            }
            rhi::IDescriptorSet* sceneSetForDraw = m_sceneBuffersSet;
            if (useResourceSpace && bonePtr && boneCount > 0)
                sceneSetForDraw = BuildSkinnedSceneSet(device, frameIndex, bonePtr, boneCount);
            if (useResourceSpace && m_frameSet && m_shadowPassSet && sceneSetForDraw) {
                rhi::IDescriptorSet* sets[3] = { m_frameSet, m_shadowPassSet, sceneSetForDraw };
                cmd->BindDescriptorSets(0, 3, sets);
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
    ResetTransientFrameStateIfNeeded(device);
    const uint32_t frameIndex = device->GetCurrentFrameIndex() % kMaxFramesInFlight;

    float aspect = aspectRatio;
    if (aspect <= 0.0f && projection[0][0] != 0.0f)
        aspect = std::abs(projection[1][1] / projection[0][0]);

    cascadeMap.Update(lightDir, view, projection, cameraPos, nearPlane, farPlane, aspect);
    cascadeMap.MapLightSpaceToBuffer();

    ShaderKey key{ PipelineStage::Depth, InstanceType::Instanced, material->GetShaderIndex() };
    auto shader = m_shaderCache->getOrCompile(device, key, material);
    if (!shader || !shader->IsValid()) return;
    rhi::IPipeline* shaderPipe = shader->GetPipelineRHI();
    if (shaderPipe && !shaderPipe->HasValidProgram()) return;

    const uint32_t splitCount = cascadeMap.GetSplitCount();
    const bool useResourceSpace = (m_frameSet && m_shadowPassSet && m_sceneBuffersSet);
    if (backend::IsVulkan(device) && !useResourceSpace)
        return;
    rhi::IDescriptorSet* sceneSetForDraw = m_sceneBuffersSet;
    if (useResourceSpace)
        sceneSetForDraw = BuildInstancedSceneSet(device, frameIndex, instanceMatrixBuffer, instanceCount);
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
        if (useResourceSpace && m_frameSet && m_shadowPassSet && sceneSetForDraw) {
            rhi::IDescriptorSet* sets[3] = { m_frameSet, m_shadowPassSet, sceneSetForDraw };
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
