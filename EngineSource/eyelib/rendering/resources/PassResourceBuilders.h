#pragma once

#include "eyelib/rendering/rhi/RHI.h"
#include <array>
#include <cstdint>
#include <vector>

namespace eyelib {
namespace rendering {
class Material;
class MaterialInstance;
namespace resources {

/**
 * Manages per-frame descriptor-set pools and dynamic draw-set allocation for pass execution.
 * Owns descriptor/buffer lifetime for the resource-space bindings used by forward/deferred passes.
 */
class PassResourceBuilder {
public:
    static constexpr uint32_t kFrames = 3u;

    /** Snapshot of currently selected per-frame resources. */
    struct CurrentFrameResources {
        rhi::IDescriptorSet* frameSet = nullptr;
        rhi::IDescriptorSet* passSet = nullptr;
        rhi::IDescriptorSet* transparentPassSet = nullptr;
        rhi::IDescriptorSet* materialSet = nullptr;
        rhi::IDescriptorSet* sceneSet = nullptr;
        rhi::IDescriptorSet* transparentSceneSet = nullptr;
        rhi::IDescriptorSet* deferredLightingSet = nullptr;
        rhi::IBuffer* frameUbo = nullptr;
        rhi::IBuffer* bonesBuffer = nullptr;
    };

    /**
     * Ensures per-frame base resources are created and selects the current frame aliases.
     * @param device Active render device.
     * @return true when required resources are available for the current frame.
     */
    bool Ensure(rhi::IDevice* device);
    /**
     * Selects the current frame resource aliases and resets dynamic cursors for this frame.
     * @param device Active render device.
     */
    void SelectFrame(rhi::IDevice* device);
    /**
     * Releases all descriptor sets and buffers owned by this builder.
     * @param device Active render device used to destroy resources.
     */
    void Shutdown(rhi::IDevice* device);

    /**
     * Acquires a dynamic Material descriptor set for the current frame.
     * @param device Active render device.
     * @return Descriptor set ready for caller updates/binds, or null on failure.
     */
    rhi::IDescriptorSet* AcquireMaterialDrawSet(rhi::IDevice* device);
    /**
     * Acquires a dynamic ForwardPass descriptor set for the current frame.
     * @param device Active render device.
     * @return Descriptor set ready for caller updates/binds, or null on failure.
     */
    rhi::IDescriptorSet* AcquirePassDrawSet(rhi::IDevice* device);
    /**
     * Acquires a dynamic SceneBuffers descriptor set for the current frame.
     * @param device Active render device.
     * @return Descriptor set ready for caller updates/binds, or null on failure.
     */
    rhi::IDescriptorSet* AcquireSceneDrawSet(rhi::IDevice* device);
    /**
     * Acquires a dynamic DeferredLightingPass descriptor set for the current frame.
     * @param device Active render device.
     * @return Descriptor set ready for caller updates/binds, or null on failure.
     */
    rhi::IDescriptorSet* AcquireDeferredLightingSet(rhi::IDevice* device);
    /**
     * Updates a material descriptor set with material buffer + texture bindings and default fallback textures.
     * @param device Active render device.
     * @param targetSet Descriptor set to update.
     * @param material Base material (optional when materialInstance is provided).
     * @param materialInstance Material instance override (optional).
     */
    void UpdateMaterialSetWithDefaults(rhi::IDevice* device, rhi::IDescriptorSet* targetSet,
                                       const Material* material, const MaterialInstance* materialInstance);

    /** @return Current frame aliases selected by Ensure/SelectFrame. */
    const CurrentFrameResources& Current() const { return m_current; }

private:
    std::array<rhi::IDescriptorSet*, kFrames> m_frameSets = {};
    std::array<rhi::IDescriptorSet*, kFrames> m_passSets = {};
    std::array<rhi::IDescriptorSet*, kFrames> m_transparentPassSets = {};
    std::array<rhi::IDescriptorSet*, kFrames> m_materialSets = {};
    std::array<rhi::IDescriptorSet*, kFrames> m_sceneSets = {};
    std::array<rhi::IDescriptorSet*, kFrames> m_transparentSceneSets = {};
    std::array<rhi::IDescriptorSet*, kFrames> m_deferredLightingSets = {};
    std::array<std::vector<rhi::IDescriptorSet*>, kFrames> m_materialDrawSets = {};
    std::array<uint32_t, kFrames> m_materialDrawSetCursor = {};
    std::array<std::vector<rhi::IDescriptorSet*>, kFrames> m_passDrawSets = {};
    std::array<uint32_t, kFrames> m_passDrawSetCursor = {};
    std::array<std::vector<rhi::IDescriptorSet*>, kFrames> m_sceneDrawSets = {};
    std::array<uint32_t, kFrames> m_sceneDrawSetCursor = {};
    std::array<std::vector<rhi::IDescriptorSet*>, kFrames> m_deferredLightingDrawSets = {};
    std::array<uint32_t, kFrames> m_deferredLightingDrawSetCursor = {};
    std::array<rhi::IBuffer*, kFrames> m_frameUbos = {};
    std::array<rhi::IBuffer*, kFrames> m_bonesBuffers = {};
    CurrentFrameResources m_current{};
};

/**
 * Fills Material resource-space set (UBO 0 + combined samplers 1–3) with material data and 1×1 fallbacks when textures are missing.
 * Used by GBuffer/forward Material::UpdateForwardResourceSet and by PassResourceBuilder::UpdateMaterialSetWithDefaults.
 */
void UpdateMaterialDescriptorSetWithDefaults(rhi::IDevice* device, rhi::IDescriptorSet* targetSet, const Material* material,
                                           const MaterialInstance* materialInstance);

}  // namespace resources
}  // namespace rendering
}  // namespace eyelib

