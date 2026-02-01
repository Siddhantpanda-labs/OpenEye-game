#pragma once

#include "eyelib/math/Types.h"
#include <functional>
#include <vector>
#include <memory>
#include <string>

namespace eyelib {
namespace render { class MaterialInstance; }
namespace api {

class Texture;
class Material;

class MaterialInstance {
public:
    MaterialInstance();
    explicit MaterialInstance(std::shared_ptr<Material> material);
    ~MaterialInstance();

    MaterialInstance(const MaterialInstance&) = delete;
    MaterialInstance& operator=(const MaterialInstance&) = delete;
    MaterialInstance(MaterialInstance&& other) noexcept;
    MaterialInstance& operator=(MaterialInstance&& other) noexcept;

    void SetMaterial(std::shared_ptr<Material> material);
    std::shared_ptr<Material> GetMaterial() const;

    void SetTextureOverride(const std::string& name, std::shared_ptr<Texture> texture);
    void SetAlbedoOverride(const math::Vec3& color);
    void SetMetallicOverride(float value);
    void SetRoughnessOverride(float value);
    void SetEmissionOverride(const math::Vec3& color);
    void SetAlphaOverride(float value);
    void SetFloatOverride(const std::string& name, float value);
    void SetVec3Override(const std::string& name, const math::Vec3& value);
    void SetVec4Override(const std::string& name, const math::Vec4& value);
    void SetMat4ArrayOverride(const std::string& name, const std::vector<math::Mat4>& values);
    void ClearOverrides();

    std::shared_ptr<Texture> GetTexture(const std::string& name) const;
    math::Vec3 GetAlbedo() const;
    float GetMetallic() const;
    float GetRoughness() const;
    math::Vec3 GetEmission() const;
    float GetAO() const;
    float GetAlpha() const;
    float GetFloat(const std::string& name, float defaultValue = 0.0f) const;
    math::Vec3 GetVec3(const std::string& name, const math::Vec3& defaultValue = math::Vec3(0.0f)) const;
    math::Vec4 GetVec4(const std::string& name, const math::Vec4& defaultValue = math::Vec4(0.0f)) const;
    void ForEachMat4Array(std::function<void(const std::string&, const math::Mat4*, size_t)> fn) const;

    std::shared_ptr<::eyelib::render::MaterialInstance> GetInternal() const;
    static std::shared_ptr<MaterialInstance> WrapInternal(const std::shared_ptr<::eyelib::render::MaterialInstance>& instance);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
