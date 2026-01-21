#pragma once

#include "FluidCommon.h"
#include <memory>
#include <glad/glad.h>

namespace OpenEye {
namespace Rendering { class Shader; }
namespace Fluids {

class FluidSimulation {
public:
	FluidSimulation();
	~FluidSimulation();

	bool Initialize(const FluidGridDesc& desc, FluidType type);
	void Shutdown();

	void Step(float deltaTime);
	void InjectDensity(const glm::vec3& positionNormalized, float radiusNormalized, float amount);
	void InjectLevelSet(const glm::vec3& positionNormalized, float radiusNormalized, float levelSetValue);
	void InjectHalfFilledLevelSet(float waterLevelNormalized); // Creates half-filled pool (negative below water level)
	void InjectVelocity(const glm::vec3& positionNormalized, float radiusNormalized, const glm::vec3& velocity);
	void InjectSineWaveVelocity(float time, float waveSpeed, float waveAmplitude, float waveFrequency, float randomNoise);

	// Debug/controls
	int  GetJacobiIterations() const { return m_jacobiIterations; }
	void SetJacobiIterations(int it) { m_jacobiIterations = it; }
	float GetDensityDissipation() const { return m_densityDissipation; }
	float GetVelocityDissipation() const { return m_velocityDissipation; }
	void SetDissipation(float density, float velocity) { m_densityDissipation = density; m_velocityDissipation = velocity; }
	void GetBuoyancy(float& sigma, float& kappa, float& ambient) const { sigma = m_sigma; kappa = m_kappa; ambient = m_ambientTemperature; }
	void SetBuoyancy(float sigma, float kappa, float ambient) { m_sigma = sigma; m_kappa = kappa; m_ambientTemperature = ambient; }
	void GetEmission(glm::vec3& pos01, float& radius01, float& densityRate, float& tempRate, bool& enabled) const { pos01 = m_emitPos; radius01 = m_emitRadius; densityRate = m_emitDensityRate; tempRate = m_emitTempRate; enabled = m_emitEnabled; }
	void SetEmission(const glm::vec3& pos01, float radius01, float densityRate, float tempRate, bool enabled) { m_emitPos = pos01; m_emitRadius = radius01; m_emitDensityRate = densityRate; m_emitTempRate = tempRate; m_emitEnabled = enabled; }
	void GetWind(glm::vec3& direction, float& strength, bool& enabled) const { direction = m_windDirection; strength = m_windStrength; enabled = m_windEnabled; }
	void SetWind(const glm::vec3& direction, float strength, bool enabled) { m_windDirection = glm::normalize(direction); m_windStrength = strength; m_windEnabled = enabled; }
	void SetWaves(float speed, float amplitude, float frequency, float randomNoise, bool enabled) {
		m_waveSpeed = speed; m_waveAmplitude = amplitude; m_waveFrequency = frequency; 
		m_waveRandomNoise = randomNoise; m_waveEnabled = enabled; m_waveTime = 0.0f;
	}
	void ClearDensity();

	// Accessors used by renderer
	GLuint GetDensityTexture() const { return m_density.tex; }
	GLuint GetObstacleTexture() const { return m_obstacles.tex; }
	glm::ivec3 GetResolution() const { return m_desc.resolution; }
	glm::vec3  GetVolumeSize() const { return m_desc.volumeSize; }
	FluidType  GetType() const { return m_type; }

private:
	struct Slab {
		GLuint fbo = 0;
		GLuint tex = 0;
		GLenum baseFormat = GL_RED; // GL_RED or GL_RGBA
	};

	FluidGridDesc m_desc{};
	FluidType      m_type{FluidType::Gas};

	// Core fields
	Slab m_velocity{};        // RGBA16F (xyz = velocity, w unused)
	Slab m_velocityTmp{};     // RGBA16F
	Slab m_density{};         // R16F   (density or level-set)
	Slab m_densityTmp{};      // R16F
	Slab m_pressure{};        // R16F
	Slab m_pressureTmp{};     // R16F
	Slab m_divergence{};      // R16F
	Slab m_obstacles{};       // R8 (0 fluid, 1 solid)
	Slab m_obstacleVelocity{};// RGBA16F
	Slab m_temperature{};     // R16F (gas)
	Slab m_temperatureTmp{};  // R16F (gas) ping-pong (avoid read/write same texture)
	Slab m_phi1Hat{};         // RGBA16F (MacCormack tmp)
	Slab m_phi2Hat{};         // RGBA16F (MacCormack tmp)

	// Shaders
	std::shared_ptr<Rendering::Shader> m_shAdvect;
	std::shared_ptr<Rendering::Shader> m_shMacCormack;
	std::shared_ptr<Rendering::Shader> m_shDivergence;
	std::shared_ptr<Rendering::Shader> m_shJacobi;
	std::shared_ptr<Rendering::Shader> m_shProjection;
	std::shared_ptr<Rendering::Shader> m_shBuoyancy;
	std::shared_ptr<Rendering::Shader> m_shAddDye;
	std::shared_ptr<Rendering::Shader> m_shAddTemperature;
	std::shared_ptr<Rendering::Shader> m_shApplyForce;

	// Draw resources for layered rendering
	GLuint m_fullscreenVAO = 0, m_fullscreenVBO = 0;

	// Params
	int   m_jacobiIterations = 30;
	float m_densityDissipation = 0.995f; // Slightly faster dissipation for clearer smoke
	float m_velocityDissipation = 0.99f;  // Allow velocity to decay slightly
	float m_sigma = 1.5f; // Higher buoyancy for more visible rising
	float m_kappa = 0.03f; // Lower smoke weight for better rising effect
	float m_ambientTemperature = 0.0f;

	// Emission params (normalized to [0,1] texture space)
	glm::vec3 m_emitPos = glm::vec3(0.5f, 0.15f, 0.5f);
	float m_emitRadius = 0.05f;
	float m_emitDensityRate = 0.8f;
	float m_emitTempRate = 1.5f;
	bool  m_emitEnabled = true;
	
	// Wind/External force params
	glm::vec3 m_windDirection = glm::vec3(0.0f, 0.0f, 0.0f); // Wind direction (normalized)
	float m_windStrength = 0.0f; // Wind strength (0 = no wind)
	bool  m_windEnabled = false;
	
	// Wave params (for liquid)
	float m_waveSpeed = 0.0f;
	float m_waveAmplitude = 0.0f;
	float m_waveFrequency = 0.0f;
	float m_waveRandomNoise = 0.0f;
	bool  m_waveEnabled = false;
	float m_waveTime = 0.0f;

	// Helpers
	void EnsureSimShaders();
	void CreateFullscreenQuad();
	void DestroyFullscreenQuad();
	void BindLayeredTarget(const Slab& target);
	void DrawFullscreenInstanced() const;
	void SwapSlabs(Slab& a, Slab& b);

	bool Create3DSlab(Slab& slab, GLint internalFormat, GLsizei w, GLsizei h, GLsizei d);
	void DestroySlab(Slab& slab);
	void Clear3D(Slab& slab, float value = 0.0f);

	// Minimal pipeline (no obstacles yet)
	void AdvectVelocity(float dt);
	void AdvectScalar(Slab& source, Slab& dest, float dt, float dissipation);
	void ComputeDivergence();
	void SolvePressure();
	void ProjectVelocity();
	void ApplyBuoyancy(float deltaTime);
	void ApplyWindForce(float deltaTime);
	void ApplyGravity(float deltaTime);
};

} // namespace Fluids
} // namespace OpenEye


