#pragma once

#include "asset/core/import_mesh.h"

#include <string>
#include <vector>

namespace asset {

/** Reads canonical .mesh binary into ImportMesh. Returns true on success. */
class MeshReader {
public:
    static bool Read(const std::string& path, ImportMesh& out);
    static bool Read(const std::vector<uint8_t>& bytes, ImportMesh& out);
};

} // namespace asset
