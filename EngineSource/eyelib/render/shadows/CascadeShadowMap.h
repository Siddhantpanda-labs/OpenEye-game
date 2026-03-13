#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <vector>

namespace eyelib {
namespace render {

/** Per-cascade frustum (Limitless-style): 8 corners, crop matrix, near/far. */
struct ShadowFrustum {
    math::Vec3 points[8];
    math::Mat4 crop{1.0f};
    float near_distance = 0.0f;
    float far_distance = 0.0f;
    float fov = 0.0f;
    float ratio = 0.0f;
};

/** Cascade shadow map for directional light (Limitless-inspired). */
class CascadeShadowMap {
public:
    static constexpr float SPLIT_WEIGHT = 0.75f;
    static constexpr uint32_t kDefaultSplitCount = 4u;
    static constexpr uint32_t kDefaultResolution = 2048u;

    CascadeShadowMap();
    ~CascadeShadowMap();

    CascadeShadowMap(const CascadeShadowMap&) = delete;
    CascadeShadowMap& operator=(const CascadeShadowMap&) = delete;
    CascadeShadowMap(CascadeShadowMap&& other) noexcept;
    CascadeShadowMap& operator=(CascadeShadowMap&& other) noexcept;

    /** Initialize with resolution and cascade count. Requires device for RHI path. */
    bool Initialize(rhi::IDevice* device, uint32_t resolution = kDefaultResolution, uint32_t splitCount = kDefaultSplitCount);

    /** Update frustums and light matrices from camera and light. Call once per frame before draw. */
    void Update(const math::Vec3& lightDir,
                const math::Mat4& view,
                const math::Mat4& projection,
                const math::Vec3& cameraPos,
                float nearPlane,
                float farPlane,
                float aspectRatio);

    /** Bind RT for rendering cascade i: sets layer, viewport, clear. Call before drawing shadow geometry. */
    void BindForShadowPass(rhi::ICommandList* cmd, uint32_t cascadeIndex);
    void Unbind(rhi::ICommandList* cmd);

    /** Light-space matrix for cascade i (for shadow pass vertex shader). */
    const math::Mat4& GetLightSpaceMatrix(uint32_t cascadeIndex) const;

    /** Depth texture (2D array); bind at shadow map slot for lighting/forward. */
    rhi::ITexture* GetDepthTextureRHI() const;
    /** SSBO of split_count mat4 light-space matrices; bind for lighting/forward. */
    rhi::IBuffer* GetLightSpaceBufferRHI() const;
    /** Upload light_space to SSBO. Call after Update, before lighting pass. */
    void MapLightSpaceToBuffer() const;

    uint32_t GetSplitCount() const { return m_splitCount; }
    uint32_t GetResolution() const { return m_resolution; }
    /** Far bounds in NDC (0..1) for cascade selection in fragment shader. Size split_count. */
    const std::vector<float>& GetFarBounds() const { return m_farBounds; }

private:
    void Destroy();
    void UpdateFrustums(float nearPlane, float farPlane, float fov, float aspectRatio);
    void UpdateLightMatrices(const math::Vec3& lightDir);

    uint32_t m_resolution = kDefaultResolution;
    uint32_t m_splitCount = kDefaultSplitCount;
    rhi::IDevice* m_device = nullptr;
    rhi::IRenderTarget* m_renderTargetRHI = nullptr;
    rhi::IBuffer* m_lightSpaceBuffer = nullptr;

    std::vector<ShadowFrustum> m_frustums;
    std::vector<float> m_farBounds;
    std::vector<math::Mat4> m_lightSpace;

    float m_cameraNear = 0.1f;
    float m_cameraFar = 100.0f;
    float m_cameraFov = 1.0f;
    float m_cameraAspect = 1.0f;
    math::Vec3 m_cameraPos{0.0f};
    math::Mat4 m_view{1.0f};
    math::Mat4 m_projection{1.0f};
};

} // namespace render
} // namespace eyelib
