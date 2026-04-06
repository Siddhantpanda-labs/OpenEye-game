#pragma once

#include <cstdint>
#include <cstring>

namespace asset {

/**
 * MAT1 — canonical binary material asset.
 *
 * A single on-disk revision is supported: `kVersion` (currently 1). The header is a fixed,
 * packed prefix; variable-length UTF-8 path/snippet blobs follow in a fixed order, then an
 * optional little-endian "MATX" extension (see MaterialReader).
 */
namespace MaterialFormat {

constexpr uint32_t kMagic = 0x4D415431u;  // "MAT1"
/** Format revision stored in-file at offset 4; bump only when the header or blob order changes. */
constexpr uint32_t kVersion = 1u;

constexpr size_t kMaxPathLen = 512u;

#pragma pack(push, 1)
/** Fixed header (104 bytes packed); string blobs follow: name, then per-slot dashed texture asset GUIDs (or empty), then snippets. */
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
    uint8_t blending = 0;       // eyelib::render::Blending
    uint8_t flags = 0;          // bit0 two-sided, bit1 refraction
    uint8_t _pad = 0;
    float reflectiveStrength = 0.f;
    uint64_t materialShaderId = 0u;
};
#pragma pack(pop)

static_assert(sizeof(MaterialHeader) == 104u, "MaterialHeader packed size");

} // namespace MaterialFormat
} // namespace asset
