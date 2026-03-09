#pragma once

#include "eyelib/assets/EditableMesh.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace assets {

/** Canonical .mesh.bin chunk-based format (Mesh Pipeline). Versioned; no GPU layout. */

namespace MeshBin {

constexpr uint32_t kMagic = 0x4D455945u; // "EYEM"
constexpr uint32_t kVersion = 1u;

enum ChunkId : uint32_t {
    VERT = 0x54524556u, // "VERT"
    INDX = 0x58444E49u, // "INDX"
    SUBM = 0x4D425553u, // "SUBM"
    SKIN = 0x4E494B53u, // "SKIN"
    MORP = 0x50524F4Du, // "MORP" (future)
    COLL = 0x4C4C4F43u, // "COLL" (future)
};

/** Write EditableMesh to .mesh.bin (deterministic; no runtime metadata). */
bool WriteToFile(const EditableMesh& mesh, const std::string& path);

/** Read .mesh.bin into EditableMesh. Unknown chunks are skipped. */
bool ReadFromFile(const std::string& path, EditableMesh& out);

/** Serialize to memory (for streaming/cache). Returns empty on failure. */
std::vector<uint8_t> Serialize(const EditableMesh& mesh);

/** Deserialize from memory. Returns false on failure. */
bool Deserialize(const uint8_t* data, size_t size, EditableMesh& out);

} // namespace MeshBin

} // namespace assets
} // namespace eyelib
