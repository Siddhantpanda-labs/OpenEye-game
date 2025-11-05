#include "FluidSimulation.h"
#include "../rendering/resources/Shader.h"
#include "../core/log/Log.h"
#include <glm/glm.hpp>
#include <vector>

using namespace OpenEye::Core;

namespace OpenEye {
namespace Fluids {

FluidSimulation::FluidSimulation() = default;
FluidSimulation::~FluidSimulation() { Shutdown(); }

bool FluidSimulation::Initialize(const FluidGridDesc& desc, FluidType type) {
	m_desc = desc;
	m_type = type;

	const auto res = m_desc.resolution;
	// Create required 3D textures
    if (!Create3DSlab(m_velocity,         GL_RGBA16F, res.x, res.y, res.z)) return false;
    if (!Create3DSlab(m_velocityTmp,      GL_RGBA16F, res.x, res.y, res.z)) return false;
    if (!Create3DSlab(m_density,          GL_R16F,    res.x, res.y, res.z)) return false;
    if (!Create3DSlab(m_densityTmp,       GL_R16F,    res.x, res.y, res.z)) return false;
    if (!Create3DSlab(m_pressure,         GL_R16F,    res.x, res.y, res.z)) return false;
    if (!Create3DSlab(m_pressureTmp,      GL_R16F,    res.x, res.y, res.z)) return false;
    if (!Create3DSlab(m_divergence,       GL_R16F,    res.x, res.y, res.z)) return false;
    if (!Create3DSlab(m_obstacles,        GL_R8,      res.x, res.y, res.z)) return false;
    if (!Create3DSlab(m_obstacleVelocity, GL_RGBA16F, res.x, res.y, res.z)) return false;
    if (!Create3DSlab(m_temperature,      GL_R16F,    res.x, res.y, res.z)) return false;
    if (!Create3DSlab(m_phi1Hat,          GL_RGBA16F, res.x, res.y, res.z)) return false;
    if (!Create3DSlab(m_phi2Hat,          GL_RGBA16F, res.x, res.y, res.z)) return false;

	Clear3D(m_velocity, 0.0f);
    Clear3D(m_velocityTmp, 0.0f);
	Clear3D(m_density,  0.0f);
    Clear3D(m_densityTmp, 0.0f);
	Clear3D(m_pressure, 0.0f);
    Clear3D(m_pressureTmp, 0.0f);
	Clear3D(m_divergence, 0.0f);
	Clear3D(m_obstacles, 0.0f);
    Clear3D(m_obstacleVelocity, 0.0f);
    Clear3D(m_temperature, 0.0f);
    Clear3D(m_phi1Hat, 0.0f);
    Clear3D(m_phi2Hat, 0.0f);

    CreateFullscreenQuad();
    EnsureSimShaders();
	OE_INFO_F("FluidSimulation initialized: %dx%dx%d", res.x, res.y, res.z);
	return true;
}

void FluidSimulation::Shutdown() {
	DestroySlab(m_velocity);
    DestroySlab(m_velocityTmp);
	DestroySlab(m_density);
    DestroySlab(m_densityTmp);
	DestroySlab(m_pressure);
    DestroySlab(m_pressureTmp);
	DestroySlab(m_divergence);
	DestroySlab(m_obstacles);
    DestroySlab(m_obstacleVelocity);
    DestroySlab(m_temperature);
    DestroySlab(m_phi1Hat);
    DestroySlab(m_phi2Hat);
    DestroyFullscreenQuad();
}

void FluidSimulation::Step(float deltaTime) {
    EnsureSimShaders();
    const glm::vec3 invSize = 1.0f / glm::vec3(m_desc.resolution);

    // Emission (gas only), controlled by parameters
    if (m_type == FluidType::Gas && m_emitEnabled) {
        BindLayeredTarget(m_density);
        // Don't clear - we want to accumulate density over time
        m_shAddDye->Bind();
        m_shAddDye->SetInt("DensityTexture", 0);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_density.tex);
        m_shAddDye->SetVec3("emitterPos", m_emitPos);
        m_shAddDye->SetFloat("emitterRadius", m_emitRadius);
        m_shAddDye->SetFloat("densityValue", m_emitDensityRate * deltaTime);
        m_shAddDye->SetVec3("InverseSize", invSize);
        DrawFullscreenInstanced();
        m_shAddDye->Unbind();

        BindLayeredTarget(m_temperature);
        // Don't clear - accumulate temperature over time
        m_shAddTemperature->Bind();
        m_shAddTemperature->SetInt("TemperatureTexture", 0);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_temperature.tex);
        m_shAddTemperature->SetVec3("emitterPos", m_emitPos);
        m_shAddTemperature->SetFloat("emitterRadius", m_emitRadius);
        m_shAddTemperature->SetFloat("temperatureValue", m_emitTempRate * deltaTime);
        m_shAddTemperature->SetVec3("InverseSize", invSize);
        DrawFullscreenInstanced();
        m_shAddTemperature->Unbind();
    }

