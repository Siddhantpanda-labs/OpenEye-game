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

    /**
     * @brief Get generated textures.
     */
    unsigned int GetIrradianceMap() const;
    unsigned int GetPrefilterMap() const;
    unsigned int GetBRDFLUT() const;

    /**
     * @brief Bind textures for lighting pass.
     * @param irradianceUnit Texture unit for irradiance map (default 11)
     * @param prefilterUnit Texture unit for prefiltered map (default 12)
     * @param brdfLUTUnit Texture unit for BRDF LUT (default 13)
     */
    void BindForLighting(unsigned int irradianceUnit = 11, unsigned int prefilterUnit = 12, unsigned int brdfLUTUnit = 13) const;

    bool IsValid() const { return m_irradianceRHI && m_prefilterRHI && m_brdfLUTRHI; }

private:
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::ITexture* m_environmentRHI = nullptr;
    rhi::ITexture* m_irradianceRHI = nullptr;
    rhi::ITexture* m_prefilterRHI = nullptr;
    rhi::ITexture* m_brdfLUTRHI = nullptr;
};

} // namespace render
} // namespace eyelib
