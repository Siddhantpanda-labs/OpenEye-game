#include "FluidRenderer.h"
#include "FluidSimulation.h"
#include "../rendering/Camera.h"
#include "../rendering/resources/Shader.h"
#include "../core/log/Log.h"

using namespace OpenEye::Core;

namespace OpenEye {
namespace Fluids {

FluidRenderer::FluidRenderer() = default;
FluidRenderer::~FluidRenderer() { Shutdown(); }

bool FluidRenderer::Initialize() {
	CreateCube();
    OE_INFO("FluidRenderer initialized");
	return true;
}

void FluidRenderer::Shutdown() {
	if (m_cubeVBO) { glDeleteBuffers(1, &m_cubeVBO); m_cubeVBO = 0; }
	if (m_cubeVAO) { glDeleteVertexArrays(1, &m_cubeVAO); m_cubeVAO = 0; }
	if (m_rayDataFrontTex) { glDeleteTextures(1, &m_rayDataFrontTex); m_rayDataFrontTex = 0; }
	if (m_rayDataBackTex) { glDeleteTextures(1, &m_rayDataBackTex); m_rayDataBackTex = 0; }
	if (m_rayDataFrontFBO) { glDeleteFramebuffers(1, &m_rayDataFrontFBO); m_rayDataFrontFBO = 0; }
	if (m_rayDataBackFBO) { glDeleteFramebuffers(1, &m_rayDataBackFBO); m_rayDataBackFBO = 0; }
}

void FluidRenderer::EnsureRayDataTargets() {
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    int w = vp[2];
    int h = vp[3];
    if (w == m_viewportW && h == m_viewportH && m_rayDataFrontTex && m_rayDataBackTex)
        return;
    m_viewportW = w; m_viewportH = h;
    auto createTex = [&](GLuint& tex){
        if (tex) glDeleteTextures(1, &tex);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    auto createFBO = [&](GLuint& fbo, GLuint tex){
        if (fbo) glDeleteFramebuffers(1, &fbo);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, drawBuffers);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    };
    createTex(m_rayDataBackTex);
    createTex(m_rayDataFrontTex);
    createFBO(m_rayDataBackFBO, m_rayDataBackTex);
    createFBO(m_rayDataFrontFBO, m_rayDataFrontTex);
}

void FluidRenderer::EnsureShaders() {
    if (!m_shRayDataBack) {
        m_shRayDataBack = std::make_shared<Rendering::Shader>();
        m_shRayDataBack->LoadFromFile("assets/shaders/fluids/rendering/raydata.vert",
                                      "assets/shaders/fluids/rendering/raydata_back.frag");
    }
    if (!m_shRayDataFront) {
        m_shRayDataFront = std::make_shared<Rendering::Shader>();
        m_shRayDataFront->LoadFromFile("assets/shaders/fluids/rendering/raydata.vert",
                                       "assets/shaders/fluids/rendering/raydata_front.frag");
    }
    if (!m_shRaymarch) {
        m_shRaymarch = std::make_shared<Rendering::Shader>();
        m_shRaymarch->LoadFromFile("assets/shaders/fluids/rendering/raydata.vert",
                                   "assets/shaders/fluids/rendering/raymarching.frag");
    }
}

void FluidRenderer::CreateCube() {
    // Cube in local space [-1, 1]^3 with consistent CCW winding for all faces
    // Each face is front-facing (CCW) when viewed from outside
	static const float verts[] = {
        // Front face (Z = -1, looking from -Z)
        -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        // Back face (Z = 1, looking from +Z)
         1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
        // Top face (Y = 1, looking from +Y)
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,
        // Bottom face (Y = -1, looking from -Y)
        -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,
        // Right face (X = 1, looking from +X)
         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,
        // Left face (X = -1, looking from -X)
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f
	};
	glGenVertexArrays(1, &m_cubeVAO);
	glGenBuffers(1, &m_cubeVBO);
	glBindVertexArray(m_cubeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glBindVertexArray(0);
}

void FluidRenderer::Render(const FluidSimulation& sim,
                             const Rendering::Camera& camera, 
                             const glm::mat4& view, 
                             const glm::mat4& proj, 
                             const glm::mat4& model) {
    EnsureShaders();
    EnsureRayDataTargets();

    // Create RayData Back
    glBindFramebuffer(GL_FRAMEBUFFER, m_rayDataBackFBO);
    glViewport(0, 0, m_viewportW, m_viewportH);
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); // render back faces
    m_shRayDataBack->Bind();
    m_shRayDataBack->SetMat4("model", model);
    m_shRayDataBack->SetMat4("view", view);
    m_shRayDataBack->SetMat4("projection", proj);
    m_shRayDataBack->SetVec3("grid_size", glm::vec3(sim.GetResolution()));
    m_shRayDataBack->SetVec2("InverseSize", glm::vec2(1.0f / m_viewportW, 1.0f / m_viewportH));
    m_shRayDataBack->SetBool("useSceneDepth", false);
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    m_shRayDataBack->Unbind();

    // Create RayData Front
    glBindFramebuffer(GL_FRAMEBUFFER, m_rayDataFrontFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    glCullFace(GL_BACK); // render front faces
    m_shRayDataFront->Bind();
    m_shRayDataFront->SetVec3("grid_size", glm::vec3(sim.GetResolution()));
    m_shRayDataFront->SetVec2("InverseSize", glm::vec2(1.0f / m_viewportW, 1.0f / m_viewportH));
    m_shRayDataFront->SetInt("RayDataTexture", 0);
    m_shRayDataFront->SetBool("useSceneDepth", false);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_rayDataBackTex);
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    m_shRayDataFront->Unbind();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    // Raymarch pass: draw cube front faces, bind DensityTexture from sim
    m_shRaymarch->Bind();
    m_shRaymarch->SetMat4("model", model);
    m_shRaymarch->SetMat4("view", view);
    m_shRaymarch->SetMat4("projection", proj);
    m_shRaymarch->SetVec3("grid_size", glm::vec3(sim.GetResolution()));
    m_shRaymarch->SetVec2("InverseScreenSize", glm::vec2(1.0f / m_viewportW, 1.0f / m_viewportH));
    m_shRaymarch->SetFloat("nearPlane", 0.1f);
    m_shRaymarch->SetVec3("eyePos", camera.GetPosition());
    m_shRaymarch->SetVec3("cameraFront", camera.GetForward());
    m_shRaymarch->SetVec3("cameraUp", camera.GetUp());
    m_shRaymarch->SetVec3("cameraRight", camera.GetRight());
    m_shRaymarch->SetFloat("rugosity", 0.3f);
    m_shRaymarch->SetFloat("F0", 0.02f);
    m_shRaymarch->SetFloat("Kd", 1.0f);
    m_shRaymarch->SetInt("u_FluidType", sim.GetType() == FluidType::Gas ? 0 : 1);
    m_shRaymarch->SetInt("RayDataBack", 0);
    m_shRaymarch->SetInt("RayDataFront", 1);
    m_shRaymarch->SetInt("DensityTexture", 2);
    m_shRaymarch->SetInt("ObstacleTexture", 3);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_rayDataBackTex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_rayDataFrontTex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, sim.GetDensityTexture());
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_3D, sim.GetObstacleTexture());
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    // Enable alpha blending and disable depth test for compositing
    GLboolean depthMask;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthTestEnabled) glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glDepthMask(depthMask);
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    m_shRaymarch->Unbind();
}

} // namespace Fluids
} // namespace OpenEye


