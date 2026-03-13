#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace render {

class TextureCube;
class Shader;
class Mesh;

/**
 * @brief Image-Based Lighting system.
 * Generates: Irradiance map (diffuse IBL), Prefiltered environment map (specular IBL), BRDF LUT.
 */
class IBL {
public:
    IBL();
    ~IBL();

    IBL(const IBL&) = delete;
    IBL& operator=(const IBL&) = delete;

    /** Initialize IBL from an environment cubemap texture. */
    bool Initialize(rhi::ITexture* environmentCubemap, uint32_t irradianceRes = 32, uint32_t prefilterRes = 512);

    void SetDevice(rhi::IDevice* device) { m_injectedDevice = device; }

    /**
     * @brief Get generated textures (native IDs for legacy bind paths).
     */
    unsigned int GetIrradianceMap() const;
    unsigned int GetPrefilterMap() const;
    unsigned int GetBRDFLUT() const;

    /** RHI texture pointers for reflection probe / external binding. */
    rhi::ITexture* GetIrradianceTextureRHI() const { return m_irradianceRHI; }
    rhi::ITexture* GetPrefilterTextureRHI() const { return m_prefilterRHI; }
    rhi::ITexture* GetBRDFLUTTextureRHI() const { return m_brdfLUTRHI; }

    /**
     * Generate irradiance and prefilter from a cubemap (e.g. for reflection probe). Caller owns returned textures and must destroy them.
     * @return true on success; outIrradiance and outPrefilter are set. brdfLUT can be from any valid IBL for binding.
     */
    static bool GenerateIrradianceAndPrefilter(rhi::IDevice* device, rhi::ITexture* environmentCubemap,
                                               uint32_t irradianceRes, uint32_t prefilterRes,
                                               rhi::ITexture*& outIrradiance, rhi::ITexture*& outPrefilter);

    /**
     * @brief Bind textures for lighting pass (preferred: use same command list as the lighting pass).
     * @param cmd Command list to bind textures on; when null, uses device immediate list.
     * @param irradianceUnit Texture unit for irradiance map (default 11)
     * @param prefilterUnit Texture unit for prefiltered map (default 12)
     * @param brdfLUTUnit Texture unit for BRDF LUT (default 13)
     */
    void BindForLighting(rhi::ICommandList* cmd, unsigned int irradianceUnit = 11, unsigned int prefilterUnit = 12, unsigned int brdfLUTUnit = 13) const;

    bool IsValid() const { return m_irradianceRHI && m_prefilterRHI && m_brdfLUTRHI; }

private:
    rhi::IDevice* m_injectedDevice = nullptr;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::ITexture* m_environmentRHI = nullptr;
    rhi::ITexture* m_irradianceRHI = nullptr;
    rhi::ITexture* m_prefilterRHI = nullptr;
    rhi::ITexture* m_brdfLUTRHI = nullptr;
};

} // namespace render
} // namespace eyelib
