#include "eyelib/geometry/fields/NoiseField.h"
#include <cmath>
#include <algorithm>

namespace eyelib {
namespace geometry {

std::function<float(const math::Vec3&)> NoiseField::Create(NoiseType type, float scale,
                                                           uint32_t seed, int octaves,
                                                           float persistence) {
    switch (type) {
        case NoiseType::Perlin:
            return [scale, seed](const math::Vec3& pos) {
                return Perlin(pos, scale, seed);
            };
        case NoiseType::FBM:
            return [scale, seed, octaves, persistence](const math::Vec3& pos) {
                return FBM(pos, scale, seed, octaves, persistence);
            };
        case NoiseType::Value:
            return [scale, seed](const math::Vec3& pos) {
                return Value(pos, scale, seed);
            };
        case NoiseType::Simplex:
            // Use Perlin for now (Simplex is more complex)
            return [scale, seed](const math::Vec3& pos) {
                return Perlin(pos, scale, seed);
            };
        default:
            return [](const math::Vec3&) { return 0.0f; };
    }
}

float NoiseField::Perlin(const math::Vec3& pos, float scale, uint32_t seed) {
    math::Vec3 scaled = pos * scale;
    
    int x0 = static_cast<int>(std::floor(scaled.x));
    int y0 = static_cast<int>(std::floor(scaled.y));
    int z0 = static_cast<int>(std::floor(scaled.z));
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    int z1 = z0 + 1;
    
    float fx = scaled.x - static_cast<float>(x0);
    float fy = scaled.y - static_cast<float>(y0);
    float fz = scaled.z - static_cast<float>(z0);
    
    // Smooth interpolation
    float u = Smoothstep(fx);
    float v = Smoothstep(fy);
    float w = Smoothstep(fz);
    
    // Get gradients at 8 corners
    float n000 = DotGradient(Hash(x0, y0, z0, seed), fx, fy, fz);
    float n100 = DotGradient(Hash(x1, y0, z0, seed), fx - 1.0f, fy, fz);
    float n010 = DotGradient(Hash(x0, y1, z0, seed), fx, fy - 1.0f, fz);
    float n110 = DotGradient(Hash(x1, y1, z0, seed), fx - 1.0f, fy - 1.0f, fz);
    float n001 = DotGradient(Hash(x0, y0, z1, seed), fx, fy, fz - 1.0f);
    float n101 = DotGradient(Hash(x1, y0, z1, seed), fx - 1.0f, fy, fz - 1.0f);
    float n011 = DotGradient(Hash(x0, y1, z1, seed), fx, fy - 1.0f, fz - 1.0f);
    float n111 = DotGradient(Hash(x1, y1, z1, seed), fx - 1.0f, fy - 1.0f, fz - 1.0f);
    
    // Trilinear interpolation
    float x00 = Lerp(n000, n100, u);
    float x10 = Lerp(n010, n110, u);
    float x01 = Lerp(n001, n101, u);
    float x11 = Lerp(n011, n111, u);
    
    float y0_val = Lerp(x00, x10, v);
    float y1_val = Lerp(x01, x11, v);
    
    return Lerp(y0_val, y1_val, w);
}

float NoiseField::FBM(const math::Vec3& pos, float scale, uint32_t seed, int octaves, float persistence) {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = scale;
    float maxValue = 0.0f;
    
    for (int i = 0; i < octaves; ++i) {
        value += Perlin(pos, frequency, seed + i) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }
    
    return value / maxValue; // Normalize to [-1, 1]
}

float NoiseField::Value(const math::Vec3& pos, float scale, uint32_t seed) {
    math::Vec3 scaled = pos * scale;
    
    int x0 = static_cast<int>(std::floor(scaled.x));
    int y0 = static_cast<int>(std::floor(scaled.y));
    int z0 = static_cast<int>(std::floor(scaled.z));
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    int z1 = z0 + 1;
    
    float fx = scaled.x - static_cast<float>(x0);
    float fy = scaled.y - static_cast<float>(y0);
    float fz = scaled.z - static_cast<float>(z0);
    
    float u = Smoothstep(fx);
    float v = Smoothstep(fy);
    float w = Smoothstep(fz);
    
    // Get values at 8 corners
    float n000 = HashFloat(x0, y0, z0, seed);
    float n100 = HashFloat(x1, y0, z0, seed);
    float n010 = HashFloat(x0, y1, z0, seed);
    float n110 = HashFloat(x1, y1, z0, seed);
    float n001 = HashFloat(x0, y0, z1, seed);
    float n101 = HashFloat(x1, y0, z1, seed);
    float n011 = HashFloat(x0, y1, z1, seed);
    float n111 = HashFloat(x1, y1, z1, seed);
    
    // Trilinear interpolation
    float x00 = Lerp(n000, n100, u);
    float x10 = Lerp(n010, n110, u);
    float x01 = Lerp(n001, n101, u);
    float x11 = Lerp(n011, n111, u);
    
    float y0_val = Lerp(x00, x10, v);
    float y1_val = Lerp(x01, x11, v);
    
    return Lerp(y0_val, y1_val, w);
}

uint32_t NoiseField::Hash(uint32_t x, uint32_t y, uint32_t z, uint32_t seed) {
    uint32_t h = seed;
    h ^= x * 374761393U;
    h ^= y * 668265263U;
    h ^= z * 2246822507U;
    h = h ^ (h >> 13);
    h = h * 1274126177U;
    return h;
}

float NoiseField::HashFloat(uint32_t x, uint32_t y, uint32_t z, uint32_t seed) {
    uint32_t h = Hash(x, y, z, seed);
    return static_cast<float>(h & 0xFFFF) / 65536.0f * 2.0f - 1.0f; // [-1, 1]
}

float NoiseField::Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float NoiseField::Smoothstep(float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
}

math::Vec3 NoiseField::Gradient(int hash) {
    // Generate deterministic gradient from hash
    hash = hash & 15;
    float u = hash < 8 ? 1.0f : -1.0f;
    float v = hash < 4 ? 1.0f : (hash == 12 || hash == 14 ? 1.0f : -1.0f);
    float w = (hash & 2) == 0 ? 1.0f : -1.0f;
    return math::Vec3(u, v, w);
}

float NoiseField::DotGradient(int hash, float x, float y, float z) {
    math::Vec3 grad = Gradient(hash);
    return x * grad.x + y * grad.y + z * grad.z;
}

} // namespace geometry
} // namespace eyelib
