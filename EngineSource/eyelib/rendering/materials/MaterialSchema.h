#pragma once

#include "eyelib/math/Types.h"

#include <array>
#include <cstdint>
#include <cstring>

namespace eyelib {
namespace rendering {
namespace materials {

enum class MaterialAlphaMode : uint32_t {
    Opaque = 0u,
    Mask = 1u,
    Blend = 2u,
};

constexpr uint32_t kInvalidMaterialTextureHandle = 0xFFFFFFFFu;

/**
 * Canonical CPU material payload for deferred raster PBR.
 * IDs and texture handles are owned by the runtime material table.
 */
struct MaterialRecord {
    math::Vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    math::Vec4 emissiveFactor{0.0f, 0.0f, 0.0f, 1.0f};
    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
    float alphaCutoff = 0.5f;
    MaterialAlphaMode alphaMode = MaterialAlphaMode::Opaque;
    bool doubleSided = false;
    uint32_t baseColorTexture = kInvalidMaterialTextureHandle;
    uint32_t normalTexture = kInvalidMaterialTextureHandle;
    uint32_t metallicRoughnessTexture = kInvalidMaterialTextureHandle;
    uint32_t emissiveTexture = kInvalidMaterialTextureHandle;
    uint32_t occlusionTexture = kInvalidMaterialTextureHandle;
};

/**
 * std430 GPU row for material-table SSBO.
 * Keep this as the only shader-facing packing contract.
 */
struct MaterialGpuStd430 {
    float baseColorFactor[4]{};
    float emissiveFactor[4]{};
    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
    float alphaCutoff = 0.5f;
    uint32_t alphaMode = 0u;
    uint32_t doubleSided = 0u;
    uint32_t _pad0 = 0u;
    uint32_t textureHandles[8]{};
};
static_assert(sizeof(MaterialGpuStd430) == 96u, "MaterialGpuStd430 packing must stay stable");

inline MaterialRecord MakeDefaultMaterialRecord() noexcept {
    MaterialRecord out{};
    return out;
}

inline void FillMaterialGpuRow(const MaterialRecord& src, MaterialGpuStd430& out) noexcept {
    std::memcpy(out.baseColorFactor, &src.baseColorFactor.x, sizeof(out.baseColorFactor));
    std::memcpy(out.emissiveFactor, &src.emissiveFactor.x, sizeof(out.emissiveFactor));
    out.metallicFactor = src.metallicFactor;
    out.roughnessFactor = src.roughnessFactor;
    out.normalScale = src.normalScale;
    out.occlusionStrength = src.occlusionStrength;
    out.alphaCutoff = src.alphaCutoff;
    out.alphaMode = static_cast<uint32_t>(src.alphaMode);
    out.doubleSided = src.doubleSided ? 1u : 0u;
    out._pad0 = 0u;
    out.textureHandles[0] = src.baseColorTexture;
    out.textureHandles[1] = src.normalTexture;
    out.textureHandles[2] = src.metallicRoughnessTexture;
    out.textureHandles[3] = src.emissiveTexture;
    out.textureHandles[4] = src.occlusionTexture;
    out.textureHandles[5] = kInvalidMaterialTextureHandle;
    out.textureHandles[6] = kInvalidMaterialTextureHandle;
    out.textureHandles[7] = kInvalidMaterialTextureHandle;
}

} // namespace materials
} // namespace rendering
} // namespace eyelib
