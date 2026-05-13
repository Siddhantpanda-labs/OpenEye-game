#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>
#include <memory>
#include <string>

namespace eyelib {
namespace api {
namespace rendering {

class Texture2D;

/**
 * @brief Public PIMPL handle to a GPU material (shader id + textures + PBR params).
 *
 * Opaque to clients: the underlying internal type (`eyelib::rendering::Material`) is
 * never named in this header. Construction is deferred to **M3** (editor
 * `GPUResourceCache` migration) — see `Mesh.h` docstring for the design rationale.
 *
 * Field semantics:
 *  - `GetMaterialId()` returns a uint32 token consumed by `DrawableInstance::materialId`.
 *    A material table maps it back to the internal material; the table is owned by the
 *    factory bound to the pipeline (M3).
 *
 * Mutators are exposed for the editor inspector (`InspectorMaterialEditor`, M3.6 / M3.7),
 * which currently flips PBR params and texture slots on materials at runtime. Without
 * these, the editor would have to re-create materials on every parameter tweak.
 *
 * Thread safety: setters mutate the underlying material; serialize externally if more
 * than one thread writes (editor today single-threads material edits on the UI thread).
 */
class Material {
public:
    Material() noexcept;
    ~Material();

    Material(const Material& other);
    Material& operator=(const Material& other);
    Material(Material&& other) noexcept;
    Material& operator=(Material&& other) noexcept;

    bool IsValid() const noexcept;

    /** Token for `DrawableInstance::materialId`. Zero when invalid. */
    uint32_t GetMaterialId() const noexcept;

    // PBR params (parity with legacy `eyelib::api::Material` setters).
    void SetAlbedo(const math::Vec3& color);
    void SetMetallic(float value);
    void SetRoughness(float value);
    void SetEmission(const math::Vec3& color);
    void SetAO(float value);
    void SetAlpha(float value);

    math::Vec3 GetAlbedo() const;
    float GetMetallic() const;
    float GetRoughness() const;
    math::Vec3 GetEmission() const;
    float GetAO() const;
    float GetAlpha() const;

    // Canonical / Legacy mapping properties
    void SetUVTransform(const math::Vec4& transform);
    void SetUVRotation(float rotation);
    void SetTexCoordIndex(float index);
    void SetFlipV(float flip);

    math::Vec4 GetUVTransform() const;
    float GetUVRotation() const;
    float GetTexCoordIndex() const;
    float GetFlipV() const;

    // Common PBR texture slots (each takes a public `Texture2D` handle; passing an
    // invalid handle clears the slot).
    void SetAlbedoTexture(const Texture2D& texture);
    void SetNormalTexture(const Texture2D& texture);
    void SetMetallicTexture(const Texture2D& texture);
    void SetRoughnessTexture(const Texture2D& texture);
    void SetMetallicRoughnessTexture(const Texture2D& texture);
    void SetAOTexture(const Texture2D& texture);
    void SetEmissiveTexture(const Texture2D& texture);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    friend class MaterialAccess;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
