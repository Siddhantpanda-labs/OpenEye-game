#pragma once

#include "eyelib/math/Types.h"
#include <memory>
#include <string>

namespace eyelib {
namespace render { class Material; }
namespace api {

class Texture;

/**
 * @brief Material handle (opaque). Set shader, textures, PBR properties.
 * No OpenGL types exposed.
 */
class Material {
public:
    Material();
    ~Material();

    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;
    Material(Material&& other) noexcept;
    Material& operator=(Material&& other) noexcept;

    /** Set texture by name. */
    void SetTexture(const std::string& name, std::shared_ptr<Texture> texture);

    /** Set PBR properties. */
    void SetAlbedo(const math::Vec3& color);
    void SetMetallic(float value);
    void SetRoughness(float value);
    void SetEmission(const math::Vec3& color);
    void SetAlpha(float value);

    /** Get PBR properties. */
    math::Vec3 GetAlbedo() const;
    float GetMetallic() const;
    float GetRoughness() const;
    math::Vec3 GetEmission() const;
    float GetAlpha() const;

    /** Convenience texture setters. */
    void SetAlbedoTexture(std::shared_ptr<Texture> t);
    void SetNormalTexture(std::shared_ptr<Texture> t);
    void SetMetallicTexture(std::shared_ptr<Texture> t);
    void SetRoughnessTexture(std::shared_ptr<Texture> t);

    static std::shared_ptr<Material> WrapInternal(const std::shared_ptr<render::Material>& material);
    std::shared_ptr<render::Material> GetInternal() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
