#pragma once

#include "eyelib/math/Types.h"
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
    void ClearOverrides();

    std::shared_ptr<::eyelib::render::MaterialInstance> GetInternal() const;
    static std::shared_ptr<MaterialInstance> WrapInternal(const std::shared_ptr<::eyelib::render::MaterialInstance>& instance);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
