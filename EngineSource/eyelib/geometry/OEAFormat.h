#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace geometry {
class EditableMesh;
}
}

namespace eyelib {
namespace geometry {

/**
 * Single engine container format: .oea (OpenEye Asset).
 * One extension, one loader; header defines internal type.
 */
namespace OEA {

constexpr uint32_t kMagic = 0x0041454Fu; // "OEA\0"
constexpr uint32_t kVersion = 1u;
constexpr size_t kHeaderSize = 16u;

enum class AssetType : uint32_t {
    Mesh = 0,
    Skeleton = 1,
    Animation = 2,
    Material = 3,
    Texture = 4,
    Unknown = 0xFFFFFFFFu,
};

/** Write .oea file: header (magic, version, type) + payload. */
bool WriteToFile(AssetType type, const uint8_t* payload, size_t payloadSize, const std::string& path);

/** Read .oea file. Returns type and payload. Returns Unknown and empty payload on failure. */
bool ReadFromFile(const std::string& path, AssetType& outType, std::vector<uint8_t>& outPayload);

/** Check if file looks like .oea (magic at start). */
bool IsOEAFile(const uint8_t* data, size_t size);

/** Write mesh as .oea (type = Mesh, payload = mesh binary). */
bool WriteMeshToFile(const EditableMesh& mesh, const std::string& path);

/** Read mesh from .oea. Returns false if file is not .oea or type is not Mesh. */
bool ReadMeshFromFile(const std::string& path, EditableMesh& out);

} // namespace OEA

} // namespace geometry
} // namespace eyelib
