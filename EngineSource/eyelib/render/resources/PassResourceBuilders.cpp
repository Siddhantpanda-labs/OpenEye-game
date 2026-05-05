#include "eyelib/render/resources/PassResourceBuilders.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialInstance.h"
#include "eyelib/render/rhi/RHIUniformLayout.h"
#include "eyelib/core/Log.h"

namespace eyelib {
namespace render {
namespace resources {

namespace {

rhi::IDescriptorSet* AcquireDynamicSet(
    rhi::IDevice* device,
    std::vector<rhi::IDescriptorSet*>& setPool,
    uint32_t& setCursor,
    rhi::IDevice::ResourceSetType setType) {
    if (!device) return nullptr;
    if (setCursor >= setPool.size()) {
        rhi::IPipelineLayout* setLayout = device->GetResourceSetLayout(setType);
        if (!setLayout) return nullptr;
        rhi::IDescriptorSet* allocatedSet = device->CreateDescriptorSet(setLayout);
        if (!allocatedSet) {
            core::Log::ErrorF("PassResourceBuilder: CreateDescriptorSet failed for dynamic ResourceSetType=%u (pool exhausted?)",
                              static_cast<unsigned>(setType));
            return nullptr;
        }
        setPool.push_back(allocatedSet);
    }
    return setPool[setCursor++];
}
}  // namespace

bool PassResourceBuilder::Ensure(rhi::IDevice* device) {
    if (!device) return false;
    if (m_frameSets[0] && m_passSets[0] && m_transparentPassSets[0] && m_sceneSets[0] &&
        m_transparentSceneSets[0]) {
        SelectFrame(device);
        return true;
    }

    using ResourceSetType = rhi::IDevice::ResourceSetType;
    rhi::IPipelineLayout* frameLayout = device->GetResourceSetLayout(ResourceSetType::Frame);
    rhi::IPipelineLayout* forwardPassLayout = device->GetResourceSetLayout(ResourceSetType::ForwardPass);
    rhi::IPipelineLayout* materialLayout = device->GetResourceSetLayout(ResourceSetType::Material);
    rhi::IPipelineLayout* sceneLayout = device->GetResourceSetLayout(ResourceSetType::SceneBuffers);
    rhi::IPipelineLayout* deferredLightingLayout = device->GetResourceSetLayout(ResourceSetType::DeferredLightingPass);
    if (!frameLayout || !forwardPassLayout || !materialLayout || !sceneLayout) return false;

    rhi::BufferDesc frameUboDesc{};
    frameUboDesc.sizeInBytes = 320;
    frameUboDesc.usage = rhi::BufferUsage::Uniform;
    frameUboDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;

    rhi::BufferDesc bonesBufferDesc{};
    bonesBufferDesc.sizeInBytes = 128 * 64;
    bonesBufferDesc.usage = rhi::BufferUsage::Storage;
    bonesBufferDesc.memoryHint = rhi::BufferMemoryHint::HostVisible;

    for (uint32_t frameSlot = 0; frameSlot < kFrames; ++frameSlot) {
        m_frameSets[frameSlot] = device->CreateDescriptorSet(frameLayout);
        m_passSets[frameSlot] = device->CreateDescriptorSet(forwardPassLayout);
        m_transparentPassSets[frameSlot] = device->CreateDescriptorSet(forwardPassLayout);
        m_materialSets[frameSlot] = device->CreateDescriptorSet(materialLayout);
        m_sceneSets[frameSlot] = device->CreateDescriptorSet(sceneLayout);
        m_transparentSceneSets[frameSlot] = device->CreateDescriptorSet(sceneLayout);
        if (!m_frameSets[frameSlot] || !m_passSets[frameSlot] || !m_transparentPassSets[frameSlot] || !m_materialSets[frameSlot] ||
            !m_sceneSets[frameSlot] || !m_transparentSceneSets[frameSlot]) {
            Shutdown(device);
            return false;
        }

        m_frameUbos[frameSlot] = device->CreateBuffer(frameUboDesc, nullptr);
        if (m_frameUbos[frameSlot]) {
            m_frameSets[frameSlot]->UpdateUniformBuffer(0, m_frameUbos[frameSlot], 0, 320);
            m_frameSets[frameSlot]->UpdateUniformBuffer(1, m_frameUbos[frameSlot], 0, 320);
        }
        m_bonesBuffers[frameSlot] = device->CreateBuffer(bonesBufferDesc, nullptr);
        if (deferredLightingLayout) m_deferredLightingSets[frameSlot] = device->CreateDescriptorSet(deferredLightingLayout);
    }

    SelectFrame(device);
    return true;
}

void PassResourceBuilder::SelectFrame(rhi::IDevice* device) {
    if (!device) return;
    const uint32_t frameIndex = device->GetCurrentFrameIndex() % kFrames;
    m_current.frameSet = m_frameSets[frameIndex];
    m_current.passSet = m_passSets[frameIndex];
    m_current.transparentPassSet = m_transparentPassSets[frameIndex];
    m_current.materialSet = m_materialSets[frameIndex];
    m_current.sceneSet = m_sceneSets[frameIndex];
    m_current.transparentSceneSet = m_transparentSceneSets[frameIndex];
    m_current.deferredLightingSet = m_deferredLightingSets[frameIndex];
    m_current.frameUbo = m_frameUbos[frameIndex];
    m_current.bonesBuffer = m_bonesBuffers[frameIndex];
    m_materialDrawSetCursor[frameIndex] = 0;
    m_passDrawSetCursor[frameIndex] = 0;
    m_sceneDrawSetCursor[frameIndex] = 0;
    m_deferredLightingDrawSetCursor[frameIndex] = 0;
}

void PassResourceBuilder::Shutdown(rhi::IDevice* device) {
    if (!device) return;
    for (rhi::IDescriptorSet*& s : m_frameSets) { if (s) { device->DestroyDescriptorSet(s); s = nullptr; } }
    for (rhi::IDescriptorSet*& s : m_passSets) { if (s) { device->DestroyDescriptorSet(s); s = nullptr; } }
    for (rhi::IDescriptorSet*& s : m_transparentPassSets) { if (s) { device->DestroyDescriptorSet(s); s = nullptr; } }
    for (rhi::IDescriptorSet*& s : m_materialSets) { if (s) { device->DestroyDescriptorSet(s); s = nullptr; } }
    for (rhi::IDescriptorSet*& s : m_sceneSets) { if (s) { device->DestroyDescriptorSet(s); s = nullptr; } }
    for (rhi::IDescriptorSet*& s : m_transparentSceneSets) { if (s) { device->DestroyDescriptorSet(s); s = nullptr; } }
    for (rhi::IDescriptorSet*& s : m_deferredLightingSets) { if (s) { device->DestroyDescriptorSet(s); s = nullptr; } }
    for (auto& drawSetsPerFrame : m_materialDrawSets) {
        for (rhi::IDescriptorSet*& s : drawSetsPerFrame) { if (s) { device->DestroyDescriptorSet(s); s = nullptr; } }
        drawSetsPerFrame.clear();
    }
    for (auto& drawSetsPerFrame : m_passDrawSets) {
        for (rhi::IDescriptorSet*& s : drawSetsPerFrame) { if (s) { device->DestroyDescriptorSet(s); s = nullptr; } }
        drawSetsPerFrame.clear();
    }
    for (auto& drawSetsPerFrame : m_sceneDrawSets) {
        for (rhi::IDescriptorSet*& s : drawSetsPerFrame) { if (s) { device->DestroyDescriptorSet(s); s = nullptr; } }
        drawSetsPerFrame.clear();
    }
    for (auto& drawSetsPerFrame : m_deferredLightingDrawSets) {
        for (rhi::IDescriptorSet*& s : drawSetsPerFrame) { if (s) { device->DestroyDescriptorSet(s); s = nullptr; } }
        drawSetsPerFrame.clear();
    }
    m_materialDrawSetCursor = {};
    m_passDrawSetCursor = {};
    m_sceneDrawSetCursor = {};
    m_deferredLightingDrawSetCursor = {};
    for (rhi::IBuffer*& b : m_frameUbos) { if (b) { device->DestroyBuffer(b); b = nullptr; } }
    for (rhi::IBuffer*& b : m_bonesBuffers) { if (b) { device->DestroyBuffer(b); b = nullptr; } }
    m_current = {};
}

rhi::IDescriptorSet* PassResourceBuilder::AcquireMaterialDrawSet(rhi::IDevice* device) {
    if (!device) return nullptr;
    const uint32_t frameIndex = device->GetCurrentFrameIndex() % kFrames;
    return AcquireDynamicSet(device, m_materialDrawSets[frameIndex], m_materialDrawSetCursor[frameIndex],
                             rhi::IDevice::ResourceSetType::Material);
}

rhi::IDescriptorSet* PassResourceBuilder::AcquirePassDrawSet(rhi::IDevice* device) {
    if (!device) return nullptr;
    const uint32_t frameIndex = device->GetCurrentFrameIndex() % kFrames;
    return AcquireDynamicSet(device, m_passDrawSets[frameIndex], m_passDrawSetCursor[frameIndex],
                             rhi::IDevice::ResourceSetType::ForwardPass);
}

rhi::IDescriptorSet* PassResourceBuilder::AcquireSceneDrawSet(rhi::IDevice* device) {
    if (!device) return nullptr;
    const uint32_t frameIndex = device->GetCurrentFrameIndex() % kFrames;
    return AcquireDynamicSet(device, m_sceneDrawSets[frameIndex], m_sceneDrawSetCursor[frameIndex],
                             rhi::IDevice::ResourceSetType::SceneBuffers);
}

rhi::IDescriptorSet* PassResourceBuilder::AcquireDeferredLightingSet(rhi::IDevice* device) {
    if (!device) return nullptr;
    const uint32_t frameIndex = device->GetCurrentFrameIndex() % kFrames;
    return AcquireDynamicSet(device, m_deferredLightingDrawSets[frameIndex], m_deferredLightingDrawSetCursor[frameIndex],
                             rhi::IDevice::ResourceSetType::DeferredLightingPass);
}

static rhi::ITexture* g_defaultAlbedoTexture = nullptr;
static rhi::ITexture* g_defaultNormalTexture = nullptr;
static rhi::ITexture* g_defaultMetalRoughTexture = nullptr;

static void EnsureSharedDefaultMaterialTextures(rhi::IDevice* device) {
    if (!device) return;
    if (!g_defaultAlbedoTexture) {
        rhi::TextureDesc desc{};
        desc.type = rhi::TextureType::Texture2D;
        desc.width = 1;
        desc.height = 1;
        desc.format = rhi::RHITextureFormat::RGBA8;
        desc.usage = rhi::TextureUsage::ShaderResource;
        desc.mipLevels = 1;
        static const uint8_t white[4] = { 255, 255, 255, 255 };
        g_defaultAlbedoTexture = device->CreateTexture(desc, white);
    }
    if (!g_defaultNormalTexture) {
        rhi::TextureDesc desc{};
        desc.type = rhi::TextureType::Texture2D;
        desc.width = 1;
        desc.height = 1;
        desc.format = rhi::RHITextureFormat::RGBA8;
        desc.usage = rhi::TextureUsage::ShaderResource;
        desc.mipLevels = 1;
        static const uint8_t normal[4] = { 128, 128, 255, 255 };
        g_defaultNormalTexture = device->CreateTexture(desc, normal);
    }
    if (!g_defaultMetalRoughTexture) {
        rhi::TextureDesc desc{};
        desc.type = rhi::TextureType::Texture2D;
        desc.width = 1;
        desc.height = 1;
        desc.format = rhi::RHITextureFormat::RGBA8;
        desc.usage = rhi::TextureUsage::ShaderResource;
        desc.mipLevels = 1;
        static const uint8_t metalRough[4] = { 0, 128, 0, 255 };
        g_defaultMetalRoughTexture = device->CreateTexture(desc, metalRough);
    }
}

void UpdateMaterialDescriptorSetWithDefaults(rhi::IDevice* device, rhi::IDescriptorSet* targetSet, const Material* material,
                                             const MaterialInstance* materialInstance) {
    if (!device || !targetSet) return;
    EnsureSharedDefaultMaterialTextures(device);
    if (!material && !materialInstance) return;

    rhi::IBuffer* materialBuffer = materialInstance ? materialInstance->GetMaterialBufferRHI(device) : material->GetMaterialBufferRHI(device);
    if (materialBuffer)
        targetSet->UpdateUniformBuffer(0, materialBuffer, 0, static_cast<uint32_t>(kMaterialBlockStd140Size));

    auto getTexture = [&](const std::string& name) -> rhi::ITexture* {
        if (materialInstance) return materialInstance->GetTextureRHI(name);
        return material ? material->GetTextureRHI(name) : nullptr;
    };

    rhi::ITexture* albedoTexture = getTexture("u_AlbedoTexture");
    rhi::ITexture* normalTexture = getTexture("u_NormalTexture");
    rhi::ITexture* metalRoughTexture = getTexture("u_MetallicRoughnessTexture");
    if (!metalRoughTexture) metalRoughTexture = getTexture("u_MetallicTexture");

    targetSet->UpdateTexture(1, albedoTexture ? albedoTexture : g_defaultAlbedoTexture);
    targetSet->UpdateTexture(2, normalTexture ? normalTexture : g_defaultNormalTexture);
    targetSet->UpdateTexture(3, metalRoughTexture ? metalRoughTexture : g_defaultMetalRoughTexture);
}

void PassResourceBuilder::UpdateMaterialSetWithDefaults(rhi::IDevice* device, rhi::IDescriptorSet* targetSet, const Material* material,
                                                      const MaterialInstance* materialInstance) {
    UpdateMaterialDescriptorSetWithDefaults(device, targetSet, material, materialInstance);
}

}  // namespace resources
}  // namespace render
}  // namespace eyelib

