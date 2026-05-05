#include "eyelib/rendering/lighting/ibl/IblCpuDefaults.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace eyelib {
namespace rendering {
namespace ibl_cpu {
namespace {

constexpr float kPi = 3.14159265358979323846f;

static uint16_t FloatToHalf(float value) {
    uint32_t f32 = 0;
    std::memcpy(&f32, &value, sizeof(uint32_t));
    const uint32_t sign = (f32 >> 16) & 0x8000u;
    int32_t exponent = static_cast<int32_t>((f32 >> 23) & 0xffu) - 127 + 15;
    uint32_t mantissa = f32 & 0x7fffffu;
    if (exponent <= 0) {
        if (exponent < -10)
            return static_cast<uint16_t>(sign);
        mantissa |= 0x800000u;
        const int32_t t = static_cast<int32_t>(mantissa >> static_cast<uint32_t>(1 - exponent));
        const uint32_t round = (static_cast<uint32_t>(t) >> 12) & 1u;
        return static_cast<uint16_t>(sign | ((static_cast<uint32_t>(t) >> 13) + round));
    }
    if (exponent >= 31) {
        if (mantissa == 0)
            return static_cast<uint16_t>(sign | 0x7c00u);
        return static_cast<uint16_t>(sign | 0x7e00u);
    }
    return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13));
}

static void WriteRgba16f(std::vector<uint8_t>& dst, size_t offset, float r, float g, float b, float a) {
    const uint16_t pr = FloatToHalf(r);
    const uint16_t pg = FloatToHalf(g);
    const uint16_t pb = FloatToHalf(b);
    const uint16_t pa = FloatToHalf(a);
    std::memcpy(dst.data() + offset + 0, &pr, 2);
    std::memcpy(dst.data() + offset + 2, &pg, 2);
    std::memcpy(dst.data() + offset + 4, &pb, 2);
    std::memcpy(dst.data() + offset + 6, &pa, 2);
}

static float RadicalInverseVdC(uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

static void Hammersley(uint32_t i, uint32_t n, float* outX, float* outY) {
    *outX = static_cast<float>(i) / static_cast<float>(n);
    *outY = RadicalInverseVdC(i);
}

static float GeometrySchlickGGX(float nDotX, float roughness) {
    const float r = roughness + 1.f;
    const float k = (r * r) / 8.f;
    return nDotX / std::max(nDotX * (1.f - k) + k, 1e-7f);
}

static float GeometrySmith(float nDotV, float nDotL, float roughness) {
    return GeometrySchlickGGX(nDotV, roughness) * GeometrySchlickGGX(nDotL, roughness);
}

static void ImportanceSampleGGX(float xiX, float xiY, float roughness, float hx, float hy, float hz, float* outHx,
                                float* outHy, float* outHz) {
    const float a = roughness * roughness;
    const float phi = 2.f * kPi * xiX;
    const float cosTheta = std::sqrt(std::max(0.f, (1.f - xiY) / (1.f + (a * a - 1.f) * xiY)));
    const float sinTheta = std::sqrt(std::max(0.f, 1.f - cosTheta * cosTheta));
    const float sx = sinTheta * std::cos(phi);
    const float sy = sinTheta * std::sin(phi);
    const float sz = cosTheta;
    const float n[3] = {hx, hy, hz};
    float up[3] = {0.f, 1.f, 0.f};
    if (std::fabs(n[1]) > 0.999f) {
        up[0] = 1.f;
        up[1] = 0.f;
        up[2] = 0.f;
    }
    float t[3] = {up[1] * n[2] - up[2] * n[1], up[2] * n[0] - up[0] * n[2], up[0] * n[1] - up[1] * n[0]};
    const float tLen = std::sqrt(std::max(t[0] * t[0] + t[1] * t[1] + t[2] * t[2], 1e-12f));
    t[0] /= tLen;
    t[1] /= tLen;
    t[2] /= tLen;
    const float b[3] = {n[1] * t[2] - n[2] * t[1], n[2] * t[0] - n[0] * t[2], n[0] * t[1] - n[1] * t[0]};
    *outHx = t[0] * sx + b[0] * sy + n[0] * sz;
    *outHy = t[1] * sx + b[1] * sy + n[1] * sz;
    *outHz = t[2] * sx + b[2] * sy + n[2] * sz;
}

static void IntegrateBrdf(float nDotV, float roughness, uint32_t sampleCount, float* outA, float* outB) {
    const uint32_t nSamples = (std::max)(sampleCount, 1u);
    float sumA = 0.f;
    float sumB = 0.f;
    const float v[3] = {std::sqrt(std::max(0.f, 1.f - nDotV * nDotV)), 0.f, nDotV};
    const float n[3] = {0.f, 0.f, 1.f};
    for (uint32_t i = 0; i < nSamples; ++i) {
        float xiX = 0.f;
        float xiY = 0.f;
        Hammersley(i, nSamples, &xiX, &xiY);
        float hx = 0.f;
        float hy = 0.f;
        float hz = 0.f;
        ImportanceSampleGGX(xiX, xiY, roughness, n[0], n[1], n[2], &hx, &hy, &hz);
        const float hLen = std::sqrt(std::max(hx * hx + hy * hy + hz * hz, 1e-12f));
        hx /= hLen;
        hy /= hLen;
        hz /= hLen;
        const float voH = std::max(v[0] * hx + v[1] * hy + v[2] * hz, 0.f);
        const float lx = 2.f * voH * hx - v[0];
        const float ly = 2.f * voH * hy - v[1];
        const float lz = 2.f * voH * hz - v[2];
        const float nDotL = std::max(lz, 0.f);
        const float nDotH = std::max(hz, 0.f);
        if (nDotL > 0.f) {
            const float g = GeometrySmith(nDotV, nDotL, roughness);
            const float gVis = (g * voH) / std::max(nDotH * nDotV, 1e-7f);
            const float fc = std::pow(1.f - voH, 5.f);
            sumA += (1.f - fc) * gVis;
            sumB += fc * gVis;
        }
    }
    const float inv = 1.f / static_cast<float>(nSamples);
    *outA = sumA * inv;
    *outB = sumB * inv;
}

static void CubemapTexelDirection(uint32_t face, float u01, float v01, float* dx, float* dy, float* dz) {
    const float u = u01 * 2.f - 1.f;
    const float v = v01 * 2.f - 1.f;
    switch (face) {
    case 0u:
        *dx = 1.f;
        *dy = -v;
        *dz = -u;
        break;
    case 1u:
        *dx = -1.f;
        *dy = -v;
        *dz = u;
        break;
    case 2u:
        *dx = u;
        *dy = 1.f;
        *dz = v;
        break;
    case 3u:
        *dx = u;
        *dy = -1.f;
        *dz = -v;
        break;
    case 4u:
        *dx = u;
        *dy = -v;
        *dz = 1.f;
        break;
    default:
        *dx = -u;
        *dy = -v;
        *dz = -1.f;
        break;
    }
    const float len = std::sqrt(std::max(*dx * *dx + *dy * *dy + *dz * *dz, 1e-12f));
    *dx /= len;
    *dy /= len;
    *dz /= len;
}

static float Smoothstep(float e0, float e1, float x) {
    const float t = std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

} // namespace

void BuildBrdfLutRgba16F(std::vector<uint8_t>& outRgba16f, uint32_t dimension, uint32_t sampleCount) {
    const uint32_t d = std::max(2u, dimension);
    const uint32_t samples = (std::max)(sampleCount, 1u);
    outRgba16f.resize(static_cast<size_t>(d) * static_cast<size_t>(d) * 8u);
    for (uint32_t y = 0; y < d; ++y) {
        for (uint32_t x = 0; x < d; ++x) {
            const float nDotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(d);
            const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(d);
            float aa = 0.f;
            float bb = 0.f;
            IntegrateBrdf(std::max(nDotV, 1e-4f), std::max(roughness, 1e-4f), samples, &aa, &bb);
            const size_t off = (static_cast<size_t>(y) * d + x) * 8u;
            WriteRgba16f(outRgba16f, off, aa, bb, 0.f, 1.f);
        }
    }
}

void BuildNeutralSkylightCubemapRgba16F(std::vector<uint8_t>& outRgba16f, uint32_t faceSize) {
    const uint32_t s = std::max(2u, faceSize);
    outRgba16f.resize(static_cast<size_t>(s) * static_cast<size_t>(s) * 6u * 8u);
    size_t faceBase = 0;
    for (uint32_t face = 0; face < 6u; ++face) {
        for (uint32_t iy = 0; iy < s; ++iy) {
            for (uint32_t ix = 0; ix < s; ++ix) {
                const float u = (static_cast<float>(ix) + 0.5f) / static_cast<float>(s);
                const float v = (static_cast<float>(iy) + 0.5f) / static_cast<float>(s);
                float dx = 0.f;
                float dy = 0.f;
                float dz = 0.f;
                CubemapTexelDirection(face, u, v, &dx, &dy, &dz);
                const float skyT = dy * 0.5f + 0.5f;
                const float gnd = Smoothstep(0.f, 0.38f, skyT);
                const float zen = Smoothstep(0.36f, 1.f, skyT);
                float r = 0.02f * (1.f - gnd) + 0.08f * gnd;
                float g = 0.022f * (1.f - gnd) + 0.09f * gnd;
                float b = 0.03f * (1.f - gnd) + 0.12f * gnd;
                const float zr = 0.12f;
                const float zg = 0.22f;
                const float zb = 0.45f;
                r = r * (1.f - zen) + zr * zen;
                g = g * (1.f - zen) + zg * zen;
                b = b * (1.f - zen) + zb * zen;
                const size_t off = faceBase + (static_cast<size_t>(iy) * s + ix) * 8u;
                WriteRgba16f(outRgba16f, off, r, g, b, 1.f);
            }
        }
        faceBase += static_cast<size_t>(s) * static_cast<size_t>(s) * 8u;
    }
}

} // namespace ibl_cpu
} // namespace rendering
} // namespace eyelib