    // Advect velocity (MacCormack)
    AdvectVelocity(deltaTime);

    // Apply wind force (before pressure projection for natural flow)
    if (m_windEnabled && m_windStrength > 0.0f) {
        ApplyWindForce(deltaTime);
    }

    // Incompressibility: divergence -> pressure -> projection
    ComputeDivergence();
    SolvePressure();
    ProjectVelocity();

    // Buoyancy for gas (after pressure projection)
    if (m_type == FluidType::Gas) {
        ApplyBuoyancy(deltaTime);
    }

    // Advect scalars
    AdvectScalar(m_density, m_densityTmp, deltaTime, m_densityDissipation);
    std::swap(m_density, m_densityTmp);
    if (m_type == FluidType::Gas) {
        AdvectScalar(m_temperature, m_temperature, deltaTime, 1.0f);
    }
}

void FluidSimulation::InjectDensity(const glm::vec3& /*positionWS*/, float /*radiusWS*/, float /*amount*/) {
    // Simple CPU-side injection at center for initial visualization
    const glm::ivec3 res = m_desc.resolution;
    std::vector<float> data(res.x * res.y * res.z, 0.0f);
    glm::vec3 center = glm::vec3(res) * 0.5f;
    float radius = 0.15f * res.x;
    for (int z = 0; z < res.z; ++z) {
        for (int y = 0; y < res.y; ++y) {
            for (int x = 0; x < res.x; ++x) {
                glm::vec3 p(x, y, z);
                float d = glm::length(p - center);
                float w = expf(-(d * d) / (2.0f * radius * radius));
                data[x + y * res.x + z * res.x * res.y] = w;
            }
        }
    }
    glBindTexture(GL_TEXTURE_3D, m_density.tex);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, res.x, res.y, res.z, GL_RED, GL_FLOAT, data.data());
    glBindTexture(GL_TEXTURE_3D, 0);
}

void FluidSimulation::ClearDensity() {
    Clear3D(m_density, 0.0f);
}

bool FluidSimulation::Create3DSlab(Slab& slab, GLint internalFormat, GLsizei w, GLsizei h, GLsizei d) {
	glGenTextures(1, &slab.tex);
	glBindTexture(GL_TEXTURE_3D, slab.tex);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    GLenum baseFormat = (internalFormat == GL_RGBA16F ? GL_RGBA : GL_RED);
    slab.baseFormat = baseFormat;
    glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, w, h, d, 0, baseFormat, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_3D, 0);

	glGenFramebuffers(1, &slab.fbo);
	return true;
}

void FluidSimulation::DestroySlab(Slab& slab) {
	if (slab.fbo) { glDeleteFramebuffers(1, &slab.fbo); slab.fbo = 0; }
	if (slab.tex) { glDeleteTextures(1, &slab.tex); slab.tex = 0; }
}

