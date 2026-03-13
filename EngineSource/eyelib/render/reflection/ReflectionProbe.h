#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace render {

namespace rhi { struct ITexture; struct IDevice; struct ICommandList; }

/**
 * Reflection probe: environment capture for reflections when SSR cannot.
 * Holds position, influence radius, and PBR cubemaps (raw, prefiltered for specular, irradiance for diffuse).
 * Pipeline: SSR → probe → skybox.
 */
class ReflectionProbe {
public:
    ReflectionProbe() = default;

    /** World position of the probe. */
    void SetPosition(const math::Vec3& pos) { m_position = pos; }
    const math::Vec3& GetPosition() const { return m_position; }

    /** Influence radius (sphere). Objects within use this probe. Use large value for global probe. */
    void SetRadius(float r) { m_radius = r; }
    float GetRadius() const { return m_radius; }

    /** Raw captured environment cubemap (optional; used for capture pipeline). */
    void SetCubemap(rhi::ITexture* cubemap) { m_cubemap = cubemap; }
    rhi::ITexture* GetCubemap() const { return m_cubemap; }

    /** Prefiltered cubemap (roughness mips) for specular reflections. Bind to shaders for reflection sampling. */
    void SetPrefilteredMap(rhi::ITexture* tex) { m_prefilteredMap = tex; }
    rhi::ITexture* GetPrefilteredMap() const { return m_prefilteredMap; }

    /** Irradiance map for diffuse IBL. */
    void SetIrradianceMap(rhi::ITexture* tex) { m_irradianceMap = tex; }
    rhi::ITexture* GetIrradianceMap() const { return m_irradianceMap; }

    /** If true, probe is updated periodically (e.g. scene capture). Static probes are captured once. */
    void SetDynamic(bool d) { m_dynamic = d; }
    bool IsDynamic() const { return m_dynamic; }

    /** For SSR fallback and reflection sampling: prefiltered map is the main reflection texture. */
    rhi::ITexture* GetCubemapTexture() const { return m_prefilteredMap ? m_prefilteredMap : m_cubemap; }

    /** Bind irradiance (11), prefilter (12), and BRDF LUT (13) for Forward/Forward+ lighting. brdfLUT from IBL (shared). */
    void BindForLighting(rhi::ICommandList* cmd, unsigned int irradianceUnit, unsigned int prefilterUnit,
                         unsigned int brdfLUTUnit, rhi::ITexture* brdfLUT) const;

    bool IsValid() const { return GetCubemapTexture() != nullptr; }

private:
    math::Vec3 m_position{0.f, 0.f, 0.f};
    float m_radius = 10000.f;  // default large = global
    rhi::ITexture* m_cubemap = nullptr;
    rhi::ITexture* m_prefilteredMap = nullptr;
    rhi::ITexture* m_irradianceMap = nullptr;
    bool m_dynamic = false;
};

} // namespace render
} // namespace eyelib
