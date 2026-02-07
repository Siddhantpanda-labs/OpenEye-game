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

    /** RHI path: set RT, viewport, and clear depth. Requires non-null cmd. */
    void BindForShadowPass(rhi::ICommandList* cmd);
    void Unbind(rhi::ICommandList* cmd);
    /** Native texture id for legacy GL bind (e.g. LightingPass). 0 if RHI-only and no texture. */
    unsigned int GetDepthTexture() const;
    /** RHI path: use with cmd->BindTexture. Null if not initialized. */
    rhi::ITexture* GetDepthTextureRHI() const { return m_renderTargetRHI ? m_renderTargetRHI->GetDepthTexture() : nullptr; }
    rhi::IRenderTarget* GetRenderTargetRHI() const { return m_renderTargetRHI; }
    const math::Mat4& GetShadowMatrix() const { return m_shadowMatrix; }
    uint32_t GetResolution() const { return m_resolution; }

private:
    void Destroy();

    uint32_t m_resolution = 2048;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::IRenderTarget* m_renderTargetRHI = nullptr;
    math::Mat4 m_lightView = math::Mat4(1.0f);
    math::Mat4 m_lightProj = math::Mat4(1.0f);
    math::Mat4 m_shadowMatrix = math::Mat4(1.0f);
};

} // namespace render
} // namespace eyelib