void FluidSimulation::Clear3D(Slab& slab, float value) {
	glBindTexture(GL_TEXTURE_3D, slab.tex);
	float clearValue[4] = { value, value, value, value };
    glClearTexImage(slab.tex, 0, slab.baseFormat, GL_FLOAT, clearValue);
	glBindTexture(GL_TEXTURE_3D, 0);
}

void FluidSimulation::EnsureSimShaders() {
    if (!m_shAdvect) {
        m_shAdvect = std::make_shared<Rendering::Shader>();
        m_shAdvect->LoadFromFile(
            "assets/shaders/fluids/generic/load_vertices.vert",
            "assets/shaders/fluids/generic/set_layer.geom",
            "assets/shaders/fluids/simulation/advection.frag");
    }
    if (!m_shMacCormack) {
        m_shMacCormack = std::make_shared<Rendering::Shader>();
        m_shMacCormack->LoadFromFile(
            "assets/shaders/fluids/generic/load_vertices.vert",
            "assets/shaders/fluids/generic/set_layer.geom",
            "assets/shaders/fluids/simulation/macCormack_advection.frag");
    }
    if (!m_shDivergence) {
        m_shDivergence = std::make_shared<Rendering::Shader>();
        m_shDivergence->LoadFromFile(
            "assets/shaders/fluids/generic/load_vertices.vert",
            "assets/shaders/fluids/generic/set_layer.geom",
            "assets/shaders/fluids/simulation/divergence.frag");
    }
    if (!m_shJacobi) {
        m_shJacobi = std::make_shared<Rendering::Shader>();
        m_shJacobi->LoadFromFile(
            "assets/shaders/fluids/generic/load_vertices.vert",
            "assets/shaders/fluids/generic/set_layer.geom",
            "assets/shaders/fluids/simulation/jacobi_pressure.frag");
    }
    if (!m_shProjection) {
        m_shProjection = std::make_shared<Rendering::Shader>();
        m_shProjection->LoadFromFile(
            "assets/shaders/fluids/generic/load_vertices.vert",
            "assets/shaders/fluids/generic/set_layer.geom",
            "assets/shaders/fluids/simulation/pressure_projection.frag");
    }
    if (!m_shBuoyancy) {
        m_shBuoyancy = std::make_shared<Rendering::Shader>();
        m_shBuoyancy->LoadFromFile(
            "assets/shaders/fluids/generic/load_vertices.vert",
            "assets/shaders/fluids/generic/set_layer.geom",
            "assets/shaders/fluids/simulation/gas/buoyancy.frag");
    }
    if (!m_shAddDye) {
        m_shAddDye = std::make_shared<Rendering::Shader>();
        m_shAddDye->LoadFromFile(
            "assets/shaders/fluids/generic/load_vertices.vert",
            "assets/shaders/fluids/generic/set_layer.geom",
            "assets/shaders/fluids/simulation/add_dye.frag");
    }
    if (!m_shAddTemperature) {
        m_shAddTemperature = std::make_shared<Rendering::Shader>();
        m_shAddTemperature->LoadFromFile(
            "assets/shaders/fluids/generic/load_vertices.vert",
            "assets/shaders/fluids/generic/set_layer.geom",
            "assets/shaders/fluids/simulation/gas/add_temperature.frag");
    }
}

void FluidSimulation::CreateFullscreenQuad() {
    if (m_fullscreenVAO)
        return;
    // Use triangle strip with 4 vertices (like 3d-fluid-simulation)
    static const short positions[] = {
        -1, -1,  // Bottom-left
         1, -1,  // Bottom-right
        -1,  1,  // Top-left
         1,  1   // Top-right
    };
    glGenVertexArrays(1, &m_fullscreenVAO);
    glGenBuffers(1, &m_fullscreenVBO);
    glBindVertexArray(m_fullscreenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_fullscreenVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_SHORT, GL_FALSE, 2 * sizeof(short), (void*)0);
    glBindVertexArray(0);
}

