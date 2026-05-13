#pragma once

#include "eyelib/math/Types.h"

#include <array>
#include <algorithm>
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
    math::Vec4 uvTransform{0.0f, 0.0f, 1.0f, 1.0f};
    float uvRotation = 0.0f;
    float texCoordIndex = 0.0f;
    float flipV = 1.0f;
    uint32_t baseColorTexture = kInvalidMaterialTextureHandle;
    uint32_t normalTexture = kInvalidMaterialTextureHandle;
    uint32_t metallicRoughnessTexture = kInvalidMaterialTextureHandle;
    uint32_t emissiveTexture = kInvalidMaterialTextureHandle;
    uint32_t occlusionTexture = kInvalidMaterialTextureHandle;
    uint32_t metallicTexture = kInvalidMaterialTextureHandle;
    uint32_t roughnessTexture = kInvalidMaterialTextureHandle;
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
    float uvTransform[4]{};
    float uvRotation = 0.0f;
    float texCoordIndex = 0.0f;
    float flipV = 1.0f;
    uint32_t _pad1 = 0u;
    uint32_t textureHandles[8]{};
};
static_assert(sizeof(MaterialGpuStd430) == 128u, "MaterialGpuStd430 packing must stay stable");

inline MaterialRecord MakeDefaultMaterialRecord() noexcept {
    MaterialRecord out{};
    return out;
}

inline void FillMaterialGpuRow(const MaterialRecord& src, MaterialGpuStd430& out) noexcept {
    std::memcpy(out.baseColorFactor, &src.baseColorFactor.x, sizeof(out.baseColorFactor));
    std::memcpy(out.emissiveFactor, &src.emissiveFactor.x, sizeof(out.emissiveFactor));
    out.metallicFactor = (std::clamp)(src.metallicFactor, 0.0f, 1.0f);
    out.roughnessFactor = (std::clamp)(src.roughnessFactor, 0.04f, 1.0f);
    out.normalScale = (std::max)(src.normalScale, 0.0f);
    out.occlusionStrength = (std::clamp)(src.occlusionStrength, 0.0f, 1.0f);
    out.alphaCutoff = (std::clamp)(src.alphaCutoff, 0.0f, 1.0f);
    out.alphaMode = (std::min)(static_cast<uint32_t>(src.alphaMode), 2u);
    out.doubleSided = src.doubleSided ? 1u : 0u;
    out.uvTransform[0] = src.uvTransform.x;
    out.uvTransform[1] = src.uvTransform.y;
    out.uvTransform[2] = src.uvTransform.z;
    out.uvTransform[3] = src.uvTransform.w;
    out.uvRotation = src.uvRotation;
    out.texCoordIndex = src.texCoordIndex;
    out.flipV = src.flipV;
    out._pad1 = 0u;
    out.textureHandles[0] = src.baseColorTexture;
    out.textureHandles[1] = src.normalTexture;
    out.textureHandles[2] = src.metallicRoughnessTexture;
    out.textureHandles[3] = src.emissiveTexture;
    out.textureHandles[4] = src.occlusionTexture;
    out.textureHandles[5] = src.metallicTexture;
    out.textureHandles[6] = src.roughnessTexture;
    out.textureHandles[7] = kInvalidMaterialTextureHandle;
}

} // namespace materials
} // namespace rendering
} // namespace eyelib
