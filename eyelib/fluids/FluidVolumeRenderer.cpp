#include "eyelib/fluids/FluidVolumeRenderer.h"

#include "eyelib/core/Log.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <vector>

namespace eyelib {
namespace fluids {

FluidVolumeRenderer::FluidVolumeRenderer() = default;
FluidVolumeRenderer::~FluidVolumeRenderer() { Shutdown(); }

bool FluidVolumeRenderer::Initialize() {
    CreateCube();
    EnsureShaders();
    if (!m_backgroundTexture) {
        m_backgroundTexture = std::make_shared<render::Texture2D>();
        const uint8_t black[4] = { 0, 0, 0, 255 };
        m_backgroundTexture->Create(1, 1, render::TextureFormat::RGBA8, black);
    }
    core::Log::Info("FluidVolumeRenderer initialized");
    return true;
}

void FluidVolumeRenderer::Shutdown() {
    if (m_cubeVBO) { glDeleteBuffers(1, &m_cubeVBO); m_cubeVBO = 0; }
    if (m_cubeVAO) { glDeleteVertexArrays(1, &m_cubeVAO); m_cubeVAO = 0; }
    if (m_rayDataFrontTex) { glDeleteTextures(1, &m_rayDataFrontTex); m_rayDataFrontTex = 0; }
    if (m_rayDataBackTex) { glDeleteTextures(1, &m_rayDataBackTex); m_rayDataBackTex = 0; }
    if (m_rayDataFrontFBO) { glDeleteFramebuffers(1, &m_rayDataFrontFBO); m_rayDataFrontFBO = 0; }
    if (m_rayDataBackFBO) { glDeleteFramebuffers(1, &m_rayDataBackFBO); m_rayDataBackFBO = 0; }
    m_densityTexture.reset();
    m_obstacleTexture.reset();
    m_backgroundTexture.reset();
}

void FluidVolumeRenderer::UpdateFromSimulation(const FluidSimulation& sim, float densityScale) {
    if (!sim.IsInitialized()) return;
    const auto desc = sim.GetDesc();
    const math::IVec3 res = desc.resolution;
    if (res.x <= 0 || res.y <= 0 || res.z <= 0) return;

    if (!m_densityTexture || !m_obstacleTexture ||
        m_cachedResolution.x != res.x || m_cachedResolution.y != res.y || m_cachedResolution.z != res.z) {
        m_densityTexture = std::make_shared<render::Texture3D>();
        m_obstacleTexture = std::make_shared<render::Texture3D>();
        m_densityTexture->Create(res.x, res.y, res.z, render::TextureFormat::R8);
        m_obstacleTexture->Create(res.x, res.y, res.z, render::TextureFormat::R8);
        m_cachedResolution = res;
    }

    const auto& density = sim.GetDensityField();
    const auto& obstacles = sim.GetObstacleMask();
    const size_t count = density.size();
    if (count == 0 || obstacles.size() != count) return;

    std::vector<uint8_t> densityBytes(count);
    std::vector<uint8_t> obstacleBytes(count);
    const float scale = std::max(0.0001f, densityScale);
    for (size_t i = 0; i < count; ++i) {
        const float d = std::max(0.0f, std::min(1.0f, density[i] * scale));
        densityBytes[i] = static_cast<uint8_t>(d * 255.0f);
        obstacleBytes[i] = obstacles[i] ? 255 : 0;
    }

    m_densityTexture->SetData(densityBytes.data());
    m_obstacleTexture->SetData(obstacleBytes.data());
}

void FluidVolumeRenderer::SetBackgroundTexture(const std::shared_ptr<render::Texture2D>& background) {
    if (background && background->IsValid()) {
        m_backgroundTexture = background;
    }
}

void FluidVolumeRenderer::RenderVolume(const FluidSimulation& sim,
                                       const render::CameraView& camera,
                                       const math::Mat4& model,
                                       const math::Vec3& emissionPos01,
                                       const math::Vec3& volumeSize,
                                       unsigned int targetFbo) {
    if (!sim.IsInitialized()) return;
    EnsureRayDataTargets();
    EnsureShaders();
    if (!m_densityTexture || !m_obstacleTexture) {
        UpdateFromSimulation(sim, 1.0f);
        if (!m_densityTexture || !m_obstacleTexture) return;
    }

    const auto desc = sim.GetDesc();
    const math::IVec3 res = desc.resolution;

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    // Render back faces (exit points)
    glCullFace(GL_FRONT);
    glBindFramebuffer(GL_FRAMEBUFFER, m_rayDataBackFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_shRayDataBack->Bind();
    m_shRayDataBack->SetMat4("model", model);
    m_shRayDataBack->SetMat4("view", camera.view);
    m_shRayDataBack->SetMat4("projection", camera.projection);
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Render front faces (entry points)
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, m_rayDataFrontFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_shRayDataFront->Bind();
    m_shRayDataFront->SetMat4("model", model);
    m_shRayDataFront->SetMat4("view", camera.view);
    m_shRayDataFront->SetMat4("projection", camera.projection);
    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Raymarch to target framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    m_shRaymarch->Bind();
    m_shRaymarch->SetMat4("model", model);
    m_shRaymarch->SetMat4("view", camera.view);
    m_shRaymarch->SetMat4("projection", camera.projection);
    m_shRaymarch->SetVec3("grid_size", math::Vec3(static_cast<float>(res.x), static_cast<float>(res.y), static_cast<float>(res.z)));
    m_shRaymarch->SetVec3("u_EmissionPos", emissionPos01);
    m_shRaymarch->SetVec3("u_VolumeSize", volumeSize);
    m_shRaymarch->SetInt("u_FluidType", sim.GetType() == FluidType::Liquid ? 1 : 0);

    const math::Mat4 invView = glm::inverse(camera.view);
    const math::Vec3 right = math::Vec3(invView[0]);
    const math::Vec3 up = math::Vec3(invView[1]);
    const math::Vec3 forward = math::Vec3(-invView[2]);

    m_shRaymarch->SetVec3("eyePos", camera.viewPos);
    m_shRaymarch->SetVec3("cameraRight", right);
    m_shRaymarch->SetVec3("cameraUp", up);
    m_shRaymarch->SetVec3("cameraFront", forward);
    m_shRaymarch->SetFloat("nearPlane", camera.nearPlane);
    m_shRaymarch->SetVec2("InverseScreenSize", math::Vec2(1.0f / std::max(1, m_viewportW),
                                                          1.0f / std::max(1, m_viewportH)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_rayDataBackTex);
    m_shRaymarch->SetInt("RayDataBack", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_rayDataFrontTex);
    m_shRaymarch->SetInt("RayDataFront", 1);

    m_densityTexture->Bind(2);
    m_shRaymarch->SetInt("DensityTexture", 2);

    m_obstacleTexture->Bind(3);
    m_shRaymarch->SetInt("ObstacleTexture", 3);

    if (m_backgroundTexture) {
        m_backgroundTexture->Bind(4);
        m_shRaymarch->SetInt("BackgroundTexture", 4);
    }

    glBindVertexArray(m_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FluidVolumeRenderer::EnsureRayDataTargets() {
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    const int w = vp[2];
    const int h = vp[3];
    if (w == m_viewportW && h == m_viewportH && m_rayDataFrontTex && m_rayDataBackTex)
        return;
    m_viewportW = w;
    m_viewportH = h;

    auto createTex = [&](unsigned int& tex) {
        if (tex) glDeleteTextures(1, &tex);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    auto createFBO = [&](unsigned int& fbo, unsigned int tex) {
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

void FluidVolumeRenderer::EnsureShaders() {
    if (!m_shRayDataBack) {
        m_shRayDataBack = std::make_shared<render::Shader>();
        m_shRayDataBack->LoadFromFile("assets/shaders/fluids/rendering/raydata.vert",
                                      "assets/shaders/fluids/rendering/raydata_back.frag");
    }
    if (!m_shRayDataFront) {
        m_shRayDataFront = std::make_shared<render::Shader>();
        m_shRayDataFront->LoadFromFile("assets/shaders/fluids/rendering/raydata.vert",
                                       "assets/shaders/fluids/rendering/raydata_front.frag");
    }
    if (!m_shRaymarch) {
        m_shRaymarch = std::make_shared<render::Shader>();
        m_shRaymarch->LoadFromFile("assets/shaders/fluids/rendering/raydata.vert",
                                   "assets/shaders/fluids/rendering/raymarching.frag");
    }
}

void FluidVolumeRenderer::CreateCube() {
    static const float verts[] = {
        -1.0f,-1.0f,-1.0f,  1.0f,-1.0f,-1.0f,  1.0f, 1.0f,-1.0f,
        -1.0f,-1.0f,-1.0f,  1.0f, 1.0f,-1.0f, -1.0f, 1.0f,-1.0f,
         1.0f,-1.0f, 1.0f, -1.0f,-1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
         1.0f,-1.0f, 1.0f, -1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f,-1.0f,  1.0f, 1.0f,-1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f,-1.0f,  1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,-1.0f, 1.0f,  1.0f,-1.0f, 1.0f,  1.0f,-1.0f,-1.0f,
        -1.0f,-1.0f, 1.0f,  1.0f,-1.0f,-1.0f, -1.0f,-1.0f,-1.0f,
         1.0f,-1.0f,-1.0f,  1.0f,-1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
         1.0f,-1.0f,-1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f,-1.0f,
        -1.0f,-1.0f, 1.0f, -1.0f,-1.0f,-1.0f, -1.0f, 1.0f,-1.0f,
        -1.0f,-1.0f, 1.0f, -1.0f, 1.0f,-1.0f, -1.0f, 1.0f, 1.0f
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

} // namespace fluids
} // namespace eyelib