void FluidSimulation::DestroyFullscreenQuad() {
    if (m_fullscreenVBO) { glDeleteBuffers(1, &m_fullscreenVBO); m_fullscreenVBO = 0; }
    if (m_fullscreenVAO) { glDeleteVertexArrays(1, &m_fullscreenVAO); m_fullscreenVAO = 0; }
}

void FluidSimulation::BindLayeredTarget(const Slab& target) {
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target.tex, 0);
    GLenum db = GL_COLOR_ATTACHMENT0; glDrawBuffers(1, &db);
    glViewport(0, 0, m_desc.resolution.x, m_desc.resolution.y);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        OE_ERROR_F("FluidSimulation: Layered FBO incomplete! Status: 0x%04X", status);
        // Check specific error
        if (status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) OE_ERROR("  -> Incomplete attachment");
        else if (status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT) OE_ERROR("  -> Missing attachment");
        else if (status == GL_FRAMEBUFFER_UNSUPPORTED) OE_ERROR("  -> Unsupported format");
    }
}

void FluidSimulation::DrawFullscreenInstanced() const {
    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthEnabled) glDisable(GL_DEPTH_TEST);
    glBindVertexArray(m_fullscreenVAO);
    // Use GL_TRIANGLE_STRIP with 4 vertices (like 3d-fluid-simulation)
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, m_desc.resolution.z);
    glBindVertexArray(0);
    if (depthEnabled) glEnable(GL_DEPTH_TEST);
}

void FluidSimulation::SwapSlabs(Slab& a, Slab& b) {
    std::swap(a.tex, b.tex);
    std::swap(a.fbo, b.fbo);
    std::swap(a.baseFormat, b.baseFormat);
}

void FluidSimulation::AdvectVelocity(float dt) {
    const glm::vec3 invSize = 1.0f / glm::vec3(m_desc.resolution);
    // Step 1: phi1_hat = advect(velocity)
    BindLayeredTarget(m_phi1Hat);
    glClear(GL_COLOR_BUFFER_BIT);
    m_shAdvect->Bind();
    m_shAdvect->SetInt("VelocityTexture", 0);
    m_shAdvect->SetInt("SourceTexture", 1);
    m_shAdvect->SetInt("ObstacleTexture", 2);
    m_shAdvect->SetFloat("timeStep", dt);
    m_shAdvect->SetVec3("InverseSize", invSize);
    m_shAdvect->SetFloat("dissipation", m_velocityDissipation);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_velocity.tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D, m_velocity.tex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, m_obstacles.tex);
    DrawFullscreenInstanced();
    m_shAdvect->Unbind();

    // Step 2: phi2_hat = advect(phi1_hat, -dt)
    BindLayeredTarget(m_phi2Hat);
    glClear(GL_COLOR_BUFFER_BIT);
    m_shAdvect->Bind();
    m_shAdvect->SetInt("VelocityTexture", 0);
    m_shAdvect->SetInt("SourceTexture", 1);
    m_shAdvect->SetInt("ObstacleTexture", 2);
    m_shAdvect->SetFloat("timeStep", -dt);
    m_shAdvect->SetVec3("InverseSize", invSize);
    m_shAdvect->SetFloat("dissipation", 1.0f);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_velocity.tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D, m_phi1Hat.tex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, m_obstacles.tex);
    DrawFullscreenInstanced();
    m_shAdvect->Unbind();

    // Step 3: final = MacCormack(velocity, phi1_hat, phi2_hat)
    BindLayeredTarget(m_velocityTmp);
    glClear(GL_COLOR_BUFFER_BIT);
    m_shMacCormack->Bind();
    m_shMacCormack->SetInt("VelocityTexture", 0);
    m_shMacCormack->SetInt("SourceTexture", 1);
    m_shMacCormack->SetInt("Phi1HatTexture", 2);
    m_shMacCormack->SetInt("Phi2HatTexture", 3);
    m_shMacCormack->SetInt("ObstacleTexture", 4);
    m_shMacCormack->SetFloat("timeStep", dt);
    m_shMacCormack->SetVec3("InverseSize", invSize);
    m_shMacCormack->SetFloat("dissipation", m_velocityDissipation);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_velocity.tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D, m_velocity.tex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, m_phi1Hat.tex);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_3D, m_phi2Hat.tex);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_3D, m_obstacles.tex);
    DrawFullscreenInstanced();
    m_shMacCormack->Unbind();

    SwapSlabs(m_velocity, m_velocityTmp);
}

