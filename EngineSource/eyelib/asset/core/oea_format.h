#pragma once

#include <cstdint>

namespace asset {
namespace OeaFormat {

constexpr uint32_t kMagic = 0x3141454Fu;  // "OEA1"
constexpr uint32_t kVersion = 2u;
constexpr uint32_t kVersionV1 = 1u;

constexpr uint32_t kFlagCompressedZstd = 1u << 0;

enum EntryType : uint32_t {
    Entry_Unknown = 0u,
    Entry_Mesh = 1u,
    Entry_Material = 2u,
    Entry_Texture = 3u,
    Entry_Animation = 4u,
    Entry_Prefab = 5u,
    Entry_MaterialMap = 6u
};

struct OeaHeader {
    uint32_t magic = kMagic;
    uint32_t version = kVersion;
    uint32_t entryCount = 0;
    uint32_t reserved = 0;
};

struct OeaEntryRecord {
    uint32_t type = Entry_Unknown;
    uint32_t nameLen = 0;
    uint32_t flags = 0;
    uint32_t reserved = 0;
    uint64_t offset = 0;
    uint64_t size = 0;
    uint64_t uncompressedSize = 0;
};

} // namespace OeaFormat
} // namespace asset

