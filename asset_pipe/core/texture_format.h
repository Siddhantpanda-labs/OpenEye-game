#pragma once

#include <cstdint>

namespace asset_pipe {

/** Canonical .tex binary layout. Shared by importer (writer) and runtime (reader). */
namespace TextureFormat {

constexpr uint32_t kMagic = 0x54455831u;  // "TEX1"
constexpr uint32_t kVersion = 1u;

// Header: magic(4), version(4), width(4), height(4), channels(4).
// If pixels present: dataSize(4), then pixel bytes (width * height * channels).
// If no pixels (stub): pathLen(4), then path bytes.

} // namespace TextureFormat
} // namespace asset_pipe