void FluidSimulation::AdvectScalar(Slab& source, Slab& dest, float dt, float dissipation) {
    const glm::vec3 invSize = 1.0f / glm::vec3(m_desc.resolution);
    // phi1_hat = advect(source, dt)
    BindLayeredTarget(m_phi1Hat);
    glClear(GL_COLOR_BUFFER_BIT);
    m_shAdvect->Bind();
    m_shAdvect->SetInt("VelocityTexture", 0);
    m_shAdvect->SetInt("SourceTexture", 1);
    m_shAdvect->SetInt("ObstacleTexture", 2);
    m_shAdvect->SetFloat("timeStep", dt);
    m_shAdvect->SetVec3("InverseSize", invSize);
    m_shAdvect->SetFloat("dissipation", dissipation);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_velocity.tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D, source.tex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, m_obstacles.tex);
    DrawFullscreenInstanced();
    m_shAdvect->Unbind();

    // phi2_hat = advect(phi1_hat, -dt)
    BindLayeredTarget(m_phi2Hat);
    glClear(GL_COLOR_BUFFER_BIT);
    m_shAdvect->Bind();
    m_shAdvect->SetInt("VelocityTexture", 0);
    m_shAdvect->SetInt("SourceTexture", 1);
    m_shAdvect->SetInt("ObstacleTexture", 2);
    m_shAdvect->SetFloat("timeStep", -dt);
    m_shAdvect->SetVec3("InverseSize", invSize);
    m_shAdvect->SetFloat("dissipation", 1.0f);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_velocity.tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D, m_phi1Hat.tex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, m_obstacles.tex);
    DrawFullscreenInstanced();
    m_shAdvect->Unbind();

    // final = MacCormack(source, phi1_hat, phi2_hat)
    BindLayeredTarget(dest);
    glClear(GL_COLOR_BUFFER_BIT);
    m_shMacCormack->Bind();
    m_shMacCormack->SetInt("VelocityTexture", 0);
    m_shMacCormack->SetInt("SourceTexture", 1);
    m_shMacCormack->SetInt("Phi1HatTexture", 2);
    m_shMacCormack->SetInt("Phi2HatTexture", 3);
    m_shMacCormack->SetInt("ObstacleTexture", 4);
    m_shMacCormack->SetFloat("timeStep", dt);
    m_shMacCormack->SetVec3("InverseSize", invSize);
    m_shMacCormack->SetFloat("dissipation", dissipation);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_velocity.tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D, source.tex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, m_phi1Hat.tex);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_3D, m_phi2Hat.tex);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_3D, m_obstacles.tex);
    DrawFullscreenInstanced();
    m_shMacCormack->Unbind();
}

void FluidSimulation::ComputeDivergence() {
    const glm::vec3 invSize = 1.0f / glm::vec3(m_desc.resolution);
    BindLayeredTarget(m_divergence);
    glClear(GL_COLOR_BUFFER_BIT);
    m_shDivergence->Bind();
    m_shDivergence->SetInt("VelocityTexture", 0);
    m_shDivergence->SetInt("ObstacleTexture", 1);
    m_shDivergence->SetInt("ObstacleVelocityTexture", 2);
    m_shDivergence->SetVec3("InverseSize", invSize);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_velocity.tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D, m_obstacles.tex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, m_obstacleVelocity.tex);
    DrawFullscreenInstanced();
    m_shDivergence->Unbind();
}

