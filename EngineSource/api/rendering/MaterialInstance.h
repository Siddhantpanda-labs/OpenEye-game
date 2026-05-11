#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace api {
namespace rendering {

class Material;
class Texture2D;

/**
 * @brief Public PIMPL handle to a material instance: shared base material + per-instance overrides.
 *
 * Opaque to clients: the underlying internal type (`eyelib::rendering::MaterialInstance`)
 * is never named in this header. Construction is deferred to **M3** — see `Mesh.h`
 * docstring for design rationale.
 *
 * `GetMaterialId()` returns the same token as the base material's `Material::GetMaterialId()`
 * — instances ride on the base material's table entry. Per-instance overrides ship via
 * the instance descriptor set updated each frame.
 *
 * Per-instance setters mirror the legacy `eyelib::api::MaterialInstance` surface so editor
 * inspectors that override per-instance params (alpha, tint, etc.) migrate by namespace
 * substitution only.
 */
class MaterialInstance {
public:
    MaterialInstance() noexcept;
    ~MaterialInstance();

    MaterialInstance(const MaterialInstance& other);
    MaterialInstance& operator=(const MaterialInstance& other);
    MaterialInstance(MaterialInstance&& other) noexcept;
    MaterialInstance& operator=(MaterialInstance&& other) noexcept;

    bool IsValid() const noexcept;

    /** Same token as the base material's `GetMaterialId()`. Zero when invalid. */
    uint32_t GetMaterialId() const noexcept;

    /** Get the base material handle (default-constructed if invalid). */
    Material GetBaseMaterial() const;

    // Per-instance overrides (parity with legacy `eyelib::api::MaterialInstance`).
    void SetAlbedoOverride(const math::Vec3& color);
    void SetMetallicOverride(float value);
    void SetRoughnessOverride(float value);
    void SetEmissionOverride(const math::Vec3& color);
    void SetAlphaOverride(float value);

    void SetTextureOverride(const char* slotName, const Texture2D& texture);

    void ClearOverrides();

    math::Vec3 GetAlbedo() const;
    float GetMetallic() const;
    float GetRoughness() const;
    math::Vec3 GetEmission() const;
    float GetAlpha() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    friend class MaterialInstanceAccess;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
