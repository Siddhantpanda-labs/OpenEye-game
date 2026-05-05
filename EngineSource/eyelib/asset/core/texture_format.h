#pragma once

#include <cstdint>

namespace asset {

/** Canonical .tex binary layout. Shared by importer (writer) and runtime (reader). */
namespace TextureFormat {

constexpr uint32_t kMagic = 0x54455831u;  // "TEX1"
constexpr uint32_t kVersion = 2u;
constexpr uint32_t kVersionV1 = 1u;

constexpr uint32_t kStorageRawPixels = 0u;
constexpr uint32_t kStorageEncodedImage = 1u;

// V2 layout:
// magic(4), version(4), width(4), height(4), channels(4), storage(4), dataSize(4), payload(dataSize)
// - storage=raw: payload is width*height*channels bytes
// - storage=encoded: payload is original encoded bytes (png/jpg/etc.), decoded at read time
//
// V1 layout (still readable):
// magic(4), version(4), width(4), height(4), channels(4), dataSize(4), payload
// where payload is raw pixel bytes only.

} // namespace TextureFormat
} // namespace asset
