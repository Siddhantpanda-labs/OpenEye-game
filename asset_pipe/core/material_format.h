#pragma once

#include <cstdint>
#include <cstring>

namespace asset_pipe {

/** Canonical .material binary format. Shared by importer and runtime. */
namespace MaterialFormat {

constexpr uint32_t kMagic = 0x4D415431u;  // "MAT1"
constexpr uint32_t kVersion = 1u;
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
};

} // namespace MaterialFormat
} // namespace asset_pipe
