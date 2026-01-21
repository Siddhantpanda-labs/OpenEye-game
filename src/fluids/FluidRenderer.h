#pragma once

#include "FluidCommon.h"
#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace OpenEye {
namespace Rendering { class Camera; class Shader; }
namespace Fluids {
class FluidSimulation;

class FluidRenderer {
public:
	FluidRenderer();
	~FluidRenderer();

	bool Initialize();
	void Shutdown();

	void Render(const FluidSimulation& sim,
	           const Rendering::Camera& camera,
	           const glm::mat4& view,
	           const glm::mat4& proj,
	           const glm::mat4& model);

private:
	// Fullscreen and cube geometry
	GLuint m_cubeVAO = 0, m_cubeVBO = 0;

	// RayData FBOs/textures
	GLuint m_rayDataFrontTex = 0, m_rayDataBackTex = 0;
	GLuint m_rayDataFrontFBO = 0, m_rayDataBackFBO = 0;

	int m_viewportW = 0, m_viewportH = 0;

    std::shared_ptr<Rendering::Shader> m_shRayDataBack;
    std::shared_ptr<Rendering::Shader> m_shRayDataFront;
    std::shared_ptr<Rendering::Shader> m_shRaymarch;
    std::shared_ptr<Rendering::Shader> m_shWireframe; // Debug wireframe shader

    // Wireframe box geometry
    GLuint m_wireframeVAO = 0, m_wireframeVBO = 0;

    void EnsureRayDataTargets();
    void EnsureShaders();
	void CreateCube();
    void CreateWireframeBox();
    void RenderWireframeBox(const glm::mat4& view, const glm::mat4& proj, const glm::mat4& model);
};

} // namespace Fluids
} // namespace OpenEye


