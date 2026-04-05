#pragma once

#include <cstdint>
#include <cstring>

namespace asset {

/** Canonical .mat binary format. Shared by importer and runtime (Limitless-style single binary). */
namespace MaterialFormat {

constexpr uint32_t kMagic = 0x4D415431u;  // "MAT1"
constexpr uint32_t kVersion = 2u;
constexpr uint32_t kVersionV3 = 3u;
constexpr uint32_t kVersionV1 = 1u;
constexpr size_t kMaxPathLen = 512u;

struct MaterialHeader {
    uint32_t magic = kMagic;
    uint32_t version = kVersion;
    uint32_t nameLen = 0;
    uint32_t albedoPathLen = 0;
    uint32_t normalPathLen = 0;
    uint32_t metallicPathLen = 0;
    uint32_t roughnessPathLen = 0;
    float metallicValue = 0.f;
    float roughnessValue = 0.5f;
    float albedoColorR = 1.f;
    float albedoColorG = 1.f;
    float albedoColorB = 1.f;
};

#pragma pack(push, 1)
/** MAT1 v3: fixed payload; string blobs follow in order (name, albedo, normal, metallic, roughness, ao, emissive, MR, frag snippet, shading snippet). */
struct MaterialHeaderV3 {
    uint32_t magic = kMagic;
    uint32_t version = kVersionV3;
    uint32_t nameLen = 0;
    uint32_t albedoPathLen = 0;
    uint32_t normalPathLen = 0;
    uint32_t metallicPathLen = 0;
    uint32_t roughnessPathLen = 0;
    float metallicValue = 0.f;
    float roughnessValue = 0.5f;
    float albedoColorR = 1.f;
    float albedoColorG = 1.f;
    float albedoColorB = 1.f;
    float emissionR = 0.f;
    float emissionG = 0.f;
    float emissionB = 0.f;
    float aoValue = 1.f;
    float alpha = 1.f;
    uint32_t aoPathLen = 0;
    uint32_t emissivePathLen = 0;
    uint32_t mrPathLen = 0;
    uint32_t fragmentSnippetLen = 0;
    uint32_t shadingSnippetLen = 0;
    uint8_t shadingModel = 0;   // eyelib::render::ShadingModel
    uint8_t blending = 0;      // eyelib::render::Blending
    uint8_t flags = 0;          // bit0 two-sided, bit1 refraction
    uint8_t _pad = 0;
    float reflectiveStrength = 0.f;
    uint64_t materialShaderId = 0u;
};
#pragma pack(pop)

} // namespace MaterialFormat
} // namespace asset
