#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <vector>

namespace eyelib {
namespace render {

/** Directional shadow map (single cascade). */
class ShadowMap {
public:
    ShadowMap();
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
    ShadowMap(ShadowMap&& other) noexcept;
    ShadowMap& operator=(ShadowMap&& other) noexcept;

    bool Initialize(uint32_t resolution = 2048);
    /** When device is non-null, creates an RHI wrapper for the depth texture so pipeline can use cmd->BindTexture. */
    bool Initialize(rhi::IDevice* device, uint32_t resolution);

    void UpdateMatrices(const math::Vec3& lightDir, const math::Mat4& view, const math::Mat4& proj,
                        const math::Vec3& cameraPos, float nearPlane, float farPlane);

    void BindForShadowPass();
    void Unbind();
    unsigned int GetDepthTexture() const { return m_depthTexture; }
    /** RHI path: use with cmd->BindTexture. Null if initialized without device. */
    rhi::ITexture* GetDepthTextureRHI() const { return m_depthTextureRHI; }
    const math::Mat4& GetShadowMatrix() const { return m_shadowMatrix; }
    uint32_t GetResolution() const { return m_resolution; }

private:
    void Destroy();

    uint32_t m_resolution = 2048;
    unsigned int m_fbo = 0;
    unsigned int m_depthTexture = 0;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::ITexture* m_depthTextureRHI = nullptr;
    math::Mat4 m_lightView = math::Mat4(1.0f);
    math::Mat4 m_lightProj = math::Mat4(1.0f);
    math::Mat4 m_shadowMatrix = math::Mat4(1.0f);
};

} // namespace render
} // namespace eyelib
