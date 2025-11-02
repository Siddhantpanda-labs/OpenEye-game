#pragma once

#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace OpenEye {
namespace Rendering {

class Cubemap;
class Shader;

// Image-Based Lighting system
// Generates: Irradiance map (diffuse IBL), Prefiltered environment map (specular IBL), BRDF LUT
class IBL {
public:
    IBL();
    ~IBL();

    // Initialize IBL from environment cubemap texture ID (typically skybox)
    // resolution: resolution of generated maps (default 32 for irradiance, 512 for prefilter)
    bool Initialize(GLuint environmentCubemapID, uint32_t irradianceRes = 32, uint32_t prefilterRes = 512);

    // Get generated textures
    GLuint GetIrradianceMap() const { return m_irradianceMap; }
    GLuint GetPrefilterMap() const { return m_prefilterMap; }
    GLuint GetBRDFLUT() const { return m_brdfLUT; }

    // Bind textures for lighting pass (texture units 11, 12, 13)
    void BindForLighting(GLuint irradianceUnit = 11, GLuint prefilterUnit = 12, GLuint brdfLUTUnit = 13) const;

    bool IsValid() const { return m_irradianceMap != 0 && m_prefilterMap != 0 && m_brdfLUT != 0; }

private:
    GLuint m_irradianceMap = 0;
    GLuint m_prefilterMap = 0;
    GLuint m_brdfLUT = 0;
    GLuint m_captureFBO = 0;
    GLuint m_captureRBO = 0;

    void GenerateIrradianceMap(GLuint environmentCubemapID, uint32_t resolution);
    void GeneratePrefilterMap(GLuint environmentCubemapID, uint32_t resolution);
    void GenerateBRDFLUT(uint32_t resolution = 512);
    
    std::shared_ptr<Shader> CreateEquirectangularToCubemapShader();
    std::shared_ptr<Shader> CreateIrradianceShader();
    std::shared_ptr<Shader> CreatePrefilterShader();
    std::shared_ptr<Shader> CreateBRDFShader();
};

} // namespace Rendering
} // namespace OpenEye

