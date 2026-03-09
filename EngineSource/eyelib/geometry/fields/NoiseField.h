#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>
#include <functional>

namespace eyelib {
namespace geometry {

/**
 * @brief Noise types for procedural generation.
 */
enum class NoiseType {
    Perlin,     // Classic Perlin noise
    Simplex,    // Simplex noise (smoother)
    FBM,        // Fractional Brownian Motion (multi-octave)
    Value       // Value noise
};

/**
 * @brief NoiseField: Procedural noise generation.
 * 
 * Deterministic noise functions for procedural mesh operations.
 * All noise is seeded for reproducibility.
 */
class NoiseField {
public:
    /**
     * @brief Create a noise field function.
     * 
     * @param type Noise type
     * @param scale Noise scale (larger = smoother)
     * @param seed Random seed (for determinism)
     * @param octaves Number of octaves (for FBM, 1 for others)
     * @param persistence Persistence between octaves (for FBM)
     * @return Function that evaluates noise at a position
     */
    static std::function<float(const math::Vec3&)> Create(NoiseType type, float scale = 1.0f,
                                                         uint32_t seed = 0, int octaves = 1,
                                                         float persistence = 0.5f);
    
    /**
     * @brief Evaluate Perlin noise at a position.
     */
    static float Perlin(const math::Vec3& pos, float scale, uint32_t seed);
    
    /**
     * @brief Evaluate FBM noise at a position.
     */
    static float FBM(const math::Vec3& pos, float scale, uint32_t seed, int octaves, float persistence);
    
    /**
     * @brief Evaluate simple value noise at a position.
     */
    static float Value(const math::Vec3& pos, float scale, uint32_t seed);

private:
    // Hash function for deterministic pseudo-random
    static uint32_t Hash(uint32_t x, uint32_t y, uint32_t z, uint32_t seed);
    static float HashFloat(uint32_t x, uint32_t y, uint32_t z, uint32_t seed);
    
    // Interpolation helpers
    static float Lerp(float a, float b, float t);
    static float Smoothstep(float t);
    
    // Gradient helpers for Perlin noise
    static math::Vec3 Gradient(int hash);
    static float DotGradient(int hash, float x, float y, float z);
};

} // namespace geometry
} // namespace eyelib
