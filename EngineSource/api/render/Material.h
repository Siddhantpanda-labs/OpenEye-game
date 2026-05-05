#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace render { class Material; }
namespace api {

class Texture;
class Shader;

/** Refractor 6.4: shading model for pipeline/material variants (Lit PBR, Unlit, Custom). */
enum class ShadingModel : uint8_t {
    Lit,
    Unlit,
    Custom
};

/** Refractor 7.2: blending mode (Limitless-aligned). */
enum class Blending : uint8_t {
    Opaque,
    Translucent,
    Additive,
    Modulate,
};

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
    /** Set material shader type (0 = PBR). Shader variant is resolved at draw time from ShaderCache. */
    void SetMaterialShaderId(uint64_t id);

    /** Set PBR properties. */
    void SetAlbedo(const math::Vec3& color);
    void SetMetallic(float value);
    void SetRoughness(float value);
    void SetEmission(const math::Vec3& color);
    void SetAO(float value);
    void SetAlpha(float value);

    /** Get PBR properties. */
    math::Vec3 GetAlbedo() const;
    float GetMetallic() const;
    float GetRoughness() const;
    math::Vec3 GetEmission() const;
    float GetAO() const;
    float GetAlpha() const;

    /** Convenience texture setters. */
    void SetAlbedoTexture(std::shared_ptr<Texture> t);
    void SetNormalTexture(std::shared_ptr<Texture> t);
    void SetMetallicTexture(std::shared_ptr<Texture> t);
    void SetRoughnessTexture(std::shared_ptr<Texture> t);
    void SetAOTexture(std::shared_ptr<Texture> t);
    void SetEmissiveTexture(std::shared_ptr<Texture> t);
    void SetMetallicRoughnessTexture(std::shared_ptr<Texture> t);

    void SetFloat(const std::string& name, float value);
    void SetVec3(const std::string& name, const math::Vec3& value);
    void SetVec4(const std::string& name, const math::Vec4& value);
    void SetMat4Array(const std::string& name, const std::vector<math::Mat4>& values);

    float GetFloat(const std::string& name, float defaultValue = 0.0f) const;
    math::Vec3 GetVec3(const std::string& name, const math::Vec3& defaultValue = math::Vec3(0.0f)) const;
    math::Vec4 GetVec4(const std::string& name, const math::Vec4& defaultValue = math::Vec4(0.0f)) const;

    void SetKeyword(const std::string& keyword, bool enabled);
    bool IsKeywordEnabled(const std::string& keyword) const;

    /** Refractor 6.4: shading model (Lit / Unlit / Custom). */
    ShadingModel GetShadingModel() const;
    void SetShadingModel(ShadingModel m);

    /** Refractor 6.4: shader variant id for cache; 0 = default. */
    uint64_t GetShaderIndex() const;
    void SetShaderIndex(uint64_t id);

    /** Refractor 6.4: optional GLSL snippets for MaterialCompiler (fragment / shading). */
    std::string GetFragmentSnippet() const;
    void SetFragmentSnippet(std::string s);
    std::string GetShadingSnippet() const;
    void SetShadingSnippet(std::string s);

    /** Refractor 7.2: blending (Opaque / Translucent / Additive / Modulate). */
    Blending GetBlending() const;
    void SetBlending(Blending b);
    /** Refractor 7.2: two-sided (disable back-face culling). */
    bool GetTwoSided() const;
    void SetTwoSided(bool v);
    /** Refractor 7.2: refraction (compile-time HAS_REFRACTION in shader). */
    bool GetRefraction() const;
    void SetRefraction(bool v);
    /** SSR: when true, surface receives screen-space reflections. */
    bool GetReflective() const;
    void SetReflective(bool v);
    /** SSR: reflection amount 0 (none) to 1 (full). */
    float GetReflectiveStrength() const;
    void SetReflectiveStrength(float v);

    static std::shared_ptr<Material> WrapInternal(const std::shared_ptr<::eyelib::render::Material>& material);
    std::shared_ptr<::eyelib::render::Material> GetInternal() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
