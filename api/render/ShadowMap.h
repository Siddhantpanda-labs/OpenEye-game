#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace render { class ShadowMap; }
namespace api {

class ShadowMap {
public:
    ShadowMap();
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
    ShadowMap(ShadowMap&& other) noexcept;
    ShadowMap& operator=(ShadowMap&& other) noexcept;

    bool Initialize(uint32_t resolution = 2048);
    void UpdateMatrices(const math::Vec3& lightDir, const math::Mat4& view, const math::Mat4& proj,
                        const math::Vec3& cameraPos, float nearPlane, float farPlane);

    void BindForShadowPass();
    void Unbind();

    uint32_t GetDepthTexture() const;
    const math::Mat4& GetShadowMatrix() const;
    uint32_t GetResolution() const;

    std::shared_ptr<::eyelib::render::ShadowMap> GetInternal() const;
    static std::shared_ptr<ShadowMap> WrapInternal(const std::shared_ptr<::eyelib::render::ShadowMap>& shadow);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
