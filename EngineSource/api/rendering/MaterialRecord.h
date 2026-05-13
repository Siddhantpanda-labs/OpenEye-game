#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Alpha pipeline behavior for `MaterialRecord`. Mirrors
 *        `eyelib::rendering::materials::MaterialAlphaMode` 1:1 (same numeric values).
 */
enum class MaterialAlphaMode : uint32_t {
    Opaque = 0u,
    Mask   = 1u,
    Blend  = 2u,
};

/**
 * @brief Sentinel for an unset texture slot inside a `MaterialRecord`.
 *
 * `MaterialRecord::*Texture` defaults to this value; setting a texture (via the
 * `Material` PIMPL setter, or by populating the field directly before
 * `ResourceCache::CreateMaterial`) replaces it with the cache-assigned texture token
 * returned alongside `Texture2D::GetMeshHandle`-style accessors. Resolution back to a
 * GPU texture happens inside the new module's MaterialTable when its texture
 * binding-table feature lands; until then the sentinel + token contract is captured
 * but not yet observed at draw time.
 */
inline constexpr uint32_t kInvalidTextureHandle = 0xFFFFFFFFu;

/**
 * @brief Public CPU material record consumed by `ResourceCache::CreateMaterial`.
 *
 * Field-for-field mirror of `eyelib::rendering::materials::MaterialRecord` (the row
 * shape uploaded to the deferred lighting MaterialTable SSBO). Translation to the
 * internal record happens inside `ResourceCache.cpp`. The public `Material` PIMPL
 * mutates this record (via `Material::SetAlbedo`, `SetMetallic`, …) and asks the
 * cache to re-publish it to every attached `RenderPipeline`'s MaterialTable.
 *
 * Defaults match the internal `MakeDefaultMaterialRecord()` (white, fully rough,
 * non-metallic). `alphaCutoff` is only sampled when `alphaMode == Mask`. Texture
 * slot tokens default to `kInvalidTextureHandle` (no texture; the per-channel
 * factor is used directly).
 */
struct MaterialRecord {
    math::Vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    math::Vec4 emissiveFactor{0.0f, 0.0f, 0.0f, 1.0f};
    float      metallicFactor      = 0.0f;
    float      roughnessFactor     = 1.0f;
    float      normalScale         = 1.0f;
    float      occlusionStrength   = 1.0f;
    float      alphaCutoff         = 0.5f;
    MaterialAlphaMode alphaMode    = MaterialAlphaMode::Opaque;
    bool       doubleSided         = false;
    math::Vec4 uvTransform{0.0f, 0.0f, 1.0f, 1.0f};
    float      uvRotation          = 0.0f;
    float      texCoordIndex       = 0.0f;
    float      flipV               = 1.0f;
    uint32_t   baseColorTexture         = kInvalidTextureHandle;
    uint32_t   normalTexture            = kInvalidTextureHandle;
    uint32_t   metallicRoughnessTexture = kInvalidTextureHandle;
    uint32_t   emissiveTexture          = kInvalidTextureHandle;
    uint32_t   occlusionTexture         = kInvalidTextureHandle;
    uint32_t   metallicTexture          = kInvalidTextureHandle;
    uint32_t   roughnessTexture         = kInvalidTextureHandle;
};

}  // namespace rendering
}  // namespace api
}  // namespace eyelib
