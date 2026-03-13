#pragma once

#include "api/render/RenderDevice.h"
#include "api/render/RenderScene.h"
#include "api/render/CascadeShadowMap.h"
#include "eyelib/math/Types.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace render { class ShadowPass; }
namespace api {

class ShadowPass {
public:
    ShadowPass();
    ~ShadowPass();

    ShadowPass(const ShadowPass&) = delete;
    ShadowPass& operator=(const ShadowPass&) = delete;
    ShadowPass(ShadowPass&& other) noexcept;
    ShadowPass& operator=(ShadowPass&& other) noexcept;

    void SetDevice(RenderDevice device) { m_device = device; }

    void Render(CascadeShadowMap& cascadeShadowMap,
                const std::vector<Drawable>& drawables,
                const math::Vec3& lightDir,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& cameraPos,
                float nearPlane,
                float farPlane,
                float aspectRatio = 0.0f);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    RenderDevice m_device;
};

} // namespace api
} // namespace eyelib
