#pragma once

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

    /**
     * @brief Initialize IBL from environment cubemap texture ID (typically skybox).
     * @param environmentCubemapID OpenGL cubemap texture ID
     * @param irradianceRes Resolution of irradiance map (default 32)
     * @param prefilterRes Resolution of prefiltered map (default 512)
     */
    bool Initialize(unsigned int environmentCubemapID, uint32_t irradianceRes = 32, uint32_t prefilterRes = 512);

    /**
     * @brief Get generated textures.
     */
    unsigned int GetIrradianceMap() const { return m_irradianceMap; }
    unsigned int GetPrefilterMap() const { return m_prefilterMap; }
    unsigned int GetBRDFLUT() const { return m_brdfLUT; }

    /**
     * @brief Bind textures for lighting pass.
     * @param irradianceUnit Texture unit for irradiance map (default 11)
     * @param prefilterUnit Texture unit for prefiltered map (default 12)
     * @param brdfLUTUnit Texture unit for BRDF LUT (default 13)
     */
    void BindForLighting(unsigned int irradianceUnit = 11, unsigned int prefilterUnit = 12, unsigned int brdfLUTUnit = 13) const;

    bool IsValid() const { return m_irradianceMap != 0 && m_prefilterMap != 0 && m_brdfLUT != 0; }

private:
    void GenerateIrradianceMap(unsigned int environmentCubemapID, uint32_t resolution);
    void GeneratePrefilterMap(unsigned int environmentCubemapID, uint32_t resolution);
    void GenerateBRDFLUT(uint32_t resolution = 512);

    std::shared_ptr<Shader> CreateIrradianceShader();
    std::shared_ptr<Shader> CreatePrefilterShader();
    std::shared_ptr<Shader> CreateBRDFShader();
    std::shared_ptr<Mesh> CreateCubeMesh();

    unsigned int m_irradianceMap = 0;
    unsigned int m_prefilterMap = 0;
    unsigned int m_brdfLUT = 0;
    unsigned int m_captureFBO = 0;
    unsigned int m_captureRBO = 0;
    std::shared_ptr<Mesh> m_cubeMesh;
};

} // namespace render
} // namespace eyelib
