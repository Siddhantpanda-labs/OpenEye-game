#pragma once

#include <glad/glad.h>
#include <memory>
#include <glm/glm.hpp>

namespace OpenEye {
namespace Rendering {

class Shader;
class Camera;
class FullscreenQuad;
class Cubemap;

class SkyboxRenderer {
public:
    SkyboxRenderer();
    ~SkyboxRenderer();

    bool Initialize(uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);

    // Renders the cubemap to the offscreen color texture
    void Render(const Camera& camera, const Cubemap& cubemap);

    GLuint GetColorTexture() const { return m_colorTex; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

private:
    void CreateTargets();

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    GLuint m_fbo = 0;
    GLuint m_colorTex = 0;
    std::shared_ptr<Shader> m_shader; // skybox shader (fullscreen)
};

} // namespace Rendering
} // namespace OpenEye


