#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace render {

/** E5.3: Pipeline cache key from shader name, topology, and optional keywords. */
std::string PipelineCacheKey(const std::string& shaderName,
                             rhi::PrimitiveTopology topology,
                             const std::vector<std::string>& keywords);

/** Cache directory (e.g. "shader_cache"). Set once; used by Save/Load. */
void SetPipelineCacheDirectory(const std::string& dir);
std::string GetPipelineCacheDirectory();

/**
 * Save pipeline binary to cache. data = format (4 bytes) + size (4 bytes) + binary.
 * Returns true on success.
 */
bool SavePipelineBinary(const std::string& key, uint32_t format, const void* data, size_t size);

/**
 * Load pipeline binary from cache. On success, outFormat and outData are set.
 * Returns true if found and read.
 */
bool LoadPipelineBinary(const std::string& key, uint32_t& outFormat, std::vector<uint8_t>& outData);

} // namespace render
} // namespace eyelib