void FluidSimulation::SolvePressure() {
    const glm::vec3 invSize = 1.0f / glm::vec3(m_desc.resolution);
    for (int i = 0; i < m_jacobiIterations; ++i) {
        BindLayeredTarget(m_pressureTmp);
        glClear(GL_COLOR_BUFFER_BIT);
        m_shJacobi->Bind();
        m_shJacobi->SetInt("Divergence", 0);
        m_shJacobi->SetInt("Pressure", 1);
        m_shJacobi->SetInt("Obstacle", 2);
        m_shJacobi->SetVec3("InverseSize", invSize);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_divergence.tex);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D, m_pressure.tex);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, m_obstacles.tex);
        DrawFullscreenInstanced();
        m_shJacobi->Unbind();
        SwapSlabs(m_pressure, m_pressureTmp);
    }
}

void FluidSimulation::ProjectVelocity() {
    const glm::vec3 invSize = 1.0f / glm::vec3(m_desc.resolution);
    BindLayeredTarget(m_velocityTmp);
    glClear(GL_COLOR_BUFFER_BIT);
    m_shProjection->Bind();
    m_shProjection->SetInt("VelocityTexture", 0);
    m_shProjection->SetInt("PressureTexture", 1);
    m_shProjection->SetInt("ObstacleTexture", 2);
    m_shProjection->SetInt("ObstacleVelocityTexture", 3);
    m_shProjection->SetVec3("InverseSize", invSize);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_velocity.tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D, m_pressure.tex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, m_obstacles.tex);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_3D, m_obstacleVelocity.tex);
    DrawFullscreenInstanced();
    m_shProjection->Unbind();
    SwapSlabs(m_velocity, m_velocityTmp);
}

void FluidSimulation::ApplyBuoyancy(float deltaTime) {
    const glm::vec3 invSize = 1.0f / glm::vec3(m_desc.resolution);
    BindLayeredTarget(m_velocityTmp);
    glClear(GL_COLOR_BUFFER_BIT);
    m_shBuoyancy->Bind();
    m_shBuoyancy->SetInt("VelocityTexture", 0);
    m_shBuoyancy->SetInt("TemperatureTexture", 1);
    m_shBuoyancy->SetInt("DensityTexture", 2);
    m_shBuoyancy->SetFloat("ambientTemperature", m_ambientTemperature);
    m_shBuoyancy->SetFloat("timeStep", deltaTime);
    m_shBuoyancy->SetFloat("sigma", m_sigma);
    m_shBuoyancy->SetFloat("kappa", m_kappa);
    m_shBuoyancy->SetVec3("InverseSize", invSize);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_velocity.tex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_3D, m_temperature.tex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, m_density.tex);
    DrawFullscreenInstanced();
    m_shBuoyancy->Unbind();
    SwapSlabs(m_velocity, m_velocityTmp);
}

void FluidSimulation::ApplyWindForce(float deltaTime) {
    const glm::vec3 invSize = 1.0f / glm::vec3(m_desc.resolution);
    BindLayeredTarget(m_velocityTmp);
    glClear(GL_COLOR_BUFFER_BIT);
    m_shApplyForce->Bind();
    m_shApplyForce->SetInt("VelocityTexture", 0);
    m_shApplyForce->SetVec3("force", m_windDirection * m_windStrength * deltaTime);
    m_shApplyForce->SetVec3("center", m_emitPos); // Apply wind from emission point
    m_shApplyForce->SetFloat("radius", m_emitRadius * 2.0f); // Wider radius for wind
    m_shApplyForce->SetVec3("InverseSize", invSize);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_3D, m_velocity.tex);
    DrawFullscreenInstanced();
    m_shApplyForce->Unbind();
    SwapSlabs(m_velocity, m_velocityTmp);
}

} // namespace Fluids
} // namespace OpenEye


